#include <stdint.h>

uint32_t board_io_read_feature_word(uint32_t offset)
{
    (void)offset;
    return 0u;
}

void board_io_write_result_word(uint32_t offset, uint32_t value)
{
    (void)offset;
    (void)value;
}
