#include "hpc/master/master_server.hpp"
#include "hpc/worker/worker_node.hpp"
#include "hpc/network/tcp_client.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

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

// 1. Un Worker puede conectarse al Master
TEST(worker_can_connect_to_master) {
    hpc::MasterConfig m_cfg;
    m_cfg.port = 0; // Puerto dinámico asignado por el SO
    hpc::MasterServer master(m_cfg);
    master.start();

    uint16_t port = master.get_port();
    ASSERT_TRUE(port > 0);

    hpc::Socket sock = hpc::TcpClient::connect("127.0.0.1", port, 3000);
    ASSERT_TRUE(sock.is_valid());

    master.stop();
}

// 2. El intercambio HELLO / HELLO_ACK funciona
TEST(hello_hello_ack_handshake) {
    hpc::MasterConfig m_cfg;
    m_cfg.port = 0;
    hpc::MasterServer master(m_cfg);
    master.start();
    uint16_t port = master.get_port();

    hpc::WorkerConfig w_cfg;
    w_cfg.master_port = port;
    w_cfg.worker_name = "Worker-Handshake-Test";
    hpc::WorkerNode worker(w_cfg);

    ASSERT_TRUE(worker.connect_and_register());
    ASSERT_TRUE(worker.is_connected());

    worker.disconnect();
    master.stop();
}

// 3. El Master registra al Worker conectado
TEST(master_registers_connected_worker) {
    hpc::MasterConfig m_cfg;
    m_cfg.port = 0;
    hpc::MasterServer master(m_cfg);
    master.start();
    uint16_t port = master.get_port();

    hpc::WorkerConfig w_cfg;
    w_cfg.master_port = port;
    w_cfg.worker_name = "AlphaWorker";
    hpc::WorkerNode worker(w_cfg);

    ASSERT_TRUE(worker.connect_and_register());
    ASSERT_EQ(master.get_registry().get_active_worker_count(), static_cast<size_t>(1));

    hpc::WorkerInfo info;
    ASSERT_TRUE(master.get_registry().get_worker_info(worker.get_worker_id(), info));
    ASSERT_EQ(info.name, "AlphaWorker");
    ASSERT_TRUE(info.state == hpc::WorkerNodeState::Connected);

    worker.disconnect();
    master.stop();
}

// 4. Varios Workers pueden conectarse al mismo tiempo
TEST(multiple_workers_can_connect_simultaneously) {
    hpc::MasterConfig m_cfg;
    m_cfg.port = 0;
    hpc::MasterServer master(m_cfg);
    master.start();
    uint16_t port = master.get_port();

    const size_t num_workers = 6;
    std::vector<std::unique_ptr<hpc::WorkerNode>> workers;

    for (size_t i = 0; i < num_workers; ++i) {
        hpc::WorkerConfig w_cfg;
        w_cfg.master_port = port;
        w_cfg.worker_name = "ConcurrentWorker-" + std::to_string(i + 1);
        auto w = std::make_unique<hpc::WorkerNode>(w_cfg);
        ASSERT_TRUE(w->connect_and_register());
        workers.push_back(std::move(w));
    }

    ASSERT_EQ(master.get_registry().get_active_worker_count(), num_workers);

    for (auto& w : workers) {
        w->disconnect();
    }
    master.stop();
}

// 5. Cada conexión recibe el identificador correcto (IDs correlativos y únicos)
TEST(each_connection_receives_correct_unique_id) {
    hpc::MasterConfig m_cfg;
    m_cfg.port = 0;
    hpc::MasterServer master(m_cfg);
    master.start();
    uint16_t port = master.get_port();

    const size_t num_workers = 4;
    std::vector<std::unique_ptr<hpc::WorkerNode>> workers;

    for (size_t i = 0; i < num_workers; ++i) {
        hpc::WorkerConfig w_cfg;
        w_cfg.master_port = port;
        w_cfg.worker_name = "WorkerIdCheck-" + std::to_string(i + 1);
        auto w = std::make_unique<hpc::WorkerNode>(w_cfg);
        ASSERT_TRUE(w->connect_and_register());
        workers.push_back(std::move(w));
    }

    for (size_t i = 0; i < num_workers; ++i) {
        ASSERT_TRUE(workers[i]->get_worker_id() > 0);
        for (size_t j = i + 1; j < num_workers; ++j) {
            ASSERT_TRUE(workers[i]->get_worker_id() != workers[j]->get_worker_id());
        }
    }

    for (auto& w : workers) {
        w->disconnect();
    }
    master.stop();
}

// 6. El intercambio PING / PONG funciona
TEST(ping_pong_exchange_works) {
    hpc::MasterConfig m_cfg;
    m_cfg.port = 0;
    hpc::MasterServer master(m_cfg);
    master.start();
    uint16_t port = master.get_port();

    hpc::WorkerConfig w_cfg;
    w_cfg.master_port = port;
    w_cfg.worker_name = "PingWorker";
    hpc::WorkerNode worker(w_cfg);
    ASSERT_TRUE(worker.connect_and_register());

    for (uint64_t seq = 1; seq <= 5; ++seq) {
        ASSERT_TRUE(worker.ping(seq));
    }

    worker.disconnect();
    master.stop();
}

