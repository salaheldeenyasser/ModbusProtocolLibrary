#include "../include/modbus/RegisterMap.h"

RegisterMap::RegisterMap(u16 numCoils, u16 numHoldingRegisters,
                         u16 numInputRegisters, u16 numDiscreteInputs)
    : coils_(numCoils, false),
      holdingRegisters_(numHoldingRegisters, 0),
      inputRegisters_(numInputRegisters, 0),
      discreteInputs_(numDiscreteInputs, false)
{
}
