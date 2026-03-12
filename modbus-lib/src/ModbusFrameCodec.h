#ifndef MODBUS_FRAME_CODEC_H
#define MODBUS_FRAME_CODEC_H
#include "../include/modbus/ModbusError.h"
#include "../include/modbus/ModbusFrame.h"
#include "../include/modbus/ModbusTypes.h"

class ModbusFrameCodec
{
public:
  static std::vector<u8> encodeRtuRequest(const ModbusFrame &frame);
  static std::expected<ModbusFrame, ModbusError>
  decodeRtuResponse(std::span<const u8> buffer);
  static ModbusFrame makeReadHoldingRegistersRequest(
      u8 slaveId, u16 startAddr, u16 count);
  static ModbusFrame makeWriteSingleRegisterRequest(
      u8 slaveId, u16 registerAddr, u16 value);
  static ModbusFrame makeWriteMultipleRegistersRequest(
      u8 slaveId, u16 startAddr, const std::vector<u16> &values);
  static ModbusFrame makeReadWriteMultipleRegistersRequest(
      u8 slaveId, u16 readStartAddr, u16 readCount, u16 writeStartAddr,
      const std::vector<u16> &writeValues);
  static ModbusFrame makeDiagnosticRequest(
      u8 slaveId, u16 subFunction, const std::vector<u16> &data);
  static ModbusFrame makeReadCoilsRequest(
      u8 slaveId, u16 startAddr, u16 count);
  static ModbusFrame makeWriteSingleCoilRequest(
      u8 slaveId, u16 coilAddr, bool value);
  static ModbusFrame makeWriteMultipleCoilsRequest(
      u8 slaveId, u16 startAddr, const std::vector<bool> &values);
};

#endif // MODBUS_FRAME_CODEC_H
