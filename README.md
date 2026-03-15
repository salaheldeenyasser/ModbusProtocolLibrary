# Modbus Protocol Library — C++23

A clean, modular, portable C++ library implementing the Modbus RTU serial
protocol with optional TCP support.  Designed for embedded systems, industrial
automation, and teaching environments.

---

## Features

| Feature | Status |
|---------|--------|
| Modbus RTU encoding / decoding | ✅ |
| CRC-16/IBM generation and verification | ✅ |
| Correct big-endian data / little-endian CRC byte order | ✅ |
| Function codes 01, 02, 03, 04, 05, 06, 0F, 10 | ✅ |
| Client API (read/write all data types) | ✅ |
| Server engine with FC dispatch | ✅ |
| Modbus exception responses (codes 01–04) | ✅ |
| Thread-safe RegisterMap with write callbacks | ✅ |
| UART transport (Linux) | ✅ |
| Virtual UART testing via `socat` | ✅ |
| Typed error handling via `std::expected` (no raw exceptions) | ✅ |
| MockTransport for hardware-free unit testing | ✅ |
| Demo server + client console programs | ✅ |
| Modbus TCP (MBAP header, no CRC) |  |

---

## Project Structure

```
modbus-lib/
├── include/modbus/          Public API headers (what users #include)
│   ├── ModbusClient.h
│   ├── ModbusServer.h
│   ├── RegisterMap.h
│   ├── IModbusTransport.h
│   ├── ModbusFrame.h
│   ├── ModbusError.h
│   └── ModbusTypes.h
├── src/                     Private implementation
│   ├── ModbusClient.cpp
│   ├── ModbusServer.cpp
│   ├── ModbusFrameCodec.h/cpp
│   ├── CrcEngine.h/cpp
│   └── RegisterMap.cpp
├── transport/               Platform-specific transports
│   ├── include/
│   │   └── UartTransport.h  (no OS headers — portable)
│   └── UartTransport_Linux.cpp
├── tests/
│   ├── MockTransport.h      In-memory transport for unit tests
│   ├── test_crc.cpp         14 CRC tests
│   ├── test_frame_codec.cpp 42 encoding/decoding tests
│   ├── test_client.cpp      36 client unit tests
│   ├── test_server.cpp      38 server unit tests
│   └── test_integration.cpp 32 client↔server integration tests
├── examples/
│   ├── server/main.cpp      modbus_server demo
│   └── client/main.cpp      modbus_client demo
└── CMakeLists.txt
```

---

## Building

### Prerequisites

- GCC ≥ 13 or Clang ≥ 17 (C++23 required for `std::expected`)
- CMake ≥ 3.16
- POSIX threads (`-lpthread`)
- Linux for UART transport; architecture is ready for Windows port

### Configure and Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Run all tests

```bash
cd build && ctest --output-on-failure
```

Or run individual test binaries:

```bash
./build/test_crc
./build/test_frame_codec
./build/test_client
./build/test_server
./build/test_integration
```

### Without CMake (direct g++)

```bash
FLAGS="-std=c++23 -I include -I transport/include -I src"

# Build library
g++ $FLAGS -c src/CrcEngine.cpp          -o /tmp/CrcEngine.o
g++ $FLAGS -c src/ModbusFrameCodec.cpp   -o /tmp/ModbusFrameCodec.o
g++ $FLAGS -c src/RegisterMap.cpp        -o /tmp/RegisterMap.o
g++ $FLAGS -c src/ModbusClient.cpp       -o /tmp/ModbusClient.o
g++ $FLAGS -c src/ModbusServer.cpp       -o /tmp/ModbusServer.o
g++ $FLAGS -c transport/UartTransport_Linux.cpp -o /tmp/UartTransport.o
ar rcs /tmp/libmodbus.a /tmp/*.o

# Run tests
g++ $FLAGS -I tests tests/test_crc.cpp /tmp/libmodbus.a -lpthread -o /tmp/t && /tmp/t
```

---

## Running the Demo Applications

### Step 1 — Create a virtual serial port pair (Linux)

```bash
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Output shows two device names, e.g. /dev/pts/5 and /dev/pts/6
```

### Step 2 — Start the server

```bash
./build/modbus_server --mode uart --port /dev/pts/5 --baud 9600 --id 1
```

The server console displays the live register map and reacts to client writes.

### Step 3 — Run client commands

