#include "ModbusFrameCodec.h"

std::vector<u8> ModbusFrameCodec::encodeRtuRequest(const ModbusFrame &frame)
{
    std::vector<u8> buffer;
    // Slave ID + Function Code + Data + CRC
    buffer.reserve(2 + frame.data.size() + 2);
    buffer.push_back(frame.slaveID);
    buffer.push_back(frame.functionCode);
    buffer.insert(buffer.end(), frame.data.begin(), frame.data.end());
    // Calculate CRC
    u16 crc = CrcEngine::calculate(buffer.data(), buffer.size());
    buffer.push_back(crc & 0xFF);       // CRC Low byte
    buffer.push_back((crc >> 8) & 0xFF); // CRC High byte
    return buffer;
}

std::expected<ModbusFrame, ModbusError> ModbusFrameCodec::decodeRtuResponse(std::span<const u8> buffer)
{
    // Minimum length check: Slave ID (1) + Function Code (1) + Data (1 + n) + CRC (2) = 5 + n bytes
    if (buffer.size() < 5)
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    if (buffer.size() >= 5)
    {
        // Verify CRC
        // 1. calculate CRC of first 7 bytes
        u16 receivedCrc = buffer[buffer.size() - 2] | (buffer[buffer.size() - 1] << 8);
        u16 calculatedCrc = CrcEngine::calculate(buffer.data(), buffer.size() - 2);
        // 2. compare with bytes[7..8]
        if (calculatedCrc != receivedCrc)
        {
            return std::unexpected(ModbusError(ProtocolErrorCode::CrcMismatch));
        }
        // 3. Extract frame fields
        ModbusFrame frame;
        frame.slaveID = buffer[0];
        u8 rawFunctionCode = buffer[1];
        // 4. Check for exception response
        if (frame.isExceptionResponse())
        {
            // Exception response: data contains exception code
            ModbusException ex;
            ex.functionCode = rawFunctionCode & 0x7F; // Original FC
            ex.exceptionCode = buffer[2];             // The error code (0x01, 0x02, etc.)
            return std::unexpected(ex);
        }
        // 5. Normal response: data contains the payload
        frame.functionCode = rawFunctionCode;
        frame.data.assign(buffer.begin() + 2, buffer.end() - 2);
        frame.crc = receivedCrc;

        return frame;
    }
    else
    {
        return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
    }
}

// FC 0x01: Read Coils
ModbusFrame ModbusFrameCodec::makeReadCoilsRequest(
      u8 slaveId, u16 startAddr, u16 count)
{
    ModbusFrame frame;
    frame.slaveID = slaveId;
    frame.functionCode = static_cast<u8>(FunctionCode::ReadCoils); // Read Coils
    frame.data.push_back((startAddr >> 8) & 0xFF); // Start Address High byte
    frame.data.push_back(startAddr & 0xFF);        // Start Address Low byte
    frame.data.push_back((count >> 8) & 0xFF);     // Quantity High byte
    frame.data.push_back(count & 0xFF);            // Quantity Low byte
    return frame;
}

// FC 0x02: Read Discrete Inputs
ModbusFrame ModbusFrameCodec::makeReadDiscreteInputsRequest(
      u8 slaveId, u16 startAddr, u16 count){
    ModbusFrame frame;
    frame.slaveID = slaveId;
    frame.functionCode = static_cast<u8>(FunctionCode::ReadDiscreteInputs); // Read Discrete Inputs
    frame.data.push_back((startAddr >> 8) & 0xFF); // Start Address High byte
    frame.data.push_back(startAddr & 0xFF);        // Start Address Low byte
    frame.data.push_back((count >> 8) & 0xFF);     // Quantity High byte
    frame.data.push_back(count & 0xFF);            // Quantity Low byte
    return frame;
}

// FC 0x03: Read Holding Registers
ModbusFrame ModbusFrameCodec::makeReadHoldingRegistersRequest(
    u8 slaveId, u16 startAddr, u16 count)
{
    ModbusFrame frame;
    frame.slaveID = slaveId;
    frame.functionCode = static_cast<u8>(FunctionCode::ReadHoldingRegisters); // Read Holding Registers
    frame.data.push_back((startAddr >> 8) & 0xFF); // Start Address High byte
    frame.data.push_back(startAddr & 0xFF);        // Start Address Low byte
    frame.data.push_back((count >> 8) & 0xFF);     // Quantity High byte
    frame.data.push_back(count & 0xFF);            // Quantity Low byte
    return frame;
}
