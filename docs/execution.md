# Guía de Ejecución — Master y Workers

Este documento detalla cómo compilar, configurar y ejecutar el nodo **Master** y múltiples nodos **Worker** distribuidos en procesos independientes mediante comunicación TCP real.

---

## 1. Compilación

### En Ubuntu Linux
```bash
# Instalar dependencias si es necesario
sudo apt update && sudo apt install -y build-essential cmake

# Compilar proyecto
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Verificar pruebas
ctest --output-on-failure
```

### En Windows (PowerShell con CMake y Ninja)
```powershell
cmake -B build -G "Ninja"
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## 2. Iniciar el Nodo Master

El Master escucha conexiones entrantes, asigna identificadores y gestiona el ciclo de vida de los Workers.

### Sintaxis
```bash
./build/hpc_master [opciones]
```

### Opciones disponibles
- `--bind <ip>`: Dirección IP donde escuchar (por defecto: `127.0.0.1` o `0.0.0.0` para todas las interfaces de red).
- `--port <puerto>`: Puerto TCP de escucha (por defecto: `8080`).
- `--max-workers <n>`: Número máximo de workers simultáneos admitidos (por defecto: `32`).
- `--timeout <ms>`: Timeout de socket en milisegundos (por defecto: `5000`).
- `--run-seconds <seg>`: Ejecutar durante $N$ segundos y finalizar (opcional, `0` = hasta Ctrl+C).
- `-h, --help`: Muestra la ayuda de opciones.

### Ejemplo de ejecución
En la Terminal 1:
```bash
./build/hpc_master --bind 0.0.0.0 --port 5000 --max-workers 8
```
Salida esperada:
```text
=== HPC Master Server ===
Configuracion: Bind=0.0.0.0, Puerto=5000, MaxWorkers=8
[Master] Escuchando activamente en puerto 5000...
[Master] Presione Ctrl+C o cierre el proceso para detener.
```

---

## 3. Iniciar Nodos Worker

Los Workers son procesos independientes que se conectan al Master, efectúan el handshake `HELLO`, reciben un `WorkerId` asignado y mantienen latidos periódicos (`PING`/`PONG`).

### Sintaxis
```bash
./build/hpc_worker [opciones]
```

### Opciones disponibles
- `--host <ip>`: Dirección IP o hostname del Master (por defecto: `127.0.0.1`).
- `--port <puerto>`: Puerto TCP del Master (por defecto: `8080`).
- `--id, --name <nombre>`: Nombre o etiqueta descriptiva del worker (por defecto: `Worker-1`).
- `--ping-interval <ms>`: Intervalo de latido `PING` en milisegundos (`0` = desactivado, por defecto: `1000`).
- `--run-seconds <seg>`: Tiempo de ejecución en segundos antes de desconectarse limpiamente (`0` = continuo).
- `-h, --help`: Muestra la ayuda de opciones.

### Ejemplo: Ejecutar Worker 1 (Terminal 2)
```bash
./build/hpc_worker --host 127.0.0.1 --port 5000 --id Worker-Alpha --ping-interval 2000
```
Salida en el Worker:
```text
=== HPC Worker Node: Worker-Alpha ===
Intentando conectar al Master en 127.0.0.1:5000...
[Worker] [OK] Conectado e identificado exitosamente. ID asignado: 1
[Worker] [PING/PONG] Latido #1 verificado.
[Worker] [PING/PONG] Latido #2 verificado.
```
Salida simultánea en la consola del Master:
```text
[Master] [CONEXION] Worker 'Worker-Alpha' registrado con ID=1 desde 127.0.0.1
```

### Ejemplo: Ejecutar Worker 2 (Terminal 3)
```bash
./build/hpc_worker --host 127.0.0.1 --port 5000 --id Worker-Beta --ping-interval 2000
```
Salida en el Worker 2:
```text
=== HPC Worker Node: Worker-Beta ===
Intentando conectar al Master en 127.0.0.1:5000...
[Worker] [OK] Conectado e identificado exitosamente. ID asignado: 2
```
Salida simultánea en la consola del Master:
```text
[Master] [CONEXION] Worker 'Worker-Beta' registrado con ID=2 desde 127.0.0.1
```

---

## 4. Detener los Procesos

- **Detener un Worker de forma controlada:**
  Presiona `Ctrl+C` en la terminal del worker o usa `--run-seconds`. El worker enviará un mensaje `SHUTDOWN` y cerrará su socket TCP.
  El Master registrará de inmediato:
  ```text
  [Master] [DESCONEXION] Worker ID=1 desconectado.
  ```
- **Detener el Master:**
  Presiona `Ctrl+C` en la terminal del Master. El servidor cerrará el puerto de escucha y desconectará ordenadamente todos los hilos de workers. Los workers activos detectarán la pérdida de latido `PING/PONG` y terminarán de forma segura.

---

## 5. Diagnóstico de Errores Comunes

| Síntoma | Causa Probable | Solución |
|---|---|---|
| `Fallo en bind: Address already in use` | Otro proceso está usando el puerto indicado. | Cambiar el puerto con `--port <otro_puerto>` o verificar procesos con `netstat -ano` / `lsof -i :<puerto>`. |
| `Fallo de conexion (timeout) a 127.0.0.1:5000` | El Master no está iniciado o hay un firewall bloqueando la conexión. | Iniciar primero el Master y comprobar la dirección con `--host`. |
| `Respuesta inesperada al HELLO: ERROR (Max workers limit reached)` | Se ha alcanzado el límite de `--max-workers`. | Incrementar `--max-workers` en el Master al iniciarlo. |
| `Fallo en latido PING/PONG o conexion perdida` | El Master se detuvo o hubo una interrupción de red. | El Worker finaliza ordenadamente tras detectar el fallo de socket. |
