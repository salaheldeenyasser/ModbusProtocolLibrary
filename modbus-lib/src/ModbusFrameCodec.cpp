#include "ModbusFrameCodec.h"

std::vector<u8> ModbusFrameCodec::encodeRtuRequest(const ModbusFrame &frame)
{
    std::vector<u8> buffer;
}

std::expected<ModbusFrame, ModbusError> ModbusFrameCodec::decodeRtuResponse(std::span<const u8> buffer)
{
    // Minimum length check: Slave ID (1) + Function Code (1) + Data (1 + n) + CRC (2) = 5 + n bytes
    if(buffer.size() < 5) {
        return std::unexpected(ModbusError(ProtocolErrorCode::InvalidFrameLength));
    }
    if(buffer.size() >= 6) {
        // Verify CRC
        // 1. calculate CRC of first 7 bytes
        u16 receivedCrc = buffer[buffer.size() - 2] | (buffer[buffer.size() - 1] << 8);
        u16 calculatedCrc = CrcEngine::calculate(buffer.data(), buffer.size() - 2);
        // 2. compare with bytes[7..8]
        if(calculatedCrc != receivedCrc) {
            return std::unexpected(ModbusError(ProtocolErrorCode::CrcMismatch));
        }
        // 3. Extract frame fields
        ModbusFrame frame;
        frame.slaveID = buffer[0];
        u8 rawFunctionCode = buffer[1];
        // 4. Check for exception response
        if(frame.isExceptionResponse()) {
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
    } else {
        return std::unexpected(ModbusError(ProtocolErrorCode::UnexpectedResponse));
    }



}

ModbusFrame ModbusFrameCodec::makeReadHoldingRegistersRequest(
    u8 slaveId, u16 startAddr, u16 count)
{

}
