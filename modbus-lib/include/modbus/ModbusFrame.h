#ifndef MODBUS_FRAME_H
#define MODBUS_FRAME_H
#include "ModbusTypes.h"


struct ModbusFrame {
  uint8_t slaveID;
  uint8_t functionCode;
  std::vector<uint8_t> data;
  uint8_t crcLow;
  uint8_t crcHigh;

  bool isExceptionResponse() const { return (functionCode & 0x7f) != 0; };

  uint8_t exceptionCode() const { return data.empty() ? 0 : data[0]; };

  uint16_t startAddress() const { return (data[0] << 8) | data[1]; };

  uint16_t quantity() const { return (data[2] << 8) | data[3]; };

  uint16_t registerValue(size_t i) const {
    return (data[0 + i * 2] << 8) | data[2 + i * 2];
  }
};

#endif // MODBUS_FRAME_H