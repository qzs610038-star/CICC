#ifndef SHAPE_DETECT_H
#define SHAPE_DETECT_H

#include <stdint.h>

/*--------------------------------------------------------------------------
 *  Constants
 *------------------------------------------------------------------------*/
#define FRAME_W         240
#define FRAME_H         135

/* Frame buffer base address in DDR.
 * Must match the downscaler BASE_ADDR parameter in top.v.
 * When downscaler is enabled, this MUST be updated to match. */
#ifndef FRAME_BASE
#define FRAME_BASE      0x04000000u   /* 64 MB offset in DDR */
#endif

/* Pixel layout (32-bit, little-endian in DDR):
 *   byte 0 : B
 *   byte 1 : G
 *   byte 2 : R
 *   byte 3 : {5'b0, color_id[2:0]}
 */
typedef struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t meta;           /* bits[2:0] = color_id */
} pixel_t;

/*--------------------------------------------------------------------------
 *  Color IDs  (matches FPGA color_classifier.v)
 *------------------------------------------------------------------------*/
#define COLOR_NONE   0
#define COLOR_WHITE  1
#define COLOR_BLACK  2
#define COLOR_RED    3
#define COLOR_BLUE   4
#define COLOR_YELLOW 5

/*--------------------------------------------------------------------------
 *  Shape IDs
 *------------------------------------------------------------------------*/
#define SHAPE_NONE     0
#define SHAPE_CUBE     1
#define SHAPE_CYLINDER 2
#define SHAPE_CONE     3

/*--------------------------------------------------------------------------
 *  Bounding-box result
 *------------------------------------------------------------------------*/
typedef struct {
    int x, y, w, h;         /* bounding box in 240x135 coords */
} bbox_t;

/*--------------------------------------------------------------------------
 *  Detection result
 *------------------------------------------------------------------------*/
typedef struct {
    uint8_t color_id;        /* dominant color in the object */
    uint8_t shape_id;        /* SHAPE_CUBE / CYLINDER / CONE */
    bbox_t  bbox;
    int     fg_count;        /* foreground pixel count */
} detect_result_t;

/*--------------------------------------------------------------------------
 *  API
 *------------------------------------------------------------------------*/

/* Detect colour + shape from the current downscaled frame in DDR.
 * Returns 1 if an object was found, 0 otherwise.                        */
int detect_object(const volatile pixel_t *frame, detect_result_t *result);

/* Return human-readable colour name string. */
const char *color_name(uint8_t id);

/* Return human-readable shape name string. */
const char *shape_name(uint8_t id);

#endif /* SHAPE_DETECT_H */
