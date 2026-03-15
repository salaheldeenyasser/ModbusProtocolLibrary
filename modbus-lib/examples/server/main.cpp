/*
1. Defines a register map with named, meaningful values
2. Opens either a virtual serial port or TCP listener
3. Starts the server in a background thread
4. Refreshes the console display every second showing current register values
5. Reacts when the client writes to a register

Usage:
  modbus_server --mode uart --port <device> --baud <rate> --id <slaveId>
  modbus_server --mode tcp  --bind-port <port> --id <slaveId>

Examples:
  modbus_server --mode uart --port /dev/pts/5 --baud 9600 --id 1
  modbus_server --mode tcp  --bind-port 5020   --id 1

*/

#include "../../include/modbus/ModbusServer.h"
#include "../../include/modbus/IModbusTransport.h"
#include "../../transport/include/UartTransport.h"
#include "register_map.h"
#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char *argv[])
{

    // For simplicity, we'll just use hardcoded config here
    // In a real application, you'd parse command-line arguments or a config file
    UartTransport::Config uartConfig{
        .devicePath = "/dev/pts/5",
        .baudRate = 9600,
        .dataBits = 8,
        .parity = UartTransport::Parity::None,
        .stopBits = UartTransport::StopBits::One
    };

    auto transport = std::make_unique<UartTransport>(uartConfig);
    auto registerMap = std::make_shared<RegisterMap>();

    ModbusServer server(std::move(transport), registerMap, 1);
    server.start();

    // Simulate some changes to the registers over time
    while (true)
    {
        // Update holding registers with some dummy values
        registerMap->writeHoldingRegister(0x0000, 1500); // Motor Speed
        registerMap->writeHoldingRegister(0x0001, 250);  // Temperature

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
