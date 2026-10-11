#pragma once

#include "hpc/common/errors.hpp"

#include <string>
#include <cstdint>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef NOGDI
        #define NOGDI
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifdef ERROR
        #undef ERROR
    #endif
    using socket_handle_t = SOCKET;
    constexpr socket_handle_t INVALID_SOCKET_HANDLE = INVALID_SOCKET;
    constexpr int SOCKET_CALL_ERROR = SOCKET_ERROR;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <fcntl.h>
    #include <errno.h>
    using socket_handle_t = int;
    constexpr socket_handle_t INVALID_SOCKET_HANDLE = -1;
    constexpr int SOCKET_CALL_ERROR = -1;
#endif

namespace hpc {

// === Jerarquía de Excepciones de Red ===

class NetworkError : public HpcError {
public:
    using HpcError::HpcError;
};

class SocketCreationError : public NetworkError {
public:
    using NetworkError::NetworkError;
};

class ConnectionError : public NetworkError {
public:
    using NetworkError::NetworkError;
};

class BindError : public NetworkError {
public:
    using NetworkError::NetworkError;
};

class ListenError : public NetworkError {
public:
    using NetworkError::NetworkError;
};

class SendError : public NetworkError {
public:
    using NetworkError::NetworkError;
};

class ReceiveError : public NetworkError {
public:
    using NetworkError::NetworkError;
};

class TimeoutError : public NetworkError {
public:
    using NetworkError::NetworkError;
};

class PeerDisconnectedError : public NetworkError {
public:
    using NetworkError::NetworkError;
};

// === Funciones de compatibilidad ===

/// Inicializa el subsistema de red si es necesario (Winsock en Windows).
/// Es seguro llamarlo múltiples veces; utiliza inicialización estática idempotente.
void ensure_network_subsystem_initialized();

/// Cierra un descriptor de socket según el SO.
void close_native_socket(socket_handle_t handle);

/// Obtiene el último código de error del subsistema de red.
int get_last_socket_error_code();

/// Obtiene la descripción textual del último error de red.
std::string get_last_socket_error_string(int error_code = -1);

} // namespace hpc
