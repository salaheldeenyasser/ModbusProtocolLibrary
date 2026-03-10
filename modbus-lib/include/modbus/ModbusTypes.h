#ifndef MODBUS_TYPES_H
#define MODBUS_TYPES_H
#include <cstddef>
#include <cstdint>
#include <vector>

#define MAX_HOLDING_REGISTERS 125
#define MAX_COILS 2000

struct ModbusFrame {
  uint8_t slaveID;
  uint8_t functionCode;
  std::vector<uint8_t> data;
  uint8_t crcLow;
  uint8_t crcHigh;

  bool isExceptionResponse() const { return (functionCode & 0x80) != 0; };

  uint8_t exceptionCode() const { return data.empty() ? 0 : data[0]; };

  uint16_t startAddress() const { return (data[0] << 8) | data[1]; };

  uint16_t quantity() const { return (data[2] << 8) | data[3]; };

  uint16_t registerValue(size_t i) const {
    return (data[1 + i * 2] << 8) | data[2 + i * 2];
  }
};

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