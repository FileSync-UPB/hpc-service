#include "hpc/master/master_server.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

void print_usage(const char* prog) {
    std::cout << "Uso: " << prog << " [opciones]\n"
              << "Opciones:\n"
              << "  --bind <ip>           Direccion IP a enlazar (defecto: 127.0.0.1 o 0.0.0.0)\n"
              << "  --port <puerto>       Puerto TCP a escuchar (defecto: 8080)\n"
              << "  --max-workers <n>     Capacidad maxima de workers concurrentes (defecto: 32)\n"
              << "  --timeout <ms>        Timeout de socket en ms (defecto: 5000)\n"
              << "  --run-seconds <sec>   Ejecutar durante N segundos y salir (0 = infinito)\n"
              << "  -h, --help            Mostrar esta ayuda\n";
}

int main(int argc, char* argv[]) {
    hpc::MasterConfig config;
    int run_seconds = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--bind" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoul(argv[++i]));
        } else if (arg == "--max-workers" && i + 1 < argc) {
            config.max_workers = std::stoul(argv[++i]);
        } else if (arg == "--timeout" && i + 1 < argc) {
            config.socket_timeout_ms = std::stoi(argv[++i]);
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

    std::cout << "=== HPC Master Server ===\n";
    std::cout << "Configuracion: Bind=" << config.host << ", Puerto=" << config.port
              << ", MaxWorkers=" << config.max_workers << "\n";

    try {
        hpc::MasterServer master(config);

        master.set_on_worker_connected([](const hpc::WorkerInfo& info) {
            std::cout << "[Master] [CONEXION] Worker '" << info.name
                      << "' registrado con ID=" << info.id
                      << " desde " << info.remote_endpoint << "\n";
        });

        master.set_on_worker_disconnected([](hpc::WorkerId id) {
            std::cout << "[Master] [DESCONEXION] Worker ID=" << id << " desconectado.\n";
        });

        master.start();
        std::cout << "[Master] Escuchando activamente en puerto " << master.get_port() << "...\n";

        if (run_seconds > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(run_seconds));
            std::cout << "[Master] Tiempo de ejecucion alcanzado (" << run_seconds << "s). Deteniendo...\n";
            master.stop();
        } else {
            std::cout << "[Master] Presione Ctrl+C o cierre el proceso para detener.\n";
            while (master.is_running()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[Master] [ERROR FATAL] " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "[Master] Finalizado limpiamente.\n";
    return EXIT_SUCCESS;
}
