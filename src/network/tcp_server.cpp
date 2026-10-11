#include "hpc/network/tcp_server.hpp"

#include <cstring>

namespace hpc {

TcpServer::TcpServer(uint16_t port, const std::string& host) {
    ensure_network_subsystem_initialized();

    socket_handle_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET_HANDLE) {
        throw SocketCreationError("Fallo al crear socket para TcpServer: " +
            get_last_socket_error_string());
    }

    listener_ = Socket(s);
    listener_.set_reuse_address(true);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (host == "0.0.0.0" || host.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
            throw BindError("Direccion host invalida: " + host);
        }
    }

    if (::bind(listener_.get_native_handle(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_CALL_ERROR) {
        int err = get_last_socket_error_code();
        throw BindError("Fallo en bind(" + host + ":" + std::to_string(port) + "): " +
            get_last_socket_error_string(err));
    }

    port_ = listener_.get_local_port();
}

TcpServer::~TcpServer() {
    close();
}

void TcpServer::listen(int backlog) {
    if (!listener_.is_valid()) {
        throw ListenError("Servidor no inicializado o cerrado");
    }

    if (::listen(listener_.get_native_handle(), backlog) == SOCKET_CALL_ERROR) {
        int err = get_last_socket_error_code();
        throw ListenError("Fallo en listen(): " + get_last_socket_error_string(err));
    }

    listening_ = true;
}

Socket TcpServer::accept() {
    if (!listening_ || !listener_.is_valid()) {
        throw NetworkError("El servidor no esta en modo escucha (listen no fue invocado)");
    }

    sockaddr_in client_addr{};
#ifdef _WIN32
    int addr_len = sizeof(client_addr);
#else
    socklen_t addr_len = sizeof(client_addr);
#endif

    socket_handle_t client_s = ::accept(
        listener_.get_native_handle(),
        reinterpret_cast<sockaddr*>(&client_addr),
        &addr_len);

    if (client_s == INVALID_SOCKET_HANDLE) {
        int err = get_last_socket_error_code();
#ifdef _WIN32
        if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
            throw TimeoutError("Timeout agotado esperando conexion en accept()");
        }
#else
        if (err == EAGAIN || err == EWOULDBLOCK) {
            throw TimeoutError("Timeout agotado esperando conexion en accept()");
        }
#endif
        throw NetworkError("Fallo en accept(): " + get_last_socket_error_string(err));
    }

    return Socket(client_s);
}

uint16_t TcpServer::get_port() const {
    return port_;
}

void TcpServer::close() {
    listening_ = false;
    listener_.close();
}

bool TcpServer::is_listening() const {
    return listening_ && listener_.is_valid();
}

void TcpServer::set_accept_timeout(int timeout_ms) {
    listener_.set_receive_timeout(timeout_ms);
}

} // namespace hpc
