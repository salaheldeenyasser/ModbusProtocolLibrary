// Frame codec test suite — encoding and decoding.
// All CRC values in test frames verified with calc_crc.cpp.

#include "../src/ModbusFrameCodec.h"
#include <iostream>
#include <vector>
#include <cstdio>

static int passed = 0;
static int failed = 0;

static void check(const char* name, bool cond) {
    if (cond) { std::cout << "  [PASS] " << name << "\n"; ++passed; }
    else       { std::cout << "  [FAIL] " << name << "\n"; ++failed; }
}

static bool eq(const std::vector<u8>& a, const std::vector<u8>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return false;
    return true;
}

// ── Encoding ──────────────────────────────────────────────────────────────────

void test_encoding() {
    std::cout << "--- Encoding ---\n";

    // FC03: read 2 holding registers from slave 1 at address 0x0000
    // CRC of [01 03 00 00 00 02] = 0x0BC4 -> [C4 0B]
    {
        auto frame = ModbusFrameCodec::makeReadHoldingRegistersRequest(1, 0x0000, 2);
        auto rtu   = ModbusFrameCodec::encodeRtuRequest(frame);
        std::vector<u8> expected = {0x01,0x03,0x00,0x00,0x00,0x02,0xC4,0x0B};
        check("FC03 read 2 regs", eq(rtu, expected));
    }

    // FC05 ON: CRC of [01 05 00 00 FF 00] = 0x3A8C -> [8C 3A]
    {
        auto frame = ModbusFrameCodec::makeWriteSingleCoilRequest(1, 0x0000, true);
        auto rtu   = ModbusFrameCodec::encodeRtuRequest(frame);
        std::vector<u8> expected = {0x01,0x05,0x00,0x00,0xFF,0x00,0x8C,0x3A};
        check("FC05 coil ON", eq(rtu, expected));
    }

    // FC05 OFF: CRC of [01 05 00 00 00 00] = 0xCACD -> [CD CA]
    {
        auto frame = ModbusFrameCodec::makeWriteSingleCoilRequest(1, 0x0000, false);
        auto rtu   = ModbusFrameCodec::encodeRtuRequest(frame);
        std::vector<u8> expected = {0x01,0x05,0x00,0x00,0x00,0x00,0xCD,0xCA};
        check("FC05 coil OFF", eq(rtu, expected));
    }

    // FC06: write 100 to register 0x0002
    // CRC of [01 06 00 02 00 64] = 0xE129 -> [29 E1]
    {
        auto frame = ModbusFrameCodec::makeWriteSingleRegisterRequest(1, 0x0002, 100);
        auto rtu   = ModbusFrameCodec::encodeRtuRequest(frame);
        std::vector<u8> expected = {0x01,0x06,0x00,0x02,0x00,0x64,0x29,0xE1};
        check("FC06 write single register", eq(rtu, expected));
    }

    // Big-endian address encoding: address 0x0100
    {
        auto frame = ModbusFrameCodec::makeReadHoldingRegistersRequest(1, 0x0100, 1);
        auto rtu   = ModbusFrameCodec::encodeRtuRequest(frame);
        check("Big-endian address hi=0x01", rtu.size() >= 4 && rtu[2] == 0x01);
        check("Big-endian address lo=0x00", rtu.size() >= 4 && rtu[3] == 0x00);
    }

    // FC16: write 2 registers {100, 200} at 0x0000
    // Layout: [01 10 00 00 00 02 04 00 64 00 C8 CRC_L CRC_H] (13 bytes)
    {
        auto frame = ModbusFrameCodec::makeWriteMultipleRegistersRequest(
            1, 0x0000, {100, 200});
        auto rtu = ModbusFrameCodec::encodeRtuRequest(frame);
        check("FC16 length == 13",       rtu.size() == 13);
        check("FC16 FC byte = 0x10",     rtu[1]  == 0x10);
        check("FC16 qty hi = 0x00",      rtu[4]  == 0x00);
        check("FC16 qty lo = 0x02",      rtu[5]  == 0x02);
        check("FC16 byte count = 0x04",  rtu[6]  == 0x04);
        check("FC16 reg[0] hi = 0x00",   rtu[7]  == 0x00);
        check("FC16 reg[0] lo = 0x64",   rtu[8]  == 0x64); // 100
        check("FC16 reg[1] hi = 0x00",   rtu[9]  == 0x00);
        check("FC16 reg[1] lo = 0xC8",   rtu[10] == 0xC8); // 200
    }

    // FC0F: write 3 coils {ON, OFF, ON} at 0x0000
    // byte_count=1, packed=0b00000101=0x05
    {
        auto frame = ModbusFrameCodec::makeWriteMultipleCoilsRequest(
            1, 0x0000, {true, false, true});
        auto rtu = ModbusFrameCodec::encodeRtuRequest(frame);
        check("FC0F length == 10",      rtu.size() == 10);
        check("FC0F byte count = 1",    rtu[6] == 0x01);
        check("FC0F coil byte = 0x05",  rtu[7] == 0x05);
    }
}

