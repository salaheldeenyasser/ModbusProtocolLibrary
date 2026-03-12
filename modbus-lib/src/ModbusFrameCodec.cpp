#include "ModbusFrameCodec.h"

std::vector<u8> ModbusFrameCodec::encodeRtuRequest(const ModbusFrame &frame)
{
    std::vector<u8> buffer;
}

std::expected<ModbusFrame, ModbusError> ModbusFrameCodec::decodeRtuResponse(std::span<const u8> buffer)
{
}
