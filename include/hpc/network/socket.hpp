#pragma once

#include "hpc/network/socket_compat.hpp"

#include <vector>
#include <string>
#include <chrono>

namespace hpc {

/// Envoltorio RAII para un descriptor de socket TCP.
///
/// Gestiona la vida del socket, asegurando el cierre automático al destruirse.
/// Implementa semántica de movimiento (move-only) y prohíbe la copia.
class Socket {
public:
    /// Construye un socket no inicializado (inválido).
    Socket();

    /// Toma posesión de un descriptor nativo existente.
    explicit Socket(socket_handle_t handle);

    /// Destructor: cierra el socket si es válido.
    ~Socket();

    // No copiable.
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Movible.
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    /// Comprueba si el socket es válido.
    bool is_valid() const;

    /// Cierra el socket explícitamente.
    void close();

    /// Libera la propiedad del descriptor sin cerrarlo.
    socket_handle_t release();

    /// Devuelve el descriptor nativo subyacente.
    socket_handle_t get_native_handle() const;

    /// Configura el timeout de recepción en milisegundos (0 = sin timeout / bloqueante).
    void set_receive_timeout(int timeout_ms);

    /// Configura el timeout de envío en milisegundos (0 = sin timeout).
    void set_send_timeout(int timeout_ms);

    /// Configura la opción SO_REUSEADDR.
    void set_reuse_address(bool reuse);

    /// Configura el retardo de Nagle (TCP_NODELAY).
    void set_no_delay(bool no_delay);

    /// Obtiene el puerto local al que está enlazado el socket (útil con puerto 0).
    uint16_t get_local_port() const;

    /// Envía todos los bytes del buffer. Garantiza que todos los bytes sean enviados
    /// mediante llamadas sucesivas si TCP fragmenta el envío.
    /// @throws SendError ante error de transmisión o desconexión.
    /// @throws TimeoutError si se agota el timeout de envío.
    void send_all(const uint8_t* data, size_t size);

    void send_all(const std::vector<uint8_t>& data) {
        send_all(data.data(), data.size());
    }

    /// Lee exactamente `size` bytes en el buffer destino. Itera hasta completar
    /// todos los bytes solicitados.
    /// @throws PeerDisconnectedError si el otro extremo cierra la conexión ordenadamente.
    /// @throws ReceiveError ante error en la llamada recv.
    /// @throws TimeoutError si expira el tiempo configurado.
    void recv_exact(uint8_t* destination, size_t size);

    std::vector<uint8_t> recv_exact(size_t size);

    /// Lee entre 1 y max_size bytes disponibles.
    /// @return Número de bytes leídos (0 indica desconexión del par).
    /// @throws ReceiveError ante fallo de recv.
    /// @throws TimeoutError si expira el timeout.
    size_t recv_some(uint8_t* destination, size_t max_size);

private:
    socket_handle_t handle_{INVALID_SOCKET_HANDLE};
};

} // namespace hpc
