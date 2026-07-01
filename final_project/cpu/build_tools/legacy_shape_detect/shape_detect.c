/*==========================================================================
 *  shape_detect.c  --  3D-object shape detection from 2D silhouette
 *
 *  Works on the 240x135 decimated frame written by the FPGA downscaler.
 *  Algorithm summary:
 *    1. Find the dominant colour (non-zero color_id) in the frame.
 *    2. Binarise: foreground = pixels matching dominant colour.
 *    3. Compute bounding box.
 *    4. Build a row-width profile (left-most / right-most FG pixel per row).
 *    5. Classify shape using:
 *       a. Taper ratio  = top-quarter avg width / bottom-quarter avg width
 *       b. Transition rows = rows from top until width >= 70% of max width
 *       c. Edge straightness of left/right contour
 *
 *  Classification decision tree:
 *    taper_ratio < 0.40   ->  CONE
 *    transition_rows > 18% of object height  ->  CYLINDER
 *    else  ->  CUBE
 *==========================================================================*/

#include "shape_detect.h"

/* ---- Helpers ----------------------------------------------------------- */

static int abs_i(int x) { return x < 0 ? -x : x; }

/* ---- Name lookup ------------------------------------------------------- */

const char *color_name(uint8_t id)
{
    switch (id) {
    case COLOR_WHITE:  return "WHITE";
    case COLOR_BLACK:  return "BLACK";
    case COLOR_RED:    return "RED";
    case COLOR_BLUE:   return "BLUE";
    case COLOR_YELLOW: return "YELLOW";
    default:           return "NONE";
    }
}

const char *shape_name(uint8_t id)
{
    switch (id) {
    case SHAPE_CUBE:     return "CUBE";
    case SHAPE_CYLINDER: return "CYLINDER";
    case SHAPE_CONE:     return "CONE";
    default:             return "NONE";
    }
}

/* ---- Step 1: dominant colour ------------------------------------------- */

static uint8_t find_dominant_color(const volatile pixel_t *frame)
{
    int counts[6] = {0};   /* index 0..5 for COLOR_NONE..COLOR_YELLOW */

    for (int i = 0; i < FRAME_W * FRAME_H; i++) {
        uint8_t c = frame[i].meta & 0x07;
        if (c > 0 && c <= 5)
            counts[c]++;
    }

    uint8_t best = 0;
    int     best_cnt = 0;
    for (int c = 1; c <= 5; c++) {
        if (counts[c] > best_cnt) {
            best_cnt = counts[c];
            best = (uint8_t)c;
        }
    }
    return best;
}

/* ---- Step 2-3: binarise + bounding box --------------------------------- */

static int compute_bbox(const volatile pixel_t *frame,
                        uint8_t dom_color,
                        bbox_t *bb, int *fg_count)
{
    int min_x = FRAME_W, max_x = -1;
    int min_y = FRAME_H, max_y = -1;
    int cnt = 0;

    for (int y = 0; y < FRAME_H; y++) {
        for (int x = 0; x < FRAME_W; x++) {
            uint8_t c = frame[y * FRAME_W + x].meta & 0x07;
            if (c == dom_color) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                cnt++;
            }
        }
    }

    if (cnt < 50 || max_x < min_x || max_y < min_y)
        return 0;   /* too few foreground pixels */

    bb->x = min_x;
    bb->y = min_y;
    bb->w = max_x - min_x + 1;
    bb->h = max_y - min_y + 1;
    *fg_count = cnt;
    return 1;
}

/* ---- Step 4: row-width profile ----------------------------------------- */

/* For each row inside the bounding box, compute width of FG region.
 * left[r], right[r] are the left-most and right-most FG x-coordinate
 * relative to bbox.x.  width[r] = right[r] - left[r] + 1.             */

#define MAX_ROWS 135

static int row_left [MAX_ROWS];
static int row_right[MAX_ROWS];
static int row_width[MAX_ROWS];

static void build_row_profile(const volatile pixel_t *frame,
                              uint8_t dom_color,
                              const bbox_t *bb)
{
    for (int r = 0; r < bb->h; r++) {
        int y = bb->y + r;
        int lft = bb->w;   /* sentinel: right of bbox */
        int rgt = -1;      /* sentinel: left of bbox */

        for (int dx = 0; dx < bb->w; dx++) {
            int x = bb->x + dx;
            uint8_t c = frame[y * FRAME_W + x].meta & 0x07;
            if (c == dom_color) {
                if (dx < lft) lft = dx;
                if (dx > rgt) rgt = dx;
            }
        }

        row_left[r]  = lft;
        row_right[r] = rgt;
        row_width[r] = (rgt >= lft) ? (rgt - lft + 1) : 0;
    }
}

