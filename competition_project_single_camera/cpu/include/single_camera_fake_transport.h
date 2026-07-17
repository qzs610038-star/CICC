#ifndef SINGLE_CAMERA_FAKE_TRANSPORT_H
#define SINGLE_CAMERA_FAKE_TRANSPORT_H

#include <stdio.h>

#include "single_camera_runtime.h"

#define SC_FAKE_MAX_SNAPSHOTS 96u
#define SC_FAKE_MAX_EVENTS 512u
#define SC_FAKE_MAX_RESULTS 96u

typedef struct {
    sc_feature_snapshot_t snapshots[SC_FAKE_MAX_SNAPSHOTS];
    sc_round_result_t results[SC_FAKE_MAX_RESULTS];
    char events[SC_FAKE_MAX_EVENTS][256];
    uint16_t snapshot_count;
    uint16_t read_index;
    uint16_t result_count;
    uint16_t event_count;
    uint16_t acked_frames[SC_FAKE_MAX_SNAPSHOTS];
    uint16_t ack_count;
    uint16_t fail_ack_frame_id;
    int next_read_status;
    FILE *raw_log;
} sc_fake_transport_t;

void sc_fake_transport_init(sc_fake_transport_t *fake);
int sc_fake_transport_push_snapshot(sc_fake_transport_t *fake,
                                    const sc_feature_snapshot_t *snapshot);
void sc_fake_transport_set_next_read_status(sc_fake_transport_t *fake, int status);
void sc_fake_transport_set_ack_failure(sc_fake_transport_t *fake, uint16_t frame_id);
void sc_fake_transport_set_raw_log(sc_fake_transport_t *fake, FILE *raw_log);
void sc_fake_transport_bind(sc_fake_transport_t *fake, sc_runtime_transport_t *transport);

#endif /* SINGLE_CAMERA_FAKE_TRANSPORT_H */
