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

    snapshot.source_flags &= (uint8_t)(0xFFu ^ SC_FEATURE_FLAG_SOURCE_CH0);
    CHECK(sc_feature_snapshot_to_observation(&snapshot, 0, &observation) == -1);
    snapshot = valid_red_cube();
    snapshot.source_flags &= (uint8_t)(0xFFu ^ SC_FEATURE_FLAG_FRAME_STABLE);
    CHECK(sc_feature_snapshot_to_observation(&snapshot, 0, &observation) == -1);
    snapshot = valid_red_cube();
    snapshot.features.roi_pixel_count = 0u;
    CHECK(sc_feature_snapshot_to_observation(&snapshot, 0, &observation) == -1);
}

static void test_reserved_bit_is_fail_closed(void)
{
    sc_feature_snapshot_t snapshot = valid_red_cube();
    sc_observation_t observation;

    CHECK(snapshot.source_flags == 0x47u);
    CHECK(sc_feature_snapshot_is_usable(&snapshot) == 0);
    snapshot.source_flags |= 0x80u;
    CHECK(snapshot.source_flags == 0xC7u);
    CHECK(sc_feature_snapshot_is_usable(&snapshot) == -1);
    CHECK(sc_feature_snapshot_to_observation(&snapshot, 0, &observation) == -1);
    CHECK(!observation.stable);
}

static void test_frame_id_wraparound_and_zero(void)
{
    /* frame_id=0 is valid (post-reset first frame). */
    sc_feature_snapshot_t snapshot = valid_red_cube();
    sc_observation_t observation;
    snapshot.frame_id = 0u;
    CHECK(sc_feature_snapshot_is_usable(&snapshot) == 0);
    CHECK(sc_feature_snapshot_to_observation(&snapshot, 0, &observation) == 0);
    CHECK(observation.stable);

    /* frame_id=65535 (max uint16) is valid. */
    snapshot = valid_red_cube();
    snapshot.frame_id = 0xFFFFu;
    CHECK(sc_feature_snapshot_is_usable(&snapshot) == 0);
    CHECK(sc_feature_snapshot_to_observation(&snapshot, 0, &observation) == 0);
    CHECK(observation.stable);
}

static void test_all_required_flags_missing_singly(void)
{
    const uint8_t required[] = {
        SC_FEATURE_FLAG_FRAME_STABLE,
        SC_FEATURE_FLAG_ROI_VALID,
        SC_FEATURE_FLAG_STATS_VALID,
        SC_FEATURE_FLAG_SOURCE_CH0
    };
    unsigned int i;
    for (i = 0u; i < sizeof(required); ++i) {
        sc_feature_snapshot_t snapshot = valid_red_cube();
        sc_observation_t observation;
        snapshot.source_flags &= (uint8_t)(0xFFu ^ required[i]);
        CHECK(sc_feature_snapshot_is_usable(&snapshot) == -1);
        CHECK(sc_feature_snapshot_to_observation(&snapshot, 0, &observation) == -1);
    }
}

static void test_custom_config_acceptance(void)
{
    sc_feature_snapshot_t snapshot = valid_red_cube();
    sc_classifier_cfg_t cfg;
    sc_observation_t observation;
    sc_classifier_cfg_default(&cfg);
    /* Lower red threshold — still passes. */
    cfg.min_red_area = 100u;
    CHECK(sc_feature_snapshot_to_observation(&snapshot, &cfg, &observation) == 0);
    CHECK(observation.color == SC_COLOR_RED && observation.stable);
}

int main(void)
{
    test_valid_snapshot_reaches_classifier();
    test_contract_rejection_flags();
    test_required_flags_and_empty_roi_rejected();
    test_reserved_bit_is_fail_closed();
    test_frame_id_wraparound_and_zero();
    test_all_required_flags_missing_singly();
    test_custom_config_acceptance();
    printf("single_camera_feature_adapter: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
