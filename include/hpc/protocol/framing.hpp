#pragma once

#include "hpc/protocol/message.hpp"
#include "hpc/protocol/serializer.hpp"

#include <cstdint>
#include <vector>

namespace hpc {

/// Ayudante para delimitación (framing) y extracción de mensajes sobre un buffer acumulador.
///
/// Dado que TCP entrega un flujo continuo de bytes donde los mensajes pueden llegar
/// fragmentados o concatenados, esta clase gestiona la acumulación y extracción ordenada.
class MessageFraming {
public:
    /// Inspecciona los primeros 20 bytes para determinar el tamaño total esperado del frame
    /// (20 bytes de cabecera + longitud de payload).
    /// @return Tamaño total esperado del frame en bytes, o 0 si no hay suficientes datos para la cabecera.
    /// @throws ProtocolError si la cabecera es inválida o el payload excede el límite.
    static size_t peek_expected_frame_size(const uint8_t* data, size_t size);

    /// Intenta extraer el siguiente mensaje completo desde el buffer acumulador.
    /// Si hay un mensaje completo disponible:
    /// - Lo deserializa en `out_msg`.
    /// - Remueve los bytes consumidos del frente de `buffer`.
    /// - Retorna true.
    /// Si aún faltan bytes (cabecera o payload parcial):
    /// - No modifica `buffer`.
    /// - Retorna false.
    /// @throws ProtocolError ante datos corruptos o cabeceras con magic/versión/tipo inválidos.
    static bool try_extract_message(std::vector<uint8_t>& buffer, Message& out_msg);
};

} // namespace hpc
