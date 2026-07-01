#include <stdint.h>

typedef struct {
    uint8_t color_id;
    uint16_t r_min;
    uint16_t g_min;
    uint16_t b_min;
    uint16_t brightness_min;
    uint16_t brightness_max;
} color_threshold_t;

const color_threshold_t g_color_thresholds[] = {
    {0, 0, 0, 0, 0, 1023}
};

const unsigned g_color_threshold_count = sizeof(g_color_thresholds) / sizeof(g_color_thresholds[0]);
