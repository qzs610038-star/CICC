/* A13: replay board-verified A11/A12 synthetic FPGA snapshots through CPU. */
#include <stdio.h>
#include <string.h>

#include "competition_host_adapter.h"

static int checks;
static int failures;

#define CHECK(expr) do { checks++; if (!(expr)) { failures++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

#define ROI_PIXELS 1036800u
#define FG_AREA 102400u
#define BBOX_MIN 0x017C0140u /* {y=380, x=320} */
#define BBOX_MAX 0x02BB027Fu /* {y=699, x=639} */

typedef struct {
    const char *name;
    uint8_t expected_color;
    uint32_t red_area;
    uint32_t blue_area;
    uint32_t yellow_area;
    uint32_t sum_y;
} a13_sample_t;

/* Red/blue/yellow areas and geometry are board-verified in A11. White/black
 * sum_y values are board-verified in A12. Uncaptured colored sum_y values are
 * intentionally zero here because color area is their classification input. */
static const a13_sample_t samples[] = {
    { "red",    COLOR_RED,    FG_AREA, 0u,      0u,      0u },
    { "blue",   COLOR_BLUE,   0u,      FG_AREA, 0u,      0u },
    { "yellow", COLOR_YELLOW, 0u,      0u,      FG_AREA, 0u },
    { "white",  COLOR_WHITE,  0u,      0u,      0u,      437145600u },
    { "black",  COLOR_BLACK,  0u,      0u,      0u,      358809600u }
};

static feature_snapshot_t make_snapshot(const a13_sample_t *sample,
                                        uint16_t frame_id)
{
    feature_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snap.red_area = sample->red_area;
    snap.blue_area = sample->blue_area;
    snap.yel_area = sample->yellow_area;
    snap.roi_pixel_count = ROI_PIXELS;
    snap.sum_y = sample->sum_y;
    snap.bbox_min = BBOX_MIN;
    snap.bbox_max = BBOX_MAX;
    snap.fg_area = FG_AREA;
    snap.frame_id = frame_id;
    return snap;
}

static vision_result_t classify_sample(const a13_sample_t *sample,
                                       uint16_t frame_id)
{
    classifier_cfg_t cfg;
    feature_snapshot_t snap = make_snapshot(sample, frame_id);
    classifier_cfg_default(&cfg);
    return classify_frame(&snap, 0, &cfg);
}

static target_config_t color_cube_target(uint8_t color)
{
    target_config_t config;
    memset(&config, 0, sizeof(config));
    config.valid = 1u;
    config.target.mode = COMP_TASK_COLOR_CUBE;
    config.target.target_color = color;
    return config;
}

static target_config_t size_target(competition_task_mode_t mode, uint8_t reference_size)
{
    target_config_t config;
    memset(&config, 0, sizeof(config));
    config.valid = 1u;
    config.target.mode = mode;
    config.target.reference_size_cm_x10 = reference_size;
    return config;
}

static void run_round(competition_host_adapter_t *adapter, uint16_t *seq,
                      const vision_result_t *observation,
                      competition_decision_t expected_decision,
                      competition_reason_t expected_reason)
{
    const result_status_t *status;
    uint16_t place_seq = (*seq)++;

    CHECK(competition_host_adapter_event(adapter, COMP_EVENT_PLACE, place_seq) == 0);
    CHECK(competition_host_adapter_observe(adapter, observation) == 0);
    status = competition_host_adapter_status(adapter);
    CHECK(status->color_id == observation->color_id);
    CHECK(status->shape_id == SHAPE_CUBE);
    CHECK(status->decision == expected_decision);
    CHECK(status->reason == expected_reason);
    CHECK(competition_host_adapter_ack(adapter, place_seq) == 0);
    CHECK(competition_host_adapter_event(adapter, COMP_EVENT_REMOVE, (*seq)++) == 0);
    competition_host_adapter_advance(adapter, 1u);
}

static void run_size_deferred_round(competition_host_adapter_t *adapter,
                                    uint16_t *seq,
                                    const vision_result_t *observation)
{
    const result_status_t *status;
    uint16_t place_seq = (*seq)++;

    CHECK(competition_host_adapter_event(adapter, COMP_EVENT_PLACE, place_seq) == 0);
    CHECK(competition_host_adapter_observe(adapter, observation) == 0);
    status = competition_host_adapter_status(adapter);
    CHECK(status->color_id == observation->color_id);
    CHECK(status->decision == COMP_DECISION_WAIT);
    CHECK(status->reason == COMP_REASON_SIZE_UNAVAILABLE);
    CHECK(competition_host_adapter_event(adapter, COMP_EVENT_ABANDON, (*seq)++) == 0);
    CHECK(competition_host_adapter_event(adapter, COMP_EVENT_REMOVE, (*seq)++) == 0);
    competition_host_adapter_advance(adapter, 1u);
}

int main(void)
{
    competition_host_adapter_t adapter;
    vision_result_t observations[sizeof(samples) / sizeof(samples[0])];
    target_config_t white_target;
    target_config_t black_target;
    target_config_t delta_target;
    target_config_t within_target;
    uint16_t seq = 1u;
    unsigned int i;

    printf("A13 FPGA snapshot replay\n");
    for (i = 0u; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        observations[i] = classify_sample(&samples[i], (uint16_t)(i + 1u));
        CHECK(observations[i].color_id == samples[i].expected_color);
        CHECK(observations[i].shape_id == SHAPE_CUBE);
        CHECK(observations[i].confidence > 0u);
    }

    competition_host_adapter_init(&adapter, SIZE_STATE_UNAVAILABLE, 100u);
    white_target = color_cube_target(COLOR_WHITE);
    CHECK(competition_host_adapter_configure(&adapter, &white_target) == 0);
    for (i = 0u; i < 5u; ++i) {
        const vision_result_t *observation = &observations[i];
        run_round(&adapter, &seq, observation,
                  observation->color_id == COLOR_WHITE ? COMP_DECISION_EXECUTE : COMP_DECISION_SKIP,
                  observation->color_id == COLOR_WHITE ? COMP_REASON_TARGET_MATCH : COMP_REASON_COLOR_MISMATCH);
    }

    black_target = color_cube_target(COLOR_BLACK);
    CHECK(competition_host_adapter_configure(&adapter, &black_target) == 0);
    for (i = 0u; i < 5u; ++i) {
        const vision_result_t *observation = &observations[4u - i];
        run_round(&adapter, &seq, observation,
                  observation->color_id == COLOR_BLACK ? COMP_DECISION_EXECUTE : COMP_DECISION_SKIP,
                  observation->color_id == COLOR_BLACK ? COMP_REASON_TARGET_MATCH : COMP_REASON_COLOR_MISMATCH);
    }

    delta_target = size_target(COMP_TASK_SIZE_DELTA_1CM_CUBE, 20u);
    CHECK(competition_host_adapter_configure(&adapter, &delta_target) == 0);
    for (i = 0u; i < 5u; ++i) {
        run_size_deferred_round(&adapter, &seq, &observations[i]);
    }

    within_target = size_target(COMP_TASK_SIZE_WITHIN_0P5CM_CUBE, 30u);
    CHECK(competition_host_adapter_configure(&adapter, &within_target) == 0);
    for (i = 0u; i < 5u; ++i) {
        run_size_deferred_round(&adapter, &seq, &observations[4u - i]);
    }

    printf("a13_fpga_snapshot_replay: %d/%d passed (20 rounds, size deferred)\n",
           checks - failures, checks);
    return failures ? 1 : 0;
}
