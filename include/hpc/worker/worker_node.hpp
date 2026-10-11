#pragma once

#include "hpc/common/types.hpp"
#include "hpc/network/connection.hpp"
#include "hpc/protocol/message.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace hpc {

/// Configuración de conexión del Worker.
struct WorkerConfig {
    std::string master_host{"127.0.0.1"};
    uint16_t master_port{8080};
    std::string worker_name{"Worker-1"};
    int connect_timeout_ms{3000};
    int socket_timeout_ms{5000};
};

/// Nodo Worker que se conecta al Master, se registra y mantiene comunicación.
class WorkerNode {
public:
    explicit WorkerNode(WorkerConfig config = {});
    ~WorkerNode();

    // No copiable, no movible
    WorkerNode(const WorkerNode&) = delete;
    WorkerNode& operator=(const WorkerNode&) = delete;

    /// Conecta con el Master y efectúa el handshake HELLO/HELLO_ACK.
    /// @return true si el registro fue exitoso, false en caso contrario.
    bool connect_and_register();

    /// Envía un PING al Master y espera la respuesta PONG.
    /// @param ping_id Identificador del mensaje de ping.
    /// @return true si recibió el PONG correspondiente, false si falló.
    bool ping(uint64_t ping_id = 1);

    /// Envía SHUTDOWN y cierra la conexión de forma ordenada.
    void disconnect();

    /// Comprueba si el worker está conectado y registrado.
    bool is_connected() const;

    /// Devuelve el WorkerId asignado por el Master.
    WorkerId get_worker_id() const;

    /// Devuelve el nombre del worker.
    const std::string& get_name() const;

    /// Acceso a la conexión activa (para pruebas o intercambio adicional).
    Connection& get_connection();

private:
    WorkerConfig config_;
    Connection connection_;
    WorkerId assigned_id_{0};
    std::atomic<bool> connected_{false};
};

} // namespace hpc
