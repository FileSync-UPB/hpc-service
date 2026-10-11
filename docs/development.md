# Guía de Desarrollo

## Requisitos previos

### En Ubuntu Linux
- **Compilador:** GCC 8+ o Clang 7+ (con soporte completo C++17).
- **CMake:** 3.16 o superior.
- **Herramienta de construcción:** Make o Ninja.

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build
```

### En Windows
- **Entorno recomendado:** CLion o terminal con MinGW-w64 (GCC 8+) y CMake 3.16+.
- Sockets provistos de forma nativa por Winsock (`ws2_32`).

---

## Compilación

### Compilación en Windows (PowerShell con Ninja y CMake)

```powershell
# Configurar y compilar en la carpeta build
cmake -B build -G "Ninja"
cmake --build build

# Ejecutar todas las pruebas
ctest --test-dir build --output-on-failure
```

### Compilación en Ubuntu Linux

```bash
# Configuración
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Compilación en paralelo
cmake --build build -j$(nproc)

# Ejecutar todas las pruebas
ctest --test-dir build --output-on-failure
```

---

## Artefactos y Ejecutables generados

Tras la compilación, se generan las siguientes bibliotecas y binarios en `build/`:

| Artefacto | Tipo | Descripción |
|---|---|---|
| `libhpc_tasks.a` | Biblioteca | Algoritmo científico de suma de cuadrados y partición (Fase 1). |
| `libhpc_protocol.a` | Biblioteca | Protocolo binario, serializador big-endian y delimitador (Fase 2). |
| `libhpc_network.a` | Biblioteca | Capa TCP portable (`Socket`, `TcpServer`, `TcpClient`, `Connection`). |
| `hpc_client` | Ejecutable | Cliente demostrativo. |
| `hpc_master` | Ejecutable | Esqueleto del nodo Master (preparado para Fase 3). |
| `hpc_worker` | Ejecutable | Esqueleto del nodo Worker (preparado para Fase 3). |
| `test_sum_squares` | Prueba | Pruebas matemáticas y límites de suma de cuadrados. |
| `test_partition` | Prueba | Pruebas de partición e invariantes de intervalos. |
| `test_protocol` | Prueba | 10 pruebas unitarias del protocolo binario. |
| `test_network` | Prueba | 8 pruebas de integración con sockets TCP en localhost. |

---

## Ejecución del cliente

```bash
# Con valores por defecto (N=1000, 4 tareas)
./build/hpc_client

# Con N y número de tareas personalizados
./build/hpc_client 10000 8
```

---

## Estructura del código

- **Headers:** `include/hpc/`
  - `common/`: Tipos (`types.hpp`) y excepciones (`errors.hpp`).
  - `tasks/`: Interfaces de cálculo (`task.hpp`, `sum_squares.hpp`, `task_registry.hpp`).
  - `protocol/`: Protocolo binario (`message.hpp`, `serializer.hpp`, `framing.hpp`).
  - `network/`: Capa de red portable (`socket_compat.hpp`, `socket.hpp`, `tcp_server.hpp`, `tcp_client.hpp`, `connection.hpp`).
- **Fuente:** `src/`
  - `tasks/`: Implementación matemática de suma de cuadrados.
  - `protocol/`: Serialización big-endian y framing.
  - `network/`: Sockets RAII, cliente, servidor y conexión estructurada.
  - `client/`, `master/`, `worker/`: Puntos de entrada ejecutables.
- **Pruebas:** `tests/`
  - `unit/`: Pruebas unitarias sin red.
  - `integration/`: Pruebas de integración con sockets TCP reales.
