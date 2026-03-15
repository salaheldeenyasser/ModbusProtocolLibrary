#ifndef MODBUS_ITRANSPORT_H
#define MODBUS_ITRANSPORT_H
#include "ModbusError.h"
#include "ModbusTypes.h"

// Pure abstract transport interface.
// Modbus client and server depend ONLY on this — never on a concrete transport.
// This is the Dependency Inversion principle in action: high-level policy (Modbus
// protocol logic) depends on an abstraction, not on UART or TCP specifics.
class IModbusTransport {
public:
    virtual ~IModbusTransport() = default;

    // Send all bytes in 'data'. Returns bytes sent or a transport error.
    virtual std::expected<size_t, ModbusError>
    send(std::span<const u8> data) = 0;

    // Receive up to 'maxLength' bytes within 'timeout'.
    // Returns the received buffer (may be shorter than maxLength on timeout).
    // Returns unexpected(Timeout) when no bytes arrive before the deadline.
    virtual std::expected<std::vector<u8>, ModbusError>
    receive(size_t maxLength, std::chrono::milliseconds timeout) = 0;

    // Discard any pending bytes in the receive buffer (clears stale data
    // from a previous failed transaction before sending a new request).
    virtual void flush() = 0;

    virtual bool isOpen() const = 0;
    virtual void close()        = 0;

protected:
    IModbusTransport() = default;
    // Non-copyable: own via unique_ptr
    IModbusTransport(const IModbusTransport&)            = delete;
    IModbusTransport& operator=(const IModbusTransport&) = delete;
};

#endif // MODBUS_ITRANSPORT_H
