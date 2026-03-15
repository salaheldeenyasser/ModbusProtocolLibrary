#include "../../include/modbus/RegisterMap.h"
#include <array>
#include <string_view>
#include <functional>

struct RegisterDescriptor
{
    u16 address;
    std::string_view name;
    std::string_view unit;
    float scale;
    bool writable;
};

constexpr std::array<RegisterDescriptor, 4> HOLDING_REGISTERS = {{
    {0x0000, "Motor Speed",           "RPM", 1.0f,  false},
    {0x0001, "Temperature",           "°C",  0.1f,  false},
    {0x0002, "Temperature Setpoint",  "°C",  0.1f,  true },
    {0x0003, "Fault Code",            "",    1.0f,  false},
}};

constexpr std::array<RegisterDescriptor, 2> COILS = {{
    {0x0000, "Pump Enable",  "", 1.0f, true},
    {0x0001, "Alarm Reset",  "", 1.0f, true},
}};
