#include <stdio.h>
#include <string.h>

#include "single_camera_fake_transport.h"
#include "single_camera_mmio_transport.h"

static int checks;
static int failures;
static FILE *raw_log;

static int open_binary_write(FILE **out, const char *path)
{
#if defined(_MSC_VER)
    return fopen_s(out, path, "wb");
#else
    *out = fopen(path, "wb");
    return *out == NULL ? 1 : 0;
#endif
}

#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

static sc_feature_snapshot_t snapshot_for(uint16_t frame_id, uint16_t config,
                                          sc_color_t color, int cube)
{
    sc_feature_snapshot_t snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.frame_id = frame_id;
    snapshot.config_seq = config;
    snapshot.source_flags = SC_FEATURE_FLAG_FRAME_STABLE | SC_FEATURE_FLAG_ROI_VALID |
                            SC_FEATURE_FLAG_STATS_VALID | SC_FEATURE_FLAG_SOURCE_CH0;
    snapshot.features.roi_pixel_count = 1000u;
    snapshot.features.bbox_width = 30u;
    snapshot.features.bbox_height = 30u;
    snapshot.features.foreground_area = cube ? 900u : 400u;
    snapshot.features.sum_luma = 120000u;
    if (color == SC_COLOR_RED) snapshot.features.red_area = 900u;
    if (color == SC_COLOR_BLUE) snapshot.features.blue_area = 900u;
    if (color == SC_COLOR_YELLOW) snapshot.features.yellow_area = 900u;
    if (color == SC_COLOR_WHITE) snapshot.features.sum_luma = 220000u;
    if (color == SC_COLOR_BLACK) snapshot.features.sum_luma = 20000u;
    return snapshot;
}
static sc_target_t color_target(sc_color_t color)
{
    sc_target_t target = { SC_TASK_COLOR_CUBE, color, 0u };
    return target;
}

static int start_color(sc_runtime_t *runtime, sc_color_t color, uint16_t place_event_seq,
                       uint32_t now_ms, uint32_t timeout_ms)
{
    sc_target_t target = color_target(color);
    return sc_runtime_start_round(runtime, &target, place_event_seq, now_ms, timeout_ms);
}

static void init_fake(sc_fake_transport_t *fake, sc_runtime_t *runtime)
{
    sc_runtime_transport_t transport;
    sc_fake_transport_init(fake);
    sc_fake_transport_set_raw_log(fake, raw_log);
    sc_fake_transport_bind(fake, &transport);
    CHECK(sc_runtime_init(runtime, &transport, 1u, 0u) == 0);
}

static int event_seen(const sc_fake_transport_t *fake, const char *fragment)
{
    uint16_t i;
    for (i = 0u; i < fake->event_count; ++i) {
        if (strstr(fake->events[i], fragment) != 0) return 1;
    }
    return 0;
}

static void test_normal_target_and_non_target(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_feature_snapshot_t red = snapshot_for(1u, 1u, SC_COLOR_RED, 1);
    sc_feature_snapshot_t blue = snapshot_for(2u, 1u, SC_COLOR_BLUE, 1);

    init_fake(&fake, &runtime);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &red) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.ack_count == 1u && fake.result_count == 1u);
    CHECK(fake.results[0].decision == SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(runtime.arm_request_count == 0u && runtime.arm_send_count == 0u);
    CHECK(event_seen(&fake, "event=SNAPSHOT_ACCEPT") && event_seen(&fake, "event=ACK"));

    CHECK(start_color(&runtime, SC_COLOR_RED, 2u, 2u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &blue) == 0);
    CHECK(sc_runtime_process_one(&runtime, 3u) == 0);
    CHECK(fake.ack_count == 2u && fake.result_count == 2u);
    CHECK(fake.results[1].decision == SC_DECISION_SKIP);
    CHECK(fake.results[1].reason == SC_REASON_COLOR_MISMATCH);
}

static void test_bad_flags_and_overrun_fail_closed(void)
{
    const uint8_t bad_flags[] = {
        SC_FEATURE_FLAG_DIAG_ACTIVE,
        SC_FEATURE_FLAG_COUNTER_OVERFLOW,
        SC_FEATURE_FLAG_SNAPSHOT_OVERRUN
    };
    unsigned int i;
    for (i = 0u; i < sizeof(bad_flags); ++i) {
        sc_fake_transport_t fake;
        sc_runtime_t runtime;
        sc_feature_snapshot_t snapshot = snapshot_for((uint16_t)(10u + i), 1u, SC_COLOR_RED, 1);
        snapshot.source_flags |= bad_flags[i];
        init_fake(&fake, &runtime);
        CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
        CHECK(sc_fake_transport_push_snapshot(&fake, &snapshot) == 0);
        CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
        CHECK(fake.ack_count == 0u && fake.result_count == 0u);
        CHECK(event_seen(&fake, "event=SNAPSHOT_REJECT"));
    }
}