/* ---- Step 5: classify -------------------------------------------------- */

static uint8_t classify_shape(const bbox_t *bb)
{
    int h = bb->h;
    if (h < 8) return SHAPE_NONE;    /* object too small */

    /* --- 5a. Taper ratio ------------------------------------------------ */
    int q1 = h / 4;                  /* top quarter boundary (row index) */
    int q3 = h - h / 4;             /* bottom quarter start */

    int top_sum = 0, top_cnt = 0;
    int bot_sum = 0, bot_cnt = 0;

    for (int r = 0; r < q1; r++) {
        if (row_width[r] > 0) { top_sum += row_width[r]; top_cnt++; }
    }
    for (int r = q3; r < h; r++) {
        if (row_width[r] > 0) { bot_sum += row_width[r]; bot_cnt++; }
    }

    if (bot_cnt == 0 || top_cnt == 0)
        return SHAPE_NONE;

    /* Use integer arithmetic: taper_ratio = top_sum * 100 / (bot_sum * top_cnt / bot_cnt)
     * Simplified: taper_pct = top_sum * bot_cnt * 100 / (bot_sum * top_cnt) */
    int taper_pct = (int)((long)top_sum * bot_cnt * 100 / ((long)bot_sum * top_cnt));

    if (taper_pct < 40)
        return SHAPE_CONE;

    /* --- 5b. Top-transition analysis ------------------------------------ */
    /* Find the maximum width in the object */
    int max_w = 0;
    for (int r = 0; r < h; r++) {
        if (row_width[r] > max_w) max_w = row_width[r];
    }

    if (max_w < 4) return SHAPE_NONE;

    int thresh_w = max_w * 70 / 100;  /* 70% of max width */
    int transition_rows = 0;

    for (int r = 0; r < h; r++) {
        if (row_width[r] >= thresh_w) break;
        transition_rows++;
    }

    /* --- 5c. Edge straightness (left contour) --------------------------- */
    /* Measure sum of absolute row-to-row differences of left edge
     * in the middle 50% of the object (rows h/4 .. 3h/4).
     * Cube/Cylinder: left edge is nearly vertical -> small delta sum.
     * Cone: left edge is diagonal -> larger delta sum (but this is
     * already captured by taper ratio, so edge straightness is a
     * secondary cue).                                                     */
    int edge_delta_sum = 0;
    int edge_rows = 0;
    for (int r = q1 + 1; r < q3; r++) {
        if (row_width[r] > 0 && row_width[r - 1] > 0) {
            edge_delta_sum += abs_i(row_left[r] - row_left[r - 1]);
            edge_rows++;
        }
    }

    /* --- Decision ------------------------------------------------------- */
    /*  Cylinder: elliptical top -> many transition rows before reaching
     *  full width.  Typical: > 18% of object height.
     *
     *  Cube: flat top -> few transition rows.  Typical: < 12%.
     *
     *  Secondary: edge straightness helps disambiguate edge cases.        */

    int trans_pct = transition_rows * 100 / h;

    if (trans_pct > 18)
        return SHAPE_CYLINDER;

    /* Edge straightness tie-breaker:
     * High average left-edge delta (>1.5 px/row) with moderate taper
     * suggests a cylinder seen at an angle.                               */
    if (edge_rows > 0) {
        int avg_delta_x10 = edge_delta_sum * 10 / edge_rows;
        if (avg_delta_x10 > 15 && taper_pct < 75)
            return SHAPE_CYLINDER;
    }

    return SHAPE_CUBE;
}

/* ---- Public API -------------------------------------------------------- */

int detect_object(const volatile pixel_t *frame, detect_result_t *result)
{
    /* 1. Dominant colour */
    uint8_t dom = find_dominant_color(frame);
    if (dom == COLOR_NONE) {
        result->color_id = COLOR_NONE;
        result->shape_id = SHAPE_NONE;
        return 0;
    }

    /* 2-3. Bounding box */
    bbox_t bb;
    int fg_cnt;
    if (!compute_bbox(frame, dom, &bb, &fg_cnt)) {
        result->color_id = dom;
        result->shape_id = SHAPE_NONE;
        return 0;
    }

    /* 4. Row-width profile */
    build_row_profile(frame, dom, &bb);

    /* 5. Classify */
    uint8_t shape = classify_shape(&bb);

    /* Fill result */
    result->color_id = dom;
    result->shape_id = shape;
    result->bbox     = bb;
    result->fg_count = fg_cnt;

    return 1;
}