// ── Decoding ──────────────────────────────────────────────────────────────────

void test_decoding() {
    std::cout << "\n--- Decoding ---\n";

    // Valid FC03 response: slave=1, 2 registers, values=[23, 43]
    // CRC of [01 03 04 00 17 00 2B] = 0x280A -> [0A 28]  (NOT A9 CA)
    {
        std::vector<u8> raw = {0x01,0x03,0x04,0x00,0x17,0x00,0x2B,0x0A,0x28};
        auto result = ModbusFrameCodec::decodeRtuResponse(raw);
        check("FC03 valid - has value",     result.has_value());
        check("FC03 valid - slaveID = 1",   result && result->slaveID == 1);
        check("FC03 valid - FC = 0x03",     result && result->functionCode == 0x03);
        // data = [byteCount=4, 0x00, 0x17, 0x00, 0x2B]
        check("FC03 valid - data size = 5", result && result->data.size() == 5);
        check("FC03 valid - reg[0] = 23",   result && result->registerValue(0) == 23);
        check("FC03 valid - reg[1] = 43",   result && result->registerValue(1) == 43);
    }

    // Exception response: FC03 -> IllegalDataAddress (code 0x02)
    // CRC of [01 83 02] = 0xF1C0 -> [C0 F1]
    {
        std::vector<u8> raw = {0x01,0x83,0x02,0xC0,0xF1};
        auto result = ModbusFrameCodec::decodeRtuResponse(raw);
        check("Exception - has value",         result.has_value());  // frame returned!
        check("Exception - FC = 0x83",         result && result->functionCode == 0x83);
        check("Exception - isExceptionResponse()", result && result->isExceptionResponse());
        check("Exception - original FC = 0x03",
              result && (result->functionCode & 0x7Fu) == 0x03);
        check("Exception - exceptionCode = 0x02",
              result && result->exceptionCode() == 0x02);
    }

    // CRC mismatch
    {
        std::vector<u8> raw = {0x01,0x03,0x04,0x00,0x17,0x00,0x2B,0xFF,0xFF};
        auto result = ModbusFrameCodec::decodeRtuResponse(raw);
        check("CRC mismatch - is unexpected", !result.has_value());
        if (!result.has_value()) {
            bool isCrc = std::holds_alternative<ProtocolErrorCode>(result.error()) &&
                         std::get<ProtocolErrorCode>(result.error()) ==
                             ProtocolErrorCode::CrcMismatch;
            check("CRC mismatch - error code", isCrc);
        }
    }

    // Too short
    {
        std::vector<u8> raw = {0x01, 0x03};
        auto result = ModbusFrameCodec::decodeRtuResponse(raw);
        check("Too short - is unexpected", !result.has_value());
    }

    // FC06 echo response
    // CRC of [01 06 00 02 00 64] = 0xE129 -> [29 E1]
    {
        std::vector<u8> raw = {0x01,0x06,0x00,0x02,0x00,0x64,0x29,0xE1};
        auto result = ModbusFrameCodec::decodeRtuResponse(raw);
        check("FC06 echo - has value",   result.has_value());
        check("FC06 echo - FC = 0x06",   result && result->functionCode == 0x06);
        check("FC06 echo - addr hi",     result && result->data.size() >= 2 && result->data[0] == 0x00);
        check("FC06 echo - addr lo",     result && result->data.size() >= 2 && result->data[1] == 0x02);
        check("FC06 echo - val hi",      result && result->data.size() >= 4 && result->data[2] == 0x00);
        check("FC06 echo - val lo",      result && result->data.size() >= 4 && result->data[3] == 0x64);
    }

    // Round-trip: encode a response frame, decode it back
    // CRC of [01 03 06 00 0A 00 14 00 1E] = 0x7879 -> [79 78]
    {
        ModbusFrame respFrame;
        respFrame.slaveID      = 1;
        respFrame.functionCode = 0x03;
        respFrame.data = {0x06, 0x00,0x0A, 0x00,0x14, 0x00,0x1E};
        auto encoded = ModbusFrameCodec::encodeRtuRequest(respFrame);
        auto decoded = ModbusFrameCodec::decodeRtuResponse(encoded);
        check("Round-trip - has value",     decoded.has_value());
        check("Round-trip - reg[0] = 10",   decoded && decoded->registerValue(0) == 10);
        check("Round-trip - reg[1] = 20",   decoded && decoded->registerValue(1) == 20);
        check("Round-trip - reg[2] = 30",   decoded && decoded->registerValue(2) == 30);
    }
}

int main() {
    std::cout << "=== ModbusFrameCodec Test Suite ===\n\n";
    test_encoding();
    test_decoding();
    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return (failed == 0) ? 0 : 1;
}
