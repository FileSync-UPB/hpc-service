# Arquitectura del Sistema HPC

## Visión general

Sistema de computación de alto rendimiento (HPC) que distribuye tareas de cálculo científico entre múltiples nodos trabajadores, coordinados por un nodo maestro, comunicándose mediante sockets TCP y un protocolo binario propio en C++17.

## Componentes

```text
┌──────────┐         ┌──────────┐         ┌──────────┐
│  Client  │────────▶│  Master  │────────▶│ Worker 1 │
│          │◀────────│          │◀────────│          │
└──────────┘   TCP   │          │   TCP   └──────────┘
                     │          │────────▶┌──────────┐
                     │          │◀────────│ Worker 2 │
                     │          │         └──────────┘
                     │          │────────▶┌──────────┐
                     │          │◀────────│ Worker N │
                     └──────────┘         └──────────┘
```

### Client (`hpc_client`)
- Envía trabajos de cálculo al Master.
- Consulta el estado y recibe los resultados agregados.
- **Estado actual:** Cliente de demostración y validación algorítmica local.

### Master (`hpc_master`, `hpc_master_lib`)
- Recibe y gestiona conexiones entrantes de múltiples Workers concurrentes vía TCP.
- Mantiene un registro `WorkerRegistry` (*thread-safe*) con los workers activos, sus identificadores correlativos asignados y tiempos de actividad.
- Procesa handshakes `HELLO`/`HELLO_ACK`, latidos `PING`/`PONG`, y desconexiones ordenadas o forzosas.
- Respeta límites configurables de workers simultáneos (`--max-workers`).
- **Estado actual:** **Implementado y verificado (Fase 3)**.

### Worker (`hpc_worker`, `hpc_worker_lib`)
- Nodo trabajador independiente con CLI (`--host`, `--port`, `--id`, `--ping-interval`, `--run-seconds`).
- Se conecta al Master, negocia identificación mediante `HELLO` y recibe `WorkerId` asignado.
- Mantiene latidos `PING`/`PONG` periódicos y detecta desconexión o fallo del Master.
- **Estado actual:** **Implementado y verificado (Fase 3)**.

### Scheduler
- Administra las colas de tareas y las distribuye a los workers disponibles.
- **Estado actual:** Pendiente (Fase 4+).

### Protocol (`hpc_protocol`)
- Define la estructura del mensaje binario (`Message`, `MessageType`), cabecera fija de 20 bytes en orden de red Big-Endian, y límite de 1 MiB de payload.
- Módulos de serialización (`serializer.hpp`) y delimitación en streams TCP (`framing.hpp`).
- **Estado actual:** **Implementado y verificado (Fase 2)**.

### Network (`hpc_network`)
- Encapsula la comunicación TCP portable entre Windows (Winsock2) y Linux (POSIX sockets).
- Clases RAII: `Socket`, `TcpServer`, `TcpClient`, `Connection`.
- Operaciones seguras contra fragmentación TCP: `send_all`, `recv_exact`.
- **Estado actual:** **Implementado y verificado con sockets reales en localhost (Fase 2)**.

### Task System (`hpc_tasks`)
- Interfaz abstracta `ITask` y registro `TaskRegistry`.
- Módulo `SumSquaresTask`: partición balanceada de intervalos `[begin, end)` y suma de cuadrados con límite de seguridad (`MAX_N = 2'000'000`).
- **Estado actual:** **Implementado y verificado (Fase 1)**.

### Aggregator
- Combina los resultados parciales de múltiples tareas.
- **Estado actual:** Implementado para suma numérica (`aggregate_results`).

### Monitoring & Recovery
- Métricas, telemetría y recuperación ante fallos de workers durante la ejecución de tareas.
- **Estado actual:** Pendiente (Fase 4+).

---

## Estructura de directorios

```text
├── CMakeLists.txt           # Configuración de compilación C++17, bibliotecas y CTest
├── config/                  # Archivos de configuración de ejemplo (master.conf, worker.conf)
├── docs/                    # Documentación del proyecto
│   ├── architecture.md      # Arquitectura y componentes
│   ├── development.md       # Guía de compilación en Windows y Linux
│   ├── execution.md         # Guía de ejecución de Master y Workers
│   ├── protocol.md          # Resumen del protocolo
│   ├── protocolo.md         # Especificación técnica detallada del protocolo binario
│   └── testing.md           # Catálogo de pruebas automatizadas y ejecución CTest
├── include/hpc/             # Headers públicos
│   ├── common/              # Tipos fundamentales y excepciones
│   ├── tasks/               # Interfaces científicas y tareas (SumSquares)
│   ├── protocol/            # Especificación, serialización y framing binario
│   ├── network/             # Sockets RAII, cliente, servidor y conexión portable
│   ├── master/              # Servidor Master y registro de Workers
│   ├── worker/              # Nodo Worker y cliente de conexión
│   └── monitoring/          # [Reservado] Métricas y telemetría
├── src/                     # Código fuente
│   ├── tasks/               # Implementación de tareas científicas
│   ├── protocol/            # Serializador y delimitador binario
│   ├── network/             # Implementación de sockets, TCP server/client y conexión
│   ├── master/              # Implementación del MasterServer y main.cpp
│   ├── worker/              # Implementación del WorkerNode y main.cpp
│   └── client/              # main.cpp del cliente
├── tests/                   # Pruebas automatizadas
│   ├── unit/                # test_sum_squares, test_partition, test_protocol
│   └── integration/         # test_network, test_master_worker
├── scripts/                 # Scripts de utilidad
└── results/                 # Resultados de ejecuciones y pruebas de rendimiento
```

---

## Limitaciones actuales (Fase 3)

- El Master y los Workers establecen la conexión de control, registro y latidos, pero **aún no distribuyen tareas científicas ni procesan resultados de cómputo en red** (planificador pendiente para fases siguientes).
- No se realiza reintentos de tareas tras desconexión abrupta de un Worker.
