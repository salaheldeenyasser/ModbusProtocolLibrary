#include "../include/modbus/ModbusClient.h"

std::expected<std::vector<u16>, ModbusError>
ModbusClient::readHoldingRegisters(u8 slaveId, u8 startAddr, u16 count)
{
    ModbusFrame request = ModbusFrameCodec::makeReadHoldingRegistersRequest(slaveId, startAddr, count);
    auto responseResult = sendAndReceive(request);
    if (!responseResult)
    {
        return std::unexpected(responseResult.error());
    }
    auto response = responseResult.value();
    // Validate function code and decode register values
    if (response.functionCode == request.functionCode)
    {
        return ModbusFrameCodec::decodeRtuResponse(response);
    }
    else if (response.functionCode == (request.functionCode | 0x80))
    {
        // Exception response
        if (!response.data.empty())
        {
            u8 exceptionCode = response.data[0];
            return std::unexpected(ModbusError(ProtocolErrorCode::ExceptionResponse, exceptionCode));
        }
        else
        {
            return std::unexpected(ModbusError(ProtocolErrorCode::InvalidResponse));
        }
    }
    else
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFunctionCode));
    }
}
