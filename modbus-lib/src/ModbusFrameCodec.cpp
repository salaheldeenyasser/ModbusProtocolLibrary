#include "ModbusFrameCodec.h"

std::expected<ModbusFrame, ModbusError> ModbusFrameCodec::decodeRtuResponse(std::span<const u8> buffer) {
    
}

std::vector<u8> ModbusFrameCodec::encodeRtuRequest(const ModbusFrame &frame) {
    
}