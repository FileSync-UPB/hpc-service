# HPC Distributed Execution Service

Sistema distribuido **HPC** desarrollado en **Java 21** para la ejecución paralela de código (**Python**) sobre grandes volúmenes de datos (*Datasets*). Comunicación mediante sockets TCP y JSON, implementando *Virtual Threads*.

---

## Arquitectura del Sistema

La arquitectura sigue el patrón **Master-Worker (Coordinador - Trabajador)**.

```mermaid
sequenceDiagram
    autonumber
    participant W1 as WorkerHPC N
    participant C as CoordinatorHPC (Master)
    participant CL as ClientTestHPC

    Note over W1,C: Fase 1: Registro de Nodos Trabajadores (TCP)
    W1->>C: REGISTER_WORKER (JSON: {type, workerId})
    Note over C: WorkerRegistry registra worker & activa hilo de monitoreo

    Note over CL,C: Fase 2: Envío de Trabajo Distribuido
    CL->>C: SUBMIT_JOB (JSON: {jobId, code, datasetChunk})

    Note over C: Fase 3: Particionamiento & Cómputo Paralelo
    Note over C: TaskSplitter divide dataset entre N workers activos
    par Despacho Paralelo a Workers
        C->>W1: EXECUTE_CHUNK (JSON: {jobId, code, datasetChunk[slice]})
        Note over W1: TaskExecutor crea dir temp,<br/>escribe script.py & input.json,<br/>ejecuta Python y lee output.json
        W1-->>C: CHUNK_RESULT (JSON: {jobId, workerId, resultChunk, executionTimeMs})
    end

    Note over C: Fase 4: Agregación & Métricas
    Note over C: ResultAggregator consolida sub-listas & calcula tiempo total HPC

    C-->>CL: JOB_RESULT (JSON: {jobId, resultChunk, executionTimeMs})
    Note over CL: Muestra dataset procesado final + métricas de RTT
```

---

## Componentes Principales

### 1. `CoordinatorHPC` (Nodo Maestro)
- **Servidor TCP (`TCPServer`)**: Gestiona conexiones entrantes de trabajadores y clientes utilizando hilos virtuales de Java 21 (`Executors.newVirtualThreadPerTaskExecutor()`).
- **Registro de Trabajadores (`WorkerRegistry`)**: Mantiene un registro dinámico y thread-safe de los trabajadores conectados y disponibles en tiempo real. Monitorea activamente la desconexión de trabajadores.
- **Divisor de Tareas (`TaskSplitter`)**: Implementa algoritmos de particionamiento balanceado para segmentar el dataset en subconjuntos proporcionales al número de trabajadores disponibles.
- **Agregador de Resultados (`ResultAggregator`)**: Consolida las respuestas parciales devueltas por cada trabajador en el dataset final ordenado.
- **Métricas e Inspección**: Registra tiempos de particionamiento, tiempos de despacho paralelo (Round-Trip Time por worker), agregación y tiempo total de cómputo HPC.

### 2. `WorkerHPC` (Nodo Trabajador)
- **Cliente TCP (`WorkerTCPClient`)**: Se conecta al Coordinador al iniciar, envía su identificador único y se mantiene a la escucha de fragmentos de tareas (`EXECUTE_CHUNK`).
- **Motor de Ejecución (`TaskExecutor`)**:
  - Genera un **directorio temporal aislado** por cada tarea recibida.
  - Escribe el dataset en un archivo `input.json` y el código del usuario en `script.py`.
  - Invoca el proceso nativo de **Python** (`python` / `python3`) para procesar los datos.
  - Mide el tiempo de procesamiento local del trabajador y devuelve los resultados en `CHUNK_RESULT`.
  - Realiza la limpieza automática de archivos temporales al finalizar.

### 3. `ClientTestHPC` (Cliente Demo)
- Define el dataset de prueba y el bloque de código Python que debe ser ejecutado.
- Se conecta al Coordinador, transmite la solicitud mediante `SUBMIT_JOB` y espera la respuesta agregada `JOB_RESULT`.

---

## 📡 Protocolo de Comunicación (TCP + JSON)

Todos los mensajes se serializan con **Jackson** y se transmiten mediante líneas delimitadas por `\n`:

| Tipo de Mensaje (`MessageType`) | Emisor | Receptor | Descripción |
| :--- | :--- | :--- | :--- |
| `REGISTER_WORKER` | Worker | Coordinator | Registro inicial del nodo trabajador. |
| `SUBMIT_JOB` | Client | Coordinator | Envío del código Python y dataset completo. |
| `EXECUTE_CHUNK` | Coordinator | Worker | Asignación de un fragmento del dataset + código Python. |
| `CHUNK_RESULT` | Worker | Coordinator | Respuesta del trabajador con resultados parciales y tiempo de ejecución. |
| `JOB_RESULT` | Coordinator | Client | Respuesta final agregada enviada al cliente. |
| `ERROR` | Coordinator | Client/Worker | Reporte de fallos en el proceso. |

---

## Configuración (`.env`)

```
COORDINATOR_HOST=localhost
COORDINATOR_PORT=8080
```

---

### Requisitos
- **Java 21** o superior.
- **Maven**.
- **Python 3.x** accesible en el `PATH` del sistema (`python` o `python3`).

### Pasos para Ejecutar el Cluster

1. **Iniciar el Coordinador (Maestro)**:
   ```bash
   cd CoordinatorHPC
   mvn exec:java -Dexec.mainClass="upb.coordinator.CoordinatorMain"
   ```

2. **Iniciar uno o más Trabajadores (Workers)**:
   En terminales independientes:
   ```bash
   cd WorkerHPC
   mvn exec:java -Dexec.mainClass="upb.worker.WorkerMain" -Dexec.args="Worker-1"
   ```
   ```bash
   cd WorkerHPC
   mvn exec:java -Dexec.mainClass="upb.worker.WorkerMain" -Dexec.args="Worker-2"
   ```

3. **Ejecutar el Cliente de Prueba**:
   ```bash
   cd ClientTestHPC
   mvn exec:java -Dexec.mainClass="upb.client.ClientMain"
   ```
