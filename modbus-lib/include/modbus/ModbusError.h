#ifndef MODBUS_ERROR_H
#define MODBUS_ERROR_H
#include "ModbusTypes.h"

enum class TransportErrorCode{
    Timeout,
    ConnectionFailed,
    ReadError,
    WriteError,
};

enum class ProtocolErrorCode{
    CrcMismatch,
    InvalidFrameLength,
    SlaveIdMismatch,
    UnexpectedResponse,
};

struct ModbusException{
    u8 functionCode;
    u8 exceptionCode;
    std::string description() const;
};

using ModbusError = std::variant<TransportErrorCode, ProtocolErrorCode, ModbusException>;
    


#endif // MODBUS_ERROR_H