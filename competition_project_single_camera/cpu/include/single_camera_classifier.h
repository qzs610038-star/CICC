#ifndef SINGLE_CAMERA_CLASSIFIER_H
#define SINGLE_CAMERA_CLASSIFIER_H

#include <stdint.h>

#include "single_camera_f1.h"

/* Pure CPU input semantics. FPGA register offsets are deliberately excluded.
 *
 * Shape classification notes:
 *   SC_SHAPE_CUBE       — production-reliable (fill rate heuristic, >= cube_fill_min_per_mille).
 *   SC_SHAPE_CYLINDER   — UNCALIBRATED HEURISTIC.  Treated as NON_CUBE in task decisions.
 *   SC_SHAPE_CONE       — UNCALIBRATED HEURISTIC.  Treated as NON_CUBE in task decisions.
 *   SC_SHAPE_UNKNOWN    — insufficient foreground or below minimum fill rate.
 *
 * No real-camera calibration has been performed; CYLINDER and CONE thresholds are
 * Host-synthetic baselines and must not be reported as field-reliable.
 *
 * Color classification notes:
 *   WHITE and BLACK depend on foreground_area, roi_pixel_count, and sum_luma.
 *   Default white_mean_luma_min=180 and black_mean_luma_max=55 are Host synthetic
 *   baselines — not field-calibrated thresholds. */
typedef struct {
    uint32_t red_area;
    uint32_t blue_area;
    uint32_t yellow_area;
    uint32_t foreground_area;
    uint32_t roi_pixel_count;
    uint32_t sum_luma;
    uint16_t bbox_width;
    uint16_t bbox_height;
} sc_features_t;

typedef struct {
    uint32_t min_red_area;
    uint32_t min_blue_area;
    uint32_t min_yellow_area;
    uint16_t white_mean_luma_min;
    uint16_t black_mean_luma_max;
    uint16_t cube_fill_min_per_mille;
    uint16_t cylinder_fill_min_per_mille;
    uint16_t cone_fill_min_per_mille;
    uint16_t min_bbox_area;
} sc_classifier_cfg_t;

void sc_classifier_cfg_default(sc_classifier_cfg_t *cfg);
int sc_classify_features(const sc_features_t *features,
                         const sc_classifier_cfg_t *cfg,
                         sc_observation_t *observation);

#endif /* SINGLE_CAMERA_CLASSIFIER_H */
