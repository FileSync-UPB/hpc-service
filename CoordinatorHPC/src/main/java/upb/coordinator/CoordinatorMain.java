package upb.coordinator;

import upb.coordinator.net.TCPServer;

public class CoordinatorMain {
    public static void main(String[] args) {
        int port = 8080;
        if (args.length > 0) {
            try {
                port = Integer.parseInt(args[0]);
            } catch (NumberFormatException ignored) {}
        }
        TCPServer server = new TCPServer(port);
        server.start();
    }
}
