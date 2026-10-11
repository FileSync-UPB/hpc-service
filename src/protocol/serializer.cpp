#include "hpc/protocol/serializer.hpp"

#include <cstring>

namespace hpc {

namespace {

inline void write_be16(uint8_t* dst, uint16_t val) {
    dst[0] = static_cast<uint8_t>((val >> 8) & 0xFF);
    dst[1] = static_cast<uint8_t>(val & 0xFF);
}

inline void write_be32(uint8_t* dst, uint32_t val) {
    dst[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
    dst[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    dst[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    dst[3] = static_cast<uint8_t>(val & 0xFF);
}

inline void write_be64(uint8_t* dst, uint64_t val) {
    for (int i = 0; i < 8; ++i) {
        dst[i] = static_cast<uint8_t>((val >> (56 - 8 * i)) & 0xFF);
    }
}

inline uint16_t read_be16(const uint8_t* src) {
    return (static_cast<uint16_t>(src[0]) << 8) |
           static_cast<uint16_t>(src[1]);
}

inline uint32_t read_be32(const uint8_t* src) {
    return (static_cast<uint32_t>(src[0]) << 24) |
           (static_cast<uint32_t>(src[1]) << 16) |
           (static_cast<uint32_t>(src[2]) << 8)  |
           static_cast<uint32_t>(src[3]);
}

inline uint64_t read_be64(const uint8_t* src) {
    uint64_t val = 0;
    for (int i = 0; i < 8; ++i) {
        val |= (static_cast<uint64_t>(src[i]) << (56 - 8 * i));
    }
    return val;
}

} // anonymous namespace

std::vector<uint8_t> serialize_header(const MessageHeader& header) {
    std::vector<uint8_t> buffer(MESSAGE_HEADER_SIZE);
    write_be32(&buffer[0], header.magic);
    write_be16(&buffer[4], header.version);
    write_be16(&buffer[6], static_cast<uint16_t>(header.type));
    write_be64(&buffer[8], header.id);
    write_be32(&buffer[16], header.payload_size);
    return buffer;
}

MessageHeader deserialize_header(const uint8_t* data, size_t size) {
    if (size < MESSAGE_HEADER_SIZE) {
        throw IncompleteMessageError(
            "Buffer insuficiente para cabecera: requeridos 20 bytes, disponibles " +
            std::to_string(size));
    }

    uint32_t magic = read_be32(&data[0]);
    if (magic != PROTOCOL_MAGIC) {
        throw InvalidMagicError("Magic number invalido: esperado 0x" +
            std::to_string(PROTOCOL_MAGIC) + ", recibido 0x" + std::to_string(magic));
    }

    uint16_t version = read_be16(&data[4]);
    if (version != PROTOCOL_VERSION) {
        throw UnsupportedVersionError("Version no soportada: esperada " +
            std::to_string(PROTOCOL_VERSION) + ", recibida " + std::to_string(version));
    }

    uint16_t type_raw = read_be16(&data[6]);
    if (!is_valid_message_type(type_raw)) {
        throw InvalidMessageTypeError("Tipo de mensaje desconocido: " +
            std::to_string(type_raw));
    }

    uint64_t id = read_be64(&data[8]);
    uint32_t payload_size = read_be32(&data[16]);

    if (payload_size > MAX_PAYLOAD_SIZE) {
        throw PayloadTooLargeError("Longitud de payload (" +
            std::to_string(payload_size) + " bytes) excede el maximo permitido de " +
            std::to_string(MAX_PAYLOAD_SIZE) + " bytes");
    }

    MessageHeader h;
    h.magic = magic;
    h.version = version;
    h.type = static_cast<MessageType>(type_raw);
    h.id = id;
    h.payload_size = payload_size;
    return h;
}

std::vector<uint8_t> serialize_message(const Message& msg) {
    if (msg.payload.size() > MAX_PAYLOAD_SIZE) {
        throw PayloadTooLargeError(
            "Payload de mensaje (" + std::to_string(msg.payload.size()) +
            " bytes) excede el maximo permitido (" +
            std::to_string(MAX_PAYLOAD_SIZE) + " bytes)");
    }

    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = msg.type;
    header.id = msg.id;
    header.payload_size = static_cast<uint32_t>(msg.payload.size());

    std::vector<uint8_t> buffer = serialize_header(header);
    if (!msg.payload.empty()) {
        buffer.insert(buffer.end(), msg.payload.begin(), msg.payload.end());
    }
    return buffer;
}

Message deserialize_message(const uint8_t* data, size_t size) {
    MessageHeader header = deserialize_header(data, size);

    size_t total_expected = MESSAGE_HEADER_SIZE + header.payload_size;
    if (size < total_expected) {
        throw IncompleteMessageError(
            "Buffer truncado: se esperaban " + std::to_string(total_expected) +
            " bytes pero solo hay " + std::to_string(size));
    }

    Message msg;
    msg.type = header.type;
    msg.id = header.id;
    if (header.payload_size > 0) {
        const uint8_t* payload_start = data + MESSAGE_HEADER_SIZE;
        msg.payload.assign(payload_start, payload_start + header.payload_size);
    }
    return msg;
}

} // namespace hpc