static void test_torn_config_and_ack_fail_closed(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_feature_snapshot_t snapshot = snapshot_for(20u, 1u, SC_COLOR_RED, 1);

    init_fake(&fake, &runtime);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    sc_fake_transport_set_next_read_status(&fake, SC_TRANSPORT_SNAPSHOT_TORN);
    CHECK(sc_runtime_process_one(&runtime, 1u) == -1 && runtime.fatal);
    CHECK(event_seen(&fake, "reason=SNAPSHOT_TORN"));

    init_fake(&fake, &runtime);
    snapshot.config_seq = 2u;
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snapshot) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == -1 && runtime.fatal);
    CHECK(event_seen(&fake, "reason=CONFIG_REVISION_MISMATCH"));

    init_fake(&fake, &runtime);
    snapshot.config_seq = 1u;
    sc_fake_transport_set_ack_failure(&fake, snapshot.frame_id);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snapshot) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == -1 && runtime.fatal);
    CHECK(fake.ack_count == 0u && fake.result_count == 0u);
    CHECK(event_seen(&fake, "reason=ACK_MISMATCH"));
}

static void test_duplicate_timeout_abandon_and_unclassified(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_feature_snapshot_t snapshot = snapshot_for(30u, 1u, SC_COLOR_RED, 1);
    sc_target_t size_target = { SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_UNKNOWN, 20u };

    init_fake(&fake, &runtime);
    CHECK(start_color(&runtime, SC_COLOR_RED, 7u, 0u, 100u) == 0);
    CHECK(start_color(&runtime, SC_COLOR_RED, 7u, 1u, 100u) == 0);
    CHECK(event_seen(&fake, "reason=PLACE_DUPLICATE"));
    CHECK(sc_fake_transport_push_snapshot(&fake, &snapshot) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snapshot) == 0);
    CHECK(sc_runtime_process_one(&runtime, 2u) == 0);
    CHECK(sc_runtime_process_one(&runtime, 3u) == 0);
    /* Second identical frame arrives after result latched → release-only ACK. */
    CHECK(fake.ack_count == 2u);
    CHECK(event_seen(&fake, "event=FRAME_RELEASED_AFTER_LATCH"));

    CHECK(start_color(&runtime, SC_COLOR_RED, 8u, 10u, 5u) == 0);
    CHECK(sc_runtime_tick(&runtime, 15u) == 1);
    CHECK(event_seen(&fake, "event=TIMEOUT"));
    CHECK(fake.result_count == 2u && fake.results[1].reason == SC_REASON_TIMEOUT);

    CHECK(start_color(&runtime, SC_COLOR_RED, 9u, 20u, 100u) == 0);
    CHECK(sc_runtime_abandon(&runtime, 10u) == 0);
    CHECK(event_seen(&fake, "event=ABANDON"));
    CHECK(fake.result_count == 3u && fake.results[2].reason == SC_REASON_OPERATOR_ABANDONED);

    CHECK(sc_runtime_start_round(&runtime, &size_target, 11u, 30u, 100u) == 0);
    snapshot.frame_id = 31u;
    CHECK(sc_fake_transport_push_snapshot(&fake, &snapshot) == 0);
    CHECK(sc_runtime_process_one(&runtime, 31u) == 0);
    CHECK(runtime.controller.reason == SC_REASON_SIZE_UNAVAILABLE && !runtime.controller.result_valid);

    CHECK(sc_runtime_abandon(&runtime, 12u) == 0);
    CHECK(fake.result_count == 4u);
    CHECK(start_color(&runtime, SC_COLOR_RED, 13u, 40u, 100u) == 0);
    snapshot = snapshot_for(32u, 1u, SC_COLOR_UNKNOWN, 1);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snapshot) == 0);
    CHECK(sc_runtime_process_one(&runtime, 41u) == 0);
    CHECK(fake.ack_count == 4u && !runtime.controller.result_valid);
    CHECK(event_seen(&fake, "reason=CLASSIFICATION_UNSTABLE"));
}

