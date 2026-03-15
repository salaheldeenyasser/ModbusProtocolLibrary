#ifndef MODBUS_FRAME_CODEC_H
#define MODBUS_FRAME_CODEC_H
#include "../include/modbus/ModbusError.h"
#include "../include/modbus/ModbusFrame.h"
#include "../include/modbus/ModbusTypes.h"
#include "CrcEngine.h"

// Stateless encode/decode utilities.
// These functions know the Modbus PDU structure but nothing about I/O.
class ModbusFrameCodec {
public:
    // ── RTU ──────────────────────────────────────────────────────────────────

    // Encode a ModbusFrame to RTU bytes: [SlaveID | FC | data... | CRC_Lo | CRC_Hi]
    static std::vector<u8> encodeRtuRequest(const ModbusFrame& frame);

    // Decode RTU bytes to a ModbusFrame.
    // Returns unexpected on CRC mismatch, too-short frame, or Modbus exception.
    static std::expected<ModbusFrame, ModbusError>
    decodeRtuResponse(std::span<const u8> buffer);

    // ── Request factory methods ───────────────────────────────────────────────

    static ModbusFrame makeReadCoilsRequest(
        u8 slaveId, u16 startAddr, u16 count);

    static ModbusFrame makeReadDiscreteInputsRequest(
        u8 slaveId, u16 startAddr, u16 count);

    static ModbusFrame makeReadHoldingRegistersRequest(
        u8 slaveId, u16 startAddr, u16 count);

    static ModbusFrame makeReadInputRegistersRequest(
        u8 slaveId, u16 startAddr, u16 count);

    static ModbusFrame makeWriteSingleCoilRequest(
        u8 slaveId, u16 coilAddr, bool value);

    static ModbusFrame makeWriteSingleRegisterRequest(
        u8 slaveId, u16 registerAddr, u16 value);

    // BUG FIX: previously missing coil byte-packing
    static ModbusFrame makeWriteMultipleCoilsRequest(
        u8 slaveId, u16 startAddr, const std::vector<bool>& values);

    // BUG FIX: previously missing register value bytes
    static ModbusFrame makeWriteMultipleRegistersRequest(
        u8 slaveId, u16 startAddr, const std::vector<u16>& values);
};

#endif // MODBUS_FRAME_CODEC_H

// ── TCP (MBAP) codec ──────────────────────────────────────────────────────────
// Modbus TCP wraps the standard PDU in a 6-byte MBAP header:
//   [TransactionID 2B | ProtocolID 2B | Length 2B | UnitID 1B | FC 1B | Data]
// No CRC is used; TCP's own checksum provides integrity.
class ModbusTcpCodec {
public:
    // Encode a frame to Modbus TCP bytes (MBAP + PDU, no CRC).
    static std::vector<u8>
    encodeTcpRequest(const ModbusFrame& frame, u16 transactionId);

    // Decode Modbus TCP bytes → ModbusFrame.
    // The buffer must contain the complete MBAP header + PDU.
    static std::expected<ModbusFrame, ModbusError>
    decodeTcpResponse(std::span<const u8> buffer);
};
