#include "hpc/network/tcp_server.hpp"
#include "hpc/network/tcp_client.hpp"
#include "hpc/network/connection.hpp"
#include "hpc/protocol/message.hpp"
#include "hpc/protocol/serializer.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdint>
#include <sstream>
#include <type_traits>

template <typename T>
inline std::string to_printable_string(const T& val) {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        return val;
    } else if constexpr (std::is_same_v<std::decay_t<T>, const char*>) {
        return std::string(val);
    } else {
        std::ostringstream oss;
        oss << val;
        return oss.str();
    }
}

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define TEST(name) \
    static void test_##name(); \
    struct Register_##name { \
        Register_##name() { \
            ++g_tests_run; \
            std::cout << "  " << #name << "... "; \
            try { \
                test_##name(); \
                std::cout << "OK\n"; \
            } catch (const std::exception& e) { \
                ++g_tests_failed; \
                std::cout << "FAILED: " << e.what() << "\n"; \
            } \
        } \
    } register_##name; \
    static void test_##name()

#define ASSERT_EQ(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (_a != _b) { \
            throw std::runtime_error( \
                std::string(#a) + " == " + to_printable_string(_a) + \
                ", expected " + to_printable_string(_b) + \
                " at line " + std::to_string(__LINE__)); \
        } \
    } while(0)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            throw std::runtime_error( \
                std::string("Assertion failed: ") + #cond + \
                " at line " + std::to_string(__LINE__)); \
        } \
    } while(0)

#define ASSERT_THROWS(expr, exception_type) \
    do { \
        bool caught = false; \
        try { expr; } \
        catch (const exception_type&) { caught = true; } \
        if (!caught) { \
            throw std::runtime_error( \
                std::string("Expected exception ") + #exception_type + \
                " not thrown at line " + std::to_string(__LINE__)); \
        } \
    } while(0)

// === 8 Pruebas de Integración con Sockets TCP Reales en localhost ===

// 1. Iniciar un servidor y conectar un cliente.
TEST(start_server_and_connect_client) {
    hpc::TcpServer server(0); // Puerto efímero asignado por el SO
    server.listen();
    uint16_t port = server.get_port();
    ASSERT_TRUE(port > 0);

    hpc::Socket client_sock;
    hpc::Socket accepted_sock;

    std::thread server_thread([&]() {
        server.set_accept_timeout(3000);
        accepted_sock = server.accept();
    });

    client_sock = hpc::TcpClient::connect("127.0.0.1", port, 3000);
    server_thread.join();

    ASSERT_TRUE(client_sock.is_valid());
    ASSERT_TRUE(accepted_sock.is_valid());
}

// 2. Enviar un mensaje HELLO y recibir HELLO_ACK.
TEST(send_hello_receive_hello_ack) {
    hpc::TcpServer server(0);
    server.listen();
    uint16_t port = server.get_port();

    std::thread server_thread([&]() {
        hpc::Socket s = server.accept();
        hpc::Connection conn(std::move(s));
        conn.set_timeout(3000);

        hpc::Message msg = conn.receive_message();
        ASSERT_EQ(static_cast<uint16_t>(msg.type), static_cast<uint16_t>(hpc::MessageType::HELLO));
        ASSERT_EQ(msg.id, 1001ULL);
        ASSERT_EQ(msg.payload_as_string(), "Worker-01");

        hpc::Message ack(hpc::MessageType::HELLO_ACK, 1002ULL, "ACK:Worker-01:Accepted");
        conn.send_message(ack);
    });

    hpc::Socket sock = hpc::TcpClient::connect("127.0.0.1", port, 3000);
    hpc::Connection client_conn(std::move(sock));
    client_conn.set_timeout(3000);

    hpc::Message hello(hpc::MessageType::HELLO, 1001ULL, "Worker-01");
    client_conn.send_message(hello);

    hpc::Message resp = client_conn.receive_message();
    ASSERT_EQ(static_cast<uint16_t>(resp.type), static_cast<uint16_t>(hpc::MessageType::HELLO_ACK));
    ASSERT_EQ(resp.id, 1002ULL);
    ASSERT_EQ(resp.payload_as_string(), "ACK:Worker-01:Accepted");

    server_thread.join();
}

