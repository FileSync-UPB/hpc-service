#include "hpc/network/socket.hpp"
#include "hpc/network/socket_compat.hpp"

#include <cstring>
#include <iostream>

namespace hpc {

// === Inicialización y compatibilidad del subsistema de red ===

void ensure_network_subsystem_initialized() {
#ifdef _WIN32
    struct WinsockInit {
        WinsockInit() {
            WSADATA wsa;
            int res = WSAStartup(MAKEWORD(2, 2), &wsa);
            if (res != 0) {
                std::cerr << "Fallo al inicializar Winsock: " << res << std::endl;
            }
        }
        ~WinsockInit() {
            WSACleanup();
        }
    };
    static WinsockInit s_init;
#endif
}

void close_native_socket(socket_handle_t handle) {
    if (handle != INVALID_SOCKET_HANDLE) {
#ifdef _WIN32
        closesocket(handle);
#else
        ::close(handle);
#endif
    }
}

int get_last_socket_error_code() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

std::string get_last_socket_error_string(int error_code) {
    if (error_code == -1) {
        error_code = get_last_socket_error_code();
    }
#ifdef _WIN32
    char msg_buf[256] = {0};
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msg_buf,
        sizeof(msg_buf) - 1,
        nullptr);
    std::string str(msg_buf);
    while (!str.empty() && (str.back() == '\r' || str.back() == '\n')) {
        str.pop_back();
    }
    return str.empty() ? ("Error " + std::to_string(error_code)) : (str + " (" + std::to_string(error_code) + ")");
#else
    return std::string(strerror(error_code)) + " (" + std::to_string(error_code) + ")";
#endif
}

// === Implementación de Socket ===

Socket::Socket() : handle_(INVALID_SOCKET_HANDLE) {
    ensure_network_subsystem_initialized();
}

Socket::Socket(socket_handle_t handle) : handle_(handle) {
    ensure_network_subsystem_initialized();
}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept : handle_(other.handle_) {
    other.handle_ = INVALID_SOCKET_HANDLE;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        other.handle_ = INVALID_SOCKET_HANDLE;
    }
    return *this;
}

bool Socket::is_valid() const {
    return handle_ != INVALID_SOCKET_HANDLE;
}

void Socket::close() {
    if (handle_ != INVALID_SOCKET_HANDLE) {
        close_native_socket(handle_);
        handle_ = INVALID_SOCKET_HANDLE;
    }
}

socket_handle_t Socket::release() {
    socket_handle_t h = handle_;
    handle_ = INVALID_SOCKET_HANDLE;
    return h;
}

socket_handle_t Socket::get_native_handle() const {
    return handle_;
}

void Socket::set_receive_timeout(int timeout_ms) {
    if (!is_valid()) return;
#ifdef _WIN32
    DWORD tv = static_cast<DWORD>(timeout_ms);
    setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

void Socket::set_send_timeout(int timeout_ms) {
    if (!is_valid()) return;
#ifdef _WIN32
    DWORD tv = static_cast<DWORD>(timeout_ms);
    setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

void Socket::set_reuse_address(bool reuse) {
    if (!is_valid()) return;
    int opt = reuse ? 1 : 0;
    setsockopt(handle_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
}

void Socket::set_no_delay(bool no_delay) {
    if (!is_valid()) return;
    int opt = no_delay ? 1 : 0;
    setsockopt(handle_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&opt), sizeof(opt));
}

uint16_t Socket::get_local_port() const {
    if (!is_valid()) return 0;
    sockaddr_in sin{};
#ifdef _WIN32
    int len = sizeof(sin);
#else
    socklen_t len = sizeof(sin);
#endif
    if (getsockname(handle_, reinterpret_cast<sockaddr*>(&sin), &len) == 0) {
        return ntohs(sin.sin_port);
    }
    return 0;
}

void Socket::send_all(const uint8_t* data, size_t size) {
    if (!is_valid()) {
        throw SendError("Intento de envio en un socket invalido");
    }

    size_t total_sent = 0;
    while (total_sent < size) {
        size_t to_send = size - total_sent;
        int sent = ::send(handle_, reinterpret_cast<const char*>(data + total_sent), static_cast<int>(to_send), 0);

        if (sent > 0) {
            total_sent += static_cast<size_t>(sent);
        } else if (sent == 0) {
            throw PeerDisconnectedError("Conexion cerrada durante envio");
        } else {
            int err = get_last_socket_error_code();
#ifdef _WIN32
            if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
                throw TimeoutError("Timeout agotado durante envio TCP");
            }
#else
            if (err == EAGAIN || err == EWOULDBLOCK) {
                throw TimeoutError("Timeout agotado durante envio TCP");
            }
#endif
            throw SendError("Fallo en send(): " + get_last_socket_error_string(err));
        }
    }
}

void Socket::recv_exact(uint8_t* destination, size_t size) {
    if (!is_valid()) {
        throw ReceiveError("Intento de recepcion en un socket invalido");
    }

    size_t total_received = 0;
    while (total_received < size) {
        size_t to_recv = size - total_received;
        int bytes = ::recv(handle_, reinterpret_cast<char*>(destination + total_received), static_cast<int>(to_recv), 0);

        if (bytes > 0) {
            total_received += static_cast<size_t>(bytes);
        } else if (bytes == 0) {
            throw PeerDisconnectedError("El par remoto cerro la conexion de forma ordenada");
        } else {
            int err = get_last_socket_error_code();
#ifdef _WIN32
            if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
                throw TimeoutError("Timeout agotado durante recepcion TCP");
            }
#else
            if (err == EAGAIN || err == EWOULDBLOCK) {
                throw TimeoutError("Timeout agotado durante recepcion TCP");
            }
#endif
            throw ReceiveError("Fallo en recv(): " + get_last_socket_error_string(err));
        }
    }
}

std::vector<uint8_t> Socket::recv_exact(size_t size) {
    std::vector<uint8_t> buffer(size);
    if (size > 0) {
        recv_exact(buffer.data(), size);
    }
    return buffer;
}

size_t Socket::recv_some(uint8_t* destination, size_t max_size) {
    if (!is_valid()) {
        throw ReceiveError("Intento de recepcion en un socket invalido");
    }

    int bytes = ::recv(handle_, reinterpret_cast<char*>(destination), static_cast<int>(max_size), 0);
    if (bytes > 0) {
        return static_cast<size_t>(bytes);
    } else if (bytes == 0) {
        return 0; // Desconexión limpia
    } else {
        int err = get_last_socket_error_code();
#ifdef _WIN32
        if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
            throw TimeoutError("Timeout agotado durante recepcion TCP");
        }
#else
        if (err == EAGAIN || err == EWOULDBLOCK) {
            throw TimeoutError("Timeout agotado durante recepcion TCP");
        }
#endif
        throw ReceiveError("Fallo en recv(): " + get_last_socket_error_string(err));
    }
}

} // namespace hpc
