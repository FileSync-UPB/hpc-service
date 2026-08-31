package upb.worker;

import upb.worker.config.Config;
import upb.worker.net.WorkerTCPClient;
import java.util.UUID;

public class WorkerMain {
    public static void main(String[] args) {
        String host = Config.getCoordinatorHost();
        int port = Config.getCoordinatorPort();
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

        System.out.println("[Worker] Loaded Configuration -> HOST: " + host + ", PORT: " + port + ", ID: " + workerId);
        WorkerTCPClient client = new WorkerTCPClient(host, port, workerId);
        client.start();
    }
}
