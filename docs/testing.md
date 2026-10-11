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
1/4 Test #1: SumSquaresTest ...................   Passed    0.02 sec
    Start 2: PartitionTest
2/4 Test #2: PartitionTest ....................   Passed    0.02 sec
    Start 3: ProtocolTest
3/4 Test #3: ProtocolTest .....................   Passed    0.02 sec
    Start 4: NetworkTest
4/4 Test #4: NetworkTest ......................   Passed    0.63 sec

100% tests passed, 0 tests failed out of 4
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
```

---

## Suites de pruebas disponibles

### 1. SumSquaresTest (`tests/unit/test_sum_squares.cpp`)

| Test | Descripción |
|---|---|
| `sequential_n_zero` | $N=0 \rightarrow$ resultado 0. |
| `sequential_n_one` | $N=1 \rightarrow$ resultado 0 (solo $0^2$). |
| `sequential_n_five` | $N=5 \rightarrow$ resultado 30. |
| `sequential_n_ten` | $N=10 \rightarrow$ resultado 285. |
| `sequential_n_hundred` | $N=100 \rightarrow$ resultado 328350. |
| `sequential_n_max` | $N=2'000'000$ no desborda `uint64_t`. |
| `partial_interval` | Intervalo $[2,5) \rightarrow 29$. |
| `partial_empty_interval` | Intervalo $[3,3) \rightarrow 0$. |
| `aggregate_matches_sequential` | Agregado = secuencial para $N=1000$. |
| `overflow_rejected` | $N > \text{MAX\_N}$ lanza `OverflowError`. |
| `overflow_divide_work_rejected` | `divide_work` con $N > \text{MAX\_N}$ lanza `OverflowError`. |

### 2. PartitionTest (`tests/unit/test_partition.cpp`)

| Test | Descripción |
|---|---|
| `partition_n_zero` | $N=0 \rightarrow$ sin tareas. |
| `partition_n_one` | $N=1 \rightarrow 1$ tarea. |
| `partition_divisible` | $N=12$, `tasks=4` $\rightarrow 4$ tareas de igual tamaño. |
| `partition_not_divisible` | $N=10$, `tasks=3` $\rightarrow$ reparto equilibrado del resto. |
| `partition_more_tasks_than_elements` | $N=3$, `tasks=10` $\rightarrow$ solo 3 tareas de 1 elemento. |
| `partition_no_gaps_no_overlaps_exhaustive` | Múltiples combinaciones $N \times \text{tasks}$ verificando sin huecos ni solapamientos. |
| `partition_aggregate_matches_sequential` | Agregado = secuencial para $N=500$ y 13 tareas. |
| `partition_zero_tasks_invalid` | `tasks=0` lanza `InvalidParameterError`. |
| `partition_ids_sequential` | IDs correlativos $0 \dots M-1$. |

### 3. ProtocolTest (`tests/unit/test_protocol.cpp`)

| Test | Descripción |
|---|---|
| `serialize_deserialize_empty_message` | Serializar y deserializar un mensaje con payload vacío. |
| `serialize_deserialize_with_payload` | Serializar y deserializar un mensaje con payload de texto. |
| `preserve_64bit_identifier` | Preservar identificador de 64 bits (`uint64_t`). |
| `preserve_arbitrary_binary_payload` | Preservar bytes binarios arbitrarios (`0x00`, `0xFF`, bytes intermedios). |
| `detect_incomplete_header` | Rechazar cabeceras de menos de 20 bytes (`IncompleteMessageError`). |
| `detect_invalid_payload_length` | Detectar longitud de payload declarada mayor a los bytes disponibles. |
| `reject_unsupported_version` | Rechazar versión distinta a 1 (`UnsupportedVersionError`). |
| `reject_invalid_message_type` | Rechazar tipo de mensaje no reconocido (`InvalidMessageTypeError`). |
| `reject_payload_exceeding_max_limit` | Rechazar payload superior a 1 MiB (`PayloadTooLargeError`). |
| `truncated_data_safety` | Probar exhaustivamente datos truncados de 0 a $N-1$ bytes sin violaciones de memoria. |
| `framing_stream_extraction` | Extraer múltiples mensajes acumulados en un buffer de flujo TCP continuo. |

### 4. NetworkTest (`tests/integration/test_network.cpp`)

| Test | Descripción |
|---|---|
| `start_server_and_connect_client` | Iniciar servidor en puerto efímero y conectar cliente real. |
| `send_hello_receive_hello_ack` | Intercambio de handshake `HELLO` y respuesta `HELLO_ACK`. |
| `exchange_consecutive_messages` | 10 mensajes continuos `PING` / `PONG` sobre la misma conexión TCP. |
| `exchange_empty_and_non_empty_payloads` | Enviar y responder mensajes sin payload y con payload de forma consecutiva. |
| `preserve_64bit_identifiers_and_binary_bytes` | Transmisión de 4096 bytes binarios complejos preservando IDs de 64 bits y contenido exacto. |
| `detect_client_disconnect` | El servidor detecta la desconexión ordenada del cliente (`PeerDisconnectedError`). |
| `connection_error_handled_promptly` | Intento de conexión a puerto cerrado falla rápidamente (< 2s) con `ConnectionError`/`TimeoutError` sin bloqueo indefinido. |
| `burst_pipelined_messages` | Enviar 5 mensajes encadenados en un único buffer y recibirlos individualmente sin asumir un mensaje por llamada `recv`. |

---

## Framework de pruebas

Se utiliza un framework minimalista propio (sin bibliotecas externas) con las macros:

- `TEST(nombre)` — Define y auto-registra la prueba.
- `ASSERT_EQ(a, b)` — Compara valores e imprime discrepancias tipadas.
- `ASSERT_TRUE(cond)` — Verifica condiciones booleanas.
- `ASSERT_THROWS(expr, tipo)` — Comprueba el lanzamiento exacto de una excepción esperada.
