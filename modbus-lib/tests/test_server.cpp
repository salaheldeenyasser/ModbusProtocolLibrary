// Server unit test suite — uses MockTransport.
// All CRC values in injected frames verified with calc_crc.cpp.

#include "../include/modbus/ModbusServer.h"
#include "../src/ModbusFrameCodec.h"
#include "./mocks/MockTransport.h"
#include <iostream>
#include <optional>

static int passed = 0;
static int failed = 0;

static void check(const char* name, bool cond) {
    if (cond) { std::cout << "  [PASS] " << name << "\n"; ++passed; }
    else       { std::cout << "  [FAIL] " << name << "\n"; ++failed; }
}

// Build a correctly CRC'd RTU request frame from raw PDU bytes
static std::vector<u8> makeRequest(u8 slaveId, u8 fc,
                                    std::initializer_list<u8> data) {
    ModbusFrame f;
    f.slaveID      = slaveId;
    f.functionCode = fc;
    f.data.assign(data);
    return ModbusFrameCodec::encodeRtuRequest(f);
}

// Feed one request to the server and return its decoded response frame.
// Now works correctly for both normal frames AND exception frames because
// decodeRtuResponse no longer converts exceptions to unexpected.
static std::optional<ModbusFrame> sendToServer(ModbusServer& server,
                                                MockTransport* transport,
                                                const std::vector<u8>& request) {
    transport->injectResponse(request);
    transport->clearSent();
    server.processOneRequest();
    if (transport->lastSentBytes().empty()) return std::nullopt;
    auto decoded = ModbusFrameCodec::decodeRtuResponse(transport->lastSentBytes());
    if (!decoded.has_value()) return std::nullopt;
    return *decoded;
}

// ── FC03 — Read Holding Registers ─────────────────────────────────────────────

void test_fc03_success() {
    std::cout << "--- FC03 Read Holding Registers ---\n";
    auto regMap = std::make_shared<RegisterMap>(2000, 10, 10, 2000);
    regMap->writeHoldingRegister(0, 1500);
    regMap->writeHoldingRegister(1, 253);

    auto* transport = new MockTransport();
    ModbusServer server(std::unique_ptr<IModbusTransport>(transport), regMap, 1);

    // Read 2 registers from address 0
    auto req  = makeRequest(1, 0x03, {0x00,0x00, 0x00,0x02});
    auto resp = sendToServer(server, transport, req);

    check("FC03 - got response",         resp.has_value());
    check("FC03 - FC = 0x03",            resp && resp->functionCode == 0x03);
    check("FC03 - byteCount = 4",        resp && resp->data.size() == 5 && resp->data[0] == 4);
    check("FC03 - reg[0] = 1500",        resp && resp->registerValue(0) == 1500);
    check("FC03 - reg[1] = 253",         resp && resp->registerValue(1) == 253);
}

void test_fc03_illegal_quantity() {
    std::cout << "\n--- FC03 Illegal Quantity ---\n";
    auto regMap = std::make_shared<RegisterMap>();
    auto* transport = new MockTransport();
    ModbusServer server(std::unique_ptr<IModbusTransport>(transport), regMap, 1);

    // quantity = 200 (> 125)
    auto req  = makeRequest(1, 0x03, {0x00,0x00, 0x00,0xC8});
    auto resp = sendToServer(server, transport, req);

    check("FC03 qty>125 - got response",    resp.has_value());
    check("FC03 qty>125 - is exception",    resp && resp->isExceptionResponse());
    check("FC03 qty>125 - original FC=0x03",
          resp && (resp->functionCode & 0x7Fu) == 0x03);
}

void test_fc03_illegal_address() {
    std::cout << "\n--- FC03 Illegal Address ---\n";
    auto regMap = std::make_shared<RegisterMap>(2000, 10, 10, 2000); // only 10 regs
    auto* transport = new MockTransport();
    ModbusServer server(std::unique_ptr<IModbusTransport>(transport), regMap, 1);

    // Address 0x0064 = 100, beyond 10-register map
    auto req  = makeRequest(1, 0x03, {0x00,0x64, 0x00,0x01});
    auto resp = sendToServer(server, transport, req);

    check("FC03 bad addr - got response",   resp.has_value());
    check("FC03 bad addr - is exception",   resp && resp->isExceptionResponse());
}

