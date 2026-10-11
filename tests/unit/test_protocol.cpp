#include "hpc/protocol/message.hpp"
#include "hpc/protocol/serializer.hpp"
#include "hpc/protocol/framing.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
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

// === 10 Pruebas Unitarias Obligatorias del Protocolo ===

// 1. Serializar y deserializar un mensaje vacío.
TEST(serialize_deserialize_empty_message) {
    hpc::Message orig(hpc::MessageType::PING, 42ULL);
    ASSERT_TRUE(orig.payload.empty());

    std::vector<uint8_t> bytes = hpc::serialize_message(orig);
    ASSERT_EQ(bytes.size(), hpc::MESSAGE_HEADER_SIZE);

    hpc::Message parsed = hpc::deserialize_message(bytes);
    ASSERT_EQ(static_cast<uint16_t>(parsed.type), static_cast<uint16_t>(hpc::MessageType::PING));
    ASSERT_EQ(parsed.id, 42ULL);
    ASSERT_TRUE(parsed.payload.empty());
}

// 2. Serializar y deserializar un mensaje con payload.
TEST(serialize_deserialize_with_payload) {
    std::string text = "Hola Mundo HPC!";
    hpc::Message orig(hpc::MessageType::HELLO, 100ULL, text);

    std::vector<uint8_t> bytes = hpc::serialize_message(orig);
    ASSERT_EQ(bytes.size(), hpc::MESSAGE_HEADER_SIZE + text.size());

    hpc::Message parsed = hpc::deserialize_message(bytes);
    ASSERT_EQ(static_cast<uint16_t>(parsed.type), static_cast<uint16_t>(hpc::MessageType::HELLO));
    ASSERT_EQ(parsed.id, 100ULL);
    ASSERT_EQ(parsed.payload_as_string(), text);
}

// 3. Preservar un identificador de mensaje de 64 bits completo.
TEST(preserve_64bit_identifier) {
    uint64_t huge_id = 0xFEEDBEEFCAFEBAFALL;
    hpc::Message orig(hpc::MessageType::HELLO_ACK, huge_id);

    std::vector<uint8_t> bytes = hpc::serialize_message(orig);
    hpc::Message parsed = hpc::deserialize_message(bytes);

    ASSERT_EQ(parsed.id, huge_id);
}

// 4. Preservar bytes arbitrarios del payload (incluyendo ceros, 0xFF, patrones binarios).
TEST(preserve_arbitrary_binary_payload) {
    std::vector<uint8_t> raw_payload(512);
    for (size_t i = 0; i < raw_payload.size(); ++i) {
        raw_payload[i] = static_cast<uint8_t>((i * 37 + 13) & 0xFF);
    }
    raw_payload[0] = 0x00;
    raw_payload[1] = 0xFF;
    raw_payload[2] = 0x00;

    hpc::Message orig(hpc::MessageType::TASK_RESULT, 777ULL, raw_payload);
    std::vector<uint8_t> bytes = hpc::serialize_message(orig);

    hpc::Message parsed = hpc::deserialize_message(bytes);
    ASSERT_EQ(parsed.payload.size(), raw_payload.size());
    ASSERT_TRUE(parsed.payload == raw_payload);
}

// 5. Detectar una cabecera incompleta (< 20 bytes).
TEST(detect_incomplete_header) {
    std::vector<uint8_t> short_bytes = {0x48, 0x50, 0x43, 0x31, 0x00, 0x01}; // solo 6 bytes
    ASSERT_THROWS(hpc::deserialize_header(short_bytes), hpc::IncompleteMessageError);
    ASSERT_THROWS(hpc::deserialize_message(short_bytes), hpc::IncompleteMessageError);
}

// 6. Detectar una longitud de payload inválida (longitud declarada pero buffer sin bytes).
TEST(detect_invalid_payload_length) {
    hpc::Message orig(hpc::MessageType::PING, 1ULL, std::vector<uint8_t>{1, 2, 3, 4, 5});
    std::vector<uint8_t> bytes = hpc::serialize_message(orig);

    // Truncamos quitando 2 bytes del payload
    bytes.resize(bytes.size() - 2);

    // Debe lanzar IncompleteMessageError porque faltan bytes declarados
    ASSERT_THROWS(hpc::deserialize_message(bytes), hpc::IncompleteMessageError);
}

// 7. Rechazar una versión desconocida.
TEST(reject_unsupported_version) {
    hpc::Message orig(hpc::MessageType::PING, 1ULL);
    std::vector<uint8_t> bytes = hpc::serialize_message(orig);

    // Cambiar versión (bytes 4-5) a versión 99
    bytes[4] = 0x00;
    bytes[5] = 0x63;

    ASSERT_THROWS(hpc::deserialize_header(bytes), hpc::UnsupportedVersionError);
    ASSERT_THROWS(hpc::deserialize_message(bytes), hpc::UnsupportedVersionError);
}

