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

// === Tests ===

// 1. N = 0: la suma debe ser 0.
TEST(sequential_n_zero) {
    ASSERT_EQ(hpc::calculate_sequential(0), static_cast<uint64_t>(0));
}

// 2. N = 1: sum(0^2) = 0.
TEST(sequential_n_one) {
    ASSERT_EQ(hpc::calculate_sequential(1), static_cast<uint64_t>(0));
}

// 3. Intervalo pequeno con resultado conocido.
//    N=5: 0^2 + 1^2 + 2^2 + 3^2 + 4^2 = 0+1+4+9+16 = 30.
TEST(sequential_n_five) {
    ASSERT_EQ(hpc::calculate_sequential(5), static_cast<uint64_t>(30));
}

// N=10: 0+1+4+9+16+25+36+49+64+81 = 285.
TEST(sequential_n_ten) {
    ASSERT_EQ(hpc::calculate_sequential(10), static_cast<uint64_t>(285));
}

// N=100: resultado por formula = 100*99*199/6 = 328350.
TEST(sequential_n_hundred) {
    ASSERT_EQ(hpc::calculate_sequential(100), static_cast<uint64_t>(328350));
}

// Valor grande dentro del rango permitido.
TEST(sequential_n_max) {
    // N = 2'000'000 -> no debe lanzar excepcion.
    uint64_t result = hpc::calculate_sequential(hpc::SUM_SQUARES_MAX_N);
    ASSERT_TRUE(result > 0);
}

// Calculo parcial de un intervalo.
TEST(partial_interval) {
    // [2, 5): 2^2 + 3^2 + 4^2 = 4 + 9 + 16 = 29.
    hpc::SumSquaresParams params{2, 5};
    ASSERT_EQ(hpc::calculate_partial(params), static_cast<uint64_t>(29));
}

// Intervalo vacio.
TEST(partial_empty_interval) {
    hpc::SumSquaresParams params{3, 3};
    ASSERT_EQ(hpc::calculate_partial(params), static_cast<uint64_t>(0));
}

// 8. Comparacion del resultado agregado con el calculo secuencial.
TEST(aggregate_matches_sequential) {
    const uint64_t N = 1000;
    const uint32_t task_count = 7;
    auto tasks = hpc::divide_work(N, task_count);

    std::vector<hpc::TaskResult> results;
    for (auto& t : tasks) {
        results.push_back(t.execute());
    }

    uint64_t aggregated = hpc::aggregate_results(results);
    uint64_t expected = hpc::calculate_sequential(N);
    ASSERT_EQ(aggregated, expected);
}

// 10. Entradas que excedan el rango numerico admitido.
TEST(overflow_rejected) {
    ASSERT_THROWS(
        hpc::calculate_sequential(hpc::SUM_SQUARES_MAX_N + 1),
        hpc::OverflowError
    );
}

TEST(overflow_divide_work_rejected) {
    ASSERT_THROWS(
        hpc::divide_work(hpc::SUM_SQUARES_MAX_N + 1, 4),
        hpc::OverflowError
    );
}

int main() {
    std::cout << "=== test_sum_squares ===\n";
    // Tests are auto-registered via static constructors.
    std::cout << "\nResultado: " << (g_tests_run - g_tests_failed)
              << "/" << g_tests_run << " tests pasaron.\n";
    return g_tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
