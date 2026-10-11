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

### Client
- Envía trabajos de cálculo al Master.
- Consulta el estado y recibe los resultados agregados.
- **Estado actual:** Cliente de demostración y validación algorítmica.

### Master
- Recibe trabajos y los divide en tareas mediante el particionador.
- Coordina la distribución de tareas a los workers mediante TCP.
- Agrega los resultados parciales recibidos de los workers.
- **Estado actual:** Esqueleto compilable enlazado a la capa de red y tareas.

### Scheduler
- Administra las colas de tareas.
- Asigna tareas a workers disponibles balanceando carga.
- **Estado actual:** Pendiente (Fase 3+).

### Worker
- Recibe tareas asignadas por el Master mediante TCP.
- Ejecuta el cálculo científico correspondiente (`ITask::execute()`).
- Devuelve el resultado parcial al Master.
- **Estado actual:** Esqueleto compilable enlazado a la capa de red y tareas.

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

### Monitoring
- Registra eventos, tiempos de ejecución y métricas de transferencia.
- **Estado actual:** Pendiente (Fase 3+).

### Recovery
- Gestiona desconexiones, timeouts y reintentos de tareas fallidas.
- **Estado actual:** Pendiente (Fase 3+).

---

## Estructura de directorios

```text
├── CMakeLists.txt           # Configuración de compilación C++17, bibliotecas y CTest
├── config/                  # Archivos de configuración de ejemplo (master.conf, worker.conf)
├── docs/                    # Documentación del proyecto
│   ├── architecture.md      # Arquitectura y componentes
│   ├── development.md       # Guía de compilación en Windows y Linux
│   ├── protocol.md          # Resumen del protocolo
│   ├── protocolo.md         # Especificación técnica detallada del protocolo binario
│   └── testing.md           # Catálogo de pruebas automatizadas y ejecución CTest
├── include/hpc/             # Headers públicos
│   ├── common/              # Tipos fundamentales y excepciones
│   ├── tasks/               # Interfaces científicas y tareas (SumSquares)
│   ├── protocol/            # Especificación, serialización y framing binario
│   ├── network/             # Sockets RAII, cliente, servidor y conexión portable
│   ├── master/              # [Reservado] Lógica interna del master
│   ├── worker/              # [Reservado] Lógica interna del worker
│   └── monitoring/          # [Reservado] Métricas y telemetría
├── src/                     # Código fuente
│   ├── tasks/               # Implementación de tareas científicas
│   ├── protocol/            # Serializador y delimitador binario
│   ├── network/             # Implementación de sockets, TCP server/client y conexión
│   ├── client/              # main.cpp del cliente
│   ├── master/              # main.cpp del master
│   └── worker/              # main.cpp del worker
├── tests/                   # Pruebas automatizadas
│   ├── unit/                # Pruebas unitarias (test_sum_squares, test_partition, test_protocol)
│   └── integration/         # Pruebas de integración con sockets TCP reales (test_network)
├── scripts/                 # Scripts de utilidad
└── results/                 # Resultados de ejecuciones y pruebas de rendimiento
```

---

## Limitaciones actuales (Fase 2)

- Se ha verificado la comunicación y el protocolo punto a punto entre cliente y servidor TCP.
- Aún no se ha implementado el bucle principal de orquestación distribuida concurrente en `hpc_master` ni `hpc_worker` (previsto para la Fase 3).
- No hay recuperación automática ante fallos de nodos ni balanceador dinámico de carga.
