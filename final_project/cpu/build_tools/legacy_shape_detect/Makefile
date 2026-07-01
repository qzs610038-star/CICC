#==========================================================================
#  Makefile for RISC-V Shape Recognition on SapphireSoC (QCRV32)
#
#  Usage:
#    make all          - Build the project
#    make clean        - Remove build artifacts
#    make flash        - Download to target via JTAG (requires openocd)
#
#  This Makefile is designed for use with the Efinity RISC-V toolchain.
#  Adjust CROSS_COMPILE and BSP_PATH as needed.
#==========================================================================

#--------------------------------------------------------------------------
# Toolchain configuration
#--------------------------------------------------------------------------
# Use Efinity's RISC-V GCC toolchain
# Default paths - update if your installation differs
EFINITY_PATH   ?= /opt/efinity
TOOLCHAIN_PATH ?= $(EFINITY_PATH)/ispFPGA/contents/bin/riscv

CROSS_COMPILE  ?= $(TOOLCHAIN_PATH)/bin/riscv32-unknown-elf-
CC              = $(CROSS_COMPILE)gcc
OBJCOPY         = $(CROSS_COMPILE)objcopy
OBJDUMP         = $(CROSS_COMPILE)objdump
SIZE            = $(CROSS_COMPILE)size

#--------------------------------------------------------------------------
# BSP path - update to match your IDE project location
#--------------------------------------------------------------------------
BSP_PATH       ?= ./bsp

#--------------------------------------------------------------------------
# Source files
#--------------------------------------------------------------------------
SRCS_C = main.c \
         shape_detect.c

SRCS_S = startup_qcrv32.S

SRCS   = $(SRCS_C) $(SRCS_S)

#--------------------------------------------------------------------------
# Include paths
#--------------------------------------------------------------------------
INCLUDES = -I. \
           -I$(BSP_PATH)/include \
           -I$(BSP_PATH)/src

#--------------------------------------------------------------------------
# Compiler flags
#--------------------------------------------------------------------------
# QCRV32 is a 32-bit RISC-V core
CPU_FLAGS   = -march=rv32imac -mabi=ilp32 -mcmodel=medlow

CFLAGS      = $(CPU_FLAGS) \
              $(INCLUDES) \
              -O2 \
              -ffunction-sections \
              -fdata-sections \
              -Wall \
              -Wextra \
              -g

# Linker flags
LDFLAGS     = $(CPU_FLAGS) \
              -Wl,--gc-sections \
              -Wl,-Map=$@.map

# If BSP linker script is available
# LDFLAGS += -T$(BSP_PATH)/ld/linker.ld

#--------------------------------------------------------------------------
# Build targets
#--------------------------------------------------------------------------
TARGET = shape_detect.elf
HEX    = shape_detect.hex
BIN    = shape_detect.bin
LST    = shape_detect.lst

OBJS = $(SRCS:.c=.o)

.PHONY: all clean flash info

all: $(TARGET) $(HEX) $(BIN) $(LST) info

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo ""
	@echo "Build successful: $@"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.hex: $(TARGET)
	$(OBJCOPY) -O ihex $< $@
	@echo "Generated hex: $@"

%.bin: $(TARGET)
	$(OBJCOPY) -O binary $< $@
	@echo "Generated bin: $@"

%.lst: $(TARGET)
	$(OBJDUMP) -d -S $< > $@
	@echo "Generated listing: $@"

info: $(TARGET)
	@echo ""
	@echo "=== Binary Size ==="
	$(SIZE) $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(HEX) $(BIN) $(LST) $(TARGET).map
	@echo "Cleaned build artifacts"

flash: $(HEX)
	@echo "Flashing via JTAG..."
	@echo "Use Efinity Programmer or openocd to download $(HEX)"
	@echo "Example: openocd -f interface/ftdi/olimex-arm-usb-tiny-h.cfg \\"
	@echo "         -f target/qcrv32.cfg -c 'program $(HEX) verify reset exit'"

#--------------------------------------------------------------------------
# Dependencies
#--------------------------------------------------------------------------
main.o: main.c shape_detect.h
shape_detect.o: shape_detect.c shape_detect.h
