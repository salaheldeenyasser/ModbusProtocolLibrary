#ifndef UART_TRANSPORT_H
#define UART_TRANSPORT_H

// BUG FIX: <termios.h>, <fcntl.h>, <unistd.h> are Linux-only and must NOT
// appear in a public header. Moved to UartTransport_Linux.cpp.
// This header is now fully portable (Windows/Linux/RTOS).
#include "../../include/modbus/IModbusTransport.h"
#include <string>

class UartTransport : public IModbusTransport {
public:
    enum class Parity   { None, Even, Odd };
    enum class StopBits { One,  Two       };

    struct Config {
        std::string devicePath;                      // "/dev/ttyUSB0" or "COM3"
        u32         baudRate  = 9600;                // 9600 / 19200 / 38400 / 115200
        u8          dataBits  = 8;                   // 7 or 8
        Parity      parity    = Parity::None;
        StopBits    stopBits  = StopBits::One;
    };

    explicit UartTransport(const Config& config);
    ~UartTransport() override;

    std::expected<size_t, ModbusError>
    send(std::span<const u8> data) override;

    std::expected<std::vector<u8>, ModbusError>
    receive(size_t maxLength, std::chrono::milliseconds timeout) override;

    void flush()          override;
    bool isOpen()  const  override;
    void close()          override;

private:
    Config config_;
    int    fd_ = -1;          // POSIX fd; on Windows this becomes HANDLE
};

#endif // UART_TRANSPORT_H
