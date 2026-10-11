#include "hpc/tasks/sum_squares.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <cstdint>

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
                std::string(#a) + " == " + std::to_string(_a) + \
                ", expected " + std::to_string(_b) + \
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

/// Verifica los invariantes de una particion:
/// - Sin huecos entre intervalos.
/// - Sin solapamientos.
/// - Cobertura completa de [0, N).
static void verify_partition_invariants(
    const std::vector<hpc::SumSquaresTask>& tasks, uint64_t N)
{
    if (N == 0) {
        ASSERT_TRUE(tasks.empty());
        return;
    }

    ASSERT_TRUE(!tasks.empty());

    // Primer intervalo debe comenzar en 0.
    ASSERT_EQ(tasks.front().get_params().begin, static_cast<uint64_t>(0));

    // Ultimo intervalo debe terminar en N.
    ASSERT_EQ(tasks.back().get_params().end, N);

    // Intervalos contiguos: end[i] == begin[i+1], y begin < end para cada uno.
    for (size_t i = 0; i < tasks.size(); ++i) {
        const auto& p = tasks[i].get_params();
        ASSERT_TRUE(p.begin < p.end);  // No vacios.

        if (i + 1 < tasks.size()) {
            ASSERT_EQ(p.end, tasks[i + 1].get_params().begin);
        }
    }

    // Verificar cobertura total contando elementos.
    uint64_t total_elements = 0;
    for (const auto& t : tasks) {
        const auto& p = t.get_params();
        total_elements += (p.end - p.begin);
    }
    ASSERT_EQ(total_elements, N);
}

// === Tests ===

// 1. N = 0.
TEST(partition_n_zero) {
    auto tasks = hpc::divide_work(0, 4);
    verify_partition_invariants(tasks, 0);
}

// 2. N = 1.
TEST(partition_n_one) {
    auto tasks = hpc::divide_work(1, 4);
    verify_partition_invariants(tasks, 1);
    ASSERT_EQ(tasks.size(), static_cast<size_t>(1));
}

// 4. Divisible exacto: N=12, tasks=4 -> 3 elementos por tarea.
TEST(partition_divisible) {
    auto tasks = hpc::divide_work(12, 4);
    verify_partition_invariants(tasks, 12);
    ASSERT_EQ(tasks.size(), static_cast<size_t>(4));

    // Cada tarea debe tener exactamente 3 elementos.
    for (const auto& t : tasks) {
        const auto& p = t.get_params();
        ASSERT_EQ(p.end - p.begin, static_cast<uint64_t>(3));
    }
}

// 5. No divisible: N=10, tasks=3 -> tamanos 4, 3, 3.
TEST(partition_not_divisible) {
    auto tasks = hpc::divide_work(10, 3);
    verify_partition_invariants(tasks, 10);
    ASSERT_EQ(tasks.size(), static_cast<size_t>(3));
}

// 6. Mas tareas que elementos: N=3, tasks=10 -> solo 3 tareas.
TEST(partition_more_tasks_than_elements) {
    auto tasks = hpc::divide_work(3, 10);
    verify_partition_invariants(tasks, 3);
    ASSERT_EQ(tasks.size(), static_cast<size_t>(3));

    // Cada tarea debe tener exactamente 1 elemento.
    for (const auto& t : tasks) {
        const auto& p = t.get_params();
        ASSERT_EQ(p.end - p.begin, static_cast<uint64_t>(1));
    }
}

// 7. Verificacion exhaustiva de ausencia de huecos y solapamientos.
TEST(partition_no_gaps_no_overlaps_exhaustive) {
    const uint64_t test_Ns[] = {0, 1, 2, 5, 10, 17, 100, 999, 1000};
    const uint32_t test_tasks[] = {1, 2, 3, 4, 7, 10, 50, 100};

    for (uint64_t N : test_Ns) {
        for (uint32_t tc : test_tasks) {
            auto tasks = hpc::divide_work(N, tc);
            verify_partition_invariants(tasks, N);
        }
    }
}

// 8. Comparacion del resultado agregado con calculo secuencial.
TEST(partition_aggregate_matches_sequential) {
    const uint64_t N = 500;
    const uint32_t task_count = 13;
    auto tasks = hpc::divide_work(N, task_count);

    std::vector<hpc::TaskResult> results;
    for (auto& t : tasks) {
        results.push_back(t.execute());
    }

    uint64_t aggregated = hpc::aggregate_results(results);
    uint64_t expected = hpc::calculate_sequential(N);
    ASSERT_EQ(aggregated, expected);
}

// 9. Cantidad de tareas invalida (task_count = 0).
TEST(partition_zero_tasks_invalid) {
    ASSERT_THROWS(
        hpc::divide_work(100, 0),
        hpc::InvalidParameterError
    );
}

// IDs secuenciales.
TEST(partition_ids_sequential) {
    auto tasks = hpc::divide_work(20, 5);
    for (size_t i = 0; i < tasks.size(); ++i) {
        ASSERT_EQ(tasks[i].get_id(), static_cast<hpc::TaskId>(i));
    }
}

int main() {
    std::cout << "=== test_partition ===\n";
    // Tests are auto-registered via static constructors.
    std::cout << "\nResultado: " << (g_tests_run - g_tests_failed)
              << "/" << g_tests_run << " tests pasaron.\n";
    return g_tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