static void test_twenty_round_fixture(void)
{
    const sc_color_t colors[] = { SC_COLOR_WHITE, SC_COLOR_BLACK, SC_COLOR_RED,
                                  SC_COLOR_BLUE, SC_COLOR_YELLOW };
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    unsigned int i;
    unsigned int exec_count = 0u;
    unsigned int skip_count = 0u;

    init_fake(&fake, &runtime);
    for (i = 0u; i < 20u; ++i) {
        sc_color_t color = colors[i % 5u];
        /* All cubes. Fixed RED target, cycling input colors →
           RED inputs match (EXECUTE), others mismatch (SKIP). */
        sc_feature_snapshot_t snapshot = snapshot_for((uint16_t)(100u + i), 1u, color, 1);
        CHECK(start_color(&runtime, SC_COLOR_RED, (uint16_t)(100u + i), 1000u + i, 50u) == 0);
        CHECK(sc_fake_transport_push_snapshot(&fake, &snapshot) == 0);
        CHECK(sc_runtime_process_one(&runtime, 1001u + i) == 0);
    }
    CHECK(runtime.controller.round_seq == 20u);
    CHECK(fake.ack_count == 20u);
    CHECK(fake.result_count == 20u);
    CHECK(runtime.arm_request_count == 0u && runtime.arm_send_count == 0u);
    for (i = 0u; i < fake.result_count; ++i) {
        CHECK(fake.results[i].arm_enabled == 0u);
        CHECK(fake.results[i].decision == SC_DECISION_EXECUTE_ARM_DISABLED ||
              fake.results[i].decision == SC_DECISION_SKIP);
        if (fake.results[i].decision == SC_DECISION_EXECUTE_ARM_DISABLED) exec_count++;
        if (fake.results[i].decision == SC_DECISION_SKIP) skip_count++;
    }
    CHECK(exec_count > 0u);
    CHECK(skip_count > 0u);
    CHECK(exec_count + skip_count == 20u);
}

static void test_mmio_is_explicitly_unavailable(void)
{
    sc_runtime_transport_t transport;
    sc_runtime_t runtime;
    sc_mmio_transport_init_fail_closed(&transport);
    CHECK(strcmp(transport.source_name, "mmio_unavailable") == 0);
    CHECK(sc_runtime_init(&runtime, &transport, 1u, 0u) != 0);
}

static void test_channel_offline_no_data_does_not_crash(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    unsigned int i;

    init_fake(&fake, &runtime);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    /* Repeated NO_DATA: must not crash, not produce results, not fatal. */
    for (i = 0u; i < 3u; ++i) {
        sc_fake_transport_set_next_read_status(&fake, SC_TRANSPORT_NO_DATA);
        CHECK(sc_runtime_process_one(&runtime, 10u + i) == 0);
    }
    CHECK(!runtime.fatal);
    CHECK(fake.result_count == 0u);
    CHECK(fake.ack_count == 0u);
}

static void test_frame_id_wraparound_in_runtime(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_feature_snapshot_t snap_fffe = snapshot_for(0xFFFEu, 1u, SC_COLOR_RED, 1);
    sc_feature_snapshot_t snap_ffff = snapshot_for(0xFFFFu, 1u, SC_COLOR_RED, 1);
    sc_feature_snapshot_t snap_0000 = snapshot_for(0x0000u, 1u, SC_COLOR_RED, 1);

    init_fake(&fake, &runtime);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap_fffe) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.ack_count == 1u && fake.result_count == 1u);

    CHECK(start_color(&runtime, SC_COLOR_RED, 2u, 2u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap_ffff) == 0);
    CHECK(sc_runtime_process_one(&runtime, 3u) == 0);
    CHECK(fake.ack_count == 2u && fake.result_count == 2u);

    CHECK(start_color(&runtime, SC_COLOR_RED, 3u, 4u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap_0000) == 0);
    CHECK(sc_runtime_process_one(&runtime, 5u) == 0);
    CHECK(fake.ack_count == 3u && fake.result_count == 3u);
    CHECK(!runtime.fatal);
}

static void test_old_stable_not_reused_across_rounds(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_feature_snapshot_t red = snapshot_for(1u, 1u, SC_COLOR_RED, 1);
    sc_feature_snapshot_t blue = snapshot_for(2u, 1u, SC_COLOR_BLUE, 1);

    init_fake(&fake, &runtime);
    /* Round 1: red cube target. */
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &red) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.results[0].decision == SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(fake.results[0].observation.color == SC_COLOR_RED);

    /* Round 2: blue cube target. Must NOT reuse round 1's red observation. */
    CHECK(start_color(&runtime, SC_COLOR_BLUE, 2u, 2u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &blue) == 0);
    CHECK(sc_runtime_process_one(&runtime, 3u) == 0);
    CHECK(fake.results[1].decision == SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(fake.results[1].observation.color == SC_COLOR_BLUE);
}

/* ================================================================
   20-round explicit truth table — four tasks × five rounds each.
   size_available=0 → tasks 3/4 always SIZE_UNAVAILABLE → ABANDON.
   ================================================================ */

typedef struct {
    int          idx;
    sc_task_t    task;
    sc_color_t   tgt_color;
    uint8_t      ref_size_cm_x10;
    uint16_t     input_frame_id;
    uint16_t     input_config_seq;
    sc_color_t   input_color;
    int          input_is_cube;
    sc_decision_t exp_decision;
    sc_reason_t  exp_reason;
    uint16_t     exp_result_frame_id;
    uint16_t     exp_config_revision;
    uint8_t      exp_input_flags;
    int          expect_abandon;
    uint16_t     exp_round_id;
    int          expect_ack;
    uint16_t     exp_ack_frame_id;
} truth_entry_t;

