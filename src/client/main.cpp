#include "hpc/tasks/sum_squares.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

int main(int argc, char* argv[]) {
    uint64_t N = 1000;
    uint32_t task_count = 4;

    if (argc >= 2) {
        N = std::stoull(argv[1]);
    }
    if (argc >= 3) {
        task_count = static_cast<uint32_t>(std::stoul(argv[2]));
    }

    std::cout << "=== HPC Client — Suma de Cuadrados ===\n";
    std::cout << "N = " << N << ", tareas = " << task_count << "\n\n";

    try {
        // Resultado de referencia (fórmula cerrada)
        auto t0 = std::chrono::steady_clock::now();
        uint64_t expected = hpc::calculate_sequential(N);
        auto t1 = std::chrono::steady_clock::now();
        auto seq_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

        std::cout << "Resultado secuencial (formula): " << expected
                  << " (" << seq_us << " us)\n";

        // División en tareas
        auto t2 = std::chrono::steady_clock::now();
        auto tasks = hpc::divide_work(N, task_count);
        auto t3 = std::chrono::steady_clock::now();
        auto div_us = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

        std::cout << "Tareas generadas: " << tasks.size()
                  << " (" << div_us << " us)\n";

        // Ejecución de cada tarea
        std::vector<hpc::TaskResult> results;
        results.reserve(tasks.size());

        auto t4 = std::chrono::steady_clock::now();
        for (auto& task : tasks) {
            results.push_back(task.execute());
        }
        auto t5 = std::chrono::steady_clock::now();
        auto exec_us = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count();

        // Agregación
        uint64_t total = hpc::aggregate_results(results);

        std::cout << "Resultado distribuido:          " << total
                  << " (" << exec_us << " us)\n";

        // Verificación
        if (total == expected) {
            std::cout << "\n[OK] Los resultados coinciden.\n";
        } else {
            std::cerr << "\n[ERROR] Discrepancia: esperado "
                      << expected << ", obtenido " << total << "\n";
            return EXIT_FAILURE;
        }

    } catch (const hpc::HpcError& e) {
        std::cerr << "Error HPC: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
