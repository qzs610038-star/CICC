#include <stdio.h>
#include <string.h>

#include "single_camera_fake_transport.h"
#include "single_camera_mmio_transport.h"

static int checks;
static int failures;
static FILE *raw_log;

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
    CHECK(fake.ack_count == 1u && fake.result_count == 1u);
    CHECK(event_seen(&fake, "reason=DUPLICATE_FRAME"));

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
    CHECK(fake.ack_count == 3u && !runtime.controller.result_valid);
    CHECK(event_seen(&fake, "reason=CLASSIFICATION_UNSTABLE"));
}

static void test_twenty_round_fixture(void)
{
    const sc_color_t colors[] = { SC_COLOR_WHITE, SC_COLOR_BLACK, SC_COLOR_RED,
                                  SC_COLOR_BLUE, SC_COLOR_YELLOW };
    sc_fake_transport_t fake;
    sc_runtime_t runtime;
    unsigned int i;

    init_fake(&fake, &runtime);
    for (i = 0u; i < 20u; ++i) {
        sc_color_t color = colors[i % 5u];
        int cube = (i % 4u) != 3u;
        sc_feature_snapshot_t snapshot = snapshot_for((uint16_t)(100u + i), 1u, color, cube);
        CHECK(start_color(&runtime, color, (uint16_t)(100u + i), 1000u + i, 50u) == 0);
        CHECK(sc_fake_transport_push_snapshot(&fake, &snapshot) == 0);
        CHECK(sc_runtime_process_one(&runtime, 1001u + i) == 0);
    }
    CHECK(runtime.controller.round_seq == 20u);
    CHECK(fake.ack_count == 20u && fake.result_count == 20u);
    CHECK(runtime.arm_request_count == 0u && runtime.arm_send_count == 0u);
    for (i = 0u; i < fake.result_count; ++i) {
        CHECK(fake.results[i].arm_enabled == 0u);
        CHECK(fake.results[i].decision == SC_DECISION_EXECUTE_ARM_DISABLED ||
              fake.results[i].decision == SC_DECISION_SKIP);
    }
}

static void test_mmio_is_explicitly_unavailable(void)
{
    sc_runtime_transport_t transport;
    sc_runtime_t runtime;
    sc_mmio_transport_init_fail_closed(&transport);
    CHECK(strcmp(transport.source_name, "mmio_unavailable") == 0);
    CHECK(sc_runtime_init(&runtime, &transport, 1u, 0u) != 0);
}

int main(int argc, char **argv)
{
    FILE *requested_raw_log = 0;
    if (argc == 3 && strcmp(argv[1], "--raw-log") == 0) {
        if (fopen_s(&requested_raw_log, argv[2], "wb") != 0 || requested_raw_log == 0) return 2;
    }
    test_normal_target_and_non_target();
    test_bad_flags_and_overrun_fail_closed();
    test_torn_config_and_ack_fail_closed();
    test_duplicate_timeout_abandon_and_unclassified();
    raw_log = requested_raw_log;
    test_twenty_round_fixture();
    raw_log = 0;
    test_mmio_is_explicitly_unavailable();
    if (requested_raw_log != 0) {
        fprintf(requested_raw_log, "TEST_SUMMARY total=%d passed=%d failures=%d\n",
                checks, checks - failures, failures);
        fclose(requested_raw_log);
    }
    printf("single_camera_runtime: %d/%d passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