#define ALL_GOOD_FLAGS ((uint8_t)(SC_FEATURE_FLAG_FRAME_STABLE | \
                                  SC_FEATURE_FLAG_ROI_VALID | \
                                  SC_FEATURE_FLAG_STATS_VALID | \
                                  SC_FEATURE_FLAG_SOURCE_CH0))

static const truth_entry_t TRUTH[20] = {
    /* ---- Task 1: COLOR_CUBE (RED target) ---- */
    { 0, SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0,
      10, 1, SC_COLOR_RED, 1,
      SC_DECISION_EXECUTE_ARM_DISABLED, SC_REASON_TARGET_MATCH,
      10, 1, ALL_GOOD_FLAGS, 0, 1, 1, 10 },
    { 1, SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0,
      11, 1, SC_COLOR_BLUE, 1,
      SC_DECISION_SKIP, SC_REASON_COLOR_MISMATCH,
      11, 1, ALL_GOOD_FLAGS, 0, 2, 1, 11 },
    { 2, SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0,
      12, 1, SC_COLOR_RED, 1,
      SC_DECISION_EXECUTE_ARM_DISABLED, SC_REASON_TARGET_MATCH,
      12, 1, ALL_GOOD_FLAGS, 0, 3, 1, 12 },
    { 3, SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0,
      13, 1, SC_COLOR_WHITE, 1,
      SC_DECISION_SKIP, SC_REASON_COLOR_MISMATCH,
      13, 1, ALL_GOOD_FLAGS, 0, 4, 1, 13 },
    { 4, SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0,
      14, 1, SC_COLOR_BLACK, 1,
      SC_DECISION_SKIP, SC_REASON_COLOR_MISMATCH,
      14, 1, ALL_GOOD_FLAGS, 0, 5, 1, 14 },

    /* ---- Task 2: SHAPE_COLOR_CUBE (BLUE target) ---- */
    { 5, SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, 0,
      20, 1, SC_COLOR_BLUE, 1,
      SC_DECISION_EXECUTE_ARM_DISABLED, SC_REASON_TARGET_MATCH,
      20, 1, ALL_GOOD_FLAGS, 0, 6, 1, 20 },
    { 6, SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, 0,
      21, 1, SC_COLOR_RED, 1,
      SC_DECISION_SKIP, SC_REASON_COLOR_MISMATCH,
      21, 1, ALL_GOOD_FLAGS, 0, 7, 1, 21 },
    { 7, SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, 0,
      22, 1, SC_COLOR_BLUE, 1,
      SC_DECISION_EXECUTE_ARM_DISABLED, SC_REASON_TARGET_MATCH,
      22, 1, ALL_GOOD_FLAGS, 0, 8, 1, 22 },
    { 8, SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, 0,
      23, 1, SC_COLOR_YELLOW, 1,
      SC_DECISION_SKIP, SC_REASON_COLOR_MISMATCH,
      23, 1, ALL_GOOD_FLAGS, 0, 9, 1, 23 },
    { 9, SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, 0,
      24, 1, SC_COLOR_RED, 1,
      SC_DECISION_SKIP, SC_REASON_COLOR_MISMATCH,
      24, 1, ALL_GOOD_FLAGS, 0, 10, 1, 24 },

    /* ---- Task 3: SIZE_DELTA_1CM (ref=20mm, size_available=0) ----
       All rounds: SIZE_UNAVAILABLE → WAIT → ABANDON.
       ABANDON result carries the consumed frame's trace metadata. */
    {10, SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_UNKNOWN, 20,
      30, 1, SC_COLOR_RED, 1,
      SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED,
      30, 1, ALL_GOOD_FLAGS, 1, 11, 1, 30 },
    {11, SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_UNKNOWN, 20,
      31, 1, SC_COLOR_BLUE, 1,
      SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED,
      31, 1, ALL_GOOD_FLAGS, 1, 12, 1, 31 },
    {12, SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_UNKNOWN, 20,
      32, 1, SC_COLOR_YELLOW, 1,
      SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED,
      32, 1, ALL_GOOD_FLAGS, 1, 13, 1, 32 },
    {13, SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_UNKNOWN, 20,
      33, 1, SC_COLOR_WHITE, 1,
      SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED,
      33, 1, ALL_GOOD_FLAGS, 1, 14, 1, 33 },
    {14, SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_UNKNOWN, 20,
      34, 1, SC_COLOR_BLACK, 1,
      SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED,
      34, 1, ALL_GOOD_FLAGS, 1, 15, 1, 34 },

    /* ---- Task 4: SIZE_WITHIN_0P5CM (ref=20mm, size_available=0) ----
       All rounds: SIZE_UNAVAILABLE → WAIT → ABANDON. */
    {15, SC_TASK_SIZE_WITHIN_0P5CM_CUBE, SC_COLOR_UNKNOWN, 20,
      40, 1, SC_COLOR_RED, 1,
      SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED,
      40, 1, ALL_GOOD_FLAGS, 1, 16, 1, 40 },
    {16, SC_TASK_SIZE_WITHIN_0P5CM_CUBE, SC_COLOR_UNKNOWN, 20,
      41, 1, SC_COLOR_BLUE, 1,
      SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED,
      41, 1, ALL_GOOD_FLAGS, 1, 17, 1, 41 },
    {17, SC_TASK_SIZE_WITHIN_0P5CM_CUBE, SC_COLOR_UNKNOWN, 20,
      42, 1, SC_COLOR_YELLOW, 1,
      SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED,
      42, 1, ALL_GOOD_FLAGS, 1, 18, 1, 42 },
    {18, SC_TASK_SIZE_WITHIN_0P5CM_CUBE, SC_COLOR_UNKNOWN, 20,
      43, 1, SC_COLOR_WHITE, 1,
      SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED,
      43, 1, ALL_GOOD_FLAGS, 1, 19, 1, 43 },
    {19, SC_TASK_SIZE_WITHIN_0P5CM_CUBE, SC_COLOR_UNKNOWN, 20,
      44, 1, SC_COLOR_BLACK, 1,
      SC_DECISION_SKIP, SC_REASON_OPERATOR_ABANDONED,
      44, 1, ALL_GOOD_FLAGS, 1, 20, 1, 44 },
};