// 7. Una desconexión actualiza correctamente el registro
TEST(worker_disconnect_updates_registry) {
    hpc::MasterConfig m_cfg;
    m_cfg.port = 0;
    hpc::MasterServer master(m_cfg);
    master.start();
    uint16_t port = master.get_port();

    hpc::WorkerConfig w_cfg;
    w_cfg.master_port = port;
    w_cfg.worker_name = "VolatileWorker";
    hpc::WorkerId wid = 0;

    {
        hpc::WorkerNode worker(w_cfg);
        ASSERT_TRUE(worker.connect_and_register());
        wid = worker.get_worker_id();
        ASSERT_EQ(master.get_registry().get_active_worker_count(), static_cast<size_t>(1));
        worker.disconnect();
    }

    // Esperar a que el hilo del Master procese el cierre del socket
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_EQ(master.get_registry().get_active_worker_count(), static_cast<size_t>(0));
    hpc::WorkerInfo info;
    ASSERT_TRUE(master.get_registry().get_worker_info(wid, info));
    ASSERT_TRUE(info.state == hpc::WorkerNodeState::Disconnected);

    master.stop();
}

// 8. Un mensaje malformado no provoca un fallo general del Master
TEST(malformed_message_does_not_crash_master) {
    hpc::MasterConfig m_cfg;
    m_cfg.port = 0;
    hpc::MasterServer master(m_cfg);
    master.start();
    uint16_t port = master.get_port();

    // Enviar datos basura al socket
    {
        hpc::Socket raw_sock = hpc::TcpClient::connect("127.0.0.1", port, 2000);
        std::vector<uint8_t> garbage = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
        raw_sock.send_all(garbage);
    }

    // Comprobar que el Master sigue aceptando workers legítimos
    hpc::WorkerConfig w_cfg;
    w_cfg.master_port = port;
    w_cfg.worker_name = "LegitWorkerAfterMalformed";
    hpc::WorkerNode worker(w_cfg);
    ASSERT_TRUE(worker.connect_and_register());
    ASSERT_TRUE(worker.is_connected());

    worker.disconnect();
    master.stop();
}

// 9. La capacidad máxima de conexiones se respeta
TEST(max_workers_capacity_limit_respected) {
    hpc::MasterConfig m_cfg;
    m_cfg.port = 0;
    m_cfg.max_workers = 2; // Máximo 2
    hpc::MasterServer master(m_cfg);
    master.start();
    uint16_t port = master.get_port();

    hpc::WorkerConfig w1_cfg{"127.0.0.1", port, "W1"};
    hpc::WorkerConfig w2_cfg{"127.0.0.1", port, "W2"};
    hpc::WorkerConfig w3_cfg{"127.0.0.1", port, "W3"};

    hpc::WorkerNode w1(w1_cfg);
    hpc::WorkerNode w2(w2_cfg);
    hpc::WorkerNode w3(w3_cfg);

    ASSERT_TRUE(w1.connect_and_register());
    ASSERT_TRUE(w2.connect_and_register());

    // El 3er worker es rechazado
    bool w3_ok = w3.connect_and_register();
    ASSERT_TRUE(!w3_ok);

    ASSERT_EQ(master.get_registry().get_active_worker_count(), static_cast<size_t>(2));

    w1.disconnect();
    w2.disconnect();
    master.stop();
}

// 10. El cierre del Master libera correctamente las conexiones y los hilos
TEST(clean_master_shutdown_releases_resources) {
    hpc::MasterConfig m_cfg;
    m_cfg.port = 0;
    hpc::MasterServer master(m_cfg);
    master.start();
    uint16_t port = master.get_port();

    hpc::WorkerConfig w_cfg{"127.0.0.1", port, "TargetWorker"};
    hpc::WorkerNode worker(w_cfg);
    ASSERT_TRUE(worker.connect_and_register());

    master.stop();
    ASSERT_TRUE(!master.is_running());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(!worker.ping(1));
}

// 11. Ejecución del Master y Workers como procesos independientes del sistema operativo
TEST(independent_processes_execution) {
    // Iniciar MasterServer en puerto 59999 o efímero
    hpc::MasterConfig m_cfg;
    m_cfg.port = 0;
    hpc::MasterServer master(m_cfg);
    master.start();
    uint16_t port = master.get_port();

    // Lanzar el ejecutable hpc_worker como proceso independiente
    // Dependiendo de si se ejecuta desde build/ o desde la raíz del proyecto
    std::string worker_bin;
#ifdef _WIN32
    if (FILE* f = fopen("hpc_worker.exe", "r")) {
        fclose(f);
        worker_bin = "hpc_worker.exe";
    } else {
        worker_bin = ".\\build\\hpc_worker.exe";
    }
#else
    if (FILE* f = fopen("hpc_worker", "r")) {
        fclose(f);
        worker_bin = "./hpc_worker";
    } else {
        worker_bin = "./build/hpc_worker";
    }
#endif

    std::string cmd = worker_bin + " --host 127.0.0.1 --port " +
                      std::to_string(port) +
                      " --id ExternalProcessWorker --run-seconds 2";
    int ret = std::system(cmd.c_str());
    ASSERT_EQ(ret, 0);

    // Esperar a que el Master procese la desconexión tras run-seconds
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(master.get_registry().get_active_worker_count(), static_cast<size_t>(0));

    master.stop();
}

int main() {
    std::cout << "=== test_master_worker (Integracion Master-Worker TCP) ===\n";
    std::cout << "\nResultado: " << (g_tests_run - g_tests_failed)
              << "/" << g_tests_run << " tests pasaron.\n";
    return g_tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
