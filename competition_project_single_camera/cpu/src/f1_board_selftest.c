#include "f1_board_selftest.h"

#include <string.h>

#include "single_camera_runtime.h"

typedef enum {
    CASE_PROCESS = 0,
    CASE_TIMEOUT,
    CASE_DUPLICATE,
    CASE_OLD_FRAME,
    CASE_BAD_FLAGS,
    CASE_ACK_FAILURE
} case_kind_t;

typedef struct {
    sc_target_t target;
    sc_feature_snapshot_t snapshot;
    case_kind_t kind;
    sc_decision_t decision;
    sc_reason_t reason;
    uint8_t expect_result;
    uint8_t expect_fatal;
} selftest_case_t;

typedef struct {
    const selftest_case_t *test_case;
    f1_board_selftest_report_t *report;
    f1_selftest_event_sink_t sink;
    void *sink_context;
    uint8_t read_done;
    uint8_t duplicate_read;
} selftest_transport_t;

static uint32_t digest_bytes(uint32_t digest, const void *data, unsigned length)
{
    const unsigned char *bytes = (const unsigned char *)data;
    unsigned i;
    for (i = 0u; i < length; ++i) {
        digest ^= bytes[i];
        digest *= 16777619u;
    }
    return digest;
}

static sc_feature_snapshot_t snapshot(uint16_t frame, uint8_t flags,
                                      sc_color_t color, uint16_t width,
                                      uint16_t height)
{
    sc_feature_snapshot_t value;
    memset(&value, 0, sizeof(value));
    value.frame_id = frame;
    value.config_seq = 1u;
    value.source_flags = flags;
    value.features.foreground_area = (uint32_t)width * (uint32_t)height;
    value.features.roi_pixel_count = value.features.foreground_area;
    value.features.bbox_width = width;
    value.features.bbox_height = height;
    value.features.sum_luma = value.features.foreground_area * 120u;
    if (color == SC_COLOR_RED) value.features.red_area = value.features.foreground_area;
    if (color == SC_COLOR_BLUE) value.features.blue_area = value.features.foreground_area;
    if (color == SC_COLOR_YELLOW) value.features.yellow_area = value.features.foreground_area;
    return value;
}

static int transport_read(void *context, sc_feature_snapshot_t *value)
{
    selftest_transport_t *transport = (selftest_transport_t *)context;
    if (transport->read_done && !transport->duplicate_read) return SC_TRANSPORT_NO_DATA;
    *value = transport->test_case->snapshot;
    transport->read_done = 1u;
    if (transport->duplicate_read) transport->duplicate_read = 0u;
    return SC_TRANSPORT_OK;
}

static int transport_ack(void *context, uint16_t frame_id)
{
    selftest_transport_t *transport = (selftest_transport_t *)context;
    transport->report->digest = digest_bytes(transport->report->digest,
                                              &frame_id, sizeof(frame_id));
    if (transport->test_case->kind == CASE_ACK_FAILURE) return SC_TRANSPORT_ERROR;
    transport->report->ack_count++;
    return SC_TRANSPORT_OK;
}

static int transport_result(void *context, const sc_round_result_t *result)
{
    selftest_transport_t *transport = (selftest_transport_t *)context;
    transport->report->result_count++;
    transport->report->digest = digest_bytes(transport->report->digest,
                                              result, sizeof(*result));
    return SC_TRANSPORT_OK;
}

static int transport_event(void *context, const char *line)
{
    selftest_transport_t *transport = (selftest_transport_t *)context;
    const char *cursor = line;
    unsigned length = 0u;
    while (cursor[length] != '\0') length++;
    transport->report->event_count++;
    transport->report->digest = digest_bytes(transport->report->digest, line, length);
    if (transport->sink != 0 && transport->sink(transport->sink_context, line) != 0) {
        transport->report->sink_failed = 1u;
        return -1;
    }
    return 0;
}

