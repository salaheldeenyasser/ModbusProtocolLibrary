#ifndef REGISTER_MAP_H
#define REGISTER_MAP_H
#include "ModbusError.h"
#include "ModbusFrame.h"
#include <mutex>
#include <unordered_map>

// Thread-safe storage for all four Modbus data types.
// The server reads/writes this; your application also reads/writes it to
// inject sensor data or react to client commands.
class RegisterMap {
public:
    static constexpr u16 DEFAULT_COILS             = 2000;
    static constexpr u16 DEFAULT_HOLDING_REGISTERS = 125;
    static constexpr u16 DEFAULT_INPUT_REGISTERS   = 125;
    static constexpr u16 DEFAULT_DISCRETE_INPUTS   = 2000;

    explicit RegisterMap(
        u16 numCoils             = DEFAULT_COILS,
        u16 numHoldingRegisters  = DEFAULT_HOLDING_REGISTERS,
        u16 numInputRegisters    = DEFAULT_INPUT_REGISTERS,
        u16 numDiscreteInputs    = DEFAULT_DISCRETE_INPUTS);

    // ── Coils (1-bit R/W) ────────────────────────────────────────────────────
    bool readCoil(u16 addr) const;
    void writeCoil(u16 addr, bool value);
    bool isValidCoilAddress(u16 addr) const;

    // ── Discrete Inputs (1-bit R only — written internally by application) ───
    bool readDiscreteInput(u16 addr) const;
    void writeDiscreteInput(u16 addr, bool value); // application-facing
    bool isValidDiscreteInputAddress(u16 addr) const;

    // ── Input Registers (16-bit R only — written internally by application) ──
    u16 readInputRegister(u16 addr) const;
    void writeInputRegister(u16 addr, u16 value);  // application-facing
    bool isValidInputRegisterAddress(u16 addr) const;

    // ── Holding Registers (16-bit R/W) ───────────────────────────────────────
    u16 readHoldingRegister(u16 addr) const;
    void writeHoldingRegister(u16 addr, u16 value);
    bool isValidHoldingRegisterAddress(u16 addr) const;

    // ── Callbacks fired when a Modbus client writes a value ──────────────────
    using HoldingRegWriteCallback = std::function<void(u16 address, u16 value)>;
    void onHoldingRegisterWrite(u16 address, HoldingRegWriteCallback callback);

    using CoilWriteCallback = std::function<void(u16 address, bool value)>;
    void onCoilWrite(u16 address, CoilWriteCallback callback);

private:
    std::vector<u16>  holdingRegisters_;
    std::vector<u16>  inputRegisters_;
    std::vector<bool> coils_;
    std::vector<bool> discreteInputs_;

    mutable std::mutex mutex_;

    std::unordered_map<u16, HoldingRegWriteCallback> holdingWriteCBs_;
    std::unordered_map<u16, CoilWriteCallback>       coilWriteCBs_;
};

#endif // REGISTER_MAP_H
