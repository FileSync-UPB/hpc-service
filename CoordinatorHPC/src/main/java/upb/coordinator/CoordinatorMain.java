package upb.coordinator;

import upb.coordinator.config.Config;
import upb.coordinator.net.TCPServer;

public class CoordinatorMain {
    public static void main(String[] args) {
        int port = Config.getPort();
        if (args.length > 0) {
            try {
                port = Integer.parseInt(args[0]);
            } catch (NumberFormatException ignored) {}
        }
        System.out.println("[Coordinator] Loaded Configuration -> PORT: " + port);
        TCPServer server = new TCPServer(port);
        server.start();
    }
}
