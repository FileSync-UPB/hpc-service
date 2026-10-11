#pragma once

#include "hpc/network/socket.hpp"

#include <string>
#include <cstdint>

namespace hpc {

/// Cliente TCP para establecer conexiones a un servidor remoto.
class TcpClient {
public:
    TcpClient() = default;

    /// Conecta a un host y puerto dados con un tiempo de espera opcional.
    /// @param host Dirección IP o nombre de host (ej. "127.0.0.1" o "localhost").
    /// @param port Puerto remoto.
    /// @param timeout_ms Tiempo máximo en milisegundos para intentar la conexión.
    /// @return Socket conectado.
    /// @throws ConnectionError si no se puede conectar o la dirección es inválida.
    /// @throws TimeoutError si se agota el tiempo de espera.
    static Socket connect(const std::string& host, uint16_t port, int timeout_ms = 3000);
};

} // namespace hpc
