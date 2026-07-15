#ifndef SINGLE_CAMERA_FEATURE_ADAPTER_H
#define SINGLE_CAMERA_FEATURE_ADAPTER_H

#include <stdint.h>

#include "single_camera_classifier.h"

/* Mirrors integration/single_camera_feature_contract.md; not a wire ABI. */
#define SC_FEATURE_FLAG_FRAME_STABLE     0x01u
#define SC_FEATURE_FLAG_ROI_VALID        0x02u
#define SC_FEATURE_FLAG_STATS_VALID      0x04u
#define SC_FEATURE_FLAG_DIAG_ACTIVE      0x08u
#define SC_FEATURE_FLAG_COUNTER_OVERFLOW 0x10u
#define SC_FEATURE_FLAG_SNAPSHOT_OVERRUN 0x20u
#define SC_FEATURE_FLAG_SOURCE_CH0       0x40u

typedef struct {
    uint16_t frame_id;
    uint16_t config_seq;
    uint8_t source_flags;
    sc_features_t features;
} sc_feature_snapshot_t;

/* Returns 0 only for a complete, non-diagnostic ch0 snapshot. */
int sc_feature_snapshot_is_usable(const sc_feature_snapshot_t *snapshot);

/* A rejected snapshot never produces a stable observation. */
int sc_feature_snapshot_to_observation(const sc_feature_snapshot_t *snapshot,
                                       const sc_classifier_cfg_t *cfg,
                                       sc_observation_t *observation);

#endif /* SINGLE_CAMERA_FEATURE_ADAPTER_H */
