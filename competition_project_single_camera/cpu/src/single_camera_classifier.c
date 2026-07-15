#include "single_camera_classifier.h"

#include <string.h>

void sc_classifier_cfg_default(sc_classifier_cfg_t *cfg)
{
    if (cfg == 0) return;
    cfg->min_red_area = 500u;
    cfg->min_blue_area = 500u;
    cfg->min_yellow_area = 500u;
    cfg->white_mean_luma_min = 180u;
    cfg->black_mean_luma_max = 55u;
    cfg->cube_fill_min_per_mille = 850u;
    cfg->cylinder_fill_min_per_mille = 650u;
    cfg->cone_fill_min_per_mille = 250u;
    cfg->min_bbox_area = 100u;
}

static sc_color_t classify_color(const sc_features_t *features,
                                 const sc_classifier_cfg_t *cfg)
{
    uint32_t maximum = features->red_area;
    uint32_t threshold = cfg->min_red_area;
    sc_color_t color = SC_COLOR_RED;

    if (features->blue_area > maximum) {
        maximum = features->blue_area;
        threshold = cfg->min_blue_area;
        color = SC_COLOR_BLUE;
    }
    if (features->yellow_area > maximum) {
        maximum = features->yellow_area;
        threshold = cfg->min_yellow_area;
        color = SC_COLOR_YELLOW;
    }
    if (maximum >= threshold) return color;

    if (features->roi_pixel_count == 0u) return SC_COLOR_UNKNOWN;
    {
        uint32_t mean_luma = features->sum_luma / features->roi_pixel_count;
        if (mean_luma >= cfg->white_mean_luma_min) return SC_COLOR_WHITE;
        if (mean_luma <= cfg->black_mean_luma_max) return SC_COLOR_BLACK;
    }
    return SC_COLOR_UNKNOWN;
}

static sc_shape_t classify_shape(const sc_features_t *features,
                                 const sc_classifier_cfg_t *cfg)
{
    uint32_t bbox_area = (uint32_t)features->bbox_width * features->bbox_height;
    uint32_t fill_per_mille;

    if (bbox_area < cfg->min_bbox_area || features->foreground_area == 0u)
        return SC_SHAPE_UNKNOWN;

    fill_per_mille = (features->foreground_area >= bbox_area)
                   ? 1000u
                   : (features->foreground_area * 1000u) / bbox_area;
    if (fill_per_mille >= cfg->cube_fill_min_per_mille) return SC_SHAPE_CUBE;
    if (fill_per_mille >= cfg->cylinder_fill_min_per_mille)
        return SC_SHAPE_CYLINDER;
    if (fill_per_mille >= cfg->cone_fill_min_per_mille) return SC_SHAPE_CONE;
    return SC_SHAPE_UNKNOWN;
}

int sc_classify_features(const sc_features_t *features,
                         const sc_classifier_cfg_t *cfg,
                         sc_observation_t *observation)
{
    sc_classifier_cfg_t default_cfg;

    if (features == 0 || observation == 0) return -1;
    if (cfg == 0) {
        sc_classifier_cfg_default(&default_cfg);
        cfg = &default_cfg;
    }

    memset(observation, 0, sizeof(*observation));
    observation->color = classify_color(features, cfg);
    observation->shape = classify_shape(features, cfg);
    observation->size_cm_x10 = 0u; /* Size remains unavailable before calibration. */
    observation->stable = observation->color != SC_COLOR_UNKNOWN &&
                          observation->shape != SC_SHAPE_UNKNOWN;
    return 0;
}