// 3. Intercambiar varios mensajes consecutivos en la misma conexión (10 PING / PONG).
TEST(exchange_consecutive_messages) {
    hpc::TcpServer server(0);
    server.listen();
    uint16_t port = server.get_port();
    const int count = 10;

    std::thread server_thread([&]() {
        hpc::Connection conn(server.accept());
        conn.set_timeout(3000);

        for (int i = 0; i < count; ++i) {
            hpc::Message ping = conn.receive_message();
            ASSERT_EQ(static_cast<uint16_t>(ping.type), static_cast<uint16_t>(hpc::MessageType::PING));
            ASSERT_EQ(ping.id, static_cast<uint64_t>(i));

            hpc::Message pong(hpc::MessageType::PONG, ping.id);
            conn.send_message(pong);
        }
    });

    hpc::Connection client_conn(hpc::TcpClient::connect("127.0.0.1", port, 3000));
    client_conn.set_timeout(3000);

    for (int i = 0; i < count; ++i) {
        hpc::Message ping(hpc::MessageType::PING, static_cast<uint64_t>(i));
        client_conn.send_message(ping);

        hpc::Message pong = client_conn.receive_message();
        ASSERT_EQ(static_cast<uint16_t>(pong.type), static_cast<uint16_t>(hpc::MessageType::PONG));
        ASSERT_EQ(pong.id, static_cast<uint64_t>(i));
    }

    server_thread.join();
}

// 4. Enviar mensajes con payload vacío y con payload no vacío.
TEST(exchange_empty_and_non_empty_payloads) {
    hpc::TcpServer server(0);
    server.listen();
    uint16_t port = server.get_port();

    std::thread server_thread([&]() {
        hpc::Connection conn(server.accept());
        conn.set_timeout(3000);

        // Mensaje sin payload
        hpc::Message empty_msg = conn.receive_message();
        ASSERT_TRUE(empty_msg.payload.empty());
        ASSERT_EQ(empty_msg.id, 10ULL);

        // Mensaje con payload
        hpc::Message full_msg = conn.receive_message();
        ASSERT_TRUE(!full_msg.payload.empty());
        ASSERT_EQ(full_msg.payload_as_string(), "ContenidoNoVacio");

        // Respuestas
        conn.send_message(hpc::Message(hpc::MessageType::PONG, 10ULL));
        conn.send_message(hpc::Message(hpc::MessageType::PONG, 20ULL, "RespuestaConDatos"));
    });

    hpc::Connection client_conn(hpc::TcpClient::connect("127.0.0.1", port, 3000));
    client_conn.set_timeout(3000);

    // Enviar vacío
    client_conn.send_message(hpc::Message(hpc::MessageType::PING, 10ULL));
    // Enviar no vacío
    client_conn.send_message(hpc::Message(hpc::MessageType::PING, 20ULL, "ContenidoNoVacio"));

    // Recibir respuestas
    hpc::Message r1 = client_conn.receive_message();
    ASSERT_TRUE(r1.payload.empty());
    ASSERT_EQ(r1.id, 10ULL);

    hpc::Message r2 = client_conn.receive_message();
    ASSERT_EQ(r2.payload_as_string(), "RespuestaConDatos");
    ASSERT_EQ(r2.id, 20ULL);

    server_thread.join();
}

// 5. Comprobar que se preservan los identificadores de 64 bits y los bytes recibidos.
TEST(preserve_64bit_identifiers_and_binary_bytes) {
    hpc::TcpServer server(0);
    server.listen();
    uint16_t port = server.get_port();

    std::vector<uint8_t> complex_payload(4096);
    for (size_t i = 0; i < complex_payload.size(); ++i) {
        complex_payload[i] = static_cast<uint8_t>((i * 73 + 19) & 0xFF);
    }
    complex_payload[0] = 0x00;
    complex_payload[100] = 0xFF;
    complex_payload[500] = 0x00;

    const uint64_t large_id = 0x8877665544332211ULL;

    std::thread server_thread([&]() {
        hpc::Connection conn(server.accept());
        conn.set_timeout(3000);

        hpc::Message binary_msg = conn.receive_message();
        ASSERT_EQ(binary_msg.id, large_id);
        ASSERT_TRUE(binary_msg.payload == complex_payload);

        // Responder con otro ID de 64 bits y el mismo payload
        const uint64_t reply_id = 0xAABBCCDDEEFF0011ULL;
        conn.send_message(hpc::Message(hpc::MessageType::TASK_RESULT, reply_id, binary_msg.payload));
    });

    hpc::Connection client_conn(hpc::TcpClient::connect("127.0.0.1", port, 3000));
    client_conn.set_timeout(3000);

    client_conn.send_message(hpc::Message(hpc::MessageType::TASK_ASSIGN, large_id, complex_payload));

    hpc::Message resp = client_conn.receive_message();
    ASSERT_EQ(resp.id, 0xAABBCCDDEEFF0011ULL);
    ASSERT_TRUE(resp.payload == complex_payload);

    server_thread.join();
}

