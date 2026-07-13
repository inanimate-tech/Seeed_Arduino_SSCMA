# Host memory-safety harness for Seeed_Arduino_SSCMA (dev-branch only).
#   make deps    # fetch ArduinoJson v7.4.3
#   make asan    # ASan+UBSan binary  -> build/test_asan
#   make fault   # fault-injection binary (no ASan) -> build/test_fault
#   make         # both
#   make test    # both + run_tests.sh
LIB_DIR   ?= $(abspath ..)/src
AJ_TAG    ?= v7.4.3
AJ_DIR    ?= $(CURDIR)/third_party/ArduinoJson
CXX       ?= clang++
BUILD     := build

# __CORRECT_ISO_CPP_STRING_H_PROTO: libc++ adds two strstr() overloads that
# make the library's strstr(response["name"], ...) ambiguous on host only
# (newlib on-target declares one). This restores the on-target overload set.
COMMON := -std=c++17 -g -O1 -fno-omit-frame-pointer \
          -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 \
          -D__CORRECT_ISO_CPP_STRING_H_PROTO \
          -I$(CURDIR)/stubs -I$(AJ_DIR)/src -I$(LIB_DIR) -I$(CURDIR)

SAN := -fsanitize=address,undefined -fno-sanitize-recover=undefined
LIBW := -Wno-deprecated-declarations   # ArduinoJson v7 deprecates containsKey()

.PHONY: all deps asan fault test clean
all: asan fault
deps: $(AJ_DIR)/src/ArduinoJson.h
$(AJ_DIR)/src/ArduinoJson.h:
	git clone --depth 1 --branch $(AJ_TAG) https://github.com/bblanchon/ArduinoJson $(AJ_DIR)
$(BUILD):
	mkdir -p $(BUILD)

# ---- ASan binary ----------------------------------------------------------
asan: deps | $(BUILD)
	$(CXX) $(COMMON) $(SAN) $(LIBW) -c $(LIB_DIR)/Seeed_Arduino_SSCMA.cpp -o $(BUILD)/lib_asan.o
	$(CXX) $(COMMON) $(SAN) -c stubs/arduino_stubs.cpp -o $(BUILD)/stubs_asan.o
	$(CXX) $(COMMON) $(SAN) $(LIBW) -c test_asan.cpp -o $(BUILD)/test_asan.o
	$(CXX) $(SAN) $(BUILD)/lib_asan.o $(BUILD)/stubs_asan.o $(BUILD)/test_asan.o -o $(BUILD)/test_asan

# ---- fault-injection binary (NO ASan) -------------------------------------
fault: deps | $(BUILD)
	$(CXX) $(COMMON) $(LIBW) -c $(LIB_DIR)/Seeed_Arduino_SSCMA.cpp -o $(BUILD)/lib_fault.o
	$(CXX) $(COMMON) -c stubs/arduino_stubs.cpp -o $(BUILD)/stubs_fault.o
	$(CXX) $(COMMON) -c support/fault_alloc.cpp -o $(BUILD)/fault_alloc.o
	$(CXX) $(COMMON) $(LIBW) -c test_fault.cpp -o $(BUILD)/test_fault.o
	$(CXX) $(BUILD)/lib_fault.o $(BUILD)/stubs_fault.o $(BUILD)/fault_alloc.o $(BUILD)/test_fault.o -o $(BUILD)/test_fault

test: all
	./run_tests.sh
clean:
	rm -rf $(BUILD)
