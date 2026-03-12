#ifndef MODBUS_TYPES_H
#define MODBUS_TYPES_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#define sz size_t
#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

const int MAX_HOLDING_REGISTERS = 125;
const int MAX_COILS = 2000;



enum class FunctionCode : uint8_t {
  ReadCoils = 0x01,
  ReadDiscreteInputs = 0x02,
  ReadHoldingRegisters = 0x03,
  ReadInputRegisters = 0x04,
  WriteSingleCoil = 0x05,
  WriteSingleRegister = 0x06,
  WriteMultipleCoils = 0x0F,
  WriteMultipleRegisters = 0x10,
  Diagnostic = 0x08,
  ReadWriteMultipleRegisters = 0x17,
};

enum class ExceptionCode : uint8_t {
  IllegalFunction = 0x01,
  IllegalDataAddress = 0x02,
  IllegalDataValue = 0x03,
  SlaveDeviceFailure = 0x04,
};

#endif // MODBUS_TYPES_H