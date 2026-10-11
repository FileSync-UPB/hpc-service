#pragma once

#include "hpc/master/worker_registry.hpp"
#include "hpc/network/connection.hpp"
#include "hpc/network/tcp_server.hpp"
#include "hpc/protocol/message.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace hpc {

/// Configuración de ejecución del nodo Master.
struct MasterConfig {
    std::string host{"127.0.0.1"};
    uint16_t port{8080};
    size_t max_workers{32};
    int socket_timeout_ms{5000};
};

/// Servidor Master que coordina conexiones de múltiples Workers.
class MasterServer {
public:
    explicit MasterServer(MasterConfig config = {});
    ~MasterServer();

    // No copiable, no movible
    MasterServer(const MasterServer&) = delete;
    MasterServer& operator=(const MasterServer&) = delete;

    /// Inicia el servidor TCP y el bucle de aceptación de conexiones en un hilo de fondo.
    void start();

    /// Detiene el servidor, interrumpe la aceptación y cierra todas las conexiones activas.
    void stop();

    /// Comprueba si el servidor está en ejecución.
    bool is_running() const;

    /// Devuelve el puerto local real en el que está escuchando (especialmente útil si port=0).
    uint16_t get_port() const;

    /// Acceso al registro de Workers.
    WorkerRegistry& get_registry();
    const WorkerRegistry& get_registry() const;

    /// Callbacks opcionales para observabilidad de eventos
    void set_on_worker_connected(std::function<void(const WorkerInfo&)> cb);
    void set_on_worker_disconnected(std::function<void(WorkerId)> cb);

private:
    void accept_loop();
    void handle_worker_connection(Socket client_socket);

    MasterConfig config_;
    std::unique_ptr<TcpServer> tcp_server_;
    WorkerRegistry registry_;

    std::atomic<bool> running_{false};
    std::thread accept_thread_;

    std::mutex worker_threads_mutex_;
    std::vector<std::thread> worker_threads_;

    std::function<void(const WorkerInfo&)> on_worker_connected_;
    std::function<void(WorkerId)> on_worker_disconnected_;
};

} // namespace hpc