// ── FC06 — Write Single Register ──────────────────────────────────────────────

void test_fc06_write_single_register() {
    std::cout << "\n--- FC06 Write Single Register ---\n";
    auto regMap = std::make_shared<RegisterMap>(2000, 10, 10, 2000);
    auto* transport = new MockTransport();
    ModbusServer server(std::unique_ptr<IModbusTransport>(transport), regMap, 1);

    // Write 350 to register 0x0002
    auto req  = makeRequest(1, 0x06, {0x00,0x02, 0x01,0x5E});
    auto resp = sendToServer(server, transport, req);

    check("FC06 - got response",        resp.has_value());
    check("FC06 - FC = 0x06",           resp && resp->functionCode == 0x06);
    check("FC06 - register updated",    regMap->readHoldingRegister(2) == 350);
    check("FC06 - echo addr hi = 0x00", resp && resp->data.size() >= 2 && resp->data[0] == 0x00);
    check("FC06 - echo addr lo = 0x02", resp && resp->data.size() >= 2 && resp->data[1] == 0x02);
    check("FC06 - echo val  lo = 0x5E", resp && resp->data.size() >= 4 && resp->data[3] == 0x5E);
}

// ── FC05 — Write Single Coil ──────────────────────────────────────────────────

void test_fc05_write_single_coil() {
    std::cout << "\n--- FC05 Write Single Coil ---\n";
    auto regMap = std::make_shared<RegisterMap>();
    auto* transport = new MockTransport();
    ModbusServer server(std::unique_ptr<IModbusTransport>(transport), regMap, 1);

    // ON
    auto req = makeRequest(1, 0x05, {0x00,0x00, 0xFF,0x00});
    auto resp = sendToServer(server, transport, req);
    check("FC05 ON - got response",  resp.has_value());
    check("FC05 ON - FC = 0x05",     resp && resp->functionCode == 0x05);
    check("FC05 ON - coil stored",   regMap->readCoil(0) == true);

    // OFF
    transport->clearSent();
    auto reqOff  = makeRequest(1, 0x05, {0x00,0x00, 0x00,0x00});
    auto respOff = sendToServer(server, transport, reqOff);
    check("FC05 OFF - got response", respOff.has_value());
    check("FC05 OFF - coil stored",  regMap->readCoil(0) == false);
}

void test_fc05_illegal_coil_value() {
    std::cout << "\n--- FC05 Illegal Coil Value ---\n";
    auto regMap = std::make_shared<RegisterMap>();
    auto* transport = new MockTransport();
    ModbusServer server(std::unique_ptr<IModbusTransport>(transport), regMap, 1);

    // Value 0x1234 is neither ON(0xFF00) nor OFF(0x0000)
    auto req  = makeRequest(1, 0x05, {0x00,0x00, 0x12,0x34});
    auto resp = sendToServer(server, transport, req);
    check("FC05 bad value - is exception", resp && resp->isExceptionResponse());
    check("FC05 bad value - code=0x03",
          resp && !resp->data.empty() &&
          resp->data[0] == static_cast<u8>(ExceptionCode::IllegalDataValue));
}

// ── FC10 — Write Multiple Registers ───────────────────────────────────────────

void test_fc16_write_multiple_registers() {
    std::cout << "\n--- FC16 Write Multiple Registers ---\n";
    auto regMap = std::make_shared<RegisterMap>(2000, 10, 10, 2000);
    auto* transport = new MockTransport();
    ModbusServer server(std::unique_ptr<IModbusTransport>(transport), regMap, 1);

    // Write {100, 200} at address 0x0000
    auto req  = makeRequest(1, 0x10,
        {0x00,0x00, 0x00,0x02, 0x04, 0x00,0x64, 0x00,0xC8});
    auto resp = sendToServer(server, transport, req);

    check("FC16 - got response",        resp.has_value());
    check("FC16 - FC = 0x10",           resp && resp->functionCode == 0x10);
    check("FC16 - reg[0] = 100",        regMap->readHoldingRegister(0) == 100);
    check("FC16 - reg[1] = 200",        regMap->readHoldingRegister(1) == 200);
}

// ── FC01 — Read Coils ─────────────────────────────────────────────────────────

void test_fc01_read_coils() {
    std::cout << "\n--- FC01 Read Coils ---\n";
    auto regMap = std::make_shared<RegisterMap>();
    regMap->writeCoil(0, true);
    regMap->writeCoil(1, false);
    regMap->writeCoil(2, true);

    auto* transport = new MockTransport();
    ModbusServer server(std::unique_ptr<IModbusTransport>(transport), regMap, 1);

    // Read 3 coils from address 0
    auto req  = makeRequest(1, 0x01, {0x00,0x00, 0x00,0x03});
    auto resp = sendToServer(server, transport, req);

    check("FC01 - got response",     resp.has_value());
    check("FC01 - FC = 0x01",        resp && resp->functionCode == 0x01);
    // byteCount=1, byte=0b00000101=0x05 (coils 0,2 ON)
    check("FC01 - byteCount = 1",    resp && resp->data.size() >= 1 && resp->data[0] == 1);
    check("FC01 - packed = 0x05",    resp && resp->data.size() >= 2 && resp->data[1] == 0x05);
}

// ── Illegal Function Code ─────────────────────────────────────────────────────

void test_illegal_function_code() {
    std::cout << "\n--- Illegal Function Code ---\n";
    auto regMap = std::make_shared<RegisterMap>();
    auto* transport = new MockTransport();
    ModbusServer server(std::unique_ptr<IModbusTransport>(transport), regMap, 1);

    auto req  = makeRequest(1, 0x07, {0x00,0x00}); // FC07 not supported
    auto resp = sendToServer(server, transport, req);

    check("illegal FC - got response",  resp.has_value());
    check("illegal FC - is exception",  resp && resp->isExceptionResponse());
    check("illegal FC - exception code",
          resp && !resp->data.empty() &&
          resp->data[0] == static_cast<u8>(ExceptionCode::IllegalFunction));
}

// ── Wrong Slave ID — no response ──────────────────────────────────────────────

void test_wrong_slave_id_no_response() {
    std::cout << "\n--- Wrong Slave ID ---\n";
    auto regMap = std::make_shared<RegisterMap>();
    auto* transport = new MockTransport();
    ModbusServer server(std::unique_ptr<IModbusTransport>(transport), regMap, 1); // ID=1

    // Request addressed to slave 99
    auto req = makeRequest(99, 0x03, {0x00,0x00, 0x00,0x01});
    transport->injectResponse(req);
    transport->clearSent();
    server.processOneRequest();
    check("wrong slave ID - no response sent", transport->lastSentBytes().empty());
}

// ── Write Callback ─────────────────────────────────────────────────────────────

void test_write_callback_fires() {
    std::cout << "\n--- Write Callback ---\n";
    auto regMap = std::make_shared<RegisterMap>();
    u16  cbAddr = 0, cbVal = 0;
    bool cbFired = false;
    regMap->onHoldingRegisterWrite(0x0002, [&](u16 a, u16 v) {
        cbFired = true; cbAddr = a; cbVal = v;
    });

    auto* transport = new MockTransport();
    ModbusServer server(std::unique_ptr<IModbusTransport>(transport), regMap, 1);

    // Write 1500 (0x05DC) to register 0x0002
    auto req = makeRequest(1, 0x06, {0x00,0x02, 0x05,0xDC});
    sendToServer(server, transport, req);

    check("callback fired",           cbFired);
    check("callback addr = 0x0002",   cbAddr == 0x0002);
    check("callback val  = 1500",     cbVal  == 1500);
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== ModbusServer Unit Tests ===\n\n";
    test_fc03_success();
    test_fc03_illegal_quantity();
    test_fc03_illegal_address();
    test_fc06_write_single_register();
    test_fc05_write_single_coil();
    test_fc05_illegal_coil_value();
    test_fc16_write_multiple_registers();
    test_fc01_read_coils();
    test_illegal_function_code();
    test_wrong_slave_id_no_response();
    test_write_callback_fires();
    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return (failed == 0) ? 0 : 1;
}
