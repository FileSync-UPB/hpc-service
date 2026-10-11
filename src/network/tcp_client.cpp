#include "hpc/network/tcp_client.hpp"

#include <cstring>
#include <iostream>

namespace hpc {

namespace {

void set_socket_blocking(socket_handle_t handle, bool blocking) {
#ifdef _WIN32
    u_long mode = blocking ? 0 : 1;
    ioctlsocket(handle, FIONBIO, &mode);
#else
    int flags = fcntl(handle, F_GETFL, 0);
    if (flags != -1) {
        if (blocking) {
            fcntl(handle, F_SETFL, flags & ~O_NONBLOCK);
        } else {
            fcntl(handle, F_SETFL, flags | O_NONBLOCK);
        }
    }
#endif
}

} // anonymous namespace

Socket TcpClient::connect(const std::string& host, uint16_t port, int timeout_ms) {
    ensure_network_subsystem_initialized();

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    std::string port_str = std::to_string(port);
    int res = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
    if (res != 0 || result == nullptr) {
        throw ConnectionError("No se pudo resolver host (" + host + "): " +
            std::to_string(res));
    }

    socket_handle_t s = INVALID_SOCKET_HANDLE;
    addrinfo* ptr = nullptr;
    std::string last_error_msg;

    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        s = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (s == INVALID_SOCKET_HANDLE) {
            continue;
        }

        // Configurar en modo no bloqueante para controlar el timeout de conexion
        set_socket_blocking(s, false);

        int connect_res = ::connect(s, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen));
        if (connect_res == 0) {
            // Conexion inmediata
            set_socket_blocking(s, true);
            break;
        }

        int err = get_last_socket_error_code();
        bool in_progress = false;
#ifdef _WIN32
        in_progress = (err == WSAEWOULDBLOCK);
#else
        in_progress = (err == EINPROGRESS);
#endif

        if (in_progress) {
            fd_set write_fds;
            fd_set err_fds;
            FD_ZERO(&write_fds);
            FD_ZERO(&err_fds);
            FD_SET(s, &write_fds);
            FD_SET(s, &err_fds);

            timeval tv{};
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

#ifdef _WIN32
            int sel_res = select(0, nullptr, &write_fds, &err_fds, &tv);
#else
            int sel_res = select(s + 1, nullptr, &write_fds, &err_fds, &tv);
#endif

            if (sel_res == 0) {
                // Timeout
                close_native_socket(s);
                s = INVALID_SOCKET_HANDLE;
                last_error_msg = "Timeout agotado al conectar a " + host + ":" + port_str;
                continue;
            } else if (sel_res > 0) {
                int sock_err = 0;
#ifdef _WIN32
                int opt_len = sizeof(sock_err);
#else
                socklen_t opt_len = sizeof(sock_err);
#endif
                getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&sock_err), &opt_len);

                if (sock_err == 0) {
                    // Exito en la conexion
                    set_socket_blocking(s, true);
                    break;
                } else {
                    last_error_msg = get_last_socket_error_string(sock_err);
                    close_native_socket(s);
                    s = INVALID_SOCKET_HANDLE;
                    continue;
                }
            } else {
                last_error_msg = get_last_socket_error_string();
                close_native_socket(s);
                s = INVALID_SOCKET_HANDLE;
                continue;
            }
        } else {
            last_error_msg = get_last_socket_error_string(err);
            close_native_socket(s);
            s = INVALID_SOCKET_HANDLE;
            continue;
        }
    }

    freeaddrinfo(result);

    if (s == INVALID_SOCKET_HANDLE) {
        if (last_error_msg.find("Timeout") != std::string::npos) {
            throw TimeoutError("Fallo de conexion (timeout) a " + host + ":" + port_str);
        }
        throw ConnectionError("No se pudo conectar a " + host + ":" + port_str + ": " + last_error_msg);
    }

    Socket client_socket(s);
    client_socket.set_no_delay(true);
    return client_socket;
}

} // namespace hpc
