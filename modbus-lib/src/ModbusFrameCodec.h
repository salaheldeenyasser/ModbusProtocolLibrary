#ifndef MODBUS_FRAME_CODEC_H
#define MODBUS_FRAME_CODEC_H
#include "../include/modbus/ModbusError.h"
#include "../include/modbus/ModbusFrame.h"
#include "../include/modbus/ModbusTypes.h"
#include "CrcEngine.h"

class ModbusFrameCodec
{
public:
  static std::vector<u8> encodeRtuRequest(const ModbusFrame &frame);
  static std::expected<ModbusFrame, ModbusError>decodeRtuResponse(
    std::span<const u8> buffer);

  // Factory methods for common Modbus requests
  // FC 0x01 - Read Coils
  static ModbusFrame makeReadCoilsRequest(
      u8 slaveId, u16 startAddr, u16 count);
  // FC 0x02 - Read Discrete Inputs
  static ModbusFrame makeReadDiscreteInputsRequest(
      u8 slaveId, u16 startAddr, u16 count);
  // FC 0x03 - Read Holding Registers
  static ModbusFrame makeReadHoldingRegistersRequest(
      u8 slaveId, u16 startAddr, u16 count);
  // FC 0x04 - Read Input Registers
  static ModbusFrame makeReadInputRegistersRequest(
      u8 slaveId, u16 startAddr, u16 count);
  // FC 0x05 - Write Single Coil
  static ModbusFrame makeWriteSingleCoilRequest(
      u8 slaveId, u16 coilAddr, bool value);
  // FC 0x06 - Write Single Register
  static ModbusFrame makeWriteSingleRegisterRequest(
      u8 slaveId, u16 registerAddr, u16 value);
  // FC 0x0F - Write Multiple Coils
  static ModbusFrame makeWriteMultipleCoilsRequest(
      u8 slaveId, u16 startAddr, const std::vector<bool> &values);
  // FC 0x10 - Write Multiple Registers
  static ModbusFrame makeWriteMultipleRegistersRequest(
      u8 slaveId, u16 startAddr, const std::vector<u16> &values);
  // FC 0x08 - Diagnostics
  static ModbusFrame makeDiagnosticRequest(
      u8 slaveId, u16 subFunction, const std::vector<u16> &data);
  // FC 0x17 - Read/Write Multiple Registers
  static ModbusFrame makeReadWriteMultipleRegistersRequest(
      u8 slaveId, u16 readStartAddr, u16 readCount, u16 writeStartAddr,
      const std::vector<u16> &writeValues);
};

#endif // MODBUS_FRAME_CODEC_H
