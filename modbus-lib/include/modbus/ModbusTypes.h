#ifndef MODBUS_TYPES_H
#define MODBUS_TYPES_H
#include <cstdint>
#include <vector>

struct ModbusFrame{
    uint8_t slaveID;
    uint8_t functionCode;
    std::vector<uint8_t> data;

    
};

#endif // MODBUS_TYPES_H