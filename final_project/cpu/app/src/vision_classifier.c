#include <stdint.h>

typedef struct {
    uint8_t color;
    uint8_t shape;
    uint8_t size_class;
    uint8_t confidence;
} vision_result_t;

vision_result_t vision_classifier_run(void)
{
    vision_result_t result = {0, 0, 0, 0};
    return result;
}
