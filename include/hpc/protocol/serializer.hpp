#pragma once

#include "hpc/protocol/message.hpp"
#include "hpc/common/errors.hpp"

#include <cstdint>
#include <vector>
#include <string>

namespace hpc {

/// Excepciones asociadas al protocolo binario.
class ProtocolError : public HpcError {
public:
    using HpcError::HpcError;
};

class InvalidMagicError : public ProtocolError {
public:
    using ProtocolError::ProtocolError;
};

class UnsupportedVersionError : public ProtocolError {
public:
    using ProtocolError::ProtocolError;
};

class InvalidMessageTypeError : public ProtocolError {
public:
    using ProtocolError::ProtocolError;
};

class PayloadTooLargeError : public ProtocolError {
public:
    using ProtocolError::ProtocolError;
};

class IncompleteMessageError : public ProtocolError {
public:
    using ProtocolError::ProtocolError;
};

/// Serializa una cabecera lógica a un buffer de bytes de 20 bytes en orden big-endian.
std::vector<uint8_t> serialize_header(const MessageHeader& header);

/// Deserializa un buffer de al menos 20 bytes a una estructura MessageHeader,
/// validando magic number, versión, tipo de mensaje y límite de payload.
/// @throws IncompleteMessageError si buffer.size() < 20.
/// @throws InvalidMagicError si magic != PROTOCOL_MAGIC.
/// @throws UnsupportedVersionError si version != PROTOCOL_VERSION.
/// @throws InvalidMessageTypeError si el tipo no es reconocido.
/// @throws PayloadTooLargeError si payload_size > MAX_PAYLOAD_SIZE.
MessageHeader deserialize_header(const uint8_t* data, size_t size);

inline MessageHeader deserialize_header(const std::vector<uint8_t>& buffer) {
    return deserialize_header(buffer.data(), buffer.size());
}

/// Serializa un Message completo a un vector de bytes (cabecera de 20 bytes + payload).
/// @throws PayloadTooLargeError si msg.payload.size() > MAX_PAYLOAD_SIZE.
std::vector<uint8_t> serialize_message(const Message& msg);

/// Deserializa un buffer de bytes a un Message completo.
/// Requiere que buffer contenga al menos 20 bytes de cabecera y exactamente
/// los bytes indicados por payload_size.
/// @throws IncompleteMessageError si el buffer no contiene la cabecera o el payload completo.
/// @throws InvalidMagicError, UnsupportedVersionError, InvalidMessageTypeError, PayloadTooLargeError
Message deserialize_message(const uint8_t* data, size_t size);

inline Message deserialize_message(const std::vector<uint8_t>& buffer) {
    return deserialize_message(buffer.data(), buffer.size());
}

} // namespace hpc
