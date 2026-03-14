#include "CrcEngine.h"

u16 CrcEngine::calculate(const u8* data, sz length) {
    u16 crc = 0xFFFF; // Initial value

    for (sz i = 0; i < length; ++i) {
        crc ^= static_cast<u16>(data[i]); // XOR byte into LSB of CRC

        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {      // If LSB is set
                crc >>= 1;           // Shift right
                crc ^= 0xA001;       // XOR with Modbus polynomial
            } else {
                crc >>= 1;           // Just shift right
            }
        }
    }
//   for (sz i = 0; i < length; i++) {
//     crc ^= data[i];
//     for (int j = 0; j < 8; j++) {
//       if (crc & 1) {
//         crc = (crc >> 1) ^ 0xA001;
//       } else {
//         crc = crc >> 1;
//       }
//     }
//   }
  return crc;
}

bool CrcEngine::verify(const u8 *data, sz length, u16 crc) {
  return calculate(data, length) == crc;
}
