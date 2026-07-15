#include <stdio.h>
#include <string.h>

#include "single_camera_feature_adapter.h"

static int checks;
static int failures;

#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

static sc_feature_snapshot_t valid_red_cube(void)
{
    sc_feature_snapshot_t snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.frame_id = 7u;
    snapshot.config_seq = 3u;
    snapshot.source_flags = SC_FEATURE_FLAG_FRAME_STABLE |
                            SC_FEATURE_FLAG_ROI_VALID |
                            SC_FEATURE_FLAG_STATS_VALID |
                            SC_FEATURE_FLAG_SOURCE_CH0;
    snapshot.features.red_area = 900u;
    snapshot.features.foreground_area = 900u;
    snapshot.features.roi_pixel_count = 200u;
    snapshot.features.sum_luma = 36000u;
    snapshot.features.bbox_width = 30u;
    snapshot.features.bbox_height = 30u;
    return snapshot;
}

static void test_valid_snapshot_reaches_classifier(void)
{
    sc_feature_snapshot_t snapshot = valid_red_cube();
    sc_observation_t observation;

    CHECK(sc_feature_snapshot_is_usable(&snapshot) == 0);
    CHECK(sc_feature_snapshot_to_observation(&snapshot, 0, &observation) == 0);
    CHECK(observation.color == SC_COLOR_RED);
    CHECK(observation.shape == SC_SHAPE_CUBE);
    CHECK(observation.stable);
}

static void test_contract_rejection_flags(void)
{
    const uint8_t rejected[] = {
        SC_FEATURE_FLAG_DIAG_ACTIVE,
        SC_FEATURE_FLAG_COUNTER_OVERFLOW,
        SC_FEATURE_FLAG_SNAPSHOT_OVERRUN
    };
    unsigned int i;

    for (i = 0u; i < sizeof(rejected); ++i) {
        sc_feature_snapshot_t snapshot = valid_red_cube();
        sc_observation_t observation;
        snapshot.source_flags |= rejected[i];
        CHECK(sc_feature_snapshot_is_usable(&snapshot) == -1);
        CHECK(sc_feature_snapshot_to_observation(&snapshot, 0, &observation) == -1);
        CHECK(!observation.stable);
    }
}

static void test_required_flags_and_empty_roi_rejected(void)
{
    sc_feature_snapshot_t snapshot = valid_red_cube();
    sc_observation_t observation;

    snapshot.source_flags &= (uint8_t)~SC_FEATURE_FLAG_SOURCE_CH0;
    CHECK(sc_feature_snapshot_to_observation(&snapshot, 0, &observation) == -1);
    snapshot = valid_red_cube();
    snapshot.source_flags &= (uint8_t)~SC_FEATURE_FLAG_FRAME_STABLE;
    CHECK(sc_feature_snapshot_to_observation(&snapshot, 0, &observation) == -1);
    snapshot = valid_red_cube();
    snapshot.features.roi_pixel_count = 0u;
    CHECK(sc_feature_snapshot_to_observation(&snapshot, 0, &observation) == -1);
}

int main(void)
{
    test_valid_snapshot_reaches_classifier();
    test_contract_rejection_flags();
    test_required_flags_and_empty_roi_rejected();
    printf("single_camera_feature_adapter: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
