#include "UartTransport.h"

UartTransport::UartTransport(const Config &config) : config_(config)
{
    // Open the serial port
    fd_ = open(config_.devicePath.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0)
    {
        throw std::runtime_error("Failed to open serial port: " + config_.devicePath);
    }

    // Configure the serial port settings
    struct termios tty;
    if (tcgetattr(fd_, &tty) != 0)
    {
        close();
        throw std::runtime_error("Failed to get terminal attributes");
    }

    // Set baud rate
    speed_t baud;
    switch (config_.baudRate)
    {
    case 9600:
        baud = B9600;
        break;
    case 19200:
        baud = B19200;
        break;
    case 38400:
        baud = B38400;
        break;
    case 115200:
        baud = B115200;
        break;
    default:
        close();
        throw std::invalid_argument("Unsupported baud rate");
    }
    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    // Set data bits
    tty.c_cflag &= ~CSIZE;
    switch (config_.dataBits)
    {
    case 7:
        tty.c_cflag |= CS7;
        break;
    case 8:
        tty.c_cflag |= CS8;
        break;
    default:
        close();
        throw std::invalid_argument("Unsupported data bits");
    }

    // Set parity
    switch (config_.parity)
    {
    case Parity::None:
        tty.c_cflag &= ~PARENB;
        break;
    case Parity::Even:
        tty.c_cflag |= PARENB;
        tty.c_cflag &= ~PARODD;
        break;
    case Parity::Odd:
        tty.c_cflag |= PARENB | PARODD;
        break;
    default:
        close();
        throw std::invalid_argument("Unsupported parity");
    }

    // Set stop bits
    switch (config_.stopBits)
    {
    case StopBits::One:
        tty.c_cflag &= ~CSTOPB;
        break;
    case StopBits::Two:
        tty.c_cflag |= CSTOPB;
        break;
    default:
        close();
        throw std::invalid_argument("Unsupported stop bits");
    }
};

// std::expected<size_t, ModbusError> UartTransport::send(std::span<const u8> data)
// {
//     if (fd_ < 0)
//     {
//         return std::unexpected(ModbusError(TransportErrorCode::ConnectionFailed));
//     }
//     ssize_t bytesWritten = write(fd_, data.data(), data.size());
//     if (bytesWritten < 0)
//     {
//         return std::unexpected(ModbusError(TransportErrorCode::WriteError));
//     }
//     return static_cast<size_t>(bytesWritten);
// }

// std::expected<std::vector<u8>, ModbusError> UartTransport::receive(
//     size_t expectedLength, std::chrono::milliseconds timeout)
// {
//     if (fd_ < 0)
//     {
//         return std::unexpected(ModbusError(TransportErrorCode::ConnectionFailed));
//     }

//     std::vector<u8> buffer(expectedLength);
//     ssize_t bytesRead = read(fd_, buffer.data(), expectedLength);
//     if (bytesRead < 0)
//     {
//         return std::unexpected(ModbusError(TransportErrorCode::ReadError));
//     }
//     buffer.resize(bytesRead);
//     return buffer;
// }

std::expected<size_t, ModbusError> UartTransport::send(std::span<const u8> data) {
    if (fd_ < 0) return std::unexpected(TransportErrorCode::ConnectionFailed);

    ssize_t bytesWritten = write(fd_, data.data(), data.size());
    if (bytesWritten < 0) return std::unexpected(TransportErrorCode::WriteError);

    // Ensure data is physically sent before returning
    tcdrain(fd_);
    return static_cast<size_t>(bytesWritten);
}

std::expected<std::vector<u8>, ModbusError> UartTransport::receive(
    size_t expectedLength, std::chrono::milliseconds timeout) {

    if (fd_ < 0) return std::unexpected(TransportErrorCode::ConnectionFailed);

    std::vector<u8> buffer;
    buffer.reserve(expectedLength);
    u8 tempByte;

    auto startTime = std::chrono::steady_clock::now();

    while (buffer.size() < expectedLength) {
        // 1. Check for timeout
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime) > timeout) {
            if (buffer.empty()) return std::unexpected(TransportErrorCode::Timeout);
            break; // Return partial frame (the parser will catch CRC error)
        }

        // 2. Non-blocking read check
        ssize_t n = read(fd_, &tempByte, 1);
        if (n > 0) {
            buffer.push_back(tempByte);
        } else if (n < 0 && errno != EAGAIN) {
            return std::unexpected(TransportErrorCode::ReadError);
        }
    }

    return buffer;
}

void UartTransport::flush() {
    if (fd_ >= 0) tcflush(fd_, TCIOFLUSH);
}

bool UartTransport::isOpen() const {
    return fd_ >= 0;
}

void UartTransport::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

UartTransport::~UartTransport() {
    close();
}
