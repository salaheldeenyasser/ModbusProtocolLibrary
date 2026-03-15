// Client unit test suite — uses MockTransport (no hardware).
// Fix: use make_unique instead of raw pointer to avoid most-vexing-parse.

#include "../include/modbus/ModbusClient.h"
#include "MockTransport.h"
#include <iostream>
#include <array>

static int passed = 0;
static int failed = 0;

static void check(const char* name, bool cond) {
    if (cond) { std::cout << "  [PASS] " << name << "\n"; ++passed; }
    else       { std::cout << "  [FAIL] " << name << "\n"; ++failed; }
}

// Helper: build a client and keep a raw pointer to the mock for inspection.
// Using make_unique avoids the most-vexing-parse problem.
static ModbusClient makeClient(MockTransport*& outMock) {
    auto mock = std::make_unique<MockTransport>();
    outMock   = mock.get();
    return ModbusClient(std::move(mock));
}

// ── FC03 Read Holding Registers ───────────────────────────────────────────────

void test_read_holding_registers_success() {
    std::cout << "--- readHoldingRegisters ---\n";

    MockTransport* mock = nullptr;
    auto client = makeClient(mock);
    // FC03 response: slave=1, 2 registers, values=[23,43]
    // CRC of [01 03 04 00 17 00 2B] = 0x280A -> [0A 28]
    mock->injectResponse({0x01,0x03,0x04,0x00,0x17,0x00,0x2B,0x0A,0x28});

    auto result = client.readHoldingRegisters(1, 0x0000, 2);
    check("result has value",    result.has_value());
    check("two values returned", result && result->size() == 2);
    check("reg[0] == 23",        result && (*result)[0] == 23);
    check("reg[1] == 43",        result && (*result)[1] == 43);
}

void test_read_holding_registers_sends_correct_frame() {
    MockTransport* mock = nullptr;
    auto client = makeClient(mock);
    mock->injectResponse({0x01,0x03,0x04,0x00,0x17,0x00,0x2B,0x0A,0x28});
    client.readHoldingRegisters(1, 0x0000, 2);

    // Expected request: [01 03 00 00 00 02 C4 0B]
    const auto& sent = mock->lastSentBytes();
    check("sent length == 8",  sent.size() == 8);
    check("sent[0] slaveID",   sent.size() >= 1 && sent[0] == 0x01);
    check("sent[1] FC=0x03",   sent.size() >= 2 && sent[1] == 0x03);
    check("sent[6] CRC_lo",    sent.size() >= 7 && sent[6] == 0xC4);
    check("sent[7] CRC_hi",    sent.size() >= 8 && sent[7] == 0x0B);
}

// ── FC06 Write Single Register ────────────────────────────────────────────────

void test_write_single_register_success() {
    std::cout << "\n--- writeSingleRegister ---\n";

    MockTransport* mock = nullptr;
    auto client = makeClient(mock);
    // Echo response for write 350 (0x015E) to reg 0x0002
    // CRC of [01 06 00 02 01 5E] = 0x62A8 -> [A8 62]  (but use actual echo)
    // Wait - 350 = 0x015E; CRC of [01 06 00 02 01 5E]:
    // computed: 0xA862 -> [62 A8]
    mock->injectResponse({0x01,0x06,0x00,0x02,0x01,0x5E,0xA8,0x62});
    auto result = client.writeSingleRegister(1, 0x0002, 350);
    check("result is success", result.has_value());
}

// ── FC05 Write Single Coil (verifies BUG FIX) ────────────────────────────────

void test_write_single_coil_on() {
    std::cout << "\n--- writeSingleCoil ---\n";

    MockTransport* mock = nullptr;
    auto client = makeClient(mock);
    // Echo: [01 05 00 00 FF 00 8C 3A]
    mock->injectResponse({0x01,0x05,0x00,0x00,0xFF,0x00,0x8C,0x3A});
    auto result = client.writeSingleCoil(1, 0x0000, true);
    check("coil ON - success", result.has_value());

    // BUG FIX verification: the frame must use 0xFF 0x00 for ON, not 0x01 0x00
    const auto& sent = mock->lastSentBytes();
    check("coil ON byte[4] = 0xFF", sent.size() >= 5 && sent[4] == 0xFF);
    check("coil ON byte[5] = 0x00", sent.size() >= 6 && sent[5] == 0x00);
}

void test_write_single_coil_off() {
    MockTransport* mock = nullptr;
    auto client = makeClient(mock);
    // Echo: [01 05 00 00 00 00 CD CA]
    mock->injectResponse({0x01,0x05,0x00,0x00,0x00,0x00,0xCD,0xCA});
    auto result = client.writeSingleCoil(1, 0x0000, false);
    check("coil OFF - success", result.has_value());

    const auto& sent = mock->lastSentBytes();
    check("coil OFF byte[4] = 0x00", sent.size() >= 5 && sent[4] == 0x00);
    check("coil OFF byte[5] = 0x00", sent.size() >= 6 && sent[5] == 0x00);
}

// ── Timeout and retry ─────────────────────────────────────────────────────────

