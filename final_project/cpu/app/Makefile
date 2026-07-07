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

# APB base address: real build MUST set SOC_H_PATH to the Efinity-generated
# soc.h directory, which provides the true IO_APB_SLAVE_0_BASE.
# Placeholder builds (unit tests, standalone) are gated behind STANDALONE_TEST=1
# to prevent accidental use of dummy base addresses in production firmware.
# See CPU_MODULE_PLAN §1.
SOC_H_PATH ?=
ifneq ($(SOC_H_PATH),)
  CFLAGS_SOC := -I$(SOC_H_PATH) -include soc.h
else ifeq ($(STANDALONE_TEST),1)
  CFLAGS_SOC := -DAPB_VISION_BASE_PLACEHOLDER=0xF0000000u
else
  $(error SOC_H_PATH not set. Set SOC_H_PATH=<dir> to the Efinity soc.h directory, or STANDALONE_TEST=1 for unit test builds)
endif

CPU_FLAGS := -march=rv32imac -mabi=ilp32 -mcmodel=medlow
CFLAGS := $(CPU_FLAGS) $(CFLAGS_SOC) -I$(INC_DIR) -O2 -ffunction-sections -fdata-sections -Wall -Wextra -g
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
