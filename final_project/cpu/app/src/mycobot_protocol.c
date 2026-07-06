#include <stdint.h>

uint8_t mycobot_build_frame(uint8_t command, const uint8_t *payload, uint8_t payload_len, uint8_t *out)
{
    (void)payload;
    if (!out) {
        return 0;
    }
    out[0] = 0xFE;
    out[1] = 0xFE;
    out[2] = payload_len + 2;
    out[3] = command;
    return (uint8_t)(payload_len + 4);
}
