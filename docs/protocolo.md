# Protocolo de Comunicación Binario HPC

## 1. Visión general

El sistema HPC utiliza un protocolo binario propio, tipado y versionado sobre sockets TCP. La comunicación no depende de serialización dependiente de compilador (evitando `reinterpret_cast` directo de structs), garantizando portabilidad absoluta e interoperabilidad entre sistemas **Windows** y **Linux** independientemente del padding o alineación interna de memoria.

---

## 2. Formato binario de la cabecera

Cada mensaje transmitido por la conexión TCP inicia con una cabecera de tamaño fijo de **20 bytes**:

| Offset (bytes) | Campo | Tamaño | Tipo | Descripción |
|:---:|---|:---:|:---:|---|
| `0 .. 3` | **Magic Number** | 4 bytes | `uint32_t` | Constante fija `0x48504331` (ASCII `"HPC1"`). Identifica el protocolo y filtra tráfico no deseado. |
| `4 .. 5` | **Versión** | 2 bytes | `uint16_t` | Versión del protocolo. Versión actual: `1`. |
| `6 .. 7` | **Tipo de Mensaje** | 2 bytes | `uint16_t` | Identificador numérico del tipo de mensaje (`MessageType`). |
| `8 .. 15` | **ID de Mensaje** | 8 bytes | `uint64_t` | Identificador único del mensaje para correlación de peticiones y respuestas. |
| `16 .. 19` | **Longitud Payload** | 4 bytes | `uint32_t` | Tamaño en bytes del cuerpo que sigue a la cabecera. Rango: `0 .. 1,048,576` (1 MiB). |

Total: **20 bytes fijos**.

### Orden de bytes (Endianness)
Todos los campos numéricos multibyte de la cabecera (`uint16_t`, `uint32_t`, `uint64_t`) se codifican en **orden de red (Big-Endian)**.
La serialización y deserialización extraen y escriben explícitamente byte a byte con desplazamientos de bits (`>> 8`, `>> 16`, `>> 24`, etc.), garantizando que arquitecturas little-endian (como x86_64) y big-endian interpreten exactamente los mismos valores sin depender de funciones específicas de plataforma.

---

## 3. Tipos de Mensajes (`MessageType`)

```cpp
enum class MessageType : uint16_t {
    HELLO       = 1,   // Handshake inicial del nodo (cliente o worker).
    HELLO_ACK   = 2,   // Confirmación del handshake y parámetros aceptados.
    PING        = 3,   // Comprobación de latencia y conectividad.
    PONG        = 4,   // Respuesta inmediata a PING.
    ERROR       = 5,   // Reporte de error en el procesamiento o protocolo.
    SHUTDOWN    = 6,   // Solicitud o aviso de desconexión ordenada.

    // Reservados para fases posteriores:
    TASK_ASSIGN = 10,  // Asignación de tarea del Master hacia un Worker.
    TASK_RESULT = 11,  // Reporte de resultado de tarea del Worker al Master.
    JOB_SUBMIT  = 20,  // Envío de trabajo científico del Cliente al Master.
    JOB_RESULT  = 21   // Devolución del resultado consolidado al Cliente.
};
```

---

## 4. Tamaño máximo de payload

- **Límite:** `MAX_PAYLOAD_SIZE = 1,048,576 bytes` (1 MiB).
- **Validación:**
  - Al serializar: Si `msg.payload.size() > MAX_PAYLOAD_SIZE`, se rechaza lanzando `PayloadTooLargeError`.
  - Al recibir/deserializar: Si el campo `payload_size` de la cabecera supera 1 MiB, se rechaza inmediatamente antes de solicitar memoria, previniendo ataques de denegación de servicio o desbordamientos.

---

## 5. Procedimiento de Envío y Recepción

TCP es un flujo continuo de bytes (*stream-oriented*) sin noción de límites de paquetes. Por lo tanto:

