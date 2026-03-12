#ifndef MODBUS_FRAME_CODEC_H
#define MODBUS_FRAME_CODEC_H
#include "../include/modbus/ModbusFrame.h"
#include "../include/modbus/ModbusTypes.h"
#include "../include/modbus/ModbusError.h"

class ModbusFrameCodec {
public:
  static std::expected<ModbusFrame, ModbusError> decodeRtuResponse(std::span<const u8> buffer);
  static std::vector<u8> encodeRtuRequest(const ModbusFrame &frame);
};

#endif // MODBUS_FRAME_CODEC_H