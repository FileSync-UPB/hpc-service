#pragma once

#include "hpc/common/types.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace hpc {

/// Estado de un Worker en el registro del Master.
enum class WorkerNodeState : uint8_t {
    Connected,    ///< Conectado y registrado activamente.
    Disconnected  ///< Desconectado o sesión terminada.
};

inline const char* to_string(WorkerNodeState state) {
    switch (state) {
        case WorkerNodeState::Connected:    return "Connected";
        case WorkerNodeState::Disconnected: return "Disconnected";
    }
    return "Unknown";
}

/// Información descriptiva de un Worker registrado.
struct WorkerInfo {
    WorkerId id{0};
    std::string name;
    std::string remote_endpoint;
    WorkerNodeState state{WorkerNodeState::Connected};
    std::chrono::steady_clock::time_point connected_at;
    std::chrono::steady_clock::time_point last_seen;
};

/// Registro thread-safe de Workers en el nodo Master.
class WorkerRegistry {
public:
    WorkerRegistry() = default;

    /// Registra un nuevo Worker asignándole un WorkerId único incremental.
    /// @param name Nombre o identificador solicitado por el worker.
    /// @param remote_endpoint Dirección "IP:puerto" del socket conectado.
    /// @return La estructura WorkerInfo asignada.
    WorkerInfo register_worker(const std::string& name, const std::string& remote_endpoint);

    /// Marca un Worker como desconectado.
    /// @return true si el worker existía y fue marcado, false en caso contrario.
    bool unregister_worker(WorkerId id);

    /// Actualiza el timestamp de última actividad (heartbeat / ping) del Worker.
    bool update_heartbeat(WorkerId id);

    /// Obtiene información de un Worker por su ID.
    bool get_worker_info(WorkerId id, WorkerInfo& out_info) const;

    /// Obtiene la lista de todos los Workers activos (conectados).
    std::vector<WorkerInfo> get_active_workers() const;

    /// Devuelve el número total de workers actualmente conectados.
    size_t get_active_worker_count() const;

    /// Limpia todo el registro.
    void clear();

private:
    mutable std::mutex mutex_;
    WorkerId next_worker_id_{1};
    std::unordered_map<WorkerId, WorkerInfo> workers_;
};

} // namespace hpc
