#ifndef MODBUS_ERROR_H
#define MODBUS_ERROR_H
#include "ModbusTypes.h"

// ── Transport-level errors ────────────────────────────────────────────────────
enum class TransportErrorCode {
    Timeout,
    ConnectionFailed,
    ReadError,
    WriteError,
};

// ── Protocol-level errors ─────────────────────────────────────────────────────
enum class ProtocolErrorCode {
    CrcMismatch,
    InvalidFrameLength,
    SlaveIdMismatch,
    UnexpectedResponse,
};

// ── Server-reported Modbus exception ─────────────────────────────────────────
struct ModbusException {
    u8 functionCode  = 0;   // original FC (without bit-7)
    u8 exceptionCode = 0;   // 0x01..0x04

    std::string description() const {
        std::string s = "Modbus exception on FC=0x";
        const char hex[] = "0123456789ABCDEF";
        s += hex[(functionCode >> 4) & 0xF];
        s += hex[ functionCode       & 0xF];
        s += " – ";
        switch (exceptionCode) {
            case 0x01: s += "Illegal Function";     break;
            case 0x02: s += "Illegal Data Address"; break;
            case 0x03: s += "Illegal Data Value";   break;
            case 0x04: s += "Slave Device Failure"; break;
            default:   s += "Unknown (code=0x";
                       s += hex[(exceptionCode >> 4) & 0xF];
                       s += hex[ exceptionCode       & 0xF];
                       s += ")";
        }
        return s;
    }
};

// Unified error type: every API method returns this via std::unexpected.
using ModbusError = std::variant<TransportErrorCode, ProtocolErrorCode, ModbusException>;

// ── Convenience helpers ───────────────────────────────────────────────────────
inline std::string modbusErrorToString(const ModbusError& err) {
    return std::visit([](auto&& e) -> std::string {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, TransportErrorCode>) {
            switch (e) {
                case TransportErrorCode::Timeout:          return "Transport: Timeout";
                case TransportErrorCode::ConnectionFailed: return "Transport: Connection failed";
                case TransportErrorCode::ReadError:        return "Transport: Read error";
                case TransportErrorCode::WriteError:       return "Transport: Write error";
            }
            return "Transport: Unknown";
        } else if constexpr (std::is_same_v<T, ProtocolErrorCode>) {
            switch (e) {
                case ProtocolErrorCode::CrcMismatch:        return "Protocol: CRC mismatch";
                case ProtocolErrorCode::InvalidFrameLength: return "Protocol: Invalid frame length";
                case ProtocolErrorCode::SlaveIdMismatch:    return "Protocol: Slave ID mismatch";
                case ProtocolErrorCode::UnexpectedResponse: return "Protocol: Unexpected response";
            }
            return "Protocol: Unknown";
        } else {
            return e.description();
        }
    }, err);
}

#endif // MODBUS_ERROR_H
