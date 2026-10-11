#pragma once

#include "hpc/common/types.hpp"

namespace hpc {

/// Interfaz abstracta para una tarea ejecutable.
///
/// Cada tipo de cálculo científico implementa esta interfaz.
/// La ejecución produce un TaskResult con el valor parcial calculado.
class ITask {
public:
    virtual ~ITask() = default;

    /// Ejecuta la tarea y devuelve el resultado.
    virtual TaskResult execute() = 0;

    /// Devuelve el identificador de la tarea.
    virtual TaskId get_id() const = 0;
};

} // namespace hpc
