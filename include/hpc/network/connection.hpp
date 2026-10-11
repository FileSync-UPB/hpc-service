#pragma once

#include "hpc/network/socket.hpp"
#include "hpc/protocol/message.hpp"
#include "hpc/protocol/serializer.hpp"

namespace hpc {

/// Representa una conexión TCP activa que transporta mensajes estructurados `Message`.
///
/// Encapsula el envío y recepción delimitada de mensajes completos mediante el protocolo binario,
/// garantizando que la lectura obtenga exactamente los 20 bytes de cabecera y el payload correspondiente.
class Connection {
public:
    Connection() = default;

    /// Toma posesión de un socket conectado.
    explicit Connection(Socket socket);

    ~Connection() = default;

    // Movible, no copiable.
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept = default;
    Connection& operator=(Connection&&) noexcept = default;

    /// Envía un mensaje completo a través de la conexión.
    /// Serializa cabecera y payload y los transmite mediante `send_all`.
    /// @throws PayloadTooLargeError si el payload supera 1 MiB.
    /// @throws SendError / TimeoutError ante fallos de transmisión.
    void send_message(const Message& msg);

    /// Recibe un mensaje completo a través de la conexión.
    /// Lee primero los 20 bytes de cabecera fija, valida campos y longitud,
    /// y a continuación lee exactamente el payload declarado.
    /// @return Mensaje deserializado.
    /// @throws PeerDisconnectedError si el otro extremo cerró la conexión.
    /// @throws ProtocolError si la cabecera es inválida o el payload excede el límite.
    /// @throws ReceiveError / TimeoutError ante fallos de red.
    Message receive_message();

    /// Configura el tiempo de espera para operaciones de envío y recepción.
    void set_timeout(int timeout_ms);

    /// Cierra la conexión.
    void close();

    /// Indica si el socket de la conexión es válido.
    bool is_open() const;

    /// Acceso al socket subyacente.
    Socket& get_socket() { return socket_; }
    const Socket& get_socket() const { return socket_; }

private:
    Socket socket_;
};

} // namespace hpc