static void test_twenty_round_truth_table(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    int i;

    init_fake(&fake, &runtime);

    for (i = 0; i < 20; ++i) {
        const truth_entry_t *e = &TRUTH[i];
        sc_target_t tgt;
        sc_feature_snapshot_t snap;
        uint16_t place_seq = (uint16_t)(100u + (unsigned)i);

        memset(&tgt, 0, sizeof(tgt));
        tgt.task = e->task;
        tgt.target_color = e->tgt_color;
        tgt.reference_size_cm_x10 = e->ref_size_cm_x10;

        snap = snapshot_for(e->input_frame_id, e->input_config_seq,
                           e->input_color, e->input_is_cube);

        CHECK(sc_runtime_start_round(&runtime, &tgt, place_seq,
                                      1000u + (unsigned)i, 50u) == 0);
        CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
        CHECK(sc_runtime_process_one(&runtime, 1001u + (unsigned)i) == 0);

        if (e->expect_abandon) {
            CHECK(sc_runtime_abandon(&runtime,
                                     (uint16_t)(200u + (unsigned)i)) == 0);
        }

        /* Verify result fields. */
        CHECK(fake.results[i].round_id == e->exp_round_id);
        CHECK(fake.results[i].decision == e->exp_decision);
        CHECK(fake.results[i].reason == e->exp_reason);
        CHECK(fake.results[i].arm_enabled == 0u);
        CHECK(fake.results[i].frame_id == e->exp_result_frame_id);
        CHECK(fake.results[i].config_revision == e->exp_config_revision);
        CHECK(fake.results[i].input_flags == e->exp_input_flags);

        /* ACK verification. */
        if (e->expect_ack) {
            CHECK(fake.acked_frames[i] == e->exp_ack_frame_id);
        }
    }

    CHECK(runtime.controller.round_seq == 20u);
    CHECK(fake.result_count == 20u);
    CHECK(fake.ack_count == 20u);
    CHECK(!runtime.fatal);
    CHECK(runtime.arm_request_count == 0u);
    CHECK(runtime.arm_send_count == 0u);
}

static void test_continuous_frames_no_fatal_no_duplicate_result(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_feature_snapshot_t snap1 = snapshot_for(1u, 1u, SC_COLOR_RED, 1);
    sc_feature_snapshot_t snap2 = snapshot_for(2u, 1u, SC_COLOR_RED, 1);
    sc_feature_snapshot_t snap3 = snapshot_for(3u, 1u, SC_COLOR_RED, 1);

    init_fake(&fake, &runtime);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);

    /* Frame 1: consumed → result latched. */
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap1) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.result_count == 1u);
    CHECK(fake.ack_count == 1u);
    CHECK(fake.results[0].decision == SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(!runtime.fatal);

    /* Frame 2: result already latched → release-only ACK, no result, no fatal. */
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap2) == 0);
    CHECK(sc_runtime_process_one(&runtime, 2u) == 0);
    CHECK(fake.result_count == 1u);
    CHECK(fake.ack_count == 2u);
    CHECK(!runtime.fatal);
    CHECK(event_seen(&fake, "event=FRAME_RELEASED_AFTER_LATCH"));

    /* Frame 3: same release-only behavior. */
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap3) == 0);
    CHECK(sc_runtime_process_one(&runtime, 3u) == 0);
    CHECK(fake.result_count == 1u);
    CHECK(fake.ack_count == 3u);
    CHECK(!runtime.fatal);

    /* arm_enabled always 0. */
    CHECK(fake.results[0].arm_enabled == 0u);
    CHECK(runtime.arm_request_count == 0u);
    CHECK(runtime.arm_send_count == 0u);
}

