cmake_minimum_required(VERSION 3.16)
project(hpc-system LANGUAGES CXX)

# ==============================================================================
# Estándar y opciones del compilador
# ==============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Soporte de hilos nativos para pruebas de red y concurrencia
find_package(Threads REQUIRED)

# Advertencias estrictas para GCC y Clang.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Enlaces estáticos de runtime en MinGW para portabilidad de ejecutables
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++)
endif()

# ==============================================================================
# Directorios de inclusión globales
# ==============================================================================
include_directories(${CMAKE_SOURCE_DIR}/include)

# ==============================================================================
# Biblioteca: hpc_tasks (Fase 1)
# ==============================================================================
add_library(hpc_tasks STATIC
    src/tasks/sum_squares.cpp
)
target_include_directories(hpc_tasks PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# ==============================================================================
# Biblioteca: hpc_protocol (Fase 2)
# ==============================================================================
add_library(hpc_protocol STATIC
    src/protocol/serializer.cpp
    src/protocol/framing.cpp
)
target_include_directories(hpc_protocol PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# ==============================================================================
# Biblioteca: hpc_network (Fase 2)
# ==============================================================================
add_library(hpc_network STATIC
    src/network/socket.cpp
    src/network/tcp_server.cpp
    src/network/tcp_client.cpp
    src/network/connection.cpp
)
target_include_directories(hpc_network PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(hpc_network PUBLIC
    hpc_protocol
    Threads::Threads
)

# Enlace con Winsock2 en entornos Windows
if(WIN32)
    target_link_libraries(hpc_network PUBLIC ws2_32)
endif()

# ==============================================================================
# Biblioteca: hpc_master_lib (Fase 3)
# ==============================================================================
add_library(hpc_master_lib STATIC
    src/master/worker_registry.cpp
    src/master/master_server.cpp
)
target_include_directories(hpc_master_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(hpc_master_lib PUBLIC
    hpc_network
    Threads::Threads
)

# ==============================================================================
# Biblioteca: hpc_worker_lib (Fase 3)
# ==============================================================================
add_library(hpc_worker_lib STATIC
    src/worker/worker_node.cpp
)
target_include_directories(hpc_worker_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(hpc_worker_lib PUBLIC
    hpc_network
    Threads::Threads
)

# ==============================================================================
# Ejecutables principales
# ==============================================================================

# Cliente: ejecuta cálculo local y demostración.
add_executable(hpc_client src/client/main.cpp)
target_link_libraries(hpc_client PRIVATE hpc_tasks hpc_network)

# Master: servidor de coordinación TCP.
add_executable(hpc_master src/master/main.cpp)
target_link_libraries(hpc_master PRIVATE hpc_master_lib hpc_tasks)

# Worker: nodo trabajador conectado por TCP.
add_executable(hpc_worker src/worker/main.cpp)
target_link_libraries(hpc_worker PRIVATE hpc_worker_lib hpc_tasks)

# ==============================================================================
# Pruebas (CTest)
# ==============================================================================
enable_testing()

# Pruebas Fase 1
add_executable(test_sum_squares tests/unit/test_sum_squares.cpp)
target_link_libraries(test_sum_squares PRIVATE hpc_tasks)
add_test(NAME SumSquaresTest COMMAND test_sum_squares)

add_executable(test_partition tests/unit/test_partition.cpp)
target_link_libraries(test_partition PRIVATE hpc_tasks)
add_test(NAME PartitionTest COMMAND test_partition)

# Pruebas Fase 2
add_executable(test_protocol tests/unit/test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE hpc_protocol)
add_test(NAME ProtocolTest COMMAND test_protocol)

add_executable(test_network tests/integration/test_network.cpp)
target_link_libraries(test_network PRIVATE hpc_network Threads::Threads)
add_test(NAME NetworkTest COMMAND test_network)

# Pruebas Fase 3
add_executable(test_master_worker tests/integration/test_master_worker.cpp)
target_link_libraries(test_master_worker PRIVATE hpc_master_lib hpc_worker_lib Threads::Threads)
add_test(NAME MasterWorkerTest COMMAND test_master_worker)
