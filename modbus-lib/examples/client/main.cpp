// modbus_client — demo client console application
//
// Usage:
//   modbus_client --mode uart --port <dev> [--baud <rate>] <command> [options]
//
// Commands:
//   read-regs  --slave <id> --addr <hex> --count <n>
//   write-reg  --slave <id> --addr <hex> --value <n>
//   read-coils --slave <id> --addr <hex> --count <n>
//   write-coil --slave <id> --addr <hex> --value [0|1]
//
// Examples (virtual UART):
//   socat -d -d pty,raw,echo=0 pty,raw,echo=0
//   modbus_client --mode uart --port /dev/pts/6 --baud 9600
//       read-regs --slave 1 --addr 0x0000 --count 3
//   modbus_client --mode uart --port /dev/pts/6 --baud 9600
//       write-reg --slave 1 --addr 0x0002 --value 350

#include "../../include/modbus/ModbusClient.h"
#include "../../include/modbus/ModbusError.h"
#include "../../transport/include/UartTransport.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <cstdlib>
#include <chrono>

// ── Argument parser ───────────────────────────────────────────────────────────
struct Args {
    std::string mode    = "uart";
    std::string port    = "/dev/ttyUSB0";
    uint32_t    baud    = 9600;
    std::string host    = "127.0.0.1";
    uint16_t    tcpPort = 5020;

    std::string command; // read-regs | write-reg | read-coils | write-coil
    uint8_t     slaveId = 1;
    uint16_t    addr    = 0;
    uint16_t    count   = 1;
    int32_t     value   = 0;
};

static void printUsage(const char* prog) {
    std::cout <<
        "Usage:\n"
        "  " << prog << " --mode uart --port <dev> [--baud <rate>] <cmd> [opts]\n"
        "\nCommands:\n"
        "  read-regs  --slave <id> --addr <hex> --count <n>\n"
        "  write-reg  --slave <id> --addr <hex> --value <n>\n"
        "  read-coils --slave <id> --addr <hex> --count <n>\n"
        "  write-coil --slave <id> --addr <hex> --value [0|1]\n"
        "\nExamples:\n"
        "  " << prog << " --mode uart --port /dev/pts/6 --baud 9600 \\\n"
        "      read-regs --slave 1 --addr 0x0000 --count 3\n"
        "  " << prog << " --mode uart --port /dev/pts/6 --baud 9600 \\\n"
        "      write-reg --slave 1 --addr 0x0002 --value 350\n";
}

static uint16_t parseHex(const std::string& s) {
    return static_cast<uint16_t>(std::stoul(s, nullptr, 0));
}

static Args parseArgs(int argc, char* argv[]) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto nextStr  = [&]() -> std::string {
            return (i + 1 < argc) ? argv[++i] : "";
        };
        if      (k == "--mode")   a.mode    = nextStr();
        else if (k == "--port")   a.port    = nextStr();
        else if (k == "--baud")   a.baud    = static_cast<uint32_t>(std::stoul(nextStr()));
        else if (k == "--host")   a.host    = nextStr();
        else if (k == "--slave")  a.slaveId = static_cast<uint8_t>(std::stoul(nextStr()));
        else if (k == "--addr")   a.addr    = parseHex(nextStr());
        else if (k == "--count")  a.count   = static_cast<uint16_t>(std::stoul(nextStr()));
        else if (k == "--value")  a.value   = std::stol(nextStr());
        else if (k == "read-regs"  || k == "write-reg" ||
                 k == "read-coils" || k == "write-coil") {
            a.command = k;
        }
    }
    return a;
}

