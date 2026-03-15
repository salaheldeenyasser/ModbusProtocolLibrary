#include "../include/modbus/ModbusClient.h"
#include "ModbusFrameCodec.h"

ModbusClient::ModbusClient(std::unique_ptr<IModbusTransport> transport)
    : transport_(std::move(transport)) {}

ModbusClient::~ModbusClient() = default;

void ModbusClient::setDefaultTimeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
}

void ModbusClient::setRetryCount(u8 count) {
    retryCount_ = count;
}

// ─── Core send/receive with retry ─────────────────────────────────────────────
//
// BUG FIXES vs original:
//  1. Used a local `attempts` counter, not the configuration field retryCount_.
//  2. Properly continue the loop on retryable errors (timeout, CRC).
//  3. Return immediately on non-retryable errors (WriteError, ModbusException).
//  4. Removed UB: never call .value() on an unexpected result.
std::expected<ModbusFrame, ModbusError>
ModbusClient::sendAndReceive(const ModbusFrame& request) {
    u8 attempts = 0;

    while (true) {
        // 1. Flush stale bytes
        transport_->flush();

        // 2. Encode and send
        auto encoded = ModbusFrameCodec::encodeRtuRequest(request);
        auto sendResult = transport_->send(std::span<const u8>(encoded));
        if (!sendResult) {
            // Write errors are not retryable — port is likely broken
            return std::unexpected(sendResult.error());
        }

        // 3. Receive response
        auto recvResult = transport_->receive(256, timeout_);
        if (!recvResult) {
            // Only retry on timeout
            if (std::holds_alternative<TransportErrorCode>(recvResult.error()) &&
                std::get<TransportErrorCode>(recvResult.error()) == TransportErrorCode::Timeout)
            {
                if (++attempts >= retryCount_) {
                    return std::unexpected(ModbusError(TransportErrorCode::Timeout));
                }
                continue;   // ← retry from the top
            }
            return std::unexpected(recvResult.error());
        }

        // 4. Decode (verifies CRC internally)
        auto decodeResult = ModbusFrameCodec::decodeRtuResponse(
            std::span<const u8>(recvResult.value()));
        if (!decodeResult) {
            // CRC mismatch is retryable (transient noise); other protocol
            // errors are not.
            const auto& err = decodeResult.error();
            if (std::holds_alternative<ProtocolErrorCode>(err) &&
                std::get<ProtocolErrorCode>(err) == ProtocolErrorCode::CrcMismatch)
            {
                if (++attempts >= retryCount_) {
                    return std::unexpected(err);
                }
                continue;
            }
            return std::unexpected(err);
        }

        ModbusFrame response = decodeResult.value();

        // 5. If the server returned an exception, propagate it immediately —
        //    do NOT retry (the server deliberately rejected the request).
        if (response.isExceptionResponse()) {
            ModbusException ex;
            ex.functionCode  = response.functionCode & 0x7Fu;
            ex.exceptionCode = response.exceptionCode();
            return std::unexpected(ModbusError(ex));
        }

        // 5. Validate slave ID
        if (response.slaveID != request.slaveID) {
            if (++attempts >= retryCount_) {
                return std::unexpected(ModbusError(ProtocolErrorCode::SlaveIdMismatch));
            }
            continue;
        }

        // 6. Validate function code (strip exception bit before comparing)
        if ((response.functionCode & 0x7Fu) != request.functionCode) {
            return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
        }

        return response;
    }
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

// Unpack a Modbus exception from a decoded frame error variant
static ModbusError makeException(const ModbusFrame& response) {
    ModbusException ex;
    ex.functionCode  = response.functionCode & 0x7Fu;
    ex.exceptionCode = response.exceptionCode();
    return ModbusError(ex);
}

// Extract register values from a read-response frame
static std::expected<std::vector<u16>, ModbusError>
parseRegisterResponse(const ModbusFrame& response, u16 expectedCount) {
    // data = [byteCount, hi0, lo0, hi1, lo1, ...]
    if (response.data.size() < 1u + static_cast<sz>(expectedCount) * 2u ||
        response.data[0] != static_cast<u8>(expectedCount * 2))
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    std::vector<u16> values(expectedCount);
    for (u16 i = 0; i < expectedCount; ++i) {
        values[i] = static_cast<u16>(
            (static_cast<u16>(response.data[1u + i * 2u]) << 8) |
             response.data[2u + i * 2u]);
    }
    return values;
}

// Extract coil/DI bits from a read-response frame
static std::expected<std::vector<bool>, ModbusError>
parseBitResponse(const ModbusFrame& response, u16 expectedCount) {
    u8 expectedBytes = static_cast<u8>((expectedCount + 7) / 8);
    if (response.data.size() < 1u + static_cast<sz>(expectedBytes) ||
        response.data[0] != expectedBytes)
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    std::vector<bool> bits(expectedCount);
    for (u16 i = 0; i < expectedCount; ++i) {
        bits[i] = (response.data[1u + i / 8u] & (1u << (i % 8u))) != 0;
    }
    return bits;
}

// ─── Public API ───────────────────────────────────────────────────────────────

std::expected<std::vector<bool>, ModbusError>
ModbusClient::readCoils(u8 slaveId, u16 startAddr, u16 count) {
    if (count == 0 || count > 2000)
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));

    auto req = ModbusFrameCodec::makeReadCoilsRequest(slaveId, startAddr, count);
    auto res = sendAndReceive(req);
    if (!res) return std::unexpected(res.error());
    if (res->isExceptionResponse()) return std::unexpected(makeException(*res));
    return parseBitResponse(*res, count);
}