```bash
# Read 3 holding registers from slave 1 at address 0x0000
./build/modbus_client --mode uart --port /dev/pts/6 --baud 9600 \
    read-regs --slave 1 --addr 0x0000 --count 3

# Write a new setpoint (35.0 °C * 10 = 350 raw)
./build/modbus_client --mode uart --port /dev/pts/6 --baud 9600 \
    write-reg --slave 1 --addr 0x0002 --value 350

# Turn pump ON
./build/modbus_client --mode uart --port /dev/pts/6 --baud 9600 \
    write-coil --slave 1 --addr 0x0000 --value 1

# Read coil status
./build/modbus_client --mode uart --port /dev/pts/6 --baud 9600 \
    read-coils --slave 1 --addr 0x0000 --count 2
```

---

## API Reference

### ModbusClient

```cpp
#include <modbus/ModbusClient.h>
#include <UartTransport.h>

UartTransport::Config cfg{ "/dev/ttyUSB0", 9600 };
ModbusClient client(std::make_unique<UartTransport>(cfg));
client.setDefaultTimeout(std::chrono::milliseconds(1000));
client.setRetryCount(3);

// Read 3 holding registers from slave 1 at address 0
auto regs = client.readHoldingRegisters(1, 0x0000, 3);
if (regs) {
    for (auto v : *regs) std::cout << v << "\n";
} else {
    std::cerr << modbusErrorToString(regs.error()) << "\n";
}

// Write a value
client.writeSingleRegister(1, 0x0002, 350);

// Write a coil
client.writeSingleCoil(1, 0x0000, true);
```

### ModbusServer

```cpp
#include <modbus/ModbusServer.h>
#include <UartTransport.h>

auto regMap = std::make_shared<RegisterMap>(2000, 10, 10, 2000);
regMap->writeHoldingRegister(0, 1500);   // initial motor speed

// React when client writes register 2 (setpoint)
regMap->onHoldingRegisterWrite(2, [](u16 addr, u16 val) {
    std::cout << "Setpoint changed to " << (val * 0.1f) << " C\n";
});

UartTransport::Config cfg{ "/dev/ttyUSB0", 9600 };
ModbusServer server(std::make_unique<UartTransport>(cfg), regMap, /*slaveId=*/1);
server.start();  // background thread

// server.stop() called on shutdown / signal handler
```

### Error Handling

All public API methods return `std::expected<T, ModbusError>`.

```cpp
auto result = client.readHoldingRegisters(1, 0x0064, 1);

if (!result) {
    std::visit([](auto&& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, TransportErrorCode>) {
            // Timeout, connection error, etc.
        } else if constexpr (std::is_same_v<T, ProtocolErrorCode>) {
            // CRC mismatch, unexpected response, etc.
        } else {
            // ModbusException: server reported an error
            std::cerr << e.description() << "\n";
        }
    }, result.error());
}
```

---

## Architecture

```
Application Code
      │
ModbusClient / ModbusServer   (public API)
      │
ModbusFrameCodec               (encode/decode PDUs — private)
      │
CrcEngine                      (CRC-16/IBM — private)
      │
IModbusTransport               (pure abstract interface)
      │
   ┌──┴──┐
   │     │
UartTransport   TcpTransport   MockTransport (tests)
```

Key design decisions:
- **Dependency Inversion**: Client and Server depend on `IModbusTransport` abstraction, never on concrete transport classes.
- **Stateless Codec**: `ModbusFrameCodec` has no state — pure byte ↔ struct transformation, trivially testable.
- **Callback-safe RegisterMap**: Callbacks are invoked **after** the mutex is released, eliminating deadlock risk.
- **Typed errors**: `std::expected` + `std::variant` forces callers to handle errors; no hidden exceptions.

---

## Testing Summary

| Suite | Tests | Result |
|-------|-------|--------|
| `test_crc` | 14 | ✅ 14/14 |
| `test_frame_codec` | 42 | ✅ 42/42 |
| `test_client` | 36 | ✅ 36/36 |
| `test_server` | 38 | ✅ 38/38 |
| `test_integration` | 32 | ✅ 32/32 |
| **Total** | **162** | **✅ 162/162** |

All tests run without hardware using `MockTransport` (unit) and `LoopTransport` (integration).

---

## Modbus Protocol Quick Reference

| Type | Bits | R/W | PDU Address | Example |
|------|------|-----|-------------|---------|
| Coils | 1 | R/W | 0x0000+ | Relay ON/OFF |
| Discrete Inputs | 1 | R | 0x0000+ | Door sensor |
| Input Registers | 16 | R | 0x0000+ | Sensor reading |
| Holding Registers | 16 | R/W | 0x0000+ | Setpoints, config |

**Data fields** are big-endian.  **CRC bytes** are little-endian (low byte first). This asymmetry is in the Modbus specification and cannot be changed.
