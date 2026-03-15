// TCP codec test suite: MBAP encoding and decoding

#include "../src/ModbusFrameCodec.h"
#include <iostream>
#include <vector>
#include <cstdio>

static int passed = 0;
static int failed = 0;
static void check(const char* n, bool c){
    if(c){ std::cout<<"  [PASS] "<<n<<"\n"; ++passed; }
    else  { std::cout<<"  [FAIL] "<<n<<"\n"; ++failed; }
}
static bool eq(const std::vector<u8>& a, const std::vector<u8>& b){
    if(a.size()!=b.size()) return false;
    for(size_t i=0;i<a.size();++i) if(a[i]!=b[i]) return false;
    return true;
}

// ── Encoding ──────────────────────────────────────────────────────────────────

void test_tcp_encode_fc03() {
    std::cout << "--- TCP encode FC03 request ---\n";

    auto frame = ModbusFrameCodec::makeReadHoldingRegistersRequest(1, 0x0000, 2);
    auto tcp   = ModbusTcpCodec::encodeTcpRequest(frame, 0x0001);

    // Expected: [00 01] [00 00] [00 06] [01] [03] [00 00] [00 02]
    //           TransID  Proto   Len    Unit  FC   StartAddr  Qty
    std::vector<u8> expected = {
        0x00,0x01,  // Transaction ID = 1
        0x00,0x00,  // Protocol ID = 0
        0x00,0x06,  // Length = 6 (UnitID + FC + 4 data bytes)
        0x01,       // Unit ID (Slave ID)
        0x03,       // FC
        0x00,0x00,  // Start Address
        0x00,0x02   // Quantity
    };
    check("FC03 TCP length = 12 bytes",  tcp.size() == 12);
    check("FC03 TCP bytes match",        eq(tcp, expected));
    check("Transaction ID hi = 0x00",    tcp[0] == 0x00);
    check("Transaction ID lo = 0x01",    tcp[1] == 0x01);
    check("Protocol ID = 0x0000",        tcp[2] == 0x00 && tcp[3] == 0x00);
    check("Length = 6",                  tcp[4] == 0x00 && tcp[5] == 0x06);
    check("Unit ID = 1",                 tcp[6] == 0x01);
    check("FC = 0x03",                   tcp[7] == 0x03);
    check("No CRC bytes at end",         tcp.size() == 12); // no 2 extra CRC bytes
}

void test_tcp_encode_fc06() {
    std::cout << "\n--- TCP encode FC06 request ---\n";

    auto frame = ModbusFrameCodec::makeWriteSingleRegisterRequest(1, 0x0002, 350);
    auto tcp   = ModbusTcpCodec::encodeTcpRequest(frame, 0x0002);

    // Length = UnitID(1) + FC(1) + addr(2) + val(2) = 6
    check("FC06 TCP length = 12",        tcp.size() == 12);
    check("Length field = 6",            tcp[4] == 0x00 && tcp[5] == 0x06);
    check("FC = 0x06",                   tcp[7] == 0x06);
    check("val hi = 0x01",               tcp[10] == 0x01);   // 350 = 0x015E
    check("val lo = 0x5E",               tcp[11] == 0x5E);
}

void test_tcp_encode_fc16() {
    std::cout << "\n--- TCP encode FC16 (write multiple registers) ---\n";

    auto frame = ModbusFrameCodec::makeWriteMultipleRegistersRequest(
        1, 0x0000, {100, 200});
    auto tcp = ModbusTcpCodec::encodeTcpRequest(frame, 0x0003);

    // PDU data = [startHi, startLo, qtyHi, qtyLo, byteCount, r0Hi, r0Lo, r1Hi, r1Lo]
    // = 9 bytes.  length = UnitID(1) + FC(1) + 9 = 11
    check("FC16 TCP length = 17",        tcp.size() == 6+11);
    check("Length field = 11",           tcp[4] == 0x00 && tcp[5] == 0x0B);
    check("FC = 0x10",                   tcp[7] == 0x10);
}

// ── Decoding ──────────────────────────────────────────────────────────────────

