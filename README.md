# Modbus Protocol Library (C++)

A lightweight Modbus RTU-oriented C++ library that provides reusable protocol building blocks:

- Frame construction and parsing
- CRC16 (Modbus) calculation and verification
- A transport abstraction for serial/UART and custom backends
- A client API for common Modbus function codes

This repository currently contains a partially implemented library skeleton. Core codec and client pieces exist, while server, UART transport, examples, and tests are scaffolded but not yet implemented.

## Project Status

Implemented now:

- `modbus-lib/src/CrcEngine.cpp`: CRC16 (polynomial `0xA001`) logic
- `modbus-lib/src/ModbusFrameCodec.cpp`: RTU encode/decode and request factory helpers
- `modbus-lib/src/ModbusClient.cpp`: most client-side FC handlers (`0x01`, `0x02`, `0x03`, `0x04`, `0x05`, `0x06`, `0x0F`, `0x10`)
- Public API headers under `modbus-lib/include/modbus/`

Scaffolded / TODO:

- `modbus-lib/src/ModbusServer.cpp` (empty)
- `modbus-lib/include/modbus/ModbusServer.h` (empty)
- `modbus-lib/src/RegisterMap.cpp` (empty)
- `modbus-lib/transport/UartTransport.cpp` and `modbus-lib/transport/include/UartTransport.h` (empty)
- `modbus-lib/examples/` (empty)
- `modbus-lib/tests/mocks/` (empty)

## Repository Layout

```text
ModbusProtocolLibrary/
├── README.md
└── modbus-lib/
		├── include/modbus/
		│   ├── IModbusTransport.h
		│   ├── ModbusClient.h
		│   ├── ModbusError.h
		│   ├── ModbusFrame.h
		│   ├── ModbusServer.h
		│   ├── ModbusTypes.h
		│   └── RegisterMap.h
		├── src/
		│   ├── CrcEngine.cpp
		│   ├── CrcEngine.h
		│   ├── ModbusClient.cpp
		│   ├── ModbusFrameCodec.cpp
		│   ├── ModbusFrameCodec.h
		│   ├── ModbusServer.cpp
		│   └── RegisterMap.cpp
		├── transport/
		│   ├── UartTransport.cpp
		│   └── include/UartTransport.h
		├── examples/
		└── tests/
				└── mocks/
```

## Requirements

### Language and toolchain

- C++23-capable compiler
	- `std::expected` is used in public interfaces
- A standard build toolchain (`g++`)
- Git


### Install dependencies


## Getting the source

```bash
git clone <your-fork-or-repo-url> ModbusProtocolLibrary
cd ModbusProtocolLibrary
```

## Build


## API Overview

### Core types

- `ModbusFrame`: RTU frame representation (`slaveID`, `functionCode`, `data`, `crc`)
- `ModbusError`: `std::variant` over:
	- `TransportErrorCode`
	- `ProtocolErrorCode`
	- `ModbusException`

### Transport abstraction

The `IModbusTransport` interface decouples protocol logic from physical transport:

- `send(std::span<const u8>) -> std::expected<size_t, ModbusError>`
- `receive(size_t expectedLength, std::chrono::milliseconds timeout) -> std::expected<std::vector<u8>, ModbusError>`
- `flush()`, `isOpen()`, `close()`

### Client operations

`ModbusClient` currently exposes:

- Read:
	- `readCoils` (`0x01`)
	- `readDiscreteInputs` (`0x02`)
	- `readHoldingRegisters` (`0x03`)
	- `readInputRegisters` (`0x04`)
- Write:
	- `writeSingleCoil` (`0x05`)
	- `writeSingleRegister` (`0x06`)
	- `writeMultipleCoils` (`0x0F`)
	- `writeMultipleRegisters` (`0x10`)

`ModbusFrameCodec` also has request factories for diagnostics (`0x08`) and read/write multiple registers (`0x17`).

## Minimal usage example

```cpp
#include "modbus/ModbusClient.h"
#include "modbus/IModbusTransport.h"
#include <memory>

class MyTransport final : public IModbusTransport {
public:
		std::expected<size_t, ModbusError> send(std::span<const u8> data) override;
		std::expected<std::vector<u8>, ModbusError> receive(size_t expectedLength, std::chrono::milliseconds timeout) override;
		void flush() override;
		bool isOpen() const override;
		void close() override;
};

int main() {
		auto transport = std::make_unique<MyTransport>();
		ModbusClient client(std::move(transport));

		auto result = client.readHoldingRegisters(1, 0, 4);
		if (!result) {
				// Inspect result.error() (TransportErrorCode / ProtocolErrorCode / ModbusException)
				return 1;
		}

		const auto &registers = result.value();
		(void)registers;
		return 0;
}
```

## Protocol and architecture diagrams (PlantUML)


## Error handling model

The code uses `std::expected<T, ModbusError>` for error propagation.

- Transport-level failures:
	- timeout / read / write / connection failures
- Protocol-level failures:
	- CRC mismatch
	- invalid frame lengths
	- unexpected response shape
	- slave ID mismatch
- Device exception responses:
	- Modbus exception frame (`functionCode | 0x80`)

## Testing


## Known limitations (current snapshot)


## Contributing

1. Fork and create a feature branch.
2. Keep protocol behavior covered by tests.
3. Prefer small focused commits.
4. Open a PR with protocol traces or test evidence.

## Suggested roadmap
