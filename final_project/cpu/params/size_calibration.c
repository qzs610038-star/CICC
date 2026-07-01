#include <stdint.h>

typedef struct {
    uint8_t size_class;
    uint16_t min_bbox_width;
    uint16_t max_bbox_width;
    uint16_t min_area;
    uint16_t max_area;
} size_calibration_t;

const size_calibration_t g_size_calibration[] = {
    {0, 0, 0, 0, 0}
};

const unsigned g_size_calibration_count = sizeof(g_size_calibration) / sizeof(g_size_calibration[0]);
