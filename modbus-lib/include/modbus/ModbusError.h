#ifndef MODBUS_ERROR_H
#define MODBUS_ERROR_H
#include <cstdint>

enum class TransportErrorCode{
    Timeout,
    ConnectionFailed,
    ReadError,
    WriteError,
};




#endif // MODBUS_ERROR_H