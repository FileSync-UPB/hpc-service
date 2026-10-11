#pragma once

#include "hpc/tasks/task.hpp"
#include "hpc/common/types.hpp"
#include "hpc/common/errors.hpp"

#include <cstdint>
#include <vector>

namespace hpc {

/// Límite superior admitido para N en suma de cuadrados.
///
/// Justificación: Para N = MAX_N, la suma S = N(N-1)(2N-1)/6 debe caber
/// en uint64_t (máximo ~1.84 × 10¹⁹). Con MAX_N = 2'000'000:
///   - Máximo i² = (1'999'999)² ≈ 4.0 × 10¹²
///   - S = 2'000'000 × 1'999'999 × 3'999'999 / 6 ≈ 2.67 × 10¹⁸
/// Esto deja margen amplio respecto al límite de uint64_t.
constexpr uint64_t SUM_SQUARES_MAX_N = 2'000'000;

/// Parámetros de una tarea de suma de cuadrados.
///
/// Representa el intervalo semiabierto [begin, end) sobre el cual
/// calcular Σ i².
struct SumSquaresParams {
    uint64_t begin;  ///< Inicio del intervalo (incluido).
    uint64_t end;    ///< Fin del intervalo (excluido).
};

/// Tarea que calcula la suma de cuadrados en un intervalo [begin, end).
class SumSquaresTask : public ITask {
public:
    /// Construye una tarea de suma de cuadrados.
    /// @param id Identificador de la tarea.
    /// @param params Parámetros del intervalo [begin, end).
    SumSquaresTask(TaskId id, SumSquaresParams params);

    TaskResult execute() override;
    TaskId get_id() const override;

    /// Acceso a los parámetros del intervalo.
    const SumSquaresParams& get_params() const;

private:
    TaskId id_;
    SumSquaresParams params_;
};

/// Divide el rango [0, N) en task_count tareas con intervalos contiguos
/// y sin solapamiento.
///
/// @param N Número total de elementos (debe ser <= SUM_SQUARES_MAX_N).
/// @param task_count Número de tareas a generar (debe ser >= 1).
/// @return Vector de SumSquaresTask con IDs asignados secuencialmente.
/// @throws InvalidParameterError si task_count == 0.
/// @throws OverflowError si N > SUM_SQUARES_MAX_N.
std::vector<SumSquaresTask> divide_work(uint64_t N, uint32_t task_count);

/// Calcula Σ i² para i ∈ [0, N) usando la fórmula cerrada N(N-1)(2N-1)/6.
///
/// @param N Número total de elementos (debe ser <= SUM_SQUARES_MAX_N).
/// @return El valor de la suma.
/// @throws OverflowError si N > SUM_SQUARES_MAX_N.
uint64_t calculate_sequential(uint64_t N);

/// Calcula Σ i² para i ∈ [begin, end) mediante iteración.
///
/// @param params Intervalo semiabierto [begin, end).
/// @return La suma parcial.
uint64_t calculate_partial(const SumSquaresParams& params);

/// Agrega los resultados parciales de múltiples tareas.
///
/// @param results Vector de resultados de tareas completadas.
/// @return La suma total de todos los valores parciales.
uint64_t aggregate_results(const std::vector<TaskResult>& results);

} // namespace hpc
