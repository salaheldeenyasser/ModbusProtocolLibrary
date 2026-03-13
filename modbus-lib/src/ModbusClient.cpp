#include "../include/modbus/ModbusClient.h"

// FC 0x01 - Read Coils
std::expected<std::vector<bool>, ModbusError>
ModbusClient::readCoils(u8 slaveId, u16 startAddr, u16 count)
{
    // 1. Validate parameters: count must be 1-2000, and startAddr+count must not exceed 0xFFFF
    if (count == 0 || count > 2000)
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    if (startAddr + count > 0xFFFF)
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    // 2. Build request frame
    ModbusFrame request = ModbusFrameCodec::makeReadCoilsRequest(slaveId, startAddr, count);
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
        // Normal response: data[0] = byte count, followed by coil status bytes
        if (response.data.size() == 1 + ((count + 7) / 8) &&
            response.data[0] == ((count + 7) / 8 ))
        {
            std::vector<bool> coilStatus(count);
            for (size_t i = 0; i < count; ++i)
            {
                coilStatus[i] = (response.data[1 + i / 8] & (1 << (i % 8))) != 0;
            }
            return coilStatus;
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
// Similar implementations would be done for readDiscreteInputs, readInputRegisters, writeSingleCoil, writeMultipleCoils, writeMultipleRegisters, etc., following the same pattern of:
// 1. Validate parameters
// 2. Build request frame
// 3. Call sendAndReceive(frame) → error? propagate error upward
// 4. Validate response: check function code, parse data, handle exceptions

// FC 0x02 - Read Discrete Inputs
std::expected<std::vector<bool>, ModbusError>
ModbusClient::readDiscreteInputs(u8 slaveId, u16 startAddr, u16 count)
{
    // 1. Validate parameters: count must be 1-2000, and startAddr+count must not exceed 0xFFFF
    if (count == 0 || count > 2000)
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    if (startAddr + count > 0xFFFF)
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    // 2. Build request frame
    ModbusFrame request = ModbusFrameCodec::makeReadDiscreteInputsRequest(slaveId, startAddr, count);
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
        // Normal response: data[0] = byte count, followed by input status bytes
        if (response.data.size() == 1 + ((count + 7) / 8) &&
            response.data[0] == ((count + 7) / 8))
        {
            std::vector<bool> inputStatus(count);
            for (size_t i = 0; i < count; ++i)
            {
                inputStatus[i] = (response.data[1 + i / 8] & (1 << (i % 8))) != 0;
            }
            return inputStatus;
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

// FC 0x03 - Read Holding Registers
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

// FC 0x04 - Read Input Registers
std::expected<std::vector<u16>, ModbusError>
ModbusClient::readInputRegisters(u8 slaveId, u16 startAddr, u16 count)
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
    ModbusFrame request = ModbusFrameCodec::makeReadInputRegistersRequest(slaveId, startAddr, count);
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

// FC 0x05 - Write Single Coil
std::expected<void, ModbusError>
ModbusClient::writeSingleCoil(u8 slaveId, u16 addr, bool value)
{
    // 1. Build request frame: data = [addr_hi, addr_lo, value_hi, value_lo] & frame = {slaveId, FC=0x05, data} (value_hi/value_lo = 0xFF00 for ON, 0x0000 for OFF)
    ModbusFrame request = ModbusFrameCodec::makeWriteSingleCoilRequest(slaveId, addr, value);
    request.data = {static_cast<u8>((addr >> 8) & 0xFF), static_cast<u8>(addr & 0xFF),
                    static_cast<u8>((value >> 8) & 0xFF), static_cast<u8>(value & 0xFF)};
    // 2. Call sendAndReceive(frame) → error? propagate error upward
    auto responseResult = sendAndReceive(request);
    if (!responseResult)    {
        return std::unexpected(responseResult.error());
    }
    auto response = responseResult.value();
    // 3. Validate echo response: - response.data == request.data (echo check) → mismatch? return UnexpectedResponse
    if (response.functionCode == request.functionCode)
    {
        if (response.data == request.data)
        {            if (response.data.size() == 4 &&
                response.data[0] == ((addr >> 8) & 0xFF) &&
                response.data[1] == (addr & 0xFF) &&
                response.data[2] == (value ? 0xFF : 0x00) &&
                response.data[3] == 0x00)
            {                return {};
            }
            else            {
                return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
            }
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
    else    {
        return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
    }
}

// FC 0x06 - Write Single Register
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

// FC 0x0F - Write Multiple Coils
std::expected<void, ModbusError>
ModbusClient::writeMultipleCoils(u8 slaveId, u16 startAddr,
                               std::span<const bool> values)
{    // 1. Validate parameters: count must be 1-1968, and startAddr+count must not exceed 0xFFFF
    u16 count = static_cast<u16>(values.size());
    if (count == 0 || count > 1968)
    {        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    if (startAddr + count > 0xFFFF)
    {        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    // 2. Build request frame: data = [startAddr_hi, startAddr_lo, count_hi, count_lo, byteCount, coilStatusBytes...] & frame = {slaveId, FC=0x0F, data}
    ModbusFrame request = ModbusFrameCodec::makeWriteMultipleCoilsRequest(slaveId, startAddr, std::vector<bool>(values.begin(), values.end()));
    // 3. Call sendAndReceive(frame) → error? propagate error upward
    auto responseResult = sendAndReceive(request);
    if (!responseResult)    {
        return std::unexpected(responseResult.error());
    }
    auto response = responseResult.value();
    // 4. Validate response: - response.data = [startAddr_hi, startAddr_lo, count_hi, count_lo] (echo check) → mismatch? return UnexpectedResponse
    if (response.functionCode == request.functionCode)
    {        if (response.data.size() == 4 &&
            response.data[0] == ((startAddr >> 8) & 0xFF) &&
            response.data[1] == (startAddr & 0xFF) &&
            response.data[2] == ((count >> 8) & 0xFF) &&
            response.data[3] == (count & 0xFF))
        {            return {};
        }
        else        {
            return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
        }
    }
    else if (response.isExceptionResponse())
    {
        // 5. If exception response: return ModbusException (NO retry)
        ModbusException ex;
        ex.functionCode = response.functionCode & 0x7F; // Original FC
        ex.exceptionCode = response.exceptionCode();
        return std::unexpected(ModbusError(ex));
    }
    else    {
        return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
    }
}

// FC 0x10 - Write Multiple Registers
std::expected<void, ModbusError>
ModbusClient::writeMultipleRegisters(u8 slaveId, u16 startAddr,
                                   std::span<const u16> values)
{    // 1. Validate parameters: count must be 1-123, and startAddr+count must not exceed 0xFFFF
    u16 count = static_cast<u16>(values.size());
    if (count == 0 || count > 123)
    {        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    if (startAddr + count > 0xFFFF)
    {        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    // 2. Build request frame: data = [startAddr_hi, startAddr_lo, count_hi, count_lo, byteCount, registerValueBytes...] & frame = {slaveId, FC=0x10, data}
    ModbusFrame request = ModbusFrameCodec::makeWriteMultipleRegistersRequest(slaveId, startAddr, std::vector<u16>(values.begin(), values.end()));
    // 3. Call sendAndReceive(frame) → error? propagate error upward
    auto responseResult = sendAndReceive(request);
    if (!responseResult)    {
        return std::unexpected(responseResult.error());
    }
    auto response = responseResult.value();
    // 4. Validate response: - response.data = [startAddr_hi, startAddr_lo, count_hi, count_lo] (echo check) → mismatch? return UnexpectedResponse
    if (response.functionCode == request.functionCode)
    {        if (response.data.size() == 4 &&
            response.data[0] == ((startAddr >> 8) & 0xFF) &&
            response.data[1] == (startAddr & 0xFF) &&
            response.data[2] == ((count >> 8) & 0xFF)&&
            response.data[3] == (count & 0xFF))
        {            return {};
        }
        else        {
            return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
        }
    }
    else if (response.isExceptionResponse())
    {
        // 5. If exception response: return ModbusException (NO retry)
        ModbusException ex;
        ex.functionCode = response.functionCode & 0x7F; // Original FC
        ex.exceptionCode = response.exceptionCode();
        return std::unexpected(ModbusError(ex));
    }
    else    {
        return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
    }
}
