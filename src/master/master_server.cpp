#include "hpc/master/master_server.hpp"
#include "hpc/network/tcp_client.hpp"
#include "hpc/protocol/serializer.hpp"

#include <iostream>

namespace hpc {

namespace {

// Serializa un WorkerId a 4 bytes big-endian + nombre opcional
std::vector<uint8_t> encode_hello_ack_payload(WorkerId id, const std::string& message) {
    std::vector<uint8_t> payload(4 + message.size());
    payload[0] = static_cast<uint8_t>((id >> 24) & 0xFF);
    payload[1] = static_cast<uint8_t>((id >> 16) & 0xFF);
    payload[2] = static_cast<uint8_t>((id >> 8) & 0xFF);
    payload[3] = static_cast<uint8_t>(id & 0xFF);
    for (size_t i = 0; i < message.size(); ++i) {
        payload[4 + i] = static_cast<uint8_t>(message[i]);
    }
    return payload;
}

} // anonymous namespace

MasterServer::MasterServer(MasterConfig config)
    : config_(std::move(config)) {
    tcp_server_ = std::make_unique<TcpServer>(config_.port, config_.host);
}

MasterServer::~MasterServer() {
    stop();
}

void MasterServer::start() {
    if (running_.exchange(true)) {
        return;
    }

    tcp_server_->listen();
    // Timeout para accept de 500ms para permitir salir del loop cuando running_ sea false
    tcp_server_->set_accept_timeout(500);

    accept_thread_ = std::thread(&MasterServer::accept_loop, this);
}

void MasterServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    // Cerrar listener para interrumpir cualquier accept bloqueante
    if (tcp_server_) {
        tcp_server_->close();
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    // Esperar y unir todos los hilos de workers
    std::vector<std::thread> threads_to_join;
    {
        std::lock_guard<std::mutex> lock(worker_threads_mutex_);
        threads_to_join = std::move(worker_threads_);
    }

    for (auto& t : threads_to_join) {
        if (t.joinable()) {
            t.join();
        }
    }
}

bool MasterServer::is_running() const {
    return running_.load();
}

uint16_t MasterServer::get_port() const {
    return tcp_server_ ? tcp_server_->get_port() : 0;
}

WorkerRegistry& MasterServer::get_registry() {
    return registry_;
}

const WorkerRegistry& MasterServer::get_registry() const {
    return registry_;
}

void MasterServer::set_on_worker_connected(std::function<void(const WorkerInfo&)> cb) {
    on_worker_connected_ = std::move(cb);
}

void MasterServer::set_on_worker_disconnected(std::function<void(WorkerId)> cb) {
    on_worker_disconnected_ = std::move(cb);
}

void MasterServer::accept_loop() {
    while (running_.load()) {
        try {
            Socket client_socket = tcp_server_->accept();
            if (!client_socket.is_valid()) {
                continue;
            }

            // Comprobar límite de workers activos
            if (registry_.get_active_worker_count() >= config_.max_workers) {
                // Notificar rechazo de conexión por capacidad excedida
                try {
                    Connection temp_conn(std::move(client_socket));
                    temp_conn.set_timeout(1000);
                    temp_conn.send_message(Message(
                        MessageType::ERROR,
                        0,
                        std::string("Max workers limit reached")
                    ));
                    temp_conn.close();
                } catch (...) {}
                continue;
            }

            // Lanzar hilo dedicado para atender al Worker
            std::lock_guard<std::mutex> lock(worker_threads_mutex_);
            worker_threads_.emplace_back(
                &MasterServer::handle_worker_connection,
                this,
                std::move(client_socket)
            );

        } catch (const TimeoutError&) {
            // Timeout normal de accept() para verificar running_
            continue;
        } catch (const std::exception&) {
            if (!running_.load()) {
                break;
            }
        }
    }
}

void MasterServer::handle_worker_connection(Socket client_socket) {
    Connection conn(std::move(client_socket));
    conn.set_timeout(config_.socket_timeout_ms);

    WorkerId worker_id = 0;
    bool registered = false;

    try {
        while (running_.load()) {
            Message msg;
            try {
                msg = conn.receive_message();
            } catch (const TimeoutError&) {
                // Timeout en espera de mensajes; seguir esperando a menos que el servidor esté deteniéndose
                if (!running_.load()) {
                    break;
                }
                continue;
            } catch (const PeerDisconnectedError&) {
                // Desconexión limpia del cliente
                break;
            } catch (const ProtocolError& pe) {
                // Mensaje malformado: no debe tumbar el Master, respondemos ERROR y cerramos esta conexión
                try {
                    conn.send_message(Message(MessageType::ERROR, 0, std::string("Malformed message: ") + pe.what()));
                } catch (...) {}
                break;
            }

            // Procesamiento de mensajes recibidos
            if (msg.type == MessageType::HELLO) {
                if (!registered) {
                    std::string worker_name = msg.payload_as_string();
                    std::string endpoint = "127.0.0.1";

                    WorkerInfo info = registry_.register_worker(worker_name, endpoint);
                    worker_id = info.id;
                    registered = true;

                    if (on_worker_connected_) {
                        on_worker_connected_(info);
                    }

                    // Responder con HELLO_ACK conteniendo el WorkerId asignado
                    auto ack_payload = encode_hello_ack_payload(worker_id, "Registered successfully");
                    conn.send_message(Message(MessageType::HELLO_ACK, msg.id, std::move(ack_payload)));
                } else {
                    // Ya registrado previamente
                    auto ack_payload = encode_hello_ack_payload(worker_id, "Already registered");
                    conn.send_message(Message(MessageType::HELLO_ACK, msg.id, std::move(ack_payload)));
                }
            } else if (msg.type == MessageType::PING) {
                if (registered) {
                    registry_.update_heartbeat(worker_id);
                }
                // Responder con PONG preservando el ID de mensaje
                conn.send_message(Message(MessageType::PONG, msg.id, msg.payload));
            } else if (msg.type == MessageType::SHUTDOWN) {
                // Orden de desconexión ordenada del worker
                break;
            } else {
                // Otros mensajes (futuras fases: TASK_RESULT, etc.)
                // Responder error o ignorar por ahora
            }
        }
    } catch (const std::exception&) {
        // Error de red en este worker en específico
    }

    if (registered && worker_id > 0) {
        registry_.unregister_worker(worker_id);
        if (on_worker_disconnected_) {
            on_worker_disconnected_(worker_id);
        }
    }

    conn.close();
}

} // namespace hpc
