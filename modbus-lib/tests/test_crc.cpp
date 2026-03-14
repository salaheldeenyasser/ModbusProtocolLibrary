/*
- Test suite for CrcEngine
- Validates CRC calculation against known vectors and verifies correct byte order in Modbus frames
- Covers edge cases like single byte input and all zeros
- Tests the verify() function with valid and corrupted frames

| Input (hex)            | Expected CRC | Appended as |
| ---------------------- | ------------ | ----------- |
| `01 03 00 00 00 02`    | `0x0BC4`     | `C4 0B`     |
| `01 03 04 00 17 00 2B` | `0xCAA9`     | `A9 CA`     |
| `01 06 00 00 05 DC`    | `0x0488`     | `88 04`     |
| `01 05 00 00 FF 00`    | `0x3A8C`     | `8C 3A`     |
| `FF FF FF FF FF FF`    | `0xF0B8`     | `B8 F0`     |

TEST: crc_known_vector_1
  Input:    [01 03 00 00 00 02]
  Expected: 0x0BC4 → appended as [C4 0B]

TEST: crc_known_vector_2
  Input:    [01 03 04 00 17 00 2B]
  Expected: 0xCAA9 → appended as [A9 CA]

TEST: crc_known_vector_3
  Input:    [01 06 00 00 05 DC]
  Expected: 0x0488 → appended as [88 04]

TEST: crc_single_byte
  Input:    [01]
  Must not crash; result defined by algorithm

TEST: crc_all_zeros
  Input:    [00 00 00 00]
  Expected: defined value (verify against a reference impl)

TEST: crc_verify_valid_frame
  Input:    [01 03 00 00 00 02 C4 0B]  (includes CRC)
  Expected: verify() returns true

TEST: crc_verify_corrupted_frame
  Input:    [01 03 00 00 00 02 C4 0C]  (CRC corrupted by 1 bit)
  Expected: verify() returns false

TEST: crc_byte_order
  CRC of [01 03 00 00 00 02] = 0x0BC4
  Low byte = 0xC4, High byte = 0x0B
  Frame must have [C4] at index 6, [0B] at index 7*/

#include <iostream>
#include <vector>
#include <cassert>
#include <iomanip>
#include "/home/seyho/Projects/ModbusProtocolLibrary/modbus-lib/src/CrcEngine.h" // Assuming this contains your static calculate/verify methods

// Helper to print hex for debugging
void printHex(const std::string& label, u16 value) {
    std::cout << label << ": 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << value << std::dec << std::endl;
}

void run_crc_tests() {
    std::cout << "Starting Modbus CRC-16 Test Suite...\n" << std::endl;

    // --- TEST: crc_known_vector_1 ---
    {
        std::vector<u8> input = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02};
        u16 result = CrcEngine::calculate(input.data(), input.size());
        assert(result == 0x0BC4);
        std::cout << "[PASS] Vector 1 (0x0BC4)" << std::endl;
    }

    // --- TEST: crc_known_vector_2 ---
    {
        std::vector<u8> input = {0x01, 0x03, 0x04, 0x00, 0x17, 0x00, 0x2B};
        u16 result = CrcEngine::calculate(input.data(), input.size());
        assert(result == 0xCAA9);
        std::cout << "[PASS] Vector 2 (0xCAA9)" << std::endl;
    }

    // --- TEST: crc_known_vector_3 ---
    {
        std::vector<u8> input = {0x01, 0x06, 0x00, 0x00, 0x05, 0xDC};
        u16 result = CrcEngine::calculate(input.data(), input.size());
        assert(result == 0x0488);
        std::cout << "[PASS] Vector 3 (0x0488)" << std::endl;
    }

    // --- TEST: crc_all_ones (FF vector) ---
    {
        std::vector<u8> input = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        u16 result = CrcEngine::calculate(input.data(), input.size());
        assert(result == 0xF0B8);
        std::cout << "[PASS] Vector FF (0xF0B8)" << std::endl;
    }

    // --- TEST: crc_single_byte ---
    {
        u8 input = 0x01;
        // Should calculate without crashing. 0x01 with init 0xFFFF results in 0x40C0
        u16 result = CrcEngine::calculate(&input, 1);
        assert(result != 0);
        std::cout << "[PASS] Single byte input" << std::endl;
    }

    // --- TEST: crc_all_zeros ---
    {
        std::vector<u8> input = {0x00, 0x00, 0x00, 0x00};
        u16 result = CrcEngine::calculate(input.data(), input.size());
        assert(result == 0x50C1); // Known Modbus CRC for 4 zeros
        std::cout << "[PASS] All zeros input" << std::endl;
    }

    // --- TEST: crc_verify_valid_frame ---
    {
        // Vector 1: [01 03 00 00 00 02] -> CRC is 0x0BC4
        // Modbus RTU order: Low Byte (C4), then High Byte (0B)
        std::vector<u8> frame = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
        bool isValid = CrcEngine::verify(frame.data(), frame.size(), 0x0BC4);
        assert(isValid == true);
        std::cout << "[PASS] Verify valid frame" << std::endl;
    }

    // --- TEST: crc_verify_corrupted_frame ---
    {
        // Change 0xC4 to 0xC5
        std::vector<u8> frame = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC5, 0x0B};
        bool isValid = CrcEngine::verify(frame.data(), frame.size(), 0x0BC4);
        assert(isValid == false);
        std::cout << "[PASS] Verify corrupted frame" << std::endl;
    }

    // --- TEST: crc_byte_order (Modbus specific) ---
    {
        std::vector<u8> input = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02};
        u16 result = CrcEngine::calculate(input.data(), input.size());

        u8 low = static_cast<u8>(result & 0xFF);
        u8 high = static_cast<u8>((result >> 8) & 0xFF);

        // Validation of your "Appended as" logic
        assert(low == 0xC4);
        assert(high == 0x0B);
        std::cout << "[PASS] Byte order verification (LSB first)" << std::endl;
    }

    std::cout << "\nAll CRC tests passed successfully!" << std::endl;
}

int main() {
    run_crc_tests();
    return 0;
}