static void test_latched_idle_drain_then_next_round(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_feature_snapshot_t snap1 = snapshot_for(40u, 1u, SC_COLOR_RED, 1);
    sc_feature_snapshot_t snap2 = snapshot_for(41u, 1u, SC_COLOR_BLUE, 1);
    sc_feature_snapshot_t snap3 = snapshot_for(42u, 1u, SC_COLOR_RED, 1);

    init_fake(&fake, &runtime);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap1) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.ack_count == 1u && fake.result_count == 1u);

    /* Even an unusable terminal snapshot must be released from the transport
       without classification or a second business result. */
    snap2.source_flags |= SC_FEATURE_FLAG_SNAPSHOT_OVERRUN;
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap2) == 0);
    CHECK(sc_runtime_process_one(&runtime, 2u) == 0);
    CHECK(fake.ack_count == 2u && fake.acked_frames[1] == 41u);
    CHECK(fake.result_count == 1u);
    CHECK(runtime.last_frame_id == 41u);
    CHECK(event_seen(&fake, "reason=RESULT_ALREADY_LATCHED_RELEASE_ONLY"));

    /* A later round starts after the terminal slot was drained and accepts a
       genuinely newer frame without inheriting the previous result. */
    CHECK(start_color(&runtime, SC_COLOR_RED, 2u, 3u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap3) == 0);
    CHECK(sc_runtime_process_one(&runtime, 4u) == 0);
    CHECK(fake.ack_count == 3u && fake.result_count == 2u);
    CHECK(fake.results[1].frame_id == 42u);
    CHECK(!runtime.fatal);
}

static void test_latched_idle_drain_ack_failure_is_fatal(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_feature_snapshot_t snap1 = snapshot_for(50u, 1u, SC_COLOR_RED, 1);
    sc_feature_snapshot_t snap2 = snapshot_for(51u, 1u, SC_COLOR_BLUE, 1);

    init_fake(&fake, &runtime);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap1) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.ack_count == 1u && fake.result_count == 1u);

    sc_fake_transport_set_ack_failure(&fake, 51u);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap2) == 0);
    CHECK(sc_runtime_process_one(&runtime, 2u) == -1);
    CHECK(runtime.fatal);
    CHECK(fake.ack_count == 1u && fake.result_count == 1u);
    CHECK(event_seen(&fake, "reason=LATCH_DRAIN_ACK_MISMATCH"));
}

static void test_runtime_without_frames_times_out(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;

    init_fake(&fake, &runtime);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 5u) == 0);
    /* No frames pushed — tick beyond deadline must timeout. */
    CHECK(sc_runtime_tick(&runtime, 5u) == 1);
    CHECK(event_seen(&fake, "event=TIMEOUT"));
    CHECK(fake.result_count == 1u);
    CHECK(fake.results[0].reason == SC_REASON_TIMEOUT);
}