// 6. Cerrar el cliente y verificar que el servidor detecta la desconexión limpia.
TEST(detect_client_disconnect) {
    hpc::TcpServer server(0);
    server.listen();
    uint16_t port = server.get_port();

    std::atomic<bool> peer_disconnected_caught{false};

    std::thread server_thread([&]() {
        hpc::Connection conn(server.accept());
        conn.set_timeout(3000);

        // Primer mensaje
        hpc::Message m = conn.receive_message();
        ASSERT_EQ(m.id, 1ULL);

        // Ahora el cliente se cerrará. La siguiente lectura debe lanzar PeerDisconnectedError
        try {
            conn.receive_message();
        } catch (const hpc::PeerDisconnectedError&) {
            peer_disconnected_caught = true;
        }
    });

    {
        hpc::Connection client_conn(hpc::TcpClient::connect("127.0.0.1", port, 3000));
        client_conn.send_message(hpc::Message(hpc::MessageType::PING, 1ULL));
        // Aquí el bloque cierra client_conn y el socket
    }

    server_thread.join();
    ASSERT_TRUE(peer_disconnected_caught.load());
}

// 7. Verificar que un error de conexión se gestiona sin bloquear indefinidamente.
TEST(connection_error_handled_promptly) {
    // Intentar conectar a un puerto local no enlazado con un timeout corto (500 ms)
    // El puerto 1 o un puerto cerrado en 127.0.0.1 debe fallar rápido
    auto start = std::chrono::steady_clock::now();
    bool threw_expected = false;

    try {
        hpc::TcpClient::connect("127.0.0.1", 65432, 500);
    } catch (const hpc::ConnectionError&) {
        threw_expected = true;
    } catch (const hpc::TimeoutError&) {
        threw_expected = true;
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    ASSERT_TRUE(threw_expected);
    // Debe haber terminado en menos de 2000 ms, demostrando que no se bloqueó indefinidamente
    ASSERT_TRUE(duration < 2000);
}

// 8. Probar intercambio en ráfaga (pipelining) sin asumir que cada send es un único recv.
TEST(burst_pipelined_messages) {
    hpc::TcpServer server(0);
    server.listen();
    uint16_t port = server.get_port();

    const int total_messages = 5;

    std::thread server_thread([&]() {
        // Pequeño retardo para dar tiempo a que el cliente envíe todos los mensajes antes
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        hpc::Connection conn(server.accept());
        conn.set_timeout(3000);

        for (int i = 0; i < total_messages; ++i) {
            hpc::Message msg = conn.receive_message();
            ASSERT_EQ(static_cast<uint16_t>(msg.type), static_cast<uint16_t>(hpc::MessageType::PING));
            ASSERT_EQ(msg.id, static_cast<uint64_t>(i + 100));
            ASSERT_EQ(msg.payload_as_string(), "BurstMessage_" + std::to_string(i));
        }
    });

    hpc::Socket client_sock = hpc::TcpClient::connect("127.0.0.1", port, 3000);

    // Concatenamos 5 mensajes en un único gran buffer de bytes y lo enviamos de un golpe
    std::vector<uint8_t> all_bytes;
    for (int i = 0; i < total_messages; ++i) {
        hpc::Message msg(hpc::MessageType::PING, static_cast<uint64_t>(i + 100), "BurstMessage_" + std::to_string(i));
        auto b = hpc::serialize_message(msg);
        all_bytes.insert(all_bytes.end(), b.begin(), b.end());
    }

    client_sock.send_all(all_bytes);

    server_thread.join();
}

int main() {
    std::cout << "=== test_network (Integracion TCP Localhost) ===\n";
    std::cout << "\nResultado: " << (g_tests_run - g_tests_failed)
              << "/" << g_tests_run << " tests pasaron.\n";
    return g_tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
