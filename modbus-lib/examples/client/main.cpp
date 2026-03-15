/*
1. Connects to the server
2. Executes the specified command
3. Prints the result (or error) to console
4. Exits with code 0 on success, non-zero on error
----------------------------------------------------
Usage:
  modbus_client [connection options] <command> [command options]

Connection options:
  --mode uart --port <device> --baud <rate>
  --mode tcp  --host <host>   --port <port>

Commands:
  read-regs  --slave <id> --addr <hex> --count <n>
  write-reg  --slave <id> --addr <hex> --value <n>
  read-coils --slave <id> --addr <hex> --count <n>
  write-coil --slave <id> --addr <hex> --value [0|1]

Examples:
  modbus_client --mode uart --port /dev/pts/6 --baud 9600 \
      read-regs --slave 1 --addr 0x0000 --count 3

  modbus_client --mode tcp --host 127.0.0.1 --port 5020 \
      write-reg --slave 1 --addr 0x0002 --value 350
*/
#include "../include/modbus/ModbusClient.h"
#include "../transport/include/UartTransport.h"
#include <iostream>