/* ---- 16-bit half-range frame ordering vectors ---- */
static void test_frame_id_half_range_ordering(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_feature_snapshot_t snap;

    /* Helper: start round, push frame, process, check acceptance.
       accepted=1 → result latched. accepted=0 → old/dup (not fatal). */
    /* First frame (no last_frame_id): always accepted. */
    init_fake(&fake, &runtime);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    snap = snapshot_for(100u, 1u, SC_COLOR_RED, 1);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.result_count == 1u);
    CHECK(fake.ack_count == 1u);

    /* 10→11: new frame — accepted. */
    init_fake(&fake, &runtime);
    snap = snapshot_for(10u, 1u, SC_COLOR_RED, 1);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.result_count == 1u);
    CHECK(start_color(&runtime, SC_COLOR_RED, 2u, 2u, 100u) == 0);
    snap = snapshot_for(11u, 1u, SC_COLOR_RED, 1);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 3u) == 0);
    CHECK(fake.result_count == 2u);
    CHECK(!runtime.fatal);

    /* 10→10 after terminal latch: release-only ACK, no duplicate result. */
    init_fake(&fake, &runtime);
    snap = snapshot_for(10u, 1u, SC_COLOR_RED, 1);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.ack_count == 1u && fake.result_count == 1u);
    snap = snapshot_for(10u, 1u, SC_COLOR_RED, 1);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 2u) == 0);
    /* Duplicate terminal frame is released without producing another result. */
    CHECK(fake.ack_count == 2u && fake.result_count == 1u);
    CHECK(event_seen(&fake, "event=FRAME_RELEASED_AFTER_LATCH"));
    CHECK(!runtime.fatal);

    /* 10→9: old frame — fail-closed reject, no ACK, no result, no fatal. */
    init_fake(&fake, &runtime);
    snap = snapshot_for(10u, 1u, SC_COLOR_RED, 1);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.result_count == 1u);
    CHECK(start_color(&runtime, SC_COLOR_RED, 2u, 2u, 100u) == 0);
    snap = snapshot_for(9u, 1u, SC_COLOR_RED, 1);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 3u) == 0);
    CHECK(fake.result_count == 1u);           /* no new result */
    CHECK(fake.ack_count == 1u);              /* no ACK for old frame */
    CHECK(event_seen(&fake, "reason=OLD_FRAME"));
    CHECK(!runtime.fatal);

    /* 0→0xFFFF: old frame (delta=0xFFFF) — rejected. */
    init_fake(&fake, &runtime);
    snap = snapshot_for(0u, 1u, SC_COLOR_RED, 1);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.result_count == 1u);
    CHECK(start_color(&runtime, SC_COLOR_RED, 2u, 2u, 100u) == 0);
    snap = snapshot_for(0xFFFFu, 1u, SC_COLOR_RED, 1);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 3u) == 0);
    CHECK(fake.result_count == 1u);
    CHECK(event_seen(&fake, "reason=OLD_FRAME"));
    CHECK(!runtime.fatal);

    /* 0xFFFF→0: legitimate wraparound new frame (delta=1) — accepted. */
    init_fake(&fake, &runtime);
    snap = snapshot_for(0xFFFFu, 1u, SC_COLOR_RED, 1);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.result_count == 1u);
    CHECK(start_color(&runtime, SC_COLOR_RED, 2u, 2u, 100u) == 0);
    snap = snapshot_for(0u, 1u, SC_COLOR_RED, 1);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 3u) == 0);
    CHECK(fake.result_count == 2u);           /* accepted */
    CHECK(!runtime.fatal);

    /* delta=0x7FFF: new frame boundary — accepted. */
    init_fake(&fake, &runtime);
    snap = snapshot_for(0u, 1u, SC_COLOR_RED, 1);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.result_count == 1u);
    CHECK(start_color(&runtime, SC_COLOR_RED, 2u, 2u, 100u) == 0);
    snap = snapshot_for(0x7FFFu, 1u, SC_COLOR_RED, 1);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 3u) == 0);
    CHECK(fake.result_count == 2u);           /* new frame boundary */
    CHECK(!runtime.fatal);

    /* delta=0x8000: ambiguous boundary — old, rejected. */
    init_fake(&fake, &runtime);
    snap = snapshot_for(0u, 1u, SC_COLOR_RED, 1);
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.result_count == 1u);
    CHECK(start_color(&runtime, SC_COLOR_RED, 2u, 2u, 100u) == 0);
    snap = snapshot_for(0x8000u, 1u, SC_COLOR_RED, 1);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 3u) == 0);
    CHECK(fake.result_count == 1u);           /* rejected */
    CHECK(event_seen(&fake, "reason=OLD_FRAME"));
    CHECK(!runtime.fatal);
}

