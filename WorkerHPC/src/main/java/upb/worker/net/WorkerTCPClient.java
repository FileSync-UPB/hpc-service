package upb.worker.net;

import com.fasterxml.jackson.databind.ObjectMapper;
import upb.worker.executor.TaskExecutor;
import upb.worker.protocol.MessageType;
import upb.worker.protocol.NetworkMessage;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;
import java.util.List;

public class WorkerTCPClient {
    private final String host;
    private final int port;
    private final String workerId;
    private final TaskExecutor taskExecutor;
    private final ObjectMapper objectMapper;

    public WorkerTCPClient(String host, int port, String workerId) {
        this.host = host;
        this.port = port;
        this.workerId = workerId;
        this.taskExecutor = new TaskExecutor();
        this.objectMapper = new ObjectMapper();
    }

    public void start() {
        System.out.println("[Worker " + workerId + "] Connecting to Coordinator at " + host + ":" + port + "...");
        try (Socket socket = new Socket(host, port);
             BufferedReader reader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
             PrintWriter writer = new PrintWriter(socket.getOutputStream(), true)) {

            // Register with coordinator
            NetworkMessage registerMsg = new NetworkMessage(MessageType.REGISTER_WORKER);
            registerMsg.setWorkerId(workerId);
            writer.println(objectMapper.writeValueAsString(registerMsg));
            System.out.println("[Worker " + workerId + "] Registered successfully. Waiting for tasks...");

            String line;
            while ((line = reader.readLine()) != null) {
                NetworkMessage taskMsg = objectMapper.readValue(line, NetworkMessage.class);

                if (taskMsg.getType() == MessageType.EXECUTE_CHUNK) {
                    System.out.println("[Worker " + workerId + "] Received EXECUTE_CHUNK for Job: " + taskMsg.getJobId() 
                            + " | Chunk size: " + (taskMsg.getDatasetChunk() != null ? taskMsg.getDatasetChunk().size() : 0));
                    
                    long startTime = System.currentTimeMillis();
                    List<Object> resultChunk = taskExecutor.execute(taskMsg.getCode(), taskMsg.getDatasetChunk());
                    long executionTime = System.currentTimeMillis() - startTime;

                    NetworkMessage responseMsg = new NetworkMessage(MessageType.CHUNK_RESULT);
                    responseMsg.setJobId(taskMsg.getJobId());
                    responseMsg.setWorkerId(workerId);
                    responseMsg.setResultChunk(resultChunk);
                    responseMsg.setExecutionTimeMs(executionTime);

                    writer.println(objectMapper.writeValueAsString(responseMsg));
                    System.out.println("[Worker " + workerId + "] Completed chunk for Job: " + taskMsg.getJobId() 
                            + " in " + executionTime + " ms.");
                }
            }

        } catch (Exception e) {
            System.err.println("[Worker " + workerId + "] Disconnected or connection error: " + e.getMessage());
        }
    }
}
