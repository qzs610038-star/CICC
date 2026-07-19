PROJ_NAME = apb_magic_onchip

STANDALONE = ../efx_hard_soc/software/standalone
BSP ?= efinix/EfxSapphireSoc

CFLAGS += -Wall -Wextra -Werror

SRCS = $(wildcard src/*.c) \
       ${STANDALONE}/common/start.S

include ${STANDALONE}/common/bsp.mk
include ${STANDALONE}/common/riscv64-unknown-elf.mk
include ${STANDALONE}/common/standalone.mk

LDSCRIPT = ${BSP_PATH}/linker/default_i.ld
