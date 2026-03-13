#ifndef REGISTER_MAP_H
#define REGISTER_MAP_H
#include "ModbusTypes.h"
class RegisterMap
{
public:
    static constexpr u16 DEFAULT_COILS = 2000;
    static constexpr u16 DEFAULT_HOLDING_REGISTERS = 125;
    static constexpr u16 DEFAULT_INPUT_REGISTERS = 125;
    static constexpr u16 DEFAULT_DISCRETE_INPUTS = 2000;

    RegisterMap(u16 numCoils = DEFAULT_COILS,
                u16 numHoldingRegisters = DEFAULT_HOLDING_REGISTERS,
                u16 numInputRegisters = DEFAULT_INPUT_REGISTERS,
                u16 numDiscreteInputs = DEFAULT_DISCRETE_INPUTS);

    bool readCoil(u16 addr) const;
    void writeCoil(u16 addr, bool value);
    bool isValidCoilAddress(u16 addr) const;

    u16 readHoldingRegister(u16 addr) const;
    void writeHoldingRegister(u16 addr, u16 value);
    bool isValidHoldingRegisterAddress(u16 addr) const;

    u16 readInputRegister(u16 addr) const;
    void writeInputRegister(u16 addr, u16 value);
    bool isValidInputRegisterAddress(u16 addr) const;

    bool readDiscreteInput(u16 addr) const;
    void writeDiscreteInput(u16 addr, bool value);
    bool isValidDiscreteInputAddress(u16 addr) const;

    using HoldingRegWriteCallback =
        std::function<void(uint16_t address, uint16_t value)>;
    void onHoldingRegisterWrite(uint16_t address,
                                HoldingRegWriteCallback callback);

    using CoilWriteCallback =
        std::function<void(uint16_t address, bool value)>;
    void onCoilWrite(uint16_t address, CoilWriteCallback callback);

private:
    std::vector<uint16_t> holdingRegisters_;
    std::vector<uint16_t> inputRegisters_;
    std::vector<bool>     coils_;
    std::vector<bool>     discreteInputs_;

    mutable std::mutex mutex_;

    std::unordered_map<uint16_t, HoldingRegWriteCallback> holdingWriteCBs_;
    std::unordered_map<uint16_t, CoilWriteCallback>       coilWriteCBs_;
};

#endif // REGISTER_MAP_H
