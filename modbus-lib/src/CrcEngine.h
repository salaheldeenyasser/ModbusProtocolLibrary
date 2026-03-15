#ifndef CRC_ENGINE_H
#define CRC_ENGINE_H
#include "../include/modbus/ModbusTypes.h"

// Stateless CRC-16/IBM (also called CRC-16/ARC) engine.
// Polynomial: 0x8005 reflected → 0xA001
// Init: 0xFFFF, no final XOR.
// Residue property: CRC of a complete valid frame (including appended CRC
// bytes) is exactly 0x0000.
class CrcEngine {
public:
    // Calculate CRC of 'length' bytes starting at 'data'.
    static u16  calculate(const u8* data, sz length);

    // Verify that calculate(data, length) == crc.
    static bool verify(const u8* data, sz length, u16 crc);
};

#endif // CRC_ENGINE_H
