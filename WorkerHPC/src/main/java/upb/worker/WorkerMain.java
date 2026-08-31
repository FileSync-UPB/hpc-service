package upb.worker;

import upb.worker.net.WorkerTCPClient;
import java.util.UUID;

public class WorkerMain {
    public static void main(String[] args) {
        String host = "localhost";
        int port = 8080;
        String workerId = "Worker-" + UUID.randomUUID().toString().substring(0, 5);

        if (args.length > 0) {
            workerId = args[0];
        }
        if (args.length > 1) {
            host = args[1];
        }
        if (args.length > 2) {
            try {
                port = Integer.parseInt(args[2]);
            } catch (NumberFormatException ignored) {}
        }

        WorkerTCPClient client = new WorkerTCPClient(host, port, workerId);
        client.start();
    }
}
