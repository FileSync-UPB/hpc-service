#include "hpc/network/connection.hpp"

namespace hpc {

Connection::Connection(Socket socket) : socket_(std::move(socket)) {
    // Activar TCP_NODELAY por defecto para minimizar latencia en RPC/mensajes
    if (socket_.is_valid()) {
        socket_.set_no_delay(true);
    }
}

void Connection::send_message(const Message& msg) {
    std::vector<uint8_t> serialized = serialize_message(msg);
    socket_.send_all(serialized);
}

Message Connection::receive_message() {
    uint8_t header_bytes[MESSAGE_HEADER_SIZE];
    socket_.recv_exact(header_bytes, MESSAGE_HEADER_SIZE);

    MessageHeader header = deserialize_header(header_bytes, MESSAGE_HEADER_SIZE);

    std::vector<uint8_t> payload;
    if (header.payload_size > 0) {
        payload.resize(header.payload_size);
        socket_.recv_exact(payload.data(), header.payload_size);
    }

    return Message(header.type, header.id, std::move(payload));
}

void Connection::set_timeout(int timeout_ms) {
    socket_.set_receive_timeout(timeout_ms);
    socket_.set_send_timeout(timeout_ms);
}

void Connection::close() {
    socket_.close();
}

bool Connection::is_open() const {
    return socket_.is_valid();
}

} // namespace hpc
