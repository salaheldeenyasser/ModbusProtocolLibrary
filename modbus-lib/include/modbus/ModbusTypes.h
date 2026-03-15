#ifndef MODBUS_TYPES_H
#define MODBUS_TYPES_H

// Core C++ standard library — minimal set needed at the type level.
// Implementation-only headers (<mutex>, <thread>, <atomic>, etc.) are
// included directly by the headers that need them, not here.
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include <expected>
#include <span>
#include <algorithm>
#include <chrono>
#include <functional>
#include <stdexcept>

// Type aliases — 'using' respects scope; #define does not.
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using sz  = size_t;

// Modbus protocol limits
static constexpr u16 MODBUS_MAX_HOLDING_REGISTERS = 125;
static constexpr u16 MODBUS_MAX_COILS             = 2000;
static constexpr u16 MODBUS_MAX_INPUT_REGISTERS   = 125;
static constexpr u16 MODBUS_MAX_DISCRETE_INPUTS   = 2000;

enum class FunctionCode : u8 {
    ReadCoils                  = 0x01,
    ReadDiscreteInputs         = 0x02,
    ReadHoldingRegisters       = 0x03,
    ReadInputRegisters         = 0x04,
    WriteSingleCoil            = 0x05,
    WriteSingleRegister        = 0x06,
    Diagnostic                 = 0x08,
    WriteMultipleCoils         = 0x0F,
    WriteMultipleRegisters     = 0x10,
    ReadWriteMultipleRegisters = 0x17,
};

enum class ExceptionCode : u8 {
    IllegalFunction    = 0x01,
    IllegalDataAddress = 0x02,
    IllegalDataValue   = 0x03,
    SlaveDeviceFailure = 0x04,
};

#endif // MODBUS_TYPES_H