// 8. Rechazar un tipo de mensaje inválido.
TEST(reject_invalid_message_type) {
    hpc::Message orig(hpc::MessageType::PING, 1ULL);
    std::vector<uint8_t> bytes = hpc::serialize_message(orig);

    // Cambiar tipo (bytes 6-7) a un tipo inexistente 0xFFFF
    bytes[6] = 0xFF;
    bytes[7] = 0xFF;

    ASSERT_THROWS(hpc::deserialize_header(bytes), hpc::InvalidMessageTypeError);
    ASSERT_THROWS(hpc::deserialize_message(bytes), hpc::InvalidMessageTypeError);
}

// 9. Rechazar un payload superior al límite permitido (1 MiB = 1048576 bytes).
TEST(reject_payload_exceeding_max_limit) {
    // A nivel de serialización:
    hpc::Message big_msg;
    big_msg.type = hpc::MessageType::ERROR;
    big_msg.id = 5ULL;
    big_msg.payload.resize(hpc::MAX_PAYLOAD_SIZE + 1, 0xAB);

    ASSERT_THROWS(hpc::serialize_message(big_msg), hpc::PayloadTooLargeError);

    // A nivel de cabecera con tamaño manipulado:
    hpc::Message normal(hpc::MessageType::PING, 1ULL);
    std::vector<uint8_t> bytes = hpc::serialize_message(normal);
    // Alterar payload_size (bytes 16-19) a 2 MiB (0x00200000)
    bytes[16] = 0x00;
    bytes[17] = 0x20;
    bytes[18] = 0x00;
    bytes[19] = 0x00;

    ASSERT_THROWS(hpc::deserialize_header(bytes), hpc::PayloadTooLargeError);
}

// 10. Comprobar que los datos truncados no provocan accesos fuera de límites.
TEST(truncated_data_safety) {
    hpc::Message orig(hpc::MessageType::JOB_SUBMIT, 999ULL, std::string("Datos cientificos de prueba"));
    std::vector<uint8_t> full_bytes = hpc::serialize_message(orig);

    // Probar cada longitud posible desde 0 hasta full_bytes.size() - 1
    for (size_t len = 0; len < full_bytes.size(); ++len) {
        std::vector<uint8_t> truncated(full_bytes.begin(), full_bytes.begin() + len);
        if (len < hpc::MESSAGE_HEADER_SIZE) {
            ASSERT_THROWS(hpc::deserialize_header(truncated), hpc::IncompleteMessageError);
        }
        ASSERT_THROWS(hpc::deserialize_message(truncated), hpc::IncompleteMessageError);
    }
}

// Extra: Framing acumulativo (try_extract_message)
TEST(framing_stream_extraction) {
    hpc::Message m1(hpc::MessageType::HELLO, 1ULL, std::string("m1"));
    hpc::Message m2(hpc::MessageType::PING, 2ULL);
    hpc::Message m3(hpc::MessageType::PONG, 3ULL, std::string("m3_payload"));

    std::vector<uint8_t> stream;
    auto b1 = hpc::serialize_message(m1);
    auto b2 = hpc::serialize_message(m2);
    auto b3 = hpc::serialize_message(m3);

    stream.insert(stream.end(), b1.begin(), b1.end());
    stream.insert(stream.end(), b2.begin(), b2.end());
    stream.insert(stream.end(), b3.begin(), b3.end());

    hpc::Message out;
    ASSERT_TRUE(hpc::MessageFraming::try_extract_message(stream, out));
    ASSERT_EQ(out.id, 1ULL);
    ASSERT_EQ(out.payload_as_string(), "m1");

    ASSERT_TRUE(hpc::MessageFraming::try_extract_message(stream, out));
    ASSERT_EQ(out.id, 2ULL);
    ASSERT_TRUE(out.payload.empty());

    ASSERT_TRUE(hpc::MessageFraming::try_extract_message(stream, out));
    ASSERT_EQ(out.id, 3ULL);
    ASSERT_EQ(out.payload_as_string(), "m3_payload");

    // Ya no quedan más mensajes
    ASSERT_TRUE(!hpc::MessageFraming::try_extract_message(stream, out));
    ASSERT_TRUE(stream.empty());
}

int main() {
    std::cout << "=== test_protocol ===\n";
    std::cout << "\nResultado: " << (g_tests_run - g_tests_failed)
              << "/" << g_tests_run << " tests pasaron.\n";
    return g_tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
