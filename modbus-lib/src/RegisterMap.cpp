#include "../include/modbus/RegisterMap.h"

RegisterMap::RegisterMap(u16 numCoils, u16 numHoldingRegisters,
                         u16 numInputRegisters, u16 numDiscreteInputs)
    : coils_(numCoils, false),
      holdingRegisters_(numHoldingRegisters, 0),
      inputRegisters_(numInputRegisters, 0),
      discreteInputs_(numDiscreteInputs, false)
{
}

bool RegisterMap::readCoil(u16 addr) const
{
    std::lock_guard lock(mutex_);
    if (!isValidCoilAddress(addr))
    {
        throw std::out_of_range("Invalid coil address");
    }
    return coils_[addr];
}

void RegisterMap::writeCoil(u16 addr, bool value)
{
    std::lock_guard lock(mutex_);
    if (!isValidCoilAddress(addr))
    {
        throw std::out_of_range("Invalid coil address");
    }
    coils_[addr] = value;
    auto cbIt = coilWriteCBs_.find(addr);
    if (cbIt != coilWriteCBs_.end())
    {
        cbIt->second(addr, value);
    }
}

bool RegisterMap::isValidCoilAddress(u16 addr) const
{
    return addr < coils_.size();
}

u16 RegisterMap::readHoldingRegister(u16 addr) const
{
    std::lock_guard lock(mutex_);
    if (!isValidHoldingRegisterAddress(addr))
    {
        throw std::out_of_range("Invalid holding register address");
    }
    return holdingRegisters_[addr];
}

void RegisterMap::writeHoldingRegister(u16 addr, u16 value)
{
    std::lock_guard lock(mutex_);
    if (!isValidHoldingRegisterAddress(addr))
    {
        throw std::out_of_range("Invalid holding register address");
    }
    holdingRegisters_[addr] = value;
    auto cbIt = holdingWriteCBs_.find(addr);
    if (cbIt != holdingWriteCBs_.end())
    {
        cbIt->second(addr, value);
    }
}

bool RegisterMap::isValidHoldingRegisterAddress(u16 addr) const
{
    return addr < holdingRegisters_.size();
}

u16 RegisterMap::readInputRegister(u16 addr) const
{
    std::lock_guard lock(mutex_);
    if (!isValidInputRegisterAddress(addr))
    {
        throw std::out_of_range("Invalid input register address");
    }
    return inputRegisters_[addr];
}

void RegisterMap::writeInputRegister(u16 addr, u16 value)
{
    std::lock_guard lock(mutex_);
    if (!isValidInputRegisterAddress(addr))
    {
        throw std::out_of_range("Invalid input register address");
    }
    inputRegisters_[addr] = value;
}

bool RegisterMap::isValidInputRegisterAddress(u16 addr) const
{
    return addr < inputRegisters_.size();
}

bool RegisterMap::readDiscreteInput(u16 addr) const
{
    std::lock_guard lock(mutex_);
    if (!isValidDiscreteInputAddress(addr))
    {
        throw std::out_of_range("Invalid discrete input address");
    }
    return discreteInputs_[addr];
}

void RegisterMap::writeDiscreteInput(u16 addr, bool value)
{
    std::lock_guard lock(mutex_);
    if (!isValidDiscreteInputAddress(addr))
    {
        throw std::out_of_range("Invalid discrete input address");
    }
    discreteInputs_[addr] = value;
}

bool RegisterMap::isValidDiscreteInputAddress(u16 addr) const
{
    return addr < discreteInputs_.size();
}

void RegisterMap::onHoldingRegisterWrite(u16 address,
                                        HoldingRegWriteCallback callback)
{
    std::lock_guard lock(mutex_);
    holdingWriteCBs_[address] = std::move(callback);
}

void RegisterMap::onCoilWrite(u16 address, CoilWriteCallback callback)
{
    std::lock_guard lock(mutex_);
    coilWriteCBs_[address] = std::move(callback);
}