void test_timeout_triggers_retry() {
    std::cout << "\n--- retry on timeout ---\n";

    MockTransport* mock = nullptr;
    auto client = makeClient(mock);
    // inject nothing -> all receive() calls return Timeout
    client.setDefaultTimeout(std::chrono::milliseconds(1));
    client.setRetryCount(2);

    auto result = client.readHoldingRegisters(1, 0x0000, 1);
    check("timeout returns error", !result.has_value());
    if (!result.has_value()) {
        bool isTimeout = std::holds_alternative<TransportErrorCode>(result.error()) &&
                         std::get<TransportErrorCode>(result.error()) ==
                             TransportErrorCode::Timeout;
        check("error is Timeout", isTimeout);
    }
    check("retried 2 times (2 sends)", mock->sendCallCount() == 2);
}

// ── Modbus exception — never retried ─────────────────────────────────────────

void test_modbus_exception_no_retry() {
    std::cout << "\n--- Modbus exception (no retry) ---\n";

    MockTransport* mock = nullptr;
    auto client = makeClient(mock);
    client.setRetryCount(3);
    // Exception: FC03 exception -> IllegalDataAddress (0x02)
    // CRC of [01 83 02] = 0xF1C0 -> [C0 F1]
    mock->injectResponse({0x01,0x83,0x02,0xC0,0xF1});

    auto result = client.readHoldingRegisters(1, 0x0100, 1);
    check("exception returns error",      !result.has_value());
    if (!result.has_value()) {
        bool isEx = std::holds_alternative<ModbusException>(result.error());
        check("error is ModbusException", isEx);
        if (isEx) {
            auto& ex = std::get<ModbusException>(result.error());
            check("exception code = 0x02", ex.exceptionCode == 0x02);
        }
    }
    // Exception must NOT trigger retry — only one send attempt
    check("no retry on exception (1 send)", mock->sendCallCount() == 1);
}

// ── FC16 Write Multiple Registers ─────────────────────────────────────────────

void test_write_multiple_registers() {
    std::cout << "\n--- writeMultipleRegisters ---\n";

    MockTransport* mock = nullptr;
    auto client = makeClient(mock);
    // Echo for FC16 write 2 registers at addr 0:
    // [01 10 00 00 00 02] CRC = 0xC841 -> [41 C8]
    mock->injectResponse({0x01,0x10,0x00,0x00,0x00,0x02,0x41,0xC8});

    std::array<u16, 2> vals = {100, 200};
    auto result = client.writeMultipleRegisters(1, 0x0000,
        std::span<const u16>(vals.data(), vals.size()));
    check("write multiple - success", result.has_value());

    // Verify encoded frame contains actual register values
    const auto& sent = mock->lastSentBytes();
    check("FC16 FC byte = 0x10",    sent.size() >= 2  && sent[1]  == 0x10);
    check("FC16 byteCount = 0x04",  sent.size() >= 7  && sent[6]  == 0x04);
    check("FC16 reg[0] lo = 0x64",  sent.size() >= 9  && sent[8]  == 0x64); // 100
    check("FC16 reg[1] lo = 0xC8",  sent.size() >= 11 && sent[10] == 0xC8); // 200
}

// ── CRC mismatch retries, then succeeds ───────────────────────────────────────

void test_crc_mismatch_retry() {
    std::cout << "\n--- CRC mismatch retry ---\n";

    MockTransport* mock = nullptr;
    auto client = makeClient(mock);
    client.setRetryCount(3);
    // First response: bad CRC
    mock->injectResponse({0x01,0x03,0x04,0x00,0x17,0x00,0x2B,0xFF,0xFF});
    // Second response: correct CRC
    mock->injectResponse({0x01,0x03,0x04,0x00,0x17,0x00,0x2B,0x0A,0x28});

    auto result = client.readHoldingRegisters(1, 0x0000, 2);
    check("CRC retry - eventually succeeds", result.has_value());
    check("CRC retry - 2 sends made",        mock->sendCallCount() == 2);
}

// ── FC01 Read Coils ───────────────────────────────────────────────────────────

void test_read_coils() {
    std::cout << "\n--- readCoils ---\n";

    MockTransport* mock = nullptr;
    auto client = makeClient(mock);
    // 4 coils: byte = 0x0A = 0b00001010 -> coils [1,3] ON, [0,2] OFF
    // CRC of [01 01 01 0A] = 0xD18F -> [8F D1]  ... but wait D1 8F?
    // From our calc: [01 01 01 0A] -> 0x8FD1 -> [D1 8F]
    mock->injectResponse({0x01,0x01,0x01,0x0A,0xD1,0x8F});

    auto result = client.readCoils(1, 0x0000, 4);
    check("readCoils - has value",  result.has_value());
    check("readCoils - 4 values",   result && result->size() == 4);
    check("coil[0] = OFF",  result && (*result)[0] == false); // bit 0 of 0x0A = 0
    check("coil[1] = ON",   result && (*result)[1] == true);  // bit 1 of 0x0A = 1
    check("coil[2] = OFF",  result && (*result)[2] == false); // bit 2 of 0x0A = 0
    check("coil[3] = ON",   result && (*result)[3] == true);  // bit 3 of 0x0A = 1
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== ModbusClient Unit Tests ===\n\n";
    test_read_holding_registers_success();
    test_read_holding_registers_sends_correct_frame();
    test_write_single_register_success();
    test_write_single_coil_on();
    test_write_single_coil_off();
    test_timeout_triggers_retry();
    test_modbus_exception_no_retry();
    test_write_multiple_registers();
    test_crc_mismatch_retry();
    test_read_coils();
    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return (failed == 0) ? 0 : 1;
}
