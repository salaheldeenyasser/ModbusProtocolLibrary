#ifndef MODBUS_I
#define MODBUS_I
#include <expected>
#include <cstdint>
class IModbusTransport{
public:
    virtual ~IModbusTransport() = default;
    virtual std::expected<size_t, TransportError> send(std)

#endif // MODBUS_I