void test_tcp_decode_fc03_response() {
    std::cout << "\n--- TCP decode FC03 response ---\n";

    // Server response: slave=1, 2 registers, values=[23,43]
    // [00 01] [00 00] [00 07] [01] [03] [04] [00 17] [00 2B]
    //  TransID  Proto   Len   Unit  FC  BC   Reg0     Reg1
    std::vector<u8> raw = {
        0x00,0x01,  // Transaction ID
        0x00,0x00,  // Protocol ID
        0x00,0x07,  // Length = 7 (UnitID + FC + ByteCount + 4 reg bytes)
        0x01,       // Unit ID
        0x03,       // FC
        0x04,       // Byte Count
        0x00,0x17,  // Register[0] = 23
        0x00,0x2B   // Register[1] = 43
    };

    auto result = ModbusTcpCodec::decodeTcpResponse(raw);
    check("result has value",     result.has_value());
    check("slaveID = 1",          result && result->slaveID == 1);
    check("FC = 0x03",            result && result->functionCode == 0x03);
    check("data size = 5",        result && result->data.size() == 5);
    check("reg[0] = 23",          result && result->registerValue(0) == 23);
    check("reg[1] = 43",          result && result->registerValue(1) == 43);
}

void test_tcp_decode_exception() {
    std::cout << "\n--- TCP decode exception response ---\n";

    // FC03 exception -> IllegalDataAddress (0x02)
    // Length = UnitID(1) + FC(1) + ExCode(1) = 3
    std::vector<u8> raw = {
        0x00,0x01,  // Transaction ID
        0x00,0x00,  // Protocol ID
        0x00,0x03,  // Length = 3
        0x01,       // Unit ID
        0x83,       // FC | 0x80 (exception)
        0x02        // Exception code: IllegalDataAddress
    };

    auto result = ModbusTcpCodec::decodeTcpResponse(raw);
    check("exception - has value",          result.has_value());
    check("exception - FC = 0x83",          result && result->functionCode == 0x83);
    check("exception - isExceptionResponse",result && result->isExceptionResponse());
    check("exception - original FC = 0x03", result && (result->functionCode&0x7Fu)==0x03);
    check("exception - code = 0x02",        result && result->exceptionCode() == 0x02);
}

void test_tcp_decode_too_short() {
    std::cout << "\n--- TCP decode too short ---\n";
    std::vector<u8> raw = {0x00,0x01,0x00,0x00,0x00,0x02}; // only MBAP header
    auto result = ModbusTcpCodec::decodeTcpResponse(raw);
    check("too short - is unexpected", !result.has_value());
}

void test_tcp_decode_wrong_protocol_id() {
    std::cout << "\n--- TCP decode wrong protocol ID ---\n";
    std::vector<u8> raw = {
        0x00,0x01, 0x00,0xFF,  // Protocol ID = 0x00FF (not Modbus)
        0x00,0x06, 0x01,0x03, 0x00,0x00, 0x00,0x02
    };
    auto result = ModbusTcpCodec::decodeTcpResponse(raw);
    check("wrong protocol - is unexpected", !result.has_value());
}

// ── Round-trip: encode then decode ────────────────────────────────────────────

void test_tcp_round_trip() {
    std::cout << "\n--- TCP encode -> decode round trip ---\n";

    // Encode a request
    auto reqFrame = ModbusFrameCodec::makeReadHoldingRegistersRequest(1, 0, 3);
    auto encoded  = ModbusTcpCodec::encodeTcpRequest(reqFrame, 0x0042);

    // Simulate a server response for the same transaction
    // 3 regs = [10, 20, 30]: byteCount=6, data=6 bytes
    std::vector<u8> respRaw = {
        0x00,0x42,  // Transaction ID (matches request)
        0x00,0x00,  // Protocol ID
        0x00,0x09,  // Length = UnitID(1)+FC(1)+BC(1)+6data = 9
        0x01,       // Unit ID
        0x03,       // FC
        0x06,       // Byte count
        0x00,0x0A,  // reg[0] = 10
        0x00,0x14,  // reg[1] = 20
        0x00,0x1E   // reg[2] = 30
    };

    auto decoded = ModbusTcpCodec::decodeTcpResponse(respRaw);
    check("round-trip - has value",     decoded.has_value());
    check("round-trip - slaveID = 1",   decoded && decoded->slaveID == 1);
    check("round-trip - reg[0] = 10",   decoded && decoded->registerValue(0) == 10);
    check("round-trip - reg[1] = 20",   decoded && decoded->registerValue(1) == 20);
    check("round-trip - reg[2] = 30",   decoded && decoded->registerValue(2) == 30);
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "=== ModbusTcpCodec Test Suite ===\n\n";
    test_tcp_encode_fc03();
    test_tcp_encode_fc06();
    test_tcp_encode_fc16();
    test_tcp_decode_fc03_response();
    test_tcp_decode_exception();
    test_tcp_decode_too_short();
    test_tcp_decode_wrong_protocol_id();
    test_tcp_round_trip();
    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return (failed==0) ? 0 : 1;
}
