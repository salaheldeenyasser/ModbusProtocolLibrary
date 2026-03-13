#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H
#include "IModbusTransport.h"
#include "ModbusError.h"
#include "ModbusFrame.h"
#include "ModbusFrameCodec.h"

class ModbusClient
{
public:
    explicit ModbusClient(std::unique_ptr<IModbusTransport> transport);
    ~ModbusClient();

    void setDefaultTimeout(std::chrono::milliseconds timeout);
    void setRetryCount(u8 count);

    // FC 0x01 - Read Coils
    std::expected<std::vector<bool>, ModbusError>
    readCoils(u8 slaveId, u16 startAddr, u16 count);

    // FC 0x02 - Read Discrete Inputs
    std::expected<std::vector<bool>, ModbusError>
    readDiscreteInputs(u8 slaveId, u16 startAddr, u16 count);

    // FC 0x03 - Read Holding Registers
    std::expected<std::vector<u16>, ModbusError>
    readHoldingRegisters(u8 slaveId, u16 startAddr, u16 count);

    // FC 0x04 - Read Input Registers
    std::expected<std::vector<u16>, ModbusError>
    readInputRegisters(u8 slaveId, u16 startAddr, u16 count);

    // FC 0x05 - Write Single Coil
    std::expected<void, ModbusError>
    writeSingleCoil(u8 slaveId, u16 addr, bool value);

    // FC 0x06 - Write Single Register
    std::expected<void, ModbusError>
    writeSingleRegister(u8 slaveId, u16 addr, u16 value);

    // FC 0x0F - Write Multiple Coils
    std::expected<void, ModbusError>
    writeMultipleCoils(u8 slaveId, u16 startAddr,
                       std::span<const bool> values);

    // FC 0x10 - Write Multiple Registers
    std::expected<void, ModbusError>
    writeMultipleRegisters(u8 slaveId, u16 startAddr,
                           std::span<const u16> values);

private:
    std::unique_ptr<IModbusTransport> transport_;
    std::chrono::milliseconds timeout_{1000};
    u8 retryCount_{3};

    // Internal: the core send-and-receive cycle with retries and error handling
    std::expected<ModbusFrame, ModbusError>
    sendAndReceive(const ModbusFrame &request)
    {
        setRetryCount(0);
        while (true)
        {
            // a. Encode request to raw bytes
            std::vector<u8> requestBytes = ModbusFrameCodec::encodeRtuRequest(request);
            // b. Flush stale bytes from receive buffer
            transport_->flush();
            // c. Send encoded bytes
            auto sendResult = transport_->send(requestBytes);
            if (!sendResult)
            {
                if (std::holds_alternative<TransportErrorCode>(sendResult.error()))
                {
                    if (std::get<TransportErrorCode>(sendResult.error()) == TransportErrorCode::WriteError)
                    {
                        return std::unexpected(ModbusError(TransportErrorCode::WriteError));
                    }
                }
            }
            // d. Receive response bytes
            auto receiveResult = transport_->receive(256, timeout_);
            if (!receiveResult)
            {
                if (std::holds_alternative<TransportErrorCode>(receiveResult.error()))
                {
                    if (std::get<TransportErrorCode>(receiveResult.error()) == TransportErrorCode::Timeout)
                    {
                        if (++retryCount_ > 3)
                        {
                            return std::unexpected(ModbusError(TransportErrorCode::Timeout));
                        }
                    }
                }
            }
            // e. Verify CRC and f. decode response frame
            auto decodeResult = ModbusFrameCodec::decodeRtuResponse(receiveResult.value());
            if (!decodeResult)
            {
                if (std::holds_alternative<ProtocolErrorCode>(decodeResult.error()))
                {
                    if (std::get<ProtocolErrorCode>(decodeResult.error()) == ProtocolErrorCode::CrcMismatch)
                    {
                        if (++retryCount_ > 3)
                        {
                            return std::unexpected(ModbusError(ProtocolErrorCode::CrcMismatch));
                        }
                    }
                }
            }
            ModbusFrame response = decodeResult.value();
            // g. Check slaveId matches
            if (response.slaveID != request.slaveID)
            {
                if (++retryCount_ > 3)
                {
                    return std::unexpected(ModbusError(ProtocolErrorCode::SlaveIdMismatch));
                }
            }
            // h. Check functionCode matches (allowing for exception flag)
            if ((response.functionCode & 0x7F) != request.functionCode)
            {
                if (++retryCount_ > 3)
                {
                    return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
                }
            }
            // i. If exception response: return ModbusException (NO retry)
            if (response.isExceptionResponse())
            {
                ModbusException ex;
                ex.functionCode = response.functionCode & 0x7F; // Original FC
                ex.exceptionCode = response.exceptionCode();    // The error code (0x01, 0x02, etc.)
                return std::unexpected(ex);
            }

            // j. Return decoded frame
            return response;
        }
    }
};

#endif // MODBUS_CLIENT_H
