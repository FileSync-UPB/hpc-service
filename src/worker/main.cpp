#include "hpc/worker/worker_node.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

void print_usage(const char* prog) {
    std::cout << "Uso: " << prog << " [opciones]\n"
              << "Opciones:\n"
              << "  --host <ip>           Direccion IP o hostname del Master (defecto: 127.0.0.1)\n"
              << "  --port <puerto>       Puerto TCP del Master (defecto: 8080)\n"
              << "  --id, --name <nombre> Identificador/nombre del worker (defecto: Worker-1)\n"
              << "  --ping-interval <ms>  Intervalo de PING en ms (0 = sin ping periodico, defecto: 1000)\n"
              << "  --run-seconds <sec>   Ejecutar durante N segundos y desconectar (0 = infinito)\n"
              << "  -h, --help            Mostrar esta ayuda\n";
}

int main(int argc, char* argv[]) {
    hpc::WorkerConfig config;
    int ping_interval_ms = 1000;
    int run_seconds = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            config.master_host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.master_port = static_cast<uint16_t>(std::stoul(argv[++i]));
        } else if ((arg == "--id" || arg == "--name") && i + 1 < argc) {
            config.worker_name = argv[++i];
        } else if (arg == "--ping-interval" && i + 1 < argc) {
            ping_interval_ms = std::stoi(argv[++i]);
        } else if (arg == "--run-seconds" && i + 1 < argc) {
            run_seconds = std::stoi(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            std::cerr << "Opcion desconocida: " << arg << "\n";
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    std::cout << "=== HPC Worker Node: " << config.worker_name << " ===\n";
    std::cout << "Intentando conectar al Master en " << config.master_host << ":" << config.master_port << "...\n";

    hpc::WorkerNode worker(config);

    if (!worker.connect_and_register()) {
        std::cerr << "[Worker] [ERROR] Fallo de conexion o identificacion con el Master.\n";
        return EXIT_FAILURE;
    }

    std::cout << "[Worker] [OK] Conectado e identificado exitosamente. ID asignado: "
              << worker.get_worker_id() << "\n";

    auto start_time = std::chrono::steady_clock::now();
    uint64_t ping_seq = 1;

    while (worker.is_connected()) {
        if (run_seconds > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time).count();
            if (elapsed >= run_seconds) {
                std::cout << "[Worker] Tiempo limite alcanzado (" << run_seconds << "s). Desconectando...\n";
                break;
            }
        }

        if (ping_interval_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ping_interval_ms));
            if (worker.ping(ping_seq++)) {
                std::cout << "[Worker] [PING/PONG] Latido #" << (ping_seq - 1) << " verificado.\n";
            } else {
                std::cerr << "[Worker] [ERROR] Fallo en latido PING/PONG o conexion perdida.\n";
                break;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    worker.disconnect();
    std::cout << "[Worker] Desconexion finalizada.\n";
    return EXIT_SUCCESS;
}
