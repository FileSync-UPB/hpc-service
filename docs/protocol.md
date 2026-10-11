# Protocolo de Comunicación Binario HPC

Para la especificación detallada y completa del protocolo binario, estructura de cabecera de 20 bytes, orden de red Big-Endian, tipos de mensajes y diagramas de secuencia, consulta el documento principal:

👉 [**docs/protocolo.md**](protocolo.md)

### Resumen Rápido

- **Cabecera Fija:** 20 bytes.
- **Magic:** `0x48504331` (`"HPC1"`).
- **Versión:** `1`.
- **Endianness:** Big-Endian (Network Byte Order).
- **Límite de Payload:** 1 MiB (`1,048,576` bytes).
- **Capa TCP:** Conexiones fiables mediante `Socket`, `TcpServer`, `TcpClient` y `Connection`.
