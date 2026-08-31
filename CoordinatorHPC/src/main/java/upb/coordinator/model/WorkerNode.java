package upb.coordinator.model;

import java.io.BufferedReader;
import java.io.PrintWriter;
import java.net.Socket;

public class WorkerNode {
    private final String workerId;
    private final Socket socket;
    private final BufferedReader reader;
    private final PrintWriter writer;

    public WorkerNode(String workerId, Socket socket, BufferedReader reader, PrintWriter writer) {
        this.workerId = workerId;
        this.socket = socket;
        this.reader = reader;
        this.writer = writer;
    }

    public String getWorkerId() {
        return workerId;
    }

    public Socket getSocket() {
        return socket;
    }

    public BufferedReader getReader() {
        return reader;
    }

    public PrintWriter getWriter() {
        return writer;
    }
}
