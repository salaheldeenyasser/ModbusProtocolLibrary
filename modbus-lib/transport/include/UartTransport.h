#ifndef UART_TRANSPORT_H
#define UART_TRANSPORT_H

#include "../../include/modbus/IModbusTransport.h"
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

class UartTransport : public IModbusTransport
{
public:
    enum class Parity
    {
        None,
        Even,
        Odd
    };
    enum class StopBits
    {
        One,
        Two
    };
    struct Config
    {
        std::string devicePath;            // "/dev/ttyS0" or "COM1"
        u32 baudRate = 9600;               // 9600 , 19200 , 38400 , 115200
        u8 dataBits = 8;                   // 7 or 8
        Parity parity = Parity::None;      // None, Even, Odd
        StopBits stopBits = StopBits::One; // One, Two
    };

    explicit UartTransport(const Config &config);
    ~UartTransport() override;

    std::expected<size_t, ModbusError> send(
        std::span<const u8> data) override;

    std::expected<std::vector<u8>, ModbusError> receive(
        size_t expectedLength, std::chrono::milliseconds timeout) override;

    void flush() override;
    bool isOpen() const override;
    void close() override;

private:
        Config config_;
        int fd_ = -1;
};

#endif // UART_TRANSPORT_H
