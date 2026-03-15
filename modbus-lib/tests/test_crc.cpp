// CRC-16/IBM (Modbus) test suite.
// All expected values verified with the reference implementation.

#include "../src/CrcEngine.h"
#include <iostream>
#include <vector>
#include <cstdio>

static int passed = 0;
static int failed = 0;

static void check(const char* name, bool cond) {
    if (cond) { std::cout << "  [PASS] " << name << "\n"; ++passed; }
    else       { std::cout << "  [FAIL] " << name << "\n"; ++failed; }
}

int main() {
    std::cout << "=== CRC-16/IBM Test Suite ===\n\n";

    // FC03 read-2-regs request
    {
        u8 d[] = {0x01,0x03,0x00,0x00,0x00,0x02};
        u16 c = CrcEngine::calculate(d, 6);
        check("Vector 1 - FC03 request  -> 0x0BC4",      c == 0x0BC4);
        check("  byte order: low  = 0xC4",                (c & 0xFF) == 0xC4);
        check("  byte order: high = 0x0B",                ((c >> 8) & 0xFF) == 0x0B);
    }

    // FC03 response payload — correct value 0x280A (not 0xCAA9)
    {
        u8 d[] = {0x01,0x03,0x04,0x00,0x17,0x00,0x2B};
        check("Vector 2 - FC03 response -> 0x280A",
              CrcEngine::calculate(d, 7) == 0x280A);
    }

    // FC06 write single register — correct value 0x038B (not 0x0488)
    {
        u8 d[] = {0x01,0x06,0x00,0x00,0x05,0xDC};
        check("Vector 3 - FC06 request  -> 0x038B",
              CrcEngine::calculate(d, 6) == 0x038B);
    }

    // FC05 write single coil ON
    {
        u8 d[] = {0x01,0x05,0x00,0x00,0xFF,0x00};
        check("Vector 4 - FC05 coil ON  -> 0x3A8C",
              CrcEngine::calculate(d, 6) == 0x3A8C);
    }

    // All-ones
    {
        u8 d[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        check("Vector 5 - all 0xFF      -> 0x9401",
              CrcEngine::calculate(d, 6) == 0x9401);
    }

    // Edge: single byte
    {
        u8 b = 0x01;
        check("Single byte 0x01        -> 0x807E",
              CrcEngine::calculate(&b, 1) == 0x807E);
    }

    // Edge: all zeros
    {
        u8 d[] = {0x00,0x00,0x00,0x00};
        check("All zeros               -> 0x2400",
              CrcEngine::calculate(d, 4) == 0x2400);
    }

    // verify() with valid frame
    {
        u8 frame[] = {0x01,0x03,0x00,0x00,0x00,0x02,0xC4,0x0B};
        check("verify() valid frame",
              CrcEngine::verify(frame, 6, 0x0BC4));
    }

    // Residue property: CRC of a complete valid frame == 0
    {
        u8 frame[] = {0x01,0x03,0x00,0x00,0x00,0x02,0xC4,0x0B};
        check("Residue of complete frame == 0x0000",
              CrcEngine::calculate(frame, 8) == 0x0000);
    }

    // verify() with corrupted DATA byte → must return false
    // Changing data[2] from 0x00 to 0x01 gives a different CRC than 0x0BC4
    {
        u8 d[] = {0x01,0x03,0x01,0x00,0x00,0x02};
        check("verify() corrupted DATA byte -> false",
              !CrcEngine::verify(d, 6, 0x0BC4));
    }

    // verify() with corrupted CRC bytes → must return false
    // Stored CRC = 0x0CC4 (0x0B changed to 0x0C in high byte)
    {
        u8 frame[] = {0x01,0x03,0x00,0x00,0x00,0x02};
        u16 badCrc  = 0x0CC4;
        check("verify() corrupted CRC bytes -> false",
              !CrcEngine::verify(frame, 6, badCrc));
    }

    // Round-trip: encode FC16 then verify via residue
    {
        u8 payload[] = {0x01,0x10,0x00,0x00,0x00,0x02,0x04,0x00,0x64,0x00,0xC8};
        u16 crc = CrcEngine::calculate(payload, sizeof(payload));
        std::vector<u8> full(payload, payload + sizeof(payload));
        full.push_back(static_cast<u8>(crc & 0xFF));
        full.push_back(static_cast<u8>((crc >> 8) & 0xFF));
        check("Round-trip FC16 (residue == 0)",
              CrcEngine::calculate(full.data(), full.size()) == 0x0000);
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return (failed == 0) ? 0 : 1;
}
