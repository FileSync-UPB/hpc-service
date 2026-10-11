#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hpc {

/// Identificador único de un trabajo (job).
using JobId = uint32_t;

/// Identificador único de una tarea dentro de un trabajo.
using TaskId = uint32_t;

/// Identificador único de un nodo trabajador.
using WorkerId = uint32_t;

/// Estados posibles de una tarea durante su ciclo de vida.
enum class TaskStatus : uint8_t {
    Pending,    ///< Tarea creada, pendiente de asignación.
    Running,    ///< Tarea en ejecución por un worker.
    Completed,  ///< Tarea finalizada con éxito.
    Failed      ///< Tarea fallida.
};

/// Resultado producido por la ejecución de una tarea.
struct TaskResult {
    TaskId   id;      ///< Identificador de la tarea.
    uint64_t value;   ///< Valor numérico del resultado parcial.
    TaskStatus status; ///< Estado final de la tarea.
};

/// Convierte un TaskStatus a su representación textual.
inline const char* to_string(TaskStatus s) {
    switch (s) {
        case TaskStatus::Pending:   return "Pending";
        case TaskStatus::Running:   return "Running";
        case TaskStatus::Completed: return "Completed";
        case TaskStatus::Failed:    return "Failed";
    }
    return "Unknown";
}

} // namespace hpc
