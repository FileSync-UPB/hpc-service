#include "hpc/worker/worker_node.hpp"
#include "hpc/network/tcp_client.hpp"

#include <iostream>

namespace hpc {

WorkerNode::WorkerNode(WorkerConfig config)
    : config_(std::move(config)) {}

WorkerNode::~WorkerNode() {
    disconnect();
}

bool WorkerNode::connect_and_register() {
    if (connected_.load()) {
        return true;
    }

    try {
        Socket sock = TcpClient::connect(
            config_.master_host,
            config_.master_port,
            config_.connect_timeout_ms
        );

        connection_ = Connection(std::move(sock));
        connection_.set_timeout(config_.socket_timeout_ms);

        // Enviar mensaje HELLO con el nombre solicitado en el payload
        Message hello(MessageType::HELLO, 1, config_.worker_name);
        connection_.send_message(hello);

        // Esperar HELLO_ACK
        Message ack = connection_.receive_message();
        if (ack.type != MessageType::HELLO_ACK) {
            std::cerr << "Respuesta inesperada al HELLO: " << to_string(ack.type) << "\n";
            connection_.close();
            return false;
        }

        // Decodificar el WorkerId (primeros 4 bytes big-endian)
        if (ack.payload.size() >= 4) {
            assigned_id_ = (static_cast<uint32_t>(ack.payload[0]) << 24) |
                           (static_cast<uint32_t>(ack.payload[1]) << 16) |
                           (static_cast<uint32_t>(ack.payload[2]) << 8)  |
                           static_cast<uint32_t>(ack.payload[3]);
        } else {
            assigned_id_ = 1; // Fallback si no viene id en payload
        }

        connected_.store(true);
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error al conectar o registrar Worker '" << config_.worker_name << "': "
                  << e.what() << "\n";
        connection_.close();
        return false;
    }
}

bool WorkerNode::ping(uint64_t ping_id) {
    if (!connected_.load()) {
        return false;
    }

    try {
        Message ping_msg(MessageType::PING, ping_id, std::string("ping"));
        connection_.send_message(ping_msg);

        Message pong_msg = connection_.receive_message();
        return (pong_msg.type == MessageType::PONG && pong_msg.id == ping_id);
    } catch (...) {
        connected_.store(false);
        return false;
    }
}

void WorkerNode::disconnect() {
    if (connected_.exchange(false)) {
        try {
            Message shutdown_msg(MessageType::SHUTDOWN, 999);
            connection_.send_message(shutdown_msg);
        } catch (...) {}
        connection_.close();
    }
}

bool WorkerNode::is_connected() const {
    return connected_.load() && connection_.is_open();
}

WorkerId WorkerNode::get_worker_id() const {
    return assigned_id_;
}

const std::string& WorkerNode::get_name() const {
    return config_.worker_name;
}

Connection& WorkerNode::get_connection() {
    return connection_;
}

} // namespace hpc
