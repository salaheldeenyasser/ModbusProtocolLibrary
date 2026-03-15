cmake_minimum_required(VERSION 3.16)
project(ModbusLib VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wno-unused-function)
endif()

# ── Core library ──────────────────────────────────────────────────────────────
add_library(modbus_lib STATIC
    src/CrcEngine.cpp
    src/ModbusFrameCodec.cpp
    src/ModbusClient.cpp
    src/ModbusServer.cpp
    src/RegisterMap.cpp
)

# Platform-specific UART transport
if(UNIX)
    target_sources(modbus_lib PRIVATE transport/UartTransport_Linux.cpp)
    message(STATUS "UART transport: Linux")
elseif(WIN32)
    message(STATUS "UART transport: Windows (add UartTransport_Windows.cpp)")
endif()

# TCP transport (POSIX sockets — works on Linux and macOS)
if(UNIX)
    target_sources(modbus_lib PRIVATE transport/TcpTransport.cpp)
    message(STATUS "TCP transport: enabled")
endif()

target_include_directories(modbus_lib
    PUBLIC  include/
    PUBLIC  transport/include/
    PRIVATE src/
)

find_package(Threads REQUIRED)
target_link_libraries(modbus_lib PUBLIC Threads::Threads)

# ── Demo applications ─────────────────────────────────────────────────────────
add_executable(modbus_server examples/server/main.cpp)
target_link_libraries(modbus_server PRIVATE modbus_lib)
target_include_directories(modbus_server PRIVATE src/)

add_executable(modbus_client examples/client/main.cpp)
target_link_libraries(modbus_client PRIVATE modbus_lib)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()

function(add_modbus_test name src)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE modbus_lib)
    target_include_directories(${name} PRIVATE src/ tests/)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_modbus_test(test_crc            tests/test_crc.cpp)
add_modbus_test(test_frame_codec    tests/test_frame_codec.cpp)
add_modbus_test(test_client         tests/test_client.cpp)
add_modbus_test(test_server         tests/test_server.cpp)
add_modbus_test(test_integration    tests/test_integration.cpp)
add_modbus_test(test_tcp_codec      tests/test_tcp_codec.cpp)