### Procedimiento de Envío (`send_message` / `send_all`)
1. Se valida que el tamaño del payload no exceda 1 MiB.
2. Se serializa la cabecera de 20 bytes en big-endian.
3. Se concatenan los bytes del payload (si existe).
4. Se transmite el buffer resultante mediante un bucle `send_all` que realiza llamadas sucesivas a `::send()` hasta que todos los bytes hayan sido confirmados por el socket.

### Procedimiento de Recepción (`receive_message` / `recv_exact`)
1. **Lectura de cabecera:** Se realiza `recv_exact(20)` para acumular exactamente los 20 bytes de la cabecera, tolerando fragmentaciones intermedias de TCP.
2. **Validación estructural:**
   - Comprueba que el Magic Number coincida con `0x48504331`. Si no coincide, lanza `InvalidMagicError`.
   - Comprueba que la versión coincida con `1`. Si no coincide, lanza `UnsupportedVersionError`.
   - Comprueba que el tipo de mensaje pertenezca a `MessageType`. Si es desconocido, lanza `InvalidMessageTypeError`.
   - Comprueba que `payload_size <= 1 MiB`. Si excede, lanza `PayloadTooLargeError`.
3. **Lectura de payload:** Si `payload_size > 0`, se realiza `recv_exact(payload_size)` para leer exactamente los bytes indicados.
4. **Construcción del mensaje:** Se retorna una instancia válida de `Message`.
5. Si durante la lectura el socket remoto se cierra ordenadamente (`recv() == 0`), se lanza `PeerDisconnectedError`.

---

## 6. Manejo de Errores

| Excepción | Condición de activación |
|---|---|
| `InvalidMagicError` | Cabecera recibida sin el número mágico `HPC1`. |
| `UnsupportedVersionError` | Versión distinta a `1`. |
| `InvalidMessageTypeError` | Código de tipo no definido en `MessageType`. |
| `PayloadTooLargeError` | Payload superior a 1 MiB. |
| `IncompleteMessageError` | Buffer recibido menor a los 20 bytes de cabecera o payload truncado. |
| `PeerDisconnectedError` | El otro extremo cerró la conexión TCP ordenadamente. |
| `TimeoutError` | Expiró el tiempo de espera configurado en `SO_RCVTIMEO` o `SO_SNDTIMEO`. |
| `ConnectionError` | Fallo al resolver dirección o conectar con el servidor remoto. |

---

## 7. Ejemplo conceptual de Handshake (`HELLO` / `HELLO_ACK`)

```text
  Cliente / Worker                                          Master
         │                                                     │
         │ ──────────── 1. Sockets TCP Handshake ────────────▶ │ (accept)
         │                                                     │
         │ 2. Mensaje HELLO (20 bytes cabecera + payload)      │
         │    Magic: 0x48504331, Ver: 1, Type: HELLO (1)       │
         │    ID: 1001, Payload: "Worker-01"                   │
         │ ──────────────────────────────────────────────────▶ │
         │                                                     │ 3. Valida cabecera y
         │                                                     │    registra worker
         │ 4. Mensaje HELLO_ACK                                │
         │    Magic: 0x48504331, Ver: 1, Type: HELLO_ACK (2)   │
         │    ID: 1002, Payload: "ACK:Worker-01:Accepted"      │
         │ ◀────────────────────────────────────────────────── │
         │                                                     │
```

---

## 8. Compilación y Ejecución de Pruebas

### En Windows (PowerShell con CLion / MinGW)

```powershell
# Configuración con Ninja y GCC
cmake -B build -G "Ninja"
cmake --build build

# Ejecución de todas las pruebas con CTest
ctest --test-dir build --output-on-failure

# Ejecución de suites individuales
.\build\test_protocol.exe
.\build\test_network.exe
```

### En Ubuntu Linux

```bash
# Instalación de dependencias básicas
sudo apt update && sudo apt install -y build-essential cmake

# Configuración y compilación
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Ejecución de pruebas con CTest
ctest --test-dir build --output-on-failure

# Ejecución de suites individuales
./build/test_protocol
./build/test_network
```
