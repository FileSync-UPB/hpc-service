#pragma once

#include <stdexcept>
#include <string>

namespace hpc {

/// Excepción base del sistema HPC.
class HpcError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Error por parámetro inválido.
class InvalidParameterError : public HpcError {
public:
    using HpcError::HpcError;
};

/// Error por desbordamiento numérico.
class OverflowError : public HpcError {
public:
    using HpcError::HpcError;
};

} // namespace hpc
