#pragma once

#include <cstdint>

namespace hpc {

/// Tipos de tareas científicas disponibles en el sistema.
///
/// Se expandirá en fases posteriores con nuevos algoritmos.
enum class TaskType : uint8_t {
    SumSquares = 0  ///< Suma de cuadrados: S = Σ i² para i ∈ [begin, end).
};

/// Devuelve el nombre textual de un tipo de tarea.
inline const char* to_string(TaskType t) {
    switch (t) {
        case TaskType::SumSquares: return "SumSquares";
    }
    return "Unknown";
}

} // namespace hpc
