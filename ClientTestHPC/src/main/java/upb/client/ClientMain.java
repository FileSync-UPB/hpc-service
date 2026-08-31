package upb.client;

import com.fasterxml.jackson.databind.ObjectMapper;
import upb.client.config.Config;
import upb.client.protocol.MessageType;
import upb.client.protocol.NetworkMessage;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class ClientMain {
    public static void main(String[] args) {
        String host = Config.getCoordinatorHost();
        int port = Config.getCoordinatorPort();

        System.out.println("=== HPC Service Demo Client (Python Execution) ===");
        System.out.println("[Client] Loaded Configuration -> TARGET HOST: " + host + ", PORT: " + port);

        // 1. Crear dataset de datos de entrada (ejemplo: números del 1 al 20)
        List<Object> dataset = new ArrayList<>();
        int dataSize = 20;
        for (int i = 1; i <= dataSize; i++) {
            dataset.add(i);
        }

        // 2. Código Python REAL que será enviado a los trabajadores
        String pythonCode = """
def process(item):
    val = item * item
    return {
        "original": item,
        "squared": val,
        "is_even": val % 2 == 0
    }
""";

        String jobId = "JOB-PY-" + UUID.randomUUID().toString().substring(0, 6);

        System.out.println("[Client] Submitting Job ID: " + jobId);
        System.out.println("[Client] Dataset Size: " + dataset.size() + " elements");
        System.out.println("[Client] Python Code to Execute:\n" + pythonCode);

        ObjectMapper objectMapper = new ObjectMapper();

        try (Socket socket = new Socket(host, port);
             BufferedReader reader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
             PrintWriter writer = new PrintWriter(socket.getOutputStream(), true)) {

            NetworkMessage jobRequest = new NetworkMessage(MessageType.SUBMIT_JOB);
            jobRequest.setJobId(jobId);
            jobRequest.setCode(pythonCode);
            jobRequest.setDatasetChunk(dataset);

            long clientStartTime = System.currentTimeMillis();
            writer.println(objectMapper.writeValueAsString(jobRequest));

            String responseLine = reader.readLine();
            long clientEndTime = System.currentTimeMillis();
            long clientTotalTime = clientEndTime - clientStartTime;

            if (responseLine != null) {
                NetworkMessage response = objectMapper.readValue(responseLine, NetworkMessage.class);
                if (response.getType() == MessageType.JOB_RESULT) {
                    System.out.println("\n========================================================");
                    System.out.println("[Client] SUCCESS! Received JOB_RESULT for Job ID: " + jobId);
                    System.out.println("--------------------------------------------------------");
                    System.out.println("[Client Metrics] HPC Server Computation Time: " + response.getExecutionTimeMs() + " ms");
                    System.out.println("[Client Metrics] Client End-to-End Round-Trip: " + clientTotalTime + " ms");
                    System.out.println("--------------------------------------------------------");
                    System.out.println("[Client] Aggregated Final Result (" + response.getResultChunk().size() + " items):");
                    for (Object resItem : response.getResultChunk()) {
                        System.out.println("  -> " + resItem);
                    }
                    System.out.println("========================================================\n");
                } else if (response.getType() == MessageType.ERROR) {
                    System.err.println("\n[Client] ERROR from Coordinator: " + response.getError());
                }
            } else {
                System.err.println("\n[Client] Server closed connection without response.");
            }

        } catch (Exception e) {
            System.err.println("[Client] Failed to execute job: " + e.getMessage());
        }
    }
}
