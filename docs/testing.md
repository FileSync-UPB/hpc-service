# Pruebas Automatizadas

## Ejecución de todas las pruebas con CTest

```bash
cd build
ctest --output-on-failure
```

Salida esperada:

```text
Test project /path/to/build
    Start 1: SumSquaresTest
1/5 Test #1: SumSquaresTest ...................   Passed    0.02 sec
    Start 2: PartitionTest
2/5 Test #2: PartitionTest ....................   Passed    0.02 sec
    Start 3: ProtocolTest
3/5 Test #3: ProtocolTest .....................   Passed    0.02 sec
    Start 4: NetworkTest
4/5 Test #4: NetworkTest ......................   Passed    0.63 sec
    Start 5: MasterWorkerTest
5/5 Test #5: MasterWorkerTest .................   Passed    2.63 sec

100% tests passed, 0 tests failed out of 5
```

---

## Ejecución individual por suite

```bash
# Fase 1: Algoritmo científico y partición
./build/test_sum_squares
./build/test_partition

# Fase 2: Protocolo binario y comunicación TCP
./build/test_protocol
./build/test_network

# Fase 3: Integración Master y Workers
./build/test_master_worker
```

---

## Suites de pruebas disponibles

### 1. SumSquaresTest (`tests/unit/test_sum_squares.cpp`)
Pruebas matemáticas del algoritmo de suma de cuadrados, cálculo analítico de referencia y límites de seguridad para evitar desbordamiento.

### 2. PartitionTest (`tests/unit/test_partition.cpp`)
Pruebas de invariantes de partición en intervalos semiabiertos `[begin, end)` sin huecos, sin solapamientos y cobertura exacta de `[0, N)`.

### 3. ProtocolTest (`tests/unit/test_protocol.cpp`)
11 pruebas unitarias del protocolo binario de 20 bytes en Big-Endian, payloads arbitrarios, límite de 1 MiB y seguridad ante datos truncados.

### 4. NetworkTest (`tests/integration/test_network.cpp`)
8 pruebas de integración sobre sockets TCP reales en `localhost` (handshake, concurrencia, streams en ráfaga).

### 5. MasterWorkerTest (`tests/integration/test_master_worker.cpp`)
11 pruebas de integración con sockets TCP reales entre Master y Workers:
1. `worker_can_connect_to_master`: Conexión exitosa al Master.
2. `hello_hello_ack_handshake`: Handshake completo `HELLO`/`HELLO_ACK`.
3. `master_registers_connected_worker`: Registro en memoria y actualización de estado.
4. `multiple_workers_can_connect_simultaneously`: Conexión concurrente de múltiples Workers.
5. `each_connection_receives_correct_unique_id`: Asignación correlativa y única de `WorkerId`.
6. `ping_pong_exchange_works`: Intercambio de mensajes periódicos `PING`/`PONG`.
7. `worker_disconnect_updates_registry`: Actualización a `Disconnected` ante cierre del socket.
8. `malformed_message_does_not_crash_master`: Resiliencia ante datos basura sin caída del servidor.
9. `max_workers_capacity_limit_respected`: Rechazo ordenado al superar `--max-workers`.
10. `clean_master_shutdown_releases_resources`: Cierre limpio del Master liberando hilos y sockets.
11. `independent_processes_execution`: Ejecución del binario `hpc_worker` como proceso independiente del sistema operativo.
