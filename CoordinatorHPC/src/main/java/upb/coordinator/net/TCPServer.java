package upb.coordinator.net;

import com.fasterxml.jackson.databind.ObjectMapper;
import upb.coordinator.model.WorkerNode;
import upb.coordinator.protocol.MessageType;
import upb.coordinator.protocol.NetworkMessage;
import upb.coordinator.service.ResultAggregator;
import upb.coordinator.service.TaskSplitter;
import upb.coordinator.service.WorkerRegistry;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class TCPServer {
    private final int port;
    private final WorkerRegistry workerRegistry;
    private final TaskSplitter taskSplitter;
    private final ResultAggregator resultAggregator;
    private final ObjectMapper objectMapper;
    private final ExecutorService threadPool;
    private boolean running;

    public TCPServer(int port) {
        this.port = port;
        this.workerRegistry = new WorkerRegistry();
        this.taskSplitter = new TaskSplitter();
        this.resultAggregator = new ResultAggregator();
        this.objectMapper = new ObjectMapper();
        this.threadPool = Executors.newVirtualThreadPerTaskExecutor();
    }

    public void start() {
        this.running = true;
        try (ServerSocket serverSocket = new ServerSocket(port)) {
            System.out.println("[Coordinator] HPC Coordinator Server listening on port " + port + "...");

            while (running) {
                Socket clientSocket = serverSocket.accept();
                threadPool.submit(() -> handleConnection(clientSocket));
            }
        } catch (Exception e) {
            System.err.println("[Coordinator] Server error: " + e.getMessage());
        }
    }

    private void handleConnection(Socket socket) {
        try {
            BufferedReader reader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            PrintWriter writer = new PrintWriter(socket.getOutputStream(), true);

            String line = reader.readLine();
            if (line == null) {
                socket.close();
                return;
            }

            NetworkMessage initialMsg = objectMapper.readValue(line, NetworkMessage.class);

            if (initialMsg.getType() == MessageType.REGISTER_WORKER) {
                String workerId = initialMsg.getWorkerId() != null ? initialMsg.getWorkerId() : UUID.randomUUID().toString();
                WorkerNode worker = new WorkerNode(workerId, socket, reader, writer);
                workerRegistry.registerWorker(worker);
                
                // Hilo de monitoreo para detectar la desconexión del worker
                threadPool.submit(() -> monitorWorkerDisconnect(worker));

            } else if (initialMsg.getType() == MessageType.SUBMIT_JOB) {
                handleJobSubmission(initialMsg, writer);
                socket.close();
            } else {
                NetworkMessage errorMsg = new NetworkMessage(MessageType.ERROR);
                errorMsg.setError("Unexpected message type: " + initialMsg.getType());
                writer.println(objectMapper.writeValueAsString(errorMsg));
                socket.close();
            }

        } catch (Exception e) {
            System.err.println("[Coordinator] Error handling connection: " + e.getMessage());
        }
    }

    private void monitorWorkerDisconnect(WorkerNode worker) {
        try {
            // Intenta leer del socket hasta que la conexión se cierre (-1 o excepción)
            String input;
            while ((input = worker.getReader().readLine()) != null) {
                // Si el worker envía mensajes heartbeat u otros mensajes fuera de tareas, se pueden procesar aquí
            }
        } catch (Exception ignored) {
        } finally {
            System.out.println("[Coordinator] WARNING: Worker disconnected -> ID: " + worker.getWorkerId());
            workerRegistry.unregisterWorker(worker.getWorkerId());
            try {
                worker.getSocket().close();
            } catch (Exception ignored) {}
        }
    }

    private void handleJobSubmission(NetworkMessage jobMsg, PrintWriter clientWriter) {
        long jobStartTimeMs = System.currentTimeMillis();
        try {
            List<WorkerNode> activeWorkers = workerRegistry.getActiveWorkers();
            int numWorkers = activeWorkers.size();

            if (numWorkers == 0) {
                NetworkMessage errorResponse = new NetworkMessage(MessageType.ERROR);
                errorResponse.setError("No active workers registered to execute the job.");
                clientWriter.println(objectMapper.writeValueAsString(errorResponse));
                return;
            }

            List<Object> fullDataset = jobMsg.getDatasetChunk();
            String code = jobMsg.getCode();
            String jobId = jobMsg.getJobId() != null ? jobMsg.getJobId() : UUID.randomUUID().toString();

            System.out.println("\n========================================================");
            System.out.println("[Coordinator] STARTING JOB " + jobId);
            System.out.println("[Coordinator] Total Items: " + (fullDataset != null ? fullDataset.size() : 0));
            System.out.println("[Coordinator] Workers Allocated: " + numWorkers);
            System.out.println("========================================================");

            long splitStartTimeMs = System.currentTimeMillis();
            List<List<Object>> datasetPartitions = taskSplitter.split(fullDataset, numWorkers);
            long splitTimeMs = System.currentTimeMillis() - splitStartTimeMs;
            System.out.println("[Metrics] Task partitioning completed in " + splitTimeMs + " ms.");

            List<CompletableFuture<WorkerResponse>> futures = new ArrayList<>();
            List<List<Object>> synchronizedResults = Collections.synchronizedList(new ArrayList<>());

            long dispatchStartTimeMs = System.currentTimeMillis();

            for (int i = 0; i < numWorkers; i++) {
                WorkerNode worker = activeWorkers.get(i);
                List<Object> partition = datasetPartitions.get(i);

                CompletableFuture<WorkerResponse> future = CompletableFuture.supplyAsync(() -> {
                    long workerDispatchStart = System.currentTimeMillis();
                    try {
                        NetworkMessage chunkMsg = new NetworkMessage(MessageType.EXECUTE_CHUNK);
                        chunkMsg.setJobId(jobId);
                        chunkMsg.setCode(code);
                        chunkMsg.setDatasetChunk(partition);

                        synchronized (worker) {
                            worker.getWriter().println(objectMapper.writeValueAsString(chunkMsg));
                            String responseLine = worker.getReader().readLine();
                            long roundTripTime = System.currentTimeMillis() - workerDispatchStart;

                            if (responseLine != null) {
                                NetworkMessage workerResp = objectMapper.readValue(responseLine, NetworkMessage.class);
                                if (workerResp.getType() == MessageType.CHUNK_RESULT) {
                                    System.out.println("[Metrics] Worker [" + worker.getWorkerId() + "] finished chunk in " 
                                            + workerResp.getExecutionTimeMs() + " ms (Total Round Trip: " + roundTripTime + " ms).");
                                    return new WorkerResponse(workerResp.getResultChunk(), roundTripTime);
                                }
                            }
                        }
                    } catch (Exception e) {
                        System.err.println("[Coordinator] Error dispatching task to worker " + worker.getWorkerId() + ": " + e.getMessage());
                        System.out.println("[Coordinator] WARNING: Worker disconnected during task execution -> ID: " + worker.getWorkerId());
                        workerRegistry.unregisterWorker(worker.getWorkerId());
                    }
                    return new WorkerResponse(Collections.emptyList(), 0);
                }, threadPool);

                futures.add(future);
            }

            // Esperar que todos los trabajadores completen
            CompletableFuture.allOf(futures.toArray(new CompletableFuture[0])).join();
            long dispatchTotalTimeMs = System.currentTimeMillis() - dispatchStartTimeMs;

            for (CompletableFuture<WorkerResponse> f : futures) {
                synchronizedResults.add(f.get().results);
            }

            long aggregateStartTimeMs = System.currentTimeMillis();
            List<Object> finalAggregatedResult = resultAggregator.aggregate(synchronizedResults);
            long aggregateTimeMs = System.currentTimeMillis() - aggregateStartTimeMs;

            long totalHpcExecutionTimeMs = System.currentTimeMillis() - jobStartTimeMs;

            System.out.println("\n--------------------------------------------------------");
            System.out.println("[Coordinator] METRICS SUMMARY FOR JOB " + jobId + ":");
            System.out.println("  - Dataset Partitioning Time: " + splitTimeMs + " ms");
            System.out.println("  - Parallel Execution & Dispatch Time: " + dispatchTotalTimeMs + " ms");
            System.out.println("  - Result Aggregation Time: " + aggregateTimeMs + " ms");
            System.out.println("  - TOTAL HPC JOB COMPUTATION TIME: " + totalHpcExecutionTimeMs + " ms");
            System.out.println("========================================================\n");

            NetworkMessage resultMsg = new NetworkMessage(MessageType.JOB_RESULT);
            resultMsg.setJobId(jobId);
            resultMsg.setResultChunk(finalAggregatedResult);
            resultMsg.setExecutionTimeMs(totalHpcExecutionTimeMs);

            clientWriter.println(objectMapper.writeValueAsString(resultMsg));

        } catch (Exception e) {
            System.err.println("[Coordinator] Error executing job: " + e.getMessage());
            try {
                NetworkMessage errorResponse = new NetworkMessage(MessageType.ERROR);
                errorResponse.setError("Internal coordinator error: " + e.getMessage());
                clientWriter.println(objectMapper.writeValueAsString(errorResponse));
            } catch (Exception ignored) {}
        }
    }

    private static record WorkerResponse(List<Object> results, long roundTripMs) {}
}
