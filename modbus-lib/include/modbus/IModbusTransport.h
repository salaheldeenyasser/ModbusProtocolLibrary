#ifndef MODBUS_ITRANSPORT_H
#define MODBUS_ITRANSPORT_H
#include "ModbusError.h"
#include "ModbusTypes.h"

class IModbusTransport
{
public:
    virtual ~IModbusTransport() = default;
    virtual std::expected<size_t, ModbusError> send(
        std::span<const u8> data) = 0;

    virtual std::expected<std::vector<u8>, ModbusError> receive(
        size_t expectedLength, std::chrono::milliseconds timeout) = 0;

    virtual void flush() = 0;
    virtual bool isOpen() const = 0;
    virtual void close() = 0;
};
#endif // MODBUS_ITRANSPORT_H
