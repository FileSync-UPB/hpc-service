#include "hpc/tasks/sum_squares.hpp"

#include <algorithm>

namespace hpc {

// --- SumSquaresTask ---

SumSquaresTask::SumSquaresTask(TaskId id, SumSquaresParams params)
    : id_(id), params_(params) {}

TaskResult SumSquaresTask::execute() {
    uint64_t value = calculate_partial(params_);
    return TaskResult{id_, value, TaskStatus::Completed};
}

TaskId SumSquaresTask::get_id() const {
    return id_;
}

const SumSquaresParams& SumSquaresTask::get_params() const {
    return params_;
}

// --- Funciones libres ---

uint64_t calculate_partial(const SumSquaresParams& params) {
    uint64_t sum = 0;
    for (uint64_t i = params.begin; i < params.end; ++i) {
        sum += i * i;
    }
    return sum;
}

uint64_t calculate_sequential(uint64_t N) {
    if (N > SUM_SQUARES_MAX_N) {
        throw OverflowError(
            "N = " + std::to_string(N) +
            " excede el maximo permitido (" +
            std::to_string(SUM_SQUARES_MAX_N) + ")");
    }
    if (N == 0) {
        return 0;
    }
    // Fórmula cerrada: S = N(N-1)(2N-1)/6
    // Para evitar overflow intermedio, dividimos por 6 de forma segura.
    // Entre tres números consecutivos N, (N-1), (2N-1), al menos uno es
    // divisible por 2 y al menos uno por 3, así que dividimos en dos pasos.
    uint64_t n = N;
    uint64_t n1 = N - 1;
    uint64_t n2 = 2 * N - 1;

    // Paso 1: dividir alguno de los factores por 2.
    if (n % 2 == 0)       { n /= 2; }
    else if (n1 % 2 == 0) { n1 /= 2; }
    else                  { n2 /= 2; }

    // Paso 2: dividir alguno de los factores por 3.
    if (n % 3 == 0)       { n /= 3; }
    else if (n1 % 3 == 0) { n1 /= 3; }
    else                  { n2 /= 3; }

    return n * n1 * n2;
}

std::vector<SumSquaresTask> divide_work(uint64_t N, uint32_t task_count) {
    if (task_count == 0) {
        throw InvalidParameterError("task_count debe ser >= 1");
    }
    if (N > SUM_SQUARES_MAX_N) {
        throw OverflowError(
            "N = " + std::to_string(N) +
            " excede el maximo permitido (" +
            std::to_string(SUM_SQUARES_MAX_N) + ")");
    }

    std::vector<SumSquaresTask> tasks;

    // Si hay más tareas que elementos, solo creamos N tareas
    // (las sobrantes no tendrían trabajo).
    uint64_t effective_tasks = std::min(static_cast<uint64_t>(task_count), N);

    // Caso especial: N == 0 → no hay tareas.
    if (effective_tasks == 0) {
        return tasks;
    }

    uint64_t base_size = N / effective_tasks;
    uint64_t remainder = N % effective_tasks;

    uint64_t current = 0;
    for (uint64_t i = 0; i < effective_tasks; ++i) {
        uint64_t chunk = base_size + (i < remainder ? 1 : 0);
        uint64_t begin = current;
        uint64_t end = current + chunk;

        tasks.emplace_back(
            static_cast<TaskId>(i),
            SumSquaresParams{begin, end}
        );

        current = end;
    }

    return tasks;
}

uint64_t aggregate_results(const std::vector<TaskResult>& results) {
    uint64_t total = 0;
    for (const auto& r : results) {
        total += r.value;
    }
    return total;
}

} // namespace hpc
