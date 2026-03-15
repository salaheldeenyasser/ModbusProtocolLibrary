#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H
#include "IModbusTransport.h"
#include "ModbusError.h"
#include "ModbusFrame.h"
#include "RegisterMap.h"
#include <atomic>
#include <thread>

// Modbus server (slave) engine.
//
// Usage:
//   auto regMap = std::make_shared<RegisterMap>();
//   regMap->writeHoldingRegister(0x0000, 1500);
//   ModbusServer server(std::make_unique<UartTransport>(cfg), regMap, 1);
//   server.start();  // spawns background thread
class ModbusServer {
public:
    ModbusServer(std::unique_ptr<IModbusTransport> transport,
                 std::shared_ptr<RegisterMap>      registerMap,
                 u8                                slaveId);
    ~ModbusServer();

    // Start the listen loop in a background thread.
    void start();
    // Signal the loop to stop and join the thread.
    void stop();
    bool isRunning() const;

    // Process exactly one request/response cycle (blocking up to 100 ms).
    // Returns true if a request was processed, false on timeout or bad frame.
    bool processOneRequest();

private:
    std::unique_ptr<IModbusTransport> transport_;
    std::shared_ptr<RegisterMap>      registerMap_;
    u8                                slaveId_;
    std::atomic<bool>                 running_{false};
    std::thread                       workerThread_;

    // Dispatch by function code
    ModbusFrame handleRequest(const ModbusFrame& request);

    // FC handlers
    ModbusFrame handleReadCoils(const ModbusFrame& req);
    ModbusFrame handleReadDiscreteInputs(const ModbusFrame& req);
    ModbusFrame handleReadHoldingRegisters(const ModbusFrame& req);
    ModbusFrame handleReadInputRegisters(const ModbusFrame& req);
    ModbusFrame handleWriteSingleCoil(const ModbusFrame& req);
    ModbusFrame handleWriteSingleRegister(const ModbusFrame& req);
    ModbusFrame handleWriteMultipleCoils(const ModbusFrame& req);
    ModbusFrame handleWriteMultipleRegisters(const ModbusFrame& req);

    ModbusFrame buildExceptionResponse(u8 fc, ExceptionCode code);
};

#endif // MODBUS_SERVER_H
