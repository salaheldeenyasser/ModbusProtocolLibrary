#include "../include/modbus/ModbusClient.h"

std::expected<std::vector<u16>, ModbusError>
ModbusClient::readHoldingRegisters(u8 slaveId, u16 startAddr, u16 count)
{
    // 1. Validate parameters: count must be 1-125, and startAddr+count must not exceed 0xFFFF
    if (count == 0 || count > 125)
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    if (startAddr + count > 0xFFFF)
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    // 2. Build request frame
    ModbusFrame request = ModbusFrameCodec::makeReadHoldingRegistersRequest(slaveId, startAddr, count);
    // 3. Call sendAndReceive(frame) → error? propagate error upward
    auto responseResult = sendAndReceive(request);
    if (!responseResult)
    {
        return std::unexpected(responseResult.error());
    }
    // 4. Validate response:
    auto response = responseResult.value();
    if (response.functionCode == request.functionCode)
    {
        // Normal response: data[0] = byte count, followed by register values
        if (response.data.size() == 1 + count * 2 &&
            response.data[0] == count * 2)
        {
            std::vector<u16> registerValues(count);
            for (size_t i = 0; i < count; ++i)
            {
                u8 highByte = response.data[1 + i * 2];
                u8 lowByte = response.data[2 + i * 2];
                registerValues[i] = (highByte << 8) | lowByte;
            }
            return registerValues;
        }
        else
        {
            return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
        }
    }
    else if (response.isExceptionResponse())
    {
        // Exception response: data[0] = exception code
        ModbusException ex;
        ex.functionCode = response.functionCode & 0x7F; // Original FC
        ex.exceptionCode = response.exceptionCode();
        return std::unexpected(ModbusError(ex));
    }
    else
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
    }
}

std::expected<void, ModbusError>
ModbusClient::writeSingleRegister(u8 slaveId, u16 addr, u16 value)
{
    // 1. Build request frame: data = [addr_hi, addr_lo, val_hi, val_lo] & frame = {slaveId, FC=0x06, data}
    ModbusFrame request = ModbusFrameCodec::makeWriteSingleRegisterRequest(slaveId, addr, value);
    request.data = {static_cast<u8>((addr >> 8) & 0xFF), static_cast<u8>(addr & 0xFF),
                    static_cast<u8>((value >> 8) & 0xFF), static_cast<u8>(value & 0xFF)};
    // 2. Call sendAndReceive(frame) → error? propagate error upward
    auto responseResult = sendAndReceive(request);
    if (!responseResult)
    {
        return std::unexpected(responseResult.error());
    }
    auto response = responseResult.value();
    // 3. Validate echo response: - response.data == request.data (echo check) → mismatch? return UnexpectedResponse

    if (response.functionCode == request.functionCode)
    {
        if (response.data == request.data)
        {
            if (response.data.size() == 4 &&
                response.data[0] == ((addr >> 8) & 0xFF) &&
                response.data[1] == (addr & 0xFF) &&
                response.data[2] == ((value >> 8) & 0xFF) &&
                response.data[3] == (value & 0xFF))
            {
                return {};
            }
            else
            {
                return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
            }
        }
        else if (response.isExceptionResponse())
        {
            // 4. If exception response: return ModbusException (NO retry)
            ModbusException ex;
            ex.functionCode = response.functionCode & 0x7F; // Original FC
            ex.exceptionCode = response.exceptionCode();
            return std::unexpected(ModbusError(ex));
        }
        else
        {
            return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
        }
    }