/* ---- ABANDON preserves consumed-frame trace metadata ---- */
static void test_abandon_preserves_trace_after_consumed_frame(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_feature_snapshot_t snap = snapshot_for(50u, 1u, SC_COLOR_RED, 1);
    sc_target_t size_target = { SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_UNKNOWN, 20u };

    init_fake(&fake, &runtime);
    /* Round with size task: frame consumed & ACKed, then SIZE_UNAVAILABLE, then ABANDON. */
    CHECK(sc_runtime_start_round(&runtime, &size_target, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(runtime.controller.reason == SC_REASON_SIZE_UNAVAILABLE);
    CHECK(!runtime.controller.result_valid);
    CHECK(fake.ack_count == 1u);   /* frame was ACKed */
    CHECK(fake.result_count == 0u); /* no terminal result yet */

    CHECK(sc_runtime_abandon(&runtime, 2u) == 0);
    CHECK(fake.result_count == 1u);
    /* ABANDON result must carry the consumed frame's trace fields. */
    CHECK(fake.results[0].round_id == 1u);
    CHECK(fake.results[0].frame_id == 50u);
    CHECK(fake.results[0].config_revision == 1u);
    CHECK(fake.results[0].input_flags == ALL_GOOD_FLAGS);
    CHECK(fake.results[0].reason == SC_REASON_OPERATOR_ABANDONED);
    CHECK(fake.results[0].arm_enabled == 0u);
    CHECK(!runtime.fatal);
}

static void test_abandon_no_frame_has_zero_trace(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_target_t size_target = { SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_UNKNOWN, 20u };

    init_fake(&fake, &runtime);
    /* Start round but never feed any frame — then tick to timeout. */
    CHECK(sc_runtime_start_round(&runtime, &size_target, 1u, 0u, 5u) == 0);
    CHECK(sc_runtime_tick(&runtime, 5u) == 1);
    CHECK(fake.result_count == 1u);
    /* Timeout without any frame: frame_id and flags must be zero. */
    CHECK(fake.results[0].frame_id == 0u);
    CHECK(fake.results[0].input_flags == 0u);
    CHECK(fake.results[0].reason == SC_REASON_TIMEOUT);
    CHECK(!runtime.fatal);
}

static void test_new_round_clears_trace_not_cross_round_leak(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_feature_snapshot_t snap1 = snapshot_for(60u, 1u, SC_COLOR_RED, 1);
    sc_feature_snapshot_t snap2 = snapshot_for(61u, 1u, SC_COLOR_RED, 1);
    sc_target_t size_target = { SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_UNKNOWN, 20u };

    init_fake(&fake, &runtime);

    /* Round 1: consume frame, SIZE_UNAVAILABLE, ABANDON.
       ABANDON result gets frame_id=60. */
    CHECK(sc_runtime_start_round(&runtime, &size_target, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap1) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(sc_runtime_abandon(&runtime, 2u) == 0);
    CHECK(fake.results[0].frame_id == 60u);

    /* Round 2: feed NO frames, timeout. Must NOT inherit round 1's frame_id. */
    CHECK(sc_runtime_start_round(&runtime, &size_target, 3u, 3u, 5u) == 0);
    CHECK(sc_runtime_tick(&runtime, 8u) == 1);
    CHECK(fake.results[1].frame_id == 0u);    /* no frame consumed */
    CHECK(fake.results[1].input_flags == 0u);
    CHECK(fake.results[1].reason == SC_REASON_TIMEOUT);

    /* Round 3: consume frame, SIZE_UNAVAILABLE, ABANDON.
       Must get round 3's own frame_id, not round 1's. */
    CHECK(sc_runtime_start_round(&runtime, &size_target, 4u, 10u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &snap2) == 0);
    CHECK(sc_runtime_process_one(&runtime, 11u) == 0);
    CHECK(sc_runtime_abandon(&runtime, 5u) == 0);
    CHECK(fake.results[2].frame_id == 61u);
    CHECK(!runtime.fatal);
}

static void test_arm_result_fields_always_safe(void)
{
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    sc_feature_snapshot_t red = snapshot_for(1u, 1u, SC_COLOR_RED, 1);
    sc_feature_snapshot_t blue = snapshot_for(2u, 1u, SC_COLOR_BLUE, 1);

    init_fake(&fake, &runtime);
    /* Target hit → arm_enabled still 0. */
    CHECK(start_color(&runtime, SC_COLOR_RED, 1u, 0u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &red) == 0);
    CHECK(sc_runtime_process_one(&runtime, 1u) == 0);
    CHECK(fake.results[0].decision == SC_DECISION_EXECUTE_ARM_DISABLED);
    CHECK(fake.results[0].arm_enabled == 0u);
    CHECK(runtime.arm_request_count == 0u);
    CHECK(runtime.arm_send_count == 0u);

    /* Non-target → arm_enabled still 0. */
    CHECK(start_color(&runtime, SC_COLOR_RED, 2u, 2u, 100u) == 0);
    CHECK(sc_fake_transport_push_snapshot(&fake, &blue) == 0);
    CHECK(sc_runtime_process_one(&runtime, 3u) == 0);
    CHECK(fake.results[1].decision == SC_DECISION_SKIP);
    CHECK(fake.results[1].arm_enabled == 0u);
    CHECK(runtime.arm_request_count == 0u);
    CHECK(runtime.arm_send_count == 0u);
}

int main(int argc, char **argv)
{
    FILE *requested_raw_log = 0;
    if (argc == 3 && strcmp(argv[1], "--raw-log") == 0) {
        if (open_binary_write(&requested_raw_log, argv[2]) != 0 || requested_raw_log == 0) return 2;
    }
    test_normal_target_and_non_target();
    test_bad_flags_and_overrun_fail_closed();
    test_torn_config_and_ack_fail_closed();
    test_duplicate_timeout_abandon_and_unclassified();
    raw_log = requested_raw_log;
    test_twenty_round_fixture();
    raw_log = 0;
    test_mmio_is_explicitly_unavailable();
    test_channel_offline_no_data_does_not_crash();
    test_frame_id_wraparound_in_runtime();
    test_old_stable_not_reused_across_rounds();
    test_twenty_round_truth_table();
    test_continuous_frames_no_fatal_no_duplicate_result();
    test_latched_idle_drain_then_next_round();
    test_latched_idle_drain_ack_failure_is_fatal();
    test_frame_id_half_range_ordering();
    test_abandon_preserves_trace_after_consumed_frame();
    test_abandon_no_frame_has_zero_trace();
    test_new_round_clears_trace_not_cross_round_leak();
    test_runtime_without_frames_times_out();
    test_arm_result_fields_always_safe();
    if (requested_raw_log != 0) {
        fprintf(requested_raw_log, "TEST_SUMMARY total=%d passed=%d failures=%d\n",
                checks, checks - failures, failures);
        fclose(requested_raw_log);
    }
    printf("single_camera_runtime: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
