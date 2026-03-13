#include "ModbusFrameCodec.h"

std::vector<u8> ModbusFrameCodec::encodeRtuRequest(const ModbusFrame &frame)
{
    std::vector<u8> buffer;
}

std::expected<ModbusFrame, ModbusError> ModbusFrameCodec::decodeRtuResponse(std::span<const u8> buffer)
{
    if(buffer.size() < 5) {
        return std::unexpected(ModbusError::ProtocolErrorCode::InvalidFrameLength);
    }
    if(buffer.size() > 256) {
        return std::unexpected(ModbusError::ProtocolErrorCode::InvalidFrameLength);
    }

    ModbusFrame frame;

}

ModbusFrame ModbusFrameCodec::makeReadHoldingRegistersRequest(
    u8 slaveId, u16 startAddr, u16 count)
{

}
