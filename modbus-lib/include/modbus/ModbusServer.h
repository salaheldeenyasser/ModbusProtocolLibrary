#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H
#include "../../src/ModbusFrameCodec.h"
#include "IModbusTransport.h"
#include "ModbusError.h"
#include "ModbusFrame.h"
#include "RegisterMap.h"

class ModbusServer
{
public:
    ModbusServer(
        std::unique_ptr<IModbusTransport> transport,
        std::shared_ptr<RegisterMap> registerMap,
        uint8_t slaveId);
    ~ModbusServer();

    void start();
    void stop();
    bool isRunning() const;
    bool processOneRequest();

private:
    std::unique_ptr<IModbusTransport> transport_;
    std::shared_ptr<RegisterMap> registerMap_;
    uint8_t slaveId_;
    std::atomic<bool> running_{false};
    std::thread workerThread_;

    ModbusFrame handleRequest(const ModbusFrame &request);

    ModbusFrame handleReadCoils(const ModbusFrame &request);
    ModbusFrame handleReadDiscreteInputs(const ModbusFrame &request);
    ModbusFrame handleReadHoldingRegisters(const ModbusFrame &request);
    ModbusFrame handleReadInputRegisters(const ModbusFrame &request);
    ModbusFrame handleWriteSingleCoil(const ModbusFrame &request);
    ModbusFrame handleWriteSingleRegister(const ModbusFrame &request);
    ModbusFrame handleWriteMultipleCoils(const ModbusFrame &request);
    ModbusFrame handleWriteMultipleRegisters(const ModbusFrame &request);

    ModbusFrame buildExceptionResponse(uint8_t fc, ExceptionCode code);
};

#endif // MODBUS_SERVER_H
