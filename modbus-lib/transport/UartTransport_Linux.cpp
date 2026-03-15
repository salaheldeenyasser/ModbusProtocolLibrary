// Linux-specific UART transport implementation.
// OS-specific headers are confined here — never in UartTransport.h.
#include "../include/UartTransport.h"
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <cerrno>
#include <cstring>

// ─── Helper: map baud rate integer to termios constant ────────────────────────
static speed_t toSpeed(u32 baud) {
    switch (baud) {
        case 1200:   return B1200;
        case 2400:   return B2400;
        case 4800:   return B4800;
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return B9600;
    }
}

// ─── Constructor ──────────────────────────────────────────────────────────────
UartTransport::UartTransport(const Config& config) : config_(config) {
    fd_ = ::open(config_.devicePath.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) return;   // isOpen() returns false; user must check

    struct termios tty{};
    if (::tcgetattr(fd_, &tty) != 0) { ::close(fd_); fd_ = -1; return; }

    // ── Baud rate ─────────────────────────────────────────────────────────────
    speed_t spd = toSpeed(config_.baudRate);
    ::cfsetispeed(&tty, spd);
    ::cfsetospeed(&tty, spd);

    // ── Character framing ─────────────────────────────────────────────────────
    tty.c_cflag &= ~static_cast<tcflag_t>(CSIZE);
    tty.c_cflag |= (config_.dataBits == 7) ? CS7 : CS8;

    switch (config_.parity) {
        case Parity::None:
            tty.c_cflag &= ~static_cast<tcflag_t>(PARENB);
            break;
        case Parity::Even:
            tty.c_cflag |=  static_cast<tcflag_t>(PARENB);
            tty.c_cflag &= ~static_cast<tcflag_t>(PARODD);
            break;
        case Parity::Odd:
            tty.c_cflag |=  static_cast<tcflag_t>(PARENB | PARODD);
            break;
    }

    if (config_.stopBits == StopBits::Two)
        tty.c_cflag |=  static_cast<tcflag_t>(CSTOPB);
    else
        tty.c_cflag &= ~static_cast<tcflag_t>(CSTOPB);

    // Enable receiver, local mode
    tty.c_cflag |= static_cast<tcflag_t>(CREAD | CLOCAL);

    // ── Raw mode (essential for binary Modbus data) ───────────────────────────
    tty.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO | ECHOE | ECHOK |
                                           ECHONL | ISIG | IEXTEN);
    tty.c_iflag &= ~static_cast<tcflag_t>(IXON | IXOFF | IXANY |
                                           IGNBRK | BRKINT | PARMRK |
                                           ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~static_cast<tcflag_t>(OPOST | ONLCR);

    // Non-blocking reads (we use select() for timeout)
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    ::tcsetattr(fd_, TCSANOW, &tty);
}

// ─── Destructor ───────────────────────────────────────────────────────────────
UartTransport::~UartTransport() {
    close();
}

// ─── send ─────────────────────────────────────────────────────────────────────
std::expected<size_t, ModbusError>
UartTransport::send(std::span<const u8> data) {
    if (fd_ < 0)
        return std::unexpected(ModbusError(TransportErrorCode::ConnectionFailed));

    ssize_t written = ::write(fd_, data.data(), data.size());
    if (written < 0)
        return std::unexpected(ModbusError(TransportErrorCode::WriteError));

    // Wait until all bytes are physically transmitted before returning
    ::tcdrain(fd_);
    return static_cast<size_t>(written);
}

// ─── receive ─────────────────────────────────────────────────────────────────
// Uses select() so the timeout is wall-clock accurate.
// Reads until maxLength bytes are accumulated OR the deadline passes.
// An inter-character gap larger than ~5 ms also terminates the read early,
// which naturally detects the Modbus RTU 3.5-character inter-frame silence.
std::expected<std::vector<u8>, ModbusError>
UartTransport::receive(size_t maxLength, std::chrono::milliseconds timeout) {
    if (fd_ < 0)
        return std::unexpected(ModbusError(TransportErrorCode::ConnectionFailed));

    std::vector<u8> buffer;
    buffer.reserve(maxLength);

    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (buffer.size() < maxLength) {
        auto now     = std::chrono::steady_clock::now();
        auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(
                             deadline - now);
        if (remaining.count() <= 0) break;   // overall deadline

        // Wait up to 'remaining' or 5 ms (inter-character gap detector),
        // whichever is shorter.
        constexpr long ICG_US = 5000;  // 5 ms ≈ 3.5 chars at 9600 baud
        long rem_l   = static_cast<long>(remaining.count());
        long wait_us = (rem_l < ICG_US) ? rem_l : ICG_US;

        struct timeval tv{};
        tv.tv_sec  = wait_us / 1'000'000L;
        tv.tv_usec = wait_us % 1'000'000L;

        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(fd_, &rset);

        int ready = ::select(fd_ + 1, &rset, nullptr, nullptr, &tv);

        if (ready < 0) {
            if (errno == EINTR) continue;
            return std::unexpected(ModbusError(TransportErrorCode::ReadError));
        }

        if (ready == 0) {
            // No data for 5 ms — inter-frame gap detected
            if (!buffer.empty()) break;      // we have a complete frame
            // Still waiting for first byte; check overall deadline
            if (std::chrono::steady_clock::now() >= deadline)
                return std::unexpected(ModbusError(TransportErrorCode::Timeout));
            continue;
        }

        u8 byte;
        ssize_t n = ::read(fd_, &byte, 1);
        if (n < 0 && errno != EAGAIN)
            return std::unexpected(ModbusError(TransportErrorCode::ReadError));
        if (n > 0)
            buffer.push_back(byte);
    }

    if (buffer.empty())
        return std::unexpected(ModbusError(TransportErrorCode::Timeout));

    return buffer;
}

// ─── flush ────────────────────────────────────────────────────────────────────
void UartTransport::flush() {
    if (fd_ >= 0) ::tcflush(fd_, TCIOFLUSH);
}

// ─── isOpen / close ───────────────────────────────────────────────────────────
bool UartTransport::isOpen() const { return fd_ >= 0; }

void UartTransport::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}
