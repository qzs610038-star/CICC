#include "single_camera_fake_transport.h"

#include <string.h>

static int fake_read(void *context, sc_feature_snapshot_t *snapshot)
{
    sc_fake_transport_t *fake = (sc_fake_transport_t *)context;
    int status;

    if (fake == 0 || snapshot == 0) return SC_TRANSPORT_ERROR;
    status = fake->next_read_status;
    fake->next_read_status = SC_TRANSPORT_OK;
    if (status != SC_TRANSPORT_OK) return status;
    if (fake->read_index >= fake->snapshot_count) return SC_TRANSPORT_NO_DATA;
    *snapshot = fake->snapshots[fake->read_index++];
    return SC_TRANSPORT_OK;
}
static int fake_ack(void *context, uint16_t frame_id)
{
    sc_fake_transport_t *fake = (sc_fake_transport_t *)context;
    if (fake == 0 || fake->ack_count >= SC_FAKE_MAX_SNAPSHOTS ||
        (fake->fail_ack_frame_id != 0u && frame_id == fake->fail_ack_frame_id)) {
        return SC_TRANSPORT_ERROR;
    }
    fake->acked_frames[fake->ack_count++] = frame_id;
    return SC_TRANSPORT_OK;
}

static int fake_submit(void *context, const sc_round_result_t *result)
{
    sc_fake_transport_t *fake = (sc_fake_transport_t *)context;
    if (fake == 0 || result == 0 || result->arm_enabled != 0u ||
        fake->result_count >= SC_FAKE_MAX_RESULTS) return SC_TRANSPORT_ERROR;
    fake->results[fake->result_count++] = *result;
    return SC_TRANSPORT_OK;
}

static int fake_emit(void *context, const char *line)
{
    sc_fake_transport_t *fake = (sc_fake_transport_t *)context;
    if (fake == 0 || line == 0 || fake->event_count >= SC_FAKE_MAX_EVENTS) {
        return SC_TRANSPORT_ERROR;
    }
    (void)snprintf(fake->events[fake->event_count], sizeof(fake->events[0]), "%s", line);
    fake->event_count++;
    if (fake->raw_log != 0) (void)fprintf(fake->raw_log, "%s\n", line);
    return SC_TRANSPORT_OK;
}

void sc_fake_transport_init(sc_fake_transport_t *fake)
{
    if (fake != 0) memset(fake, 0, sizeof(*fake));
}

int sc_fake_transport_push_snapshot(sc_fake_transport_t *fake,
                                    const sc_feature_snapshot_t *snapshot)
{
    if (fake == 0 || snapshot == 0 || fake->snapshot_count >= SC_FAKE_MAX_SNAPSHOTS) return -1;
    fake->snapshots[fake->snapshot_count++] = *snapshot;
    return 0;
}

void sc_fake_transport_set_next_read_status(sc_fake_transport_t *fake, int status)
{
    if (fake != 0) fake->next_read_status = status;
}

void sc_fake_transport_set_ack_failure(sc_fake_transport_t *fake, uint16_t frame_id)
{
    if (fake != 0) fake->fail_ack_frame_id = frame_id;
}

void sc_fake_transport_set_raw_log(sc_fake_transport_t *fake, FILE *raw_log)
{
    if (fake != 0) fake->raw_log = raw_log;
}

void sc_fake_transport_bind(sc_fake_transport_t *fake, sc_runtime_transport_t *transport)
{
    if (fake == 0 || transport == 0) return;
    memset(transport, 0, sizeof(*transport));
    transport->read_feature_snapshot = fake_read;
    transport->ack_feature_frame = fake_ack;
    transport->submit_round_result = fake_submit;
    transport->emit_diagnostic_event = fake_emit;
    transport->source_name = "fake_transport";
    transport->context = fake;
}
