#include "ModbusFrameCodec.h"

// ─── Encode ──────────────────────────────────────────────────────────────────

std::vector<u8> ModbusFrameCodec::encodeRtuRequest(const ModbusFrame& frame) {
    std::vector<u8> buf;
    buf.reserve(2 + frame.data.size() + 2);
    buf.push_back(frame.slaveID);
    buf.push_back(frame.functionCode);
    buf.insert(buf.end(), frame.data.begin(), frame.data.end());
    u16 crc = CrcEngine::calculate(buf.data(), buf.size());
    buf.push_back(static_cast<u8>(crc & 0xFF));        // CRC low byte  (little-endian)
    buf.push_back(static_cast<u8>((crc >> 8) & 0xFF)); // CRC high byte
    return buf;
}

// ─── Decode ──────────────────────────────────────────────────────────────────

// BUG FIX: the original code checked frame.isExceptionResponse() BEFORE
// assigning frame.functionCode, so functionCode was always 0 and the check
// never fired.  We now assign functionCode FIRST.
std::expected<ModbusFrame, ModbusError>
ModbusFrameCodec::decodeRtuResponse(std::span<const u8> buffer) {
    if (buffer.size() < 5) {
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }

    // Reconstruct the received CRC from the last two bytes (little-endian)
    u16 receivedCrc =
        static_cast<u16>(buffer[buffer.size() - 2]) |
        (static_cast<u16>(buffer[buffer.size() - 1]) << 8);

    u16 calculatedCrc = CrcEngine::calculate(buffer.data(), buffer.size() - 2);
    if (calculatedCrc != receivedCrc) {
        return std::unexpected(ModbusError(ProtocolErrorCode::CrcMismatch));
    }

    ModbusFrame frame;
    frame.slaveID      = buffer[0];
    frame.functionCode = buffer[1];   // assign BEFORE checking isExceptionResponse
    frame.crc          = receivedCrc;
    // Payload = everything between FC byte and the 2 CRC bytes
    frame.data.assign(buffer.begin() + 2, buffer.end() - 2);

    // NOTE: We return the frame as-is even for exception responses.
    // The caller is responsible for checking frame.isExceptionResponse().
    // This keeps decoding orthogonal to protocol semantics.
    return frame;
}

// ─── Request factories ────────────────────────────────────────────────────────

static void pushU16(std::vector<u8>& v, u16 val) {
    v.push_back(static_cast<u8>((val >> 8) & 0xFF)); // high byte
    v.push_back(static_cast<u8>( val       & 0xFF)); // low  byte
}

ModbusFrame ModbusFrameCodec::makeReadCoilsRequest(
    u8 slaveId, u16 startAddr, u16 count)
{
    ModbusFrame f;
    f.slaveID      = slaveId;
    f.functionCode = static_cast<u8>(FunctionCode::ReadCoils);
    pushU16(f.data, startAddr);
    pushU16(f.data, count);
    return f;
}

ModbusFrame ModbusFrameCodec::makeReadDiscreteInputsRequest(
    u8 slaveId, u16 startAddr, u16 count)
{
    ModbusFrame f;
    f.slaveID      = slaveId;
    f.functionCode = static_cast<u8>(FunctionCode::ReadDiscreteInputs);
    pushU16(f.data, startAddr);
    pushU16(f.data, count);
    return f;
}

ModbusFrame ModbusFrameCodec::makeReadHoldingRegistersRequest(
    u8 slaveId, u16 startAddr, u16 count)
{
    ModbusFrame f;
    f.slaveID      = slaveId;
    f.functionCode = static_cast<u8>(FunctionCode::ReadHoldingRegisters);
    pushU16(f.data, startAddr);
    pushU16(f.data, count);
    return f;
}

ModbusFrame ModbusFrameCodec::makeReadInputRegistersRequest(
    u8 slaveId, u16 startAddr, u16 count)
{
    ModbusFrame f;
    f.slaveID      = slaveId;
    f.functionCode = static_cast<u8>(FunctionCode::ReadInputRegisters);
    pushU16(f.data, startAddr);
    pushU16(f.data, count);
    return f;
}

ModbusFrame ModbusFrameCodec::makeWriteSingleCoilRequest(
    u8 slaveId, u16 coilAddr, bool value)
{
    ModbusFrame f;
    f.slaveID      = slaveId;
    f.functionCode = static_cast<u8>(FunctionCode::WriteSingleCoil);
    pushU16(f.data, coilAddr);
    // Modbus spec: 0xFF00 = ON, 0x0000 = OFF — any other value is illegal
    pushU16(f.data, value ? static_cast<u16>(0xFF00) : static_cast<u16>(0x0000));
    return f;
}