// ── Error reporting ───────────────────────────────────────────────────────────
static void printError(const ModbusError& err) {
    std::cerr << "[ERROR] " << modbusErrorToString(err) << "\n";
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc < 2) { printUsage(argv[0]); return 1; }

    Args args = parseArgs(argc, argv);

    if (args.command.empty()) {
        std::cerr << "[ERROR] No command specified.\n";
        printUsage(argv[0]);
        return 1;
    }

    // ── Build transport ───────────────────────────────────────────────────────
    std::unique_ptr<IModbusTransport> transport;

    if (args.mode == "uart") {
        UartTransport::Config cfg;
        cfg.devicePath = args.port;
        cfg.baudRate   = args.baud;
        cfg.parity     = UartTransport::Parity::None;
        cfg.stopBits   = UartTransport::StopBits::One;
        transport = std::make_unique<UartTransport>(cfg);

        if (!transport->isOpen()) {
            std::cerr << "[ERROR] Cannot open UART port: " << args.port << "\n";
            return 1;
        }
    } else {
        std::cerr << "[ERROR] --mode tcp not built in this binary.\n";
        return 1;
    }

    ModbusClient client(std::move(transport));
    client.setDefaultTimeout(std::chrono::milliseconds(2000));
    client.setRetryCount(3);

    auto t0 = std::chrono::steady_clock::now();

    // ── Dispatch ─────────────────────────────────────────────────────────────
    int exitCode = 0;

    if (args.command == "read-regs") {
        std::cout << "Reading " << args.count << " holding register(s) from slave "
                  << static_cast<int>(args.slaveId)
                  << " starting at 0x" << std::hex << std::setw(4) << std::setfill('0')
                  << args.addr << std::dec << "...\n";

        auto result = client.readHoldingRegisters(args.slaveId, args.addr, args.count);
        if (result) {
            for (uint16_t i = 0; i < args.count; ++i) {
                std::cout << "  Register[0x" << std::hex << std::setw(4) << std::setfill('0')
                          << (args.addr + i) << std::dec << "] = " << (*result)[i] << "\n";
            }
        } else {
            printError(result.error());
            exitCode = 1;
        }

    } else if (args.command == "write-reg") {
        std::cout << "Writing " << args.value << " to register 0x"
                  << std::hex << std::setw(4) << std::setfill('0') << args.addr
                  << std::dec << " on slave " << static_cast<int>(args.slaveId) << "...\n";

        auto result = client.writeSingleRegister(args.slaveId, args.addr,
                          static_cast<uint16_t>(args.value));
        if (result) {
            std::cout << "  Success (echo confirmed).\n";
        } else {
            printError(result.error());
            exitCode = 1;
        }

    } else if (args.command == "read-coils") {
        std::cout << "Reading " << args.count << " coil(s) from slave "
                  << static_cast<int>(args.slaveId)
                  << " starting at 0x" << std::hex << std::setw(4) << std::setfill('0')
                  << args.addr << std::dec << "...\n";

        auto result = client.readCoils(args.slaveId, args.addr, args.count);
        if (result) {
            for (uint16_t i = 0; i < args.count; ++i) {
                std::cout << "  Coil[0x" << std::hex << std::setw(4) << std::setfill('0')
                          << (args.addr + i) << std::dec << "] = "
                          << ((*result)[i] ? "ON" : "OFF") << "\n";
            }
        } else {
            printError(result.error());
            exitCode = 1;
        }

    } else if (args.command == "write-coil") {
        bool on = (args.value != 0);
        std::cout << "Writing coil 0x" << std::hex << std::setw(4) << std::setfill('0')
                  << args.addr << std::dec << " = " << (on ? "ON" : "OFF")
                  << " on slave " << static_cast<int>(args.slaveId) << "...\n";

        auto result = client.writeSingleCoil(args.slaveId, args.addr, on);
        if (result) {
            std::cout << "  Success.\n";
        } else {
            printError(result.error());
            exitCode = 1;
        }

    } else {
        std::cerr << "[ERROR] Unknown command: " << args.command << "\n";
        exitCode = 1;
    }

    // ── Timing ───────────────────────────────────────────────────────────────
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0);
    std::cout << "Completed in "
              << std::fixed << std::setprecision(1)
              << (elapsed.count() / 1000.0) << " ms.\n";

    return exitCode;
}
