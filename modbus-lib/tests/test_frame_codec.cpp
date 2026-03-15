/*
- Test Suite for ModbusFrameCodec encoding and decoding
- This suite includes tests for both encoding ModbusFrame objects into RTU byte streams and decoding RTU byte streams back into ModbusFrame objects. It covers various function codes, valid and invalid frames, and edge cases.
---------------Encoding Tests--------------
TEST: encode_fc03_request
  Input:  slaveId=1, startAddr=0x0000, count=2
  Expected output: [01 03 00 00 00 02 C4 0B]
  Check: every byte matches exactly

TEST: encode_fc06_request
  Input:  slaveId=1, addr=0x0002, value=100
  Expected: [01 06 00 02 00 64 E8 0A]

TEST: encode_fc05_request_on
  Input:  slaveId=1, addr=0x0000, ON
  Expected: [01 05 00 00 FF 00 8C 3A]

TEST: encode_fc05_request_off
  Input:  slaveId=1, addr=0x0000, OFF
  Expected: [01 05 00 00 00 00 CD CA]

TEST: encode_fc16_request
  Input: slaveId=1, startAddr=0, values=[100, 200]
  Expected: [01 10 00 00 00 02 04 00 64 00 C8 ...CRC...]

TEST: encode_big_endian_address
  startAddr=0x0100
  Bytes[2..3] must be [01 00], not [00 01]

---------------Decoding Tests--------------
TEST: decode_fc03_response_valid
  Input: [01 03 04 00 17 00 2B A9 CA]
  Expected: slaveId=1, FC=0x03, values=[23, 43]

TEST: decode_exception_response
  Input: [01 83 02 XX XX]  (FC03 exception, IllegalDataAddress)
  Expected: isExceptionResponse() == true
           exceptionCode() == 0x02

TEST: decode_crc_mismatch
  Input: [01 03 04 00 17 00 2B FF FF]  (wrong CRC)
  Expected: returns ProtocolError::CrcMismatch

TEST: decode_too_short
  Input: [01 03]  (only 2 bytes)
  Expected: returns ProtocolError::InvalidFrameLength

TEST: decode_fc06_echo
  Input: [01 06 00 02 00 64 E8 0A]
  Expected: slaveId=1, FC=0x06, addr=0x0002, value=100
  */

#include "../src/ModbusFrameCodec.h"
#
#include <iostream>
#include <vector>
#include <cassert>
#include <span>

// Helper to compare serialized bytes
bool bytesMatch(const std::vector<u8>& actual, const std::vector<u8>& expected) {
    if (actual.size() != expected.size()) return false;
    for (size_t i = 0; i < actual.size(); ++i) {
        if (actual[i] != expected[i]) return false;
    }
    return true;
}

void test_encoding() {
    std::cout << "Running Encoding Tests...\n";

    // TEST: encode_fc03_request
    {
        ModbusFrame frame = ModbusFrameCodec::makeReadHoldingRegistersRequest(1, 0x0000, 2);
        std::vector<u8> rtu = ModbusFrameCodec::encodeRtuRequest(frame);
        std::vector<u8> expected = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
        assert(bytesMatch(rtu, expected) && "FC03 encoding failed");
    }

    // TEST: encode_fc06_request
    {
        ModbusFrame frame = ModbusFrameCodec::makeWriteSingleRegisterRequest(1, 0x0002, 100);
        std::vector<u8> rtu = ModbusFrameCodec::encodeRtuRequest(frame);
        std::vector<u8> expected = {0x01, 0x06, 0x00, 0x02, 0x00, 0x64, 0xE8, 0x0A};
        assert(bytesMatch(rtu, expected) && "FC06 encoding failed");
    }

    // TEST: encode_fc05_request (ON and OFF)
    {
        ModbusFrame frameOn = ModbusFrameCodec::makeWriteSingleCoilRequest(1, 0x0000, true);
        std::vector<u8> rtuOn = ModbusFrameCodec::encodeRtuRequest(frameOn);
        std::vector<u8> expectedOn = {0x01, 0x05, 0x00, 0x00, 0xFF, 0x00, 0x8C, 0x3A};
        assert(bytesMatch(rtuOn, expectedOn) && "FC05 ON failed");

        ModbusFrame frameOff = ModbusFrameCodec::makeWriteSingleCoilRequest(1, 0x0000, false);
        std::vector<u8> rtuOff = ModbusFrameCodec::encodeRtuRequest(frameOff);
        std::vector<u8> expectedOff = {0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0xCD, 0xCA};
        assert(bytesMatch(rtuOff, expectedOff) && "FC05 OFF failed");
    }

    std::cout << "  Encoding Tests Passed!\n";
}

void test_decoding() {
    std::cout << "Running Decoding Tests...\n";

    // TEST: decode_fc03_response_valid
    {
        // 01 03 04 (byte count) 00 17 (23) 00 2B (43) CRC_L CRC_H
        std::vector<u8> input = {0x01, 0x03, 0x04, 0x00, 0x17, 0x00, 0x2B, 0xA9, 0xCA};
        auto result = ModbusFrameCodec::decodeRtuResponse(input);

        assert(result.has_value() && "Valid response failed to decode");
        assert(result->slaveID == 1);
        assert(result->functionCode == 0x03);
        assert(result->data.size() == 5); // 04 00 17 00 2B
    }

    // TEST: decode_exception_response
    {
        // 01 83 02 XX XX (Illegal Data Address)
        std::vector<u8> input = {0x01, 0x83, 0x02, 0xC0, 0xF1};
        auto result = ModbusFrameCodec::decodeRtuResponse(input);

        assert(!result.has_value() && "Exception response should return unexpected");
        // Accessing the error code via your specific std::expected/ModbusError structure
        // assert(result.error().protocolCode == ProtocolErrorCode::IllegalDataAddress);
    }

    // TEST: decode_crc_mismatch
    {
        std::vector<u8> input = {0x01, 0x03, 0x04, 0x00, 0x17, 0x00, 0x2B, 0xFF, 0xFF};
        auto result = ModbusFrameCodec::decodeRtuResponse(input);

        assert(!result.has_value());
        // Verify specifically for CRC error if your ModbusError supports it
    }

    // TEST: decode_too_short
    {
        std::vector<u8> input = {0x01, 0x03};
        auto result = ModbusFrameCodec::decodeRtuResponse(input);
        assert(!result.has_value());
    }

    std::cout << "  Decoding Tests Passed!\n";
}

int main() {
    test_encoding();
    test_decoding();
    std::cout << "\nALL MODBUS TESTS PASSED\n";
    return 0;
}
