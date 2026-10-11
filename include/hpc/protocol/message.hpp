#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hpc {

/// Número mágico identificador del protocolo: 'H' 'P' 'C' '1' (0x48504331).
constexpr uint32_t PROTOCOL_MAGIC = 0x48504331;

/// Versión inicial del protocolo binario.
constexpr uint16_t PROTOCOL_VERSION = 1;

/// Tamaño fijo de la cabecera del mensaje en bytes:
/// Magic (4) + Versión (2) + Tipo (2) + ID (8) + Payload Size (4) = 20 bytes.
constexpr size_t MESSAGE_HEADER_SIZE = 20;

/// Tamaño máximo permitido para el payload de un mensaje (1 MiB).
constexpr uint32_t MAX_PAYLOAD_SIZE = 1024 * 1024; // 1,048,576 bytes

#ifdef ERROR
#undef ERROR
#endif

/// Tipos de mensaje definidos en el protocolo binario.
enum class MessageType : uint16_t {
    HELLO       = 1,  ///< Saludo inicial del nodo / handshake.
    HELLO_ACK   = 2,  ///< Confirmación de saludo y parámetros aceptados.
    PING        = 3,  ///< Verificación periódica de conectividad (liveness).
    PONG        = 4,  ///< Respuesta a PING.
    ERROR       = 5,  ///< Notificación estructurada de error.
    SHUTDOWN    = 6,  ///< Notificación de desconexión o apagado ordenado.

    // Tipos reservados para fases futuras (distribución de tareas):
    TASK_ASSIGN = 10, ///< Asignación de tarea desde Master a Worker.
    TASK_RESULT = 11, ///< Reporte de resultado de tarea desde Worker a Master.
    JOB_SUBMIT  = 20, ///< Envío de trabajo científico desde Client a Master.
    JOB_RESULT  = 21  ///< Devolución de resultado global desde Master a Client.
};

/// Convierte un MessageType a su representación en texto.
inline const char* to_string(MessageType type) {
    switch (type) {
        case MessageType::HELLO:       return "HELLO";
        case MessageType::HELLO_ACK:   return "HELLO_ACK";
        case MessageType::PING:        return "PING";
        case MessageType::PONG:        return "PONG";
        case MessageType::ERROR:       return "ERROR";
        case MessageType::SHUTDOWN:    return "SHUTDOWN";
        case MessageType::TASK_ASSIGN: return "TASK_ASSIGN";
        case MessageType::TASK_RESULT: return "TASK_RESULT";
        case MessageType::JOB_SUBMIT:  return "JOB_SUBMIT";
        case MessageType::JOB_RESULT:  return "JOB_RESULT";
    }
    return "UNKNOWN";
}

/// Comprueba si un valor numérico corresponde a un MessageType válido.
inline bool is_valid_message_type(uint16_t val) {
    switch (static_cast<MessageType>(val)) {
        case MessageType::HELLO:
        case MessageType::HELLO_ACK:
        case MessageType::PING:
        case MessageType::PONG:
        case MessageType::ERROR:
        case MessageType::SHUTDOWN:
        case MessageType::TASK_ASSIGN:
        case MessageType::TASK_RESULT:
        case MessageType::JOB_SUBMIT:
        case MessageType::JOB_RESULT:
            return true;
        default:
            return false;
    }
}

/// Representación lógica de la cabecera de 20 bytes.
struct MessageHeader {
    uint32_t magic{PROTOCOL_MAGIC};
    uint16_t version{PROTOCOL_VERSION};
    MessageType type{MessageType::HELLO};
    uint64_t id{0};
    uint32_t payload_size{0};
};

/// Representación lógica de un mensaje completo del protocolo HPC.
struct Message {
    MessageType type{MessageType::HELLO};
    uint64_t id{0};
    std::vector<uint8_t> payload{};

    Message() = default;

    Message(MessageType t, uint64_t msg_id, std::vector<uint8_t> data = {})
        : type(t), id(msg_id), payload(std::move(data)) {}

    Message(MessageType t, uint64_t msg_id, const std::string& str_payload)
        : type(t), id(msg_id), payload(str_payload.begin(), str_payload.end()) {}

    /// Devuelve el payload interpretado como string UTF-8.
    std::string payload_as_string() const {
        return std::string(payload.begin(), payload.end());
    }
};

} // namespace hpc
