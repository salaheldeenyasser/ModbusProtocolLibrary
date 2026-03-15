#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H
#include "IModbusTransport.h"
#include "ModbusError.h"
#include "ModbusFrame.h"

// High-level Modbus client (master) API.
// Users only need this class + a concrete IModbusTransport.
//
// Usage:
//   UartTransport::Config cfg{"/dev/ttyUSB0", 9600};
//   ModbusClient client(std::make_unique<UartTransport>(cfg));
//   auto regs = client.readHoldingRegisters(1, 0x0000, 3);
class ModbusClient {
public:
    explicit ModbusClient(std::unique_ptr<IModbusTransport> transport);
    ~ModbusClient();

    // How long to wait for a response before timing out and retrying.
    void setDefaultTimeout(std::chrono::milliseconds timeout);
    // How many times to retry on timeout or CRC error (default: 3).
    void setRetryCount(u8 count);

    // ── Read operations ───────────────────────────────────────────────────────

    // FC 0x01 – Read Coils (1–2000 coils)
    std::expected<std::vector<bool>, ModbusError>
    readCoils(u8 slaveId, u16 startAddr, u16 count);

    // FC 0x02 – Read Discrete Inputs (1–2000 inputs)
    std::expected<std::vector<bool>, ModbusError>
    readDiscreteInputs(u8 slaveId, u16 startAddr, u16 count);

    // FC 0x03 – Read Holding Registers (1–125 registers)
    std::expected<std::vector<u16>, ModbusError>
    readHoldingRegisters(u8 slaveId, u16 startAddr, u16 count);

    // FC 0x04 – Read Input Registers (1–125 registers)
    std::expected<std::vector<u16>, ModbusError>
    readInputRegisters(u8 slaveId, u16 startAddr, u16 count);

    // ── Write operations ──────────────────────────────────────────────────────

    // FC 0x05 – Write Single Coil (value: true=ON, false=OFF)
    std::expected<void, ModbusError>
    writeSingleCoil(u8 slaveId, u16 addr, bool value);

    // FC 0x06 – Write Single Register
    std::expected<void, ModbusError>
    writeSingleRegister(u8 slaveId, u16 addr, u16 value);

    // FC 0x0F – Write Multiple Coils (1–1968 coils)
    std::expected<void, ModbusError>
    writeMultipleCoils(u8 slaveId, u16 startAddr,
                       std::span<const bool> values);

    // FC 0x10 – Write Multiple Registers (1–123 registers)
    std::expected<void, ModbusError>
    writeMultipleRegisters(u8 slaveId, u16 startAddr,
                           std::span<const u16> values);

private:
    std::unique_ptr<IModbusTransport> transport_;
    std::chrono::milliseconds         timeout_{1000};
    u8                                retryCount_{3};

    // Core: encode → send → receive → decode, with retry logic.
    // Defined in ModbusClient.cpp to keep the private codec include out of
    // this public header.
    std::expected<ModbusFrame, ModbusError>
    sendAndReceive(const ModbusFrame& request);
};

#endif // MODBUS_CLIENT_H
