# Compiler and flags
CC = gcc
CFLAGS = -Wall -Werror -I/usr/include/libxml2
LDFLAGS = -lpthread -lxml2

# Targets and source files
TARGET_1 = onvif_discovery
SRC_1 = src/onvif_discovery.c
BUILD_DIR = build

# Ensure the build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile the target and save the output in the build directory
$(BUILD_DIR)/$(TARGET_1): $(SRC_1) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# Default target
all: $(BUILD_DIR)/$(TARGET_1)

# Clean up build artifacts
.PHONY: clean
clean:
	rm -f $(BUILD_DIR)/$(TARGET_1)

# Phony targets
.PHONY: all clean