#include "single_camera_feature_adapter.h"

#include <string.h>

int sc_feature_snapshot_is_usable(const sc_feature_snapshot_t *snapshot)
{
    uint8_t required = SC_FEATURE_FLAG_FRAME_STABLE |
                       SC_FEATURE_FLAG_ROI_VALID |
                       SC_FEATURE_FLAG_STATS_VALID |
                       SC_FEATURE_FLAG_SOURCE_CH0;
    uint8_t rejected = SC_FEATURE_FLAG_DIAG_ACTIVE |
                       SC_FEATURE_FLAG_COUNTER_OVERFLOW |
                       SC_FEATURE_FLAG_SNAPSHOT_OVERRUN;

    if (snapshot == 0) return -1;
    if ((snapshot->source_flags & required) != required) return -1;
    if (snapshot->source_flags & rejected) return -1;
    if (snapshot->features.roi_pixel_count == 0u) return -1;
    return 0;
}

int sc_feature_snapshot_to_observation(const sc_feature_snapshot_t *snapshot,
                                       const sc_classifier_cfg_t *cfg,
                                       sc_observation_t *observation)
{
    if (observation == 0) return -1;
    memset(observation, 0, sizeof(*observation));
    if (sc_feature_snapshot_is_usable(snapshot) != 0) return -1;
    return sc_classify_features(&snapshot->features, cfg, observation);
}
