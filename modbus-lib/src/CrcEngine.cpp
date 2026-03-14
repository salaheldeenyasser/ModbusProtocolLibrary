#include "CrcEngine.h"

u16 CrcEngine::calculate(const u8* data, size_t length) {
    u16 crc = 0xFFFF;

    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<u16>(data[i] & 0xFF);

        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

bool CrcEngine::verify(const u8 *data, sz length, u16 crc) {
  return calculate(data, length) == crc;
}
