#pragma once

#include "hpc/network/socket.hpp"

#include <string>
#include <cstdint>

namespace hpc {

/// Servidor TCP que escucha conexiones entrantes.
class TcpServer {
public:
    /// Crea un servidor TCP enlazado a la dirección y puerto especificados.
    /// Si port es 0, el sistema operativo asigna un puerto libre automáticamente.
    explicit TcpServer(uint16_t port = 0, const std::string& host = "127.0.0.1");

    ~TcpServer();

    // No copiable, movible.
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) noexcept = default;
    TcpServer& operator=(TcpServer&&) noexcept = default;

    /// Pone el socket en estado de escucha.
    void listen(int backlog = 128);

    /// Acepta una nueva conexión entrante (bloqueante, o con timeout si se configuró).
    /// @return Socket conectado al cliente.
    /// @throws AcceptError / TimeoutError si falla o expira.
    Socket accept();

    /// Devuelve el puerto local en el que está escuchando.
    uint16_t get_port() const;

    /// Cierra el socket del servidor.
    void close();

    /// Comprueba si el servidor está en escucha activa.
    bool is_listening() const;

    /// Configura el timeout para accept() en milisegundos (0 = sin timeout).
    void set_accept_timeout(int timeout_ms);

private:
    Socket listener_;
    uint16_t port_{0};
    bool listening_{false};
};

} // namespace hpc
