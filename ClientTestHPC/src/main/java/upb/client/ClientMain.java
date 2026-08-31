package upb.client;

import com.fasterxml.jackson.databind.ObjectMapper;
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
        String host = "localhost";
        int port = 8080;

        System.out.println("=== HPC Service Demo Client ===");

        // 1. Crear un dataset de prueba masivo
        List<Object> dataset = new ArrayList<>();
        int dataSize = 50;
        for (int i = 1; i <= dataSize; i++) {
            dataset.add(i);
        }

        String codeInstruction = "SQUARE"; // Operación enviada a los trabajadores
        String jobId = "JOB-" + UUID.randomUUID().toString().substring(0, 6);

        System.out.println("[Client] Submitting Job ID: " + jobId);
        System.out.println("[Client] Dataset Size: " + dataset.size() + " elements");
        System.out.println("[Client] Executable Code Rule: " + codeInstruction);

        ObjectMapper objectMapper = new ObjectMapper();

        try (Socket socket = new Socket(host, port);
             BufferedReader reader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
             PrintWriter writer = new PrintWriter(socket.getOutputStream(), true)) {

            NetworkMessage jobRequest = new NetworkMessage(MessageType.SUBMIT_JOB);
            jobRequest.setJobId(jobId);
            jobRequest.setCode(codeInstruction);
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
                    System.out.println("[Client Metrics] HPC Server Processing Time: " + response.getExecutionTimeMs() + " ms");
                    System.out.println("[Client Metrics] End-to-End Client Round-Trip: " + clientTotalTime + " ms");
                    System.out.println("--------------------------------------------------------");
                    System.out.println("[Client] Aggregated Final Result (" + response.getResultChunk().size() + " items):");
                    System.out.println("--> " + response.getResultChunk());
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
