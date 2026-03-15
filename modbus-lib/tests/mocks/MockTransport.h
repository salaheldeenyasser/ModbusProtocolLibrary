#ifndef MOCK_TRANSPORT_H
#define MOCK_TRANSPORT_H

#include "../../include/modbus/IModbusTransport.h"
#include <queue>

// In-memory transport for unit and integration testing.
// No hardware, no threads, no I/O — completely deterministic.
//
// Usage:
//   auto mock = std::make_unique<MockTransport>();
//   mock->injectResponse({0x01,0x03,0x04,0x00,0x17,0x00,0x2B,0xA9,0xCA});
//   ModbusClient client(std::move(mock));
//   auto result = client.readHoldingRegisters(1, 0, 2);
class MockTransport : public IModbusTransport {
public:
    // Queue a response that the next receive() call will return.
    void injectResponse(std::vector<u8> response) {
        responseQueue_.push(std::move(response));
    }

    // All bytes sent since the last call to clearSent().
    const std::vector<u8>& lastSentBytes() const { return lastSent_; }
    void clearSent() { lastSent_.clear(); }

    // Number of send() calls made (useful for retry-count assertions).
    int sendCallCount() const { return sendCalls_; }
    void resetCounts()  { sendCalls_ = 0; }

    // ── IModbusTransport ──────────────────────────────────────────────────────

    std::expected<size_t, ModbusError>
    send(std::span<const u8> data) override {
        ++sendCalls_;
        lastSent_.assign(data.begin(), data.end());
        return data.size();
    }

    std::expected<std::vector<u8>, ModbusError>
    receive(size_t /*maxLength*/, std::chrono::milliseconds /*timeout*/) override {
        if (responseQueue_.empty())
            return std::unexpected(ModbusError(TransportErrorCode::Timeout));
        auto resp = std::move(responseQueue_.front());
        responseQueue_.pop();
        return resp;
    }

    void flush()          override { lastSent_.clear(); }
    bool isOpen()  const  override { return open_; }
    void close()          override { open_ = false; }

    void setOpen(bool v)  { open_ = v; }

private:
    std::queue<std::vector<u8>> responseQueue_;
    std::vector<u8>             lastSent_;
    int                         sendCalls_ = 0;
    bool                        open_      = true;
};

#endif // MOCK_TRANSPORT_H