std::expected<std::vector<bool>, ModbusError>
ModbusClient::readDiscreteInputs(u8 slaveId, u16 startAddr, u16 count) {
    if (count == 0 || count > 2000)
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));

    auto req = ModbusFrameCodec::makeReadDiscreteInputsRequest(slaveId, startAddr, count);
    auto res = sendAndReceive(req);
    if (!res) return std::unexpected(res.error());
    if (res->isExceptionResponse()) return std::unexpected(makeException(*res));
    return parseBitResponse(*res, count);
}

std::expected<std::vector<u16>, ModbusError>
ModbusClient::readHoldingRegisters(u8 slaveId, u16 startAddr, u16 count) {
    if (count == 0 || count > 125)
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));

    auto req = ModbusFrameCodec::makeReadHoldingRegistersRequest(slaveId, startAddr, count);
    auto res = sendAndReceive(req);
    if (!res) return std::unexpected(res.error());
    if (res->isExceptionResponse()) return std::unexpected(makeException(*res));
    return parseRegisterResponse(*res, count);
}

std::expected<std::vector<u16>, ModbusError>
ModbusClient::readInputRegisters(u8 slaveId, u16 startAddr, u16 count) {
    if (count == 0 || count > 125)
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));

    auto req = ModbusFrameCodec::makeReadInputRegistersRequest(slaveId, startAddr, count);
    auto res = sendAndReceive(req);
    if (!res) return std::unexpected(res.error());
    if (res->isExceptionResponse()) return std::unexpected(makeException(*res));
    return parseRegisterResponse(*res, count);
}

std::expected<void, ModbusError>
ModbusClient::writeSingleCoil(u8 slaveId, u16 addr, bool value) {
    // BUG FIX: use makeWriteSingleCoilRequest only — do NOT overwrite data
    auto req = ModbusFrameCodec::makeWriteSingleCoilRequest(slaveId, addr, value);
    auto res = sendAndReceive(req);
    if (!res) return std::unexpected(res.error());
    if (res->isExceptionResponse()) return std::unexpected(makeException(*res));
    // Server echoes the request — verify echo
    if (res->data != req.data)
        return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
    return {};
}

std::expected<void, ModbusError>
ModbusClient::writeSingleRegister(u8 slaveId, u16 addr, u16 value) {
    auto req = ModbusFrameCodec::makeWriteSingleRegisterRequest(slaveId, addr, value);
    auto res = sendAndReceive(req);
    if (!res) return std::unexpected(res.error());
    if (res->isExceptionResponse()) return std::unexpected(makeException(*res));
    if (res->data != req.data)
        return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
    return {};
}

std::expected<void, ModbusError>
ModbusClient::writeMultipleCoils(u8 slaveId, u16 startAddr,
                                  std::span<const bool> values) {
    u16 count = static_cast<u16>(values.size());
    if (count == 0 || count > 1968)
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));

    auto req = ModbusFrameCodec::makeWriteMultipleCoilsRequest(
        slaveId, startAddr, std::vector<bool>(values.begin(), values.end()));
    auto res = sendAndReceive(req);
    if (!res) return std::unexpected(res.error());
    if (res->isExceptionResponse()) return std::unexpected(makeException(*res));
    // Response echoes startAddr + count (4 bytes)
    if (res->data.size() < 4 ||
        res->data[0] != static_cast<u8>((startAddr >> 8) & 0xFF) ||
        res->data[1] != static_cast<u8>( startAddr       & 0xFF) ||
        res->data[2] != static_cast<u8>((count    >> 8) & 0xFF) ||
        res->data[3] != static_cast<u8>( count          & 0xFF))
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
    }
    return {};
}

std::expected<void, ModbusError>
ModbusClient::writeMultipleRegisters(u8 slaveId, u16 startAddr,
                                      std::span<const u16> values) {
    u16 count = static_cast<u16>(values.size());
    if (count == 0 || count > 123)
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));

    auto req = ModbusFrameCodec::makeWriteMultipleRegistersRequest(
        slaveId, startAddr, std::vector<u16>(values.begin(), values.end()));
    auto res = sendAndReceive(req);
    if (!res) return std::unexpected(res.error());
    if (res->isExceptionResponse()) return std::unexpected(makeException(*res));
    if (res->data.size() < 4 ||
        res->data[0] != static_cast<u8>((startAddr >> 8) & 0xFF) ||
        res->data[1] != static_cast<u8>( startAddr       & 0xFF) ||
        res->data[2] != static_cast<u8>((count    >> 8) & 0xFF) ||
        res->data[3] != static_cast<u8>( count          & 0xFF))
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
    }
    return {};
}
