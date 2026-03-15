# Simple Makefile for building without CMake.
# Usage:
#   make          - build library + tests + demos
#   make test     - run all tests
#   make clean    - remove build artefacts

CXX      = g++
STD      = -std=c++23
WARN     = -Wall -Wextra -Wpedantic -Wshadow -Wno-unused-function
CXXFLAGS = $(STD) $(WARN) -I include -I transport/include -I src

BUILD    = build
LIB      = $(BUILD)/libmodbus.a

# ── Library sources ────────────────────────────────────────────────────────────
LIB_SRCS = \
    src/CrcEngine.cpp \
    src/ModbusFrameCodec.cpp \
    src/RegisterMap.cpp \
    src/ModbusClient.cpp \
    src/ModbusServer.cpp \
    transport/UartTransport_Linux.cpp

LIB_OBJS = $(LIB_SRCS:%.cpp=$(BUILD)/%.o)

# ── Test binaries ──────────────────────────────────────────────────────────────
TESTS = \
    $(BUILD)/test_crc \
    $(BUILD)/test_frame_codec \
    $(BUILD)/test_client \
    $(BUILD)/test_server \
    $(BUILD)/test_integration \
    $(BUILD)/test_tcp_codec

# ── Demo binaries ──────────────────────────────────────────────────────────────
DEMOS = \
    $(BUILD)/modbus_server \
    $(BUILD)/modbus_client

.PHONY: all test clean dirs

all: dirs $(LIB) $(TESTS) $(DEMOS)
	@echo ""
	@echo "Build complete."
	@echo "Run tests:    make test"
	@echo "Demo server:  $(BUILD)/modbus_server --help"
	@echo "Demo client:  $(BUILD)/modbus_client --help"

# ── Build library ──────────────────────────────────────────────────────────────
$(LIB): $(LIB_OBJS)
	ar rcs $@ $^
	@echo "  AR  $@"

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@echo "  CXX $<"

# ── Tests ──────────────────────────────────────────────────────────────────────
TEST_FLAGS = $(CXXFLAGS) -I tests

$(BUILD)/test_%: tests/test_%.cpp $(LIB)
	$(CXX) $(TEST_FLAGS) $< $(LIB) -lpthread -o $@

test: all
	@echo ""
	@echo "=============================="
	@echo "        Running tests"
	@echo "=============================="
	@PASS=0; FAIL=0; \
	for t in $(TESTS); do \
	    NAME=$$(basename $$t); \
	    if $$t > /tmp/$$NAME.out 2>&1; then \
	        P=$$(tail -1 /tmp/$$NAME.out | grep -oP '\d+(?= passed)'); \
	        printf "  %-26s  $$P tests  ✅\n" "$$NAME"; \
	        PASS=$$((PASS + $${P:-0})); \
	    else \
	        printf "  %-26s  FAILED ❌\n" "$$NAME"; \
	        cat /tmp/$$NAME.out; \
	        FAIL=$$((FAIL + 1)); \
	    fi; \
	done; \
	echo "------------------------------"; \
	echo "  Total: $$PASS passed, $$FAIL suites failed"

# ── Demo applications ──────────────────────────────────────────────────────────
$(BUILD)/modbus_server: examples/server/main.cpp $(LIB)
	$(CXX) $(CXXFLAGS) -I src $< $(LIB) -lpthread -o $@

$(BUILD)/modbus_client: examples/client/main.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $< $(LIB) -lpthread -o $@

# ── Utility ────────────────────────────────────────────────────────────────────
dirs:
	@mkdir -p $(BUILD)/src $(BUILD)/transport $(BUILD)/tests

clean:
	rm -rf $(BUILD)
	@echo "Cleaned."
