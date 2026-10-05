# Galena native (PC) build.
#
# The PC side is a separate target: its entry point is pc/main.cpp, the
# firmware's is esp32/main.cpp. Both call the same galena::run_program pipeline
# out of lib/GalenaRuntime/, and both compile the same lib/Resiris sources, so
# their output is identical. The only difference is which text sink is installed:
# stdout here, Serial on the device.
#
# Building the firmware needs PlatformIO:  pio run -t upload

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

RESIRIS_DIR   := lib/Resiris
SYSTEM_DIR    := $(RESIRIS_DIR)/System
MODULES_DIR   := $(RESIRIS_DIR)/modules
RUNTIME_DIR   := lib/GalenaRuntime/src
INCLUDES      := -I$(RESIRIS_DIR) -I$(RUNTIME_DIR)

BUILD_DIR := build
BINARY    := $(BUILD_DIR)/galena

# Each header sits next to the .cpp that implements it, so there is one folder
# per concern and no src/ level to remember.
SOURCES := $(wildcard $(SYSTEM_DIR)/*.cpp) $(wildcard $(MODULES_DIR)/*.cpp) \
           $(RUNTIME_DIR)/galena_runtime.cpp pc/main.cpp
OBJECTS := $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)

# The host build needs no Arduino, but it does need the generated program header,
# because --embedded uses the same selection as the firmware.
NATIVE_DEPS := $(RUNTIME_DIR)/generated_programs.hpp $(RUNTIME_DIR)/generated_modules.hpp

# Adding or editing a module header changes the registry contents, so the
# generated header is rebuilt from the folder. Adding a new file with an older
# timestamp than the header will not trigger this; run `make generate` or the
# script by hand in that case, or build the firmware, which always regenerates.
MODULE_HEADERS := $(wildcard $(MODULES_DIR)/*.hpp)

.PHONY: all run embedded test generate clean

all: $(BINARY)

generate: $(RUNTIME_DIR)/generated_modules.hpp

$(RUNTIME_DIR)/generated_modules.hpp: $(MODULE_HEADERS) scripts/generate_modules.py
	@python3 scripts/generate_modules.py

$(BINARY): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@echo "  -> $@"

$(BUILD_DIR)/%.o: %.cpp $(NATIVE_DEPS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# one given .resy program:  make run FILE=programs/features.resy
FILE ?= programs/features.resy
run: $(BINARY)
	@./$(BINARY) $(FILE)

# exactly what the device's current build runs
embedded: $(BINARY)
	@./$(BINARY) --embedded

# runs every programs/*.resy, with no output
test: $(BINARY)
	@fail=0; \
	for program in programs/*.resy; do \
		if ./$(BINARY) $$program > /dev/null; then \
			echo "  ok    $$program"; \
		else \
			echo "  FAIL  $$program"; fail=1; \
		fi; \
	done; \
	exit $$fail

clean:
	rm -rf $(BUILD_DIR)