#ifndef CRC_ENGINE_H
#define CRC_ENGINE_H
#include "../include/modbus/ModbusTypes.h"

class CrcEngine {
public:
  static u16 calculate(const u8 *data, sz length);
  static bool verify(const u8 *data, sz length, u16 crc);
};

#endif // CRC_ENGINE_H