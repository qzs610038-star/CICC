PROJ_NAME = uart_hello_onchip

STANDALONE = ../../embedded_sw/efx_hard_soc/software/standalone
BSP ?= efinix/EfxSapphireSoc

# SMP startup elects hart 0 to run main; the other QCRV32 harts wait safely.
CFLAGS += -DSMP -Wall -Wextra -Werror

SRCS = $(wildcard src/*.c) \
       ${STANDALONE}/common/start.S

include ${STANDALONE}/common/bsp.mk
include ${STANDALONE}/common/riscv64-unknown-elf.mk
include ${STANDALONE}/common/standalone.mk

# This bring-up must not depend on external DDR or the legacy 0x1000 map.
LDSCRIPT = ${BSP_PATH}/linker/default_i.ld
