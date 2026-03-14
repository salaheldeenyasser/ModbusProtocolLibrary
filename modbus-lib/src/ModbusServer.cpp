#include "../include/modbus/ModbusServer.h"

ModbusServer::ModbusServer(
    std::unique_ptr<IModbusTransport> transport,
    std::shared_ptr<RegisterMap> registerMap,
    uint8_t slaveId)
    : transport_(std::move(transport)),
      registerMap_(registerMap),
      slaveId_(slaveId)
{
}

ModbusServer::~ModbusServer()
{
    stop();
}

void ModbusServer::start()
{
    if (running_)
        return;
    running_ = true;
    workerThread_ = std::thread([this]()
                                {
        while (running_)
        {
            if (!processOneRequest())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } });
}

void ModbusServer::stop()
{
    if (!running_)
        return;
    running_ = false;
    if (workerThread_.joinable())
        workerThread_.join();
}

bool ModbusServer::isRunning() const
{
    return running_;
}

bool ModbusServer::processOneRequest()
{
    if (!transport_->isOpen())
        return false;

    auto receiveResult = transport_->receive(256, std::chrono::milliseconds(100));
    if (!receiveResult)
    {
        // Timeout or transport error - just return and wait for next call
        return false;
    }

    auto decodeResult = ModbusFrameCodec::decodeRtuResponse(receiveResult.value());
    if (!decodeResult)
    {
        // Invalid frame - ignore and wait for next call
        return false;
    }

    ModbusFrame request = decodeResult.value();
    if (request.slaveID != slaveId_)
    {
        // Not for us - ignore
        return false;
    }

    ModbusFrame response = handleRequest(request);
    auto responseBytes = ModbusFrameCodec::encodeRtuRequest(response);
    transport_->flush();
    transport_->send(responseBytes);
    return true;
}

ModbusFrame ModbusServer::handleRequest(const ModbusFrame &req)
{
    switch (req.functionCode)
    {
    case 0x01:
        return handleReadCoils(req);
    case 0x02:
        return handleReadDiscreteInputs(req);
    case 0x03:
        return handleReadHoldingRegisters(req);
    case 0x04:
        return handleReadInputRegisters(req);
    case 0x05:
        return handleWriteSingleCoil(req);
    case 0x06:
        return handleWriteSingleRegister(req);
    case 0x0F:
        return handleWriteMultipleCoils(req);
    case 0x10:
        return handleWriteMultipleRegisters(req);
    default:
        return buildExceptionResponse(
            req.functionCode, ExceptionCode::IllegalFunction);
    }
}

ModbusFrame ModbusServer::buildExceptionResponse(u8 fc, ExceptionCode code)
{
    return ModbusFrame{
        .slaveID = slaveId_,
        .functionCode = static_cast<u8>(fc | 0x80),
        .data = {static_cast<u8>(code)}};
}
