/*
  modbus_server — demo Modbus RTU server

  Usage:
    modbus_server --mode uart --port <device> --baud <rate> --id <slaveId>

  Example (virtual UART with socat):
    socat -d -d pty,raw,echo=0 pty,raw,echo=0
    modbus_server --mode uart --port /dev/pts/6 --baud 9600 --id 1
*/

#include "../../include/modbus/ModbusServer.h"
#include "../../include/modbus/IModbusTransport.h"
#include "../../transport/include/UartTransport.h"
#include "register_map.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <string>
#include <cstdlib>

// ── Signal handler ─────────────────────────────────────────────────────────
static std::atomic<bool> g_shutdown{false};
static void onSignal(int) { g_shutdown = true; }

// ── Argument parsing ────────────────────────────────────────────────────────
struct Args {
    std::string mode    = "uart";
    std::string port    = "";        // must be supplied via --port
    uint32_t    baud    = 9600;
    uint8_t     slaveId = 1;
};

static void printUsage(const char* prog) {
    std::cout << "Usage:\n"
              << "  " << prog << " --mode uart --port <device> [--baud <rate>] [--id <n>]\n"
              << "\nExample:\n"
              << "  socat -d -d pty,raw,echo=0 pty,raw,echo=0\n"
              << "  " << prog << " --mode uart --port /dev/pts/6 --baud 9600 --id 1\n";
}

static Args parseArgs(int argc, char* argv[]) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto next = [&]() -> std::string {
            return (i + 1 < argc) ? argv[++i] : "";
        };
        if      (k == "--mode")  a.mode    = next();
        else if (k == "--port")  a.port    = next();
        else if (k == "--baud")  a.baud    = static_cast<uint32_t>(std::stoul(next()));
        else if (k == "--id")    a.slaveId = static_cast<uint8_t>(std::stoul(next()));
    }
    return a;
}

// ── Display ─────────────────────────────────────────────────────────────────
static std::string fmtVal(uint16_t raw, float scale, const char* unit) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << (raw * scale);
    if (unit[0]) ss << " " << unit;
    ss << "  (raw: " << raw << ")";
    return ss.str();
}

static void printRegisters(const std::shared_ptr<RegisterMap>& regMap) {
    static bool first = true;
    if (!first) {
        // Move cursor up to overwrite previous output (14 lines)
        for (int i = 0; i < 14; ++i) std::cout << "\033[A\033[2K";
    }
    first = false;

    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "  Holding Registers:\n";
    for (const auto& r : HOLDING_REGISTERS) {
        uint16_t raw = regMap->readHoldingRegister(r.address);
        std::cout << "    [0x" << std::hex << std::setw(4) << std::setfill('0')
                  << r.address << std::dec << "] "
                  << std::left << std::setw(24) << r.name << " = "
                  << fmtVal(raw, r.scale, r.unit.data())
                  << (r.writable ? "  [writable]" : "") << "\n";
    }
    std::cout << "\n  Coils:\n";
    for (const auto& c : COILS) {
        bool val = regMap->readCoil(c.address);
        std::cout << "    [0x" << std::hex << std::setw(4) << std::setfill('0')
                  << c.address << std::dec << "] "
                  << std::left << std::setw(24) << c.name << " = "
                  << (val ? "ON" : "OFF") << "\n";
    }
    std::cout << "╚══════════════════════════════════════════════════╝\n";
    std::cout.flush();
}

// ── main ────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    if (argc < 2) { printUsage(argv[0]); return 1; }

    Args args = parseArgs(argc, argv);

    if (args.port.empty()) {
        std::cerr << "[ERROR] --port is required. Example: --port /dev/pts/6\n";
        printUsage(argv[0]);
        return 1;
    }

    // Build transport
    UartTransport::Config cfg;
    cfg.devicePath = args.port;
    cfg.baudRate   = args.baud;
    cfg.parity     = UartTransport::Parity::None;
    cfg.stopBits   = UartTransport::StopBits::One;
    auto transport = std::make_unique<UartTransport>(cfg);

    if (!transport->isOpen()) {
        std::cerr << "[ERROR] Cannot open UART port: " << args.port << "\n"
                  << "        Is socat running? Is the port path correct?\n";
        return 1;
    }
    std::cout << "[INFO] Opened " << args.port << " @ " << args.baud
              << " baud, slave ID=" << static_cast<int>(args.slaveId) << "\n";

    // Build register map with initial values
    auto regMap = std::make_shared<RegisterMap>();
    regMap->writeHoldingRegister(0x0000, 1500); // Motor Speed
    regMap->writeHoldingRegister(0x0001, 253);  // Temperature (25.3 °C × 10)
    regMap->writeHoldingRegister(0x0002, 300);  // Setpoint    (30.0 °C × 10)
    regMap->writeHoldingRegister(0x0003, 0);    // Fault Code

    // React when client writes to the setpoint register
    regMap->onHoldingRegisterWrite(0x0002, [](u16, u16 val) {
        std::cout << "\n[EVENT] Setpoint updated to "
                  << std::fixed << std::setprecision(1) << (val * 0.1f)
                  << " C  (raw=" << val << ")\n";
    });
    regMap->onCoilWrite(0x0000, [](u16, bool val) {
        std::cout << "\n[EVENT] Pump " << (val ? "ENABLED" : "DISABLED") << "\n";
    });

    // Start server
    ModbusServer server(std::move(transport), regMap,
                        static_cast<u8>(args.slaveId));
    server.start();

    std::cout << "  Modbus server running. Press Ctrl+C to stop.\n\n";

    // Display loop
    while (!g_shutdown) {
        printRegisters(regMap);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n[INFO] Shutting down...\n";
    server.stop();
    return 0;
}