static int run_case(const selftest_case_t *test_case, uint16_t index,
                    f1_selftest_event_sink_t sink, void *sink_context,
                    f1_board_selftest_report_t *report)
{
    selftest_transport_t context;
    sc_runtime_transport_t transport;
    sc_runtime_t runtime;
    unsigned results_before = report->result_count;
    int status;

    memset(&context, 0, sizeof(context));
    memset(&transport, 0, sizeof(transport));
    context.test_case = test_case;
    context.report = report;
    context.sink = sink;
    context.sink_context = sink_context;
    transport.read_feature_snapshot = transport_read;
    transport.ack_feature_frame = transport_ack;
    transport.submit_round_result = transport_result;
    transport.emit_diagnostic_event = transport_event;
    transport.source_name = "f1_selftest";
    transport.context = &context;

    status = sc_runtime_init(&runtime, &transport, 1u, 0u);
    if (status != 0) return report->sink_failed ? 0 : -1;
    if (sc_runtime_start_round(&runtime, &test_case->target,
                               (uint16_t)(index + 1u), 0u, 10u) != 0) return -1;

    if (test_case->kind == CASE_OLD_FRAME) {
        runtime.has_last_frame = 1u;
        runtime.last_frame_id = (uint16_t)(test_case->snapshot.frame_id + 1u);
    }
    if (test_case->kind == CASE_TIMEOUT) {
        status = sc_runtime_tick(&runtime, 10u);
    } else {
        status = sc_runtime_process_one(&runtime, 1u);
        if (test_case->kind == CASE_DUPLICATE && status == 0) {
            context.read_done = 0u;
            context.duplicate_read = 1u;
            status = sc_runtime_process_one(&runtime, 2u);
        }
    }
    if ((runtime.fatal != 0u) != (test_case->expect_fatal != 0u)) return -1;
    if ((report->result_count != results_before) != (test_case->expect_result != 0u)) return -1;
    if (test_case->expect_result) {
        if (runtime.controller.decision != test_case->decision ||
            runtime.controller.reason != test_case->reason) return -1;
    }
    if (runtime.arm_request_count != 0u || runtime.arm_send_count != 0u ||
        SC_RUNTIME_ARM_ENABLED != 0u) return -1;
    return (status < 0 && !test_case->expect_fatal) ? -1 : 0;
}

int f1_board_selftest_run(f1_selftest_event_sink_t sink, void *sink_context,
                          f1_board_selftest_report_t *report)
{
    const uint8_t valid = SC_FEATURE_FLAG_FRAME_STABLE | SC_FEATURE_FLAG_ROI_VALID |
                          SC_FEATURE_FLAG_STATS_VALID | SC_FEATURE_FLAG_SOURCE_CH0;
    selftest_case_t cases[F1_BOARD_SELFTEST_CASES];
    unsigned i;

    if (report == 0) return -1;
    memset(report, 0, sizeof(*report));
    report->canary = F1_BOARD_SELFTEST_CANARY;
    report->digest = 2166136261u;
    report->arm_enabled = SC_RUNTIME_ARM_ENABLED;

    memset(cases, 0, sizeof(cases));
    cases[0] = (selftest_case_t){{SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0u}, snapshot(10u, valid, SC_COLOR_RED, 30u, 30u), CASE_PROCESS, SC_DECISION_EXECUTE_ARM_DISABLED, SC_REASON_TARGET_MATCH, 1u, 0u};
    cases[1] = (selftest_case_t){{SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0u}, snapshot(20u, valid, SC_COLOR_BLUE, 30u, 30u), CASE_PROCESS, SC_DECISION_SKIP, SC_REASON_COLOR_MISMATCH, 1u, 0u};
    cases[2] = (selftest_case_t){{SC_TASK_SHAPE_COLOR_CUBE, SC_COLOR_BLUE, 0u}, snapshot(30u, valid, SC_COLOR_BLUE, 30u, 30u), CASE_PROCESS, SC_DECISION_EXECUTE_ARM_DISABLED, SC_REASON_TARGET_MATCH, 1u, 0u};
    cases[3] = (selftest_case_t){{SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_RED, 20u}, snapshot(40u, valid, SC_COLOR_RED, 30u, 30u), CASE_TIMEOUT, SC_DECISION_WAIT, SC_REASON_TIMEOUT, 1u, 0u};
    cases[4] = (selftest_case_t){{SC_TASK_SIZE_DELTA_1CM_CUBE, SC_COLOR_RED, 20u}, snapshot(50u, valid, SC_COLOR_RED, 30u, 30u), CASE_DUPLICATE, SC_DECISION_WAIT, SC_REASON_SIZE_UNAVAILABLE, 0u, 0u};
    cases[5] = (selftest_case_t){{SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0u}, snapshot(60u, valid, SC_COLOR_RED, 30u, 30u), CASE_OLD_FRAME, SC_DECISION_WAIT, SC_REASON_NONE, 0u, 0u};
    cases[6] = (selftest_case_t){{SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0u}, snapshot(70u, (uint8_t)(valid | SC_FEATURE_FLAG_DIAG_ACTIVE), SC_COLOR_RED, 30u, 30u), CASE_BAD_FLAGS, SC_DECISION_WAIT, SC_REASON_NONE, 0u, 0u};
    cases[7] = (selftest_case_t){{SC_TASK_COLOR_CUBE, SC_COLOR_RED, 0u}, snapshot(80u, valid, SC_COLOR_RED, 30u, 30u), CASE_ACK_FAILURE, SC_DECISION_WAIT, SC_REASON_NONE, 0u, 1u};

    for (i = 0u; i < F1_BOARD_SELFTEST_CASES; ++i) {
        report->cases_run++;
        if (run_case(&cases[i], (uint16_t)i, sink, sink_context, report) != 0) return -1;
        report->cases_passed++;
        if (report->sink_failed) break;
    }
    report->arm_request_count = 0u;
    report->arm_send_count = 0u;
    return report->sink_failed ? 1 : 0;
}
