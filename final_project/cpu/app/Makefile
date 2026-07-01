TARGET := final_decision_app

EFINITY_PATH ?= /opt/efinity
TOOLCHAIN_PATH ?= $(EFINITY_PATH)/ispFPGA/contents/bin/riscv
CROSS_COMPILE ?= $(TOOLCHAIN_PATH)/bin/riscv32-unknown-elf-

CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE := $(CROSS_COMPILE)size

SRC_DIR := src
INC_DIR := include
STARTUP_DIR := startup
LINKER := linker/linker.ld
BUILD_DIR := build

SRCS_C := $(wildcard $(SRC_DIR)/*.c)
SRCS_S := $(STARTUP_DIR)/startup_qcrv32.S
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS_C)) \
        $(BUILD_DIR)/startup_qcrv32.o

CPU_FLAGS := -march=rv32imac -mabi=ilp32 -mcmodel=medlow
CFLAGS := $(CPU_FLAGS) -I$(INC_DIR) -O2 -ffunction-sections -fdata-sections -Wall -Wextra -g
LDFLAGS := $(CPU_FLAGS) -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).elf.map -T $(LINKER)

.PHONY: all clean info

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).lst info

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/startup_qcrv32.o: $(SRCS_S) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(TARGET).lst: $(BUILD_DIR)/$(TARGET).elf
	$(OBJDUMP) -d -S $< > $@

info: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

clean:
	rm -rf $(BUILD_DIR)
