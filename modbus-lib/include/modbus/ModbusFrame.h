#ifndef MODBUS_FRAME_H
#define MODBUS_FRAME_H
#include "ModbusTypes.h"

struct ModbusFrame {
    u8             slaveID      = 0;
    u8             functionCode = 0;
    std::vector<u8> data;
    u16            crc          = 0;  // stored after decode; not used for re-encode

    // ── Helpers ─────────────────────────────────────────────────────────────

    // True when the server returned an exception (bit 7 set in FC byte).
    bool isExceptionResponse() const {
        return (functionCode & 0x80) != 0;
    }

    // Exception code from data[0], valid only when isExceptionResponse().
    u8 exceptionCode() const {
        return data.empty() ? 0 : data[0];
    }

    // For read-request frames: start address from data[0..1]
    u16 startAddress() const {
        if (data.size() < 2) return 0;
        return static_cast<u16>((static_cast<u16>(data[0]) << 8) | data[1]);
    }

    // For read-request frames: quantity from data[2..3]
    u16 quantity() const {
        if (data.size() < 4) return 0;
        return static_cast<u16>((static_cast<u16>(data[2]) << 8) | data[3]);
    }

    // For register-read-response frames:
    //   data[0]        = byte count
    //   data[1+i*2]    = register[i] high byte
    //   data[2+i*2]    = register[i] low  byte
    // BUG FIX: original code used data[0+i*2] and data[2+i*2] which skipped
    // the byte-count byte and produced wrong values.
    u16 registerValue(sz i) const {
        sz offset = 1u + i * 2u;           // skip byte-count byte at data[0]
        if (offset + 1 >= data.size()) return 0;
        return static_cast<u16>(
            (static_cast<u16>(data[offset]) << 8) | data[offset + 1]);
    }
};

#endif // MODBUS_FRAME_H
