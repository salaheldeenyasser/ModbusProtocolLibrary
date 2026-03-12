#ifndef MODBUS_FRAME_H
#define MODBUS_FRAME_H
#include "ModbusTypes.h"


struct ModbusFrame {
  u8 slaveID;
  u8 functionCode;
  std::vector<u8> data;
  u8 crcLow;
  u8 crcHigh;

  bool isExceptionResponse() const { return (functionCode & 0x7f) != 0; };

  u8 exceptionCode() const { return data.empty() ? 0 : data[0]; };

  u16 startAddress() const { return (data[0] << 8) | data[1]; };

  u16 quantity() const { return (data[2] << 8) | data[3]; };

  u16 registerValue(sz i) const {
    return (data[0 + i * 2] << 8) | data[2 + i * 2];
  }
};

#endif // MODBUS_FRAME_H