#include "../include/modbus/ModbusServer.h"
#include "ModbusFrameCodec.h"

// ─── Lifecycle ────────────────────────────────────────────────────────────────

ModbusServer::ModbusServer(std::unique_ptr<IModbusTransport> transport,
                           std::shared_ptr<RegisterMap>      registerMap,
                           u8                                slaveId)
    : transport_(std::move(transport))
    , registerMap_(std::move(registerMap))
    , slaveId_(slaveId)
{}

ModbusServer::~ModbusServer() {
    stop();
}

void ModbusServer::start() {
    if (running_) return;
    running_ = true;
    workerThread_ = std::thread([this]() {
        while (running_) {
            if (!processOneRequest()) {
                // Brief yield to avoid busy-spinning on continuous timeouts
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });
}

void ModbusServer::stop() {
    if (!running_) return;
    running_ = false;
    if (workerThread_.joinable())
        workerThread_.join();
}

bool ModbusServer::isRunning() const {
    return running_;
}

// ─── Main request cycle ───────────────────────────────────────────────────────

bool ModbusServer::processOneRequest() {
    if (!transport_->isOpen()) return false;

    // Receive up to 256 bytes with a short timeout (100 ms)
    auto recvResult = transport_->receive(256, std::chrono::milliseconds(100));
    if (!recvResult) return false;                   // timeout or error — nothing to do

    // Decode (verifies CRC internally)
    auto decodeResult = ModbusFrameCodec::decodeRtuResponse(
        std::span<const u8>(recvResult.value()));
    if (!decodeResult) return false;                 // bad CRC or too short — discard

    const ModbusFrame& request = decodeResult.value();

    // Broadcast (slaveId == 0) OR addressed to us?
    bool isBroadcast = (request.slaveID == 0);
    if (!isBroadcast && request.slaveID != slaveId_) return false;

    // Dispatch
    ModbusFrame response = handleRequest(request);

    // Never respond to a broadcast
    if (!isBroadcast) {
        auto responseBytes = ModbusFrameCodec::encodeRtuRequest(response);
        transport_->send(std::span<const u8>(responseBytes));
    }
    return true;
}

// ─── Dispatch ────────────────────────────────────────────────────────────────

ModbusFrame ModbusServer::handleRequest(const ModbusFrame& req) {
    switch (req.functionCode) {
        case 0x01: return handleReadCoils(req);
        case 0x02: return handleReadDiscreteInputs(req);
        case 0x03: return handleReadHoldingRegisters(req);
        case 0x04: return handleReadInputRegisters(req);
        case 0x05: return handleWriteSingleCoil(req);
        case 0x06: return handleWriteSingleRegister(req);
        case 0x0F: return handleWriteMultipleCoils(req);
        case 0x10: return handleWriteMultipleRegisters(req);
        default:
            return buildExceptionResponse(req.functionCode,
                                          ExceptionCode::IllegalFunction);
    }
}

// ─── Exception builder ────────────────────────────────────────────────────────

ModbusFrame ModbusServer::buildExceptionResponse(u8 fc, ExceptionCode code) {
    ModbusFrame f;
    f.slaveID      = slaveId_;
    f.functionCode = static_cast<u8>(fc | 0x80u);
    f.data         = { static_cast<u8>(code) };
    return f;
}

// ─── FC handlers (all were missing in original) ───────────────────────────────

// Helper: push a big-endian u16 into a byte vector
static void pushU16(std::vector<u8>& v, u16 val) {
    v.push_back(static_cast<u8>((val >> 8) & 0xFF));
    v.push_back(static_cast<u8>( val       & 0xFF));
}

// ── FC 0x01 — Read Coils ──────────────────────────────────────────────────────
ModbusFrame ModbusServer::handleReadCoils(const ModbusFrame& req) {
    if (req.data.size() < 4)
        return buildExceptionResponse(0x01, ExceptionCode::IllegalDataValue);

    u16 startAddr = static_cast<u16>((static_cast<u16>(req.data[0]) << 8) | req.data[1]);
    u16 quantity  = static_cast<u16>((static_cast<u16>(req.data[2]) << 8) | req.data[3]);

    if (quantity == 0 || quantity > 2000)
        return buildExceptionResponse(0x01, ExceptionCode::IllegalDataValue);

    // Check every address in the range
    if (!registerMap_->isValidCoilAddress(startAddr) ||
        !registerMap_->isValidCoilAddress(static_cast<u16>(startAddr + quantity - 1)))
        return buildExceptionResponse(0x01, ExceptionCode::IllegalDataAddress);

    u8 byteCount = static_cast<u8>((quantity + 7) / 8);
    ModbusFrame resp;
    resp.slaveID      = slaveId_;
    resp.functionCode = 0x01;
    resp.data.push_back(byteCount);
    resp.data.resize(1 + byteCount, 0);

    for (u16 i = 0; i < quantity; ++i) {
        if (registerMap_->readCoil(static_cast<u16>(startAddr + i))) {
            resp.data[1 + i / 8] |= static_cast<u8>(1u << (i % 8));
        }
    }
    return resp;
}

// ── FC 0x02 — Read Discrete Inputs ───────────────────────────────────────────
ModbusFrame ModbusServer::handleReadDiscreteInputs(const ModbusFrame& req) {
    if (req.data.size() < 4)
        return buildExceptionResponse(0x02, ExceptionCode::IllegalDataValue);

    u16 startAddr = static_cast<u16>((static_cast<u16>(req.data[0]) << 8) | req.data[1]);
    u16 quantity  = static_cast<u16>((static_cast<u16>(req.data[2]) << 8) | req.data[3]);

    if (quantity == 0 || quantity > 2000)
        return buildExceptionResponse(0x02, ExceptionCode::IllegalDataValue);

    if (!registerMap_->isValidDiscreteInputAddress(startAddr) ||
        !registerMap_->isValidDiscreteInputAddress(static_cast<u16>(startAddr + quantity - 1)))
        return buildExceptionResponse(0x02, ExceptionCode::IllegalDataAddress);

    u8 byteCount = static_cast<u8>((quantity + 7) / 8);
    ModbusFrame resp;
    resp.slaveID      = slaveId_;
    resp.functionCode = 0x02;
    resp.data.push_back(byteCount);
    resp.data.resize(1 + byteCount, 0);

    for (u16 i = 0; i < quantity; ++i) {
        if (registerMap_->readDiscreteInput(static_cast<u16>(startAddr + i))) {
            resp.data[1 + i / 8] |= static_cast<u8>(1u << (i % 8));
        }
    }
    return resp;
}

// ── FC 0x03 — Read Holding Registers ─────────────────────────────────────────
ModbusFrame ModbusServer::handleReadHoldingRegisters(const ModbusFrame& req) {
    if (req.data.size() < 4)
        return buildExceptionResponse(0x03, ExceptionCode::IllegalDataValue);

    u16 startAddr = static_cast<u16>((static_cast<u16>(req.data[0]) << 8) | req.data[1]);
    u16 quantity  = static_cast<u16>((static_cast<u16>(req.data[2]) << 8) | req.data[3]);

    if (quantity == 0 || quantity > 125)
        return buildExceptionResponse(0x03, ExceptionCode::IllegalDataValue);

    if (!registerMap_->isValidHoldingRegisterAddress(startAddr) ||
        !registerMap_->isValidHoldingRegisterAddress(static_cast<u16>(startAddr + quantity - 1)))
        return buildExceptionResponse(0x03, ExceptionCode::IllegalDataAddress);

    ModbusFrame resp;
    resp.slaveID      = slaveId_;
    resp.functionCode = 0x03;
    resp.data.push_back(static_cast<u8>(quantity * 2)); // byte count
    for (u16 i = 0; i < quantity; ++i) {
        u16 val = registerMap_->readHoldingRegister(static_cast<u16>(startAddr + i));
        pushU16(resp.data, val);
    }
    return resp;
}

// ── FC 0x04 — Read Input Registers ───────────────────────────────────────────
ModbusFrame ModbusServer::handleReadInputRegisters(const ModbusFrame& req) {
    if (req.data.size() < 4)
        return buildExceptionResponse(0x04, ExceptionCode::IllegalDataValue);

    u16 startAddr = static_cast<u16>((static_cast<u16>(req.data[0]) << 8) | req.data[1]);
    u16 quantity  = static_cast<u16>((static_cast<u16>(req.data[2]) << 8) | req.data[3]);

    if (quantity == 0 || quantity > 125)
        return buildExceptionResponse(0x04, ExceptionCode::IllegalDataValue);

    if (!registerMap_->isValidInputRegisterAddress(startAddr) ||
        !registerMap_->isValidInputRegisterAddress(static_cast<u16>(startAddr + quantity - 1)))
        return buildExceptionResponse(0x04, ExceptionCode::IllegalDataAddress);

    ModbusFrame resp;
    resp.slaveID      = slaveId_;
    resp.functionCode = 0x04;
    resp.data.push_back(static_cast<u8>(quantity * 2));
    for (u16 i = 0; i < quantity; ++i) {
        u16 val = registerMap_->readInputRegister(static_cast<u16>(startAddr + i));
        pushU16(resp.data, val);
    }
    return resp;
}

// ── FC 0x05 — Write Single Coil ──────────────────────────────────────────────
ModbusFrame ModbusServer::handleWriteSingleCoil(const ModbusFrame& req) {
    if (req.data.size() < 4)
        return buildExceptionResponse(0x05, ExceptionCode::IllegalDataValue);

    u16 coilAddr  = static_cast<u16>((static_cast<u16>(req.data[0]) << 8) | req.data[1]);
    u16 coilValue = static_cast<u16>((static_cast<u16>(req.data[2]) << 8) | req.data[3]);

    // Spec: only 0xFF00 (ON) or 0x0000 (OFF) are valid
    if (coilValue != 0xFF00 && coilValue != 0x0000)
        return buildExceptionResponse(0x05, ExceptionCode::IllegalDataValue);

    if (!registerMap_->isValidCoilAddress(coilAddr))
        return buildExceptionResponse(0x05, ExceptionCode::IllegalDataAddress);

    registerMap_->writeCoil(coilAddr, coilValue == 0xFF00);

    // Echo the request
    ModbusFrame resp;
    resp.slaveID      = slaveId_;
    resp.functionCode = 0x05;
    resp.data         = req.data;
    return resp;
}

// ── FC 0x06 — Write Single Register ──────────────────────────────────────────
ModbusFrame ModbusServer::handleWriteSingleRegister(const ModbusFrame& req) {
    if (req.data.size() < 4)
        return buildExceptionResponse(0x06, ExceptionCode::IllegalDataValue);

    u16 regAddr = static_cast<u16>((static_cast<u16>(req.data[0]) << 8) | req.data[1]);
    u16 value   = static_cast<u16>((static_cast<u16>(req.data[2]) << 8) | req.data[3]);

    if (!registerMap_->isValidHoldingRegisterAddress(regAddr))
        return buildExceptionResponse(0x06, ExceptionCode::IllegalDataAddress);

    registerMap_->writeHoldingRegister(regAddr, value);

    // Echo the request
    ModbusFrame resp;
    resp.slaveID      = slaveId_;
    resp.functionCode = 0x06;
    resp.data         = req.data;
    return resp;
}

// ── FC 0x0F — Write Multiple Coils ───────────────────────────────────────────
ModbusFrame ModbusServer::handleWriteMultipleCoils(const ModbusFrame& req) {
    if (req.data.size() < 5)
        return buildExceptionResponse(0x0F, ExceptionCode::IllegalDataValue);

    u16 startAddr = static_cast<u16>((static_cast<u16>(req.data[0]) << 8) | req.data[1]);
    u16 quantity  = static_cast<u16>((static_cast<u16>(req.data[2]) << 8) | req.data[3]);
    u8  byteCount = req.data[4];

    if (quantity == 0 || quantity > 1968)
        return buildExceptionResponse(0x0F, ExceptionCode::IllegalDataValue);
    if (byteCount != static_cast<u8>((quantity + 7) / 8))
        return buildExceptionResponse(0x0F, ExceptionCode::IllegalDataValue);
    if (req.data.size() < static_cast<sz>(5 + byteCount))
        return buildExceptionResponse(0x0F, ExceptionCode::IllegalDataValue);

    if (!registerMap_->isValidCoilAddress(startAddr) ||
        !registerMap_->isValidCoilAddress(static_cast<u16>(startAddr + quantity - 1)))
        return buildExceptionResponse(0x0F, ExceptionCode::IllegalDataAddress);

    for (u16 i = 0; i < quantity; ++i) {
        bool val = (req.data[5 + i / 8] & (1u << (i % 8))) != 0;
        registerMap_->writeCoil(static_cast<u16>(startAddr + i), val);
    }

    // Response: echo startAddr + quantity only
    ModbusFrame resp;
    resp.slaveID      = slaveId_;
    resp.functionCode = 0x0F;
    pushU16(resp.data, startAddr);
    pushU16(resp.data, quantity);
    return resp;
}

// ── FC 0x10 — Write Multiple Registers ───────────────────────────────────────
ModbusFrame ModbusServer::handleWriteMultipleRegisters(const ModbusFrame& req) {
    if (req.data.size() < 5)
        return buildExceptionResponse(0x10, ExceptionCode::IllegalDataValue);

    u16 startAddr = static_cast<u16>((static_cast<u16>(req.data[0]) << 8) | req.data[1]);
    u16 quantity  = static_cast<u16>((static_cast<u16>(req.data[2]) << 8) | req.data[3]);
    u8  byteCount = req.data[4];

    if (quantity == 0 || quantity > 123)
        return buildExceptionResponse(0x10, ExceptionCode::IllegalDataValue);
    if (byteCount != static_cast<u8>(quantity * 2))
        return buildExceptionResponse(0x10, ExceptionCode::IllegalDataValue);
    if (req.data.size() < static_cast<sz>(5 + byteCount))
        return buildExceptionResponse(0x10, ExceptionCode::IllegalDataValue);

    if (!registerMap_->isValidHoldingRegisterAddress(startAddr) ||
        !registerMap_->isValidHoldingRegisterAddress(static_cast<u16>(startAddr + quantity - 1)))
        return buildExceptionResponse(0x10, ExceptionCode::IllegalDataAddress);

    for (u16 i = 0; i < quantity; ++i) {
        u16 val = static_cast<u16>(
            (static_cast<u16>(req.data[5 + i * 2]) << 8) | req.data[6 + i * 2]);
        registerMap_->writeHoldingRegister(static_cast<u16>(startAddr + i), val);
    }

    // Response: echo startAddr + quantity only
    ModbusFrame resp;
    resp.slaveID      = slaveId_;
    resp.functionCode = 0x10;
    pushU16(resp.data, startAddr);
    pushU16(resp.data, quantity);
    return resp;
}