ModbusFrame ModbusFrameCodec::makeWriteSingleRegisterRequest(
    u8 slaveId, u16 registerAddr, u16 value)
{
    ModbusFrame f;
    f.slaveID      = slaveId;
    f.functionCode = static_cast<u8>(FunctionCode::WriteSingleRegister);
    pushU16(f.data, registerAddr);
    pushU16(f.data, value);
    return f;
}

// BUG FIX: original was missing byteCount + actual coil bit-packing
ModbusFrame ModbusFrameCodec::makeWriteMultipleCoilsRequest(
    u8 slaveId, u16 startAddr, const std::vector<bool>& values)
{
    ModbusFrame f;
    f.slaveID      = slaveId;
    f.functionCode = static_cast<u8>(FunctionCode::WriteMultipleCoils);
    u16 count      = static_cast<u16>(values.size());
    pushU16(f.data, startAddr);
    pushU16(f.data, count);
    // Byte count = ceil(count / 8)
    u8 byteCount = static_cast<u8>((count + 7) / 8);
    f.data.push_back(byteCount);
    // Pack coils into bytes, LSB first
    for (u8 i = 0; i < byteCount; ++i) {
        u8 byte = 0;
        for (int bit = 0; bit < 8; ++bit) {
            sz idx = static_cast<sz>(i * 8 + bit);
            if (idx < values.size() && values[idx]) {
                byte |= static_cast<u8>(1u << bit);
            }
        }
        f.data.push_back(byte);
    }
    return f;
}

// BUG FIX: original was missing byteCount + actual register data bytes
ModbusFrame ModbusFrameCodec::makeWriteMultipleRegistersRequest(
    u8 slaveId, u16 startAddr, const std::vector<u16>& values)
{
    ModbusFrame f;
    f.slaveID      = slaveId;
    f.functionCode = static_cast<u8>(FunctionCode::WriteMultipleRegisters);
    u16 count      = static_cast<u16>(values.size());
    pushU16(f.data, startAddr);
    pushU16(f.data, count);
    u8 byteCount = static_cast<u8>(count * 2);
    f.data.push_back(byteCount);
    for (u16 val : values) {
        pushU16(f.data, val);
    }
    return f;
}

// ── Modbus TCP codec ──────────────────────────────────────────────────────────

// MBAP header layout:
//   bytes[0..1]  Transaction Identifier (big-endian)
//   bytes[2..3]  Protocol Identifier    = 0x0000 for Modbus
//   bytes[4..5]  Length                 = bytes following this field
//   bytes[6]     Unit Identifier        = Slave ID
// Then immediately: FC byte, then data bytes.

std::vector<u8>
ModbusTcpCodec::encodeTcpRequest(const ModbusFrame& frame, u16 transactionId) {
    // length = UnitID(1) + FC(1) + data(N)
    u16 length = static_cast<u16>(2u + frame.data.size());

    std::vector<u8> buf;
    buf.reserve(6u + 1u + 1u + frame.data.size());

    // MBAP header
    buf.push_back(static_cast<u8>((transactionId >> 8) & 0xFF)); // TransID hi
    buf.push_back(static_cast<u8>( transactionId       & 0xFF)); // TransID lo
    buf.push_back(0x00);                                          // Protocol hi
    buf.push_back(0x00);                                          // Protocol lo
    buf.push_back(static_cast<u8>((length >> 8) & 0xFF));        // Length hi
    buf.push_back(static_cast<u8>( length       & 0xFF));        // Length lo

    // PDU
    buf.push_back(frame.slaveID);
    buf.push_back(frame.functionCode);
    buf.insert(buf.end(), frame.data.begin(), frame.data.end());

    return buf;
}

std::expected<ModbusFrame, ModbusError>
ModbusTcpCodec::decodeTcpResponse(std::span<const u8> buffer) {
    // Minimum: 6 bytes MBAP + 1 UnitID + 1 FC = 8 bytes
    if (buffer.size() < 8)
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));

    // Validate Protocol ID
    if (buffer[2] != 0x00 || buffer[3] != 0x00)
        return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));

    u16 declaredLength = static_cast<u16>(
        (static_cast<u16>(buffer[4]) << 8) | buffer[5]);

    // buffer must be exactly 6 (MBAP) + declaredLength bytes
    if (buffer.size() < static_cast<sz>(6u + declaredLength))
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));

    ModbusFrame frame;
    frame.slaveID      = buffer[6];
    frame.functionCode = buffer[7];
    // Payload starts at byte 8, length = declaredLength - 2 (UnitID + FC)
    if (declaredLength > 2) {
        frame.data.assign(buffer.begin() + 8,
                          buffer.begin() + 6 + declaredLength);
    }
    return frame;
}
