PROJ_NAME = p0a_uart1_diag
STANDALONE = ../../embedded_sw/efx_hard_soc/software/standalone
BSP ?= efinix/EfxSapphireSoc
CFLAGS += -Wall -Wextra -Werror -Ibuild
SRCS = $(wildcard src/*.c) \
       ${STANDALONE}/common/start.S
include ${STANDALONE}/common/bsp.mk
include ${STANDALONE}/common/riscv64-unknown-elf.mk
include ${STANDALONE}/common/standalone.mk
LDSCRIPT = linker/p0a_diag.ld
