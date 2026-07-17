#include "single_camera_mmio_transport.h"

#include <string.h>

static int unavailable_read(void *context, sc_feature_snapshot_t *snapshot)
{
    (void)context;
    if (snapshot != 0) memset(snapshot, 0, sizeof(*snapshot));
    return SC_TRANSPORT_UNAVAILABLE;
}
static int unavailable_ack(void *context, uint16_t frame_id)
{
    (void)context;
    (void)frame_id;
    return SC_TRANSPORT_UNAVAILABLE;
}

static int unavailable_result(void *context, const sc_round_result_t *result)
{
    (void)context;
    (void)result;
    return SC_TRANSPORT_UNAVAILABLE;
}

static int unavailable_event(void *context, const char *line)
{
    (void)context;
    (void)line;
    return SC_TRANSPORT_UNAVAILABLE;
}

void sc_mmio_transport_init_fail_closed(sc_runtime_transport_t *transport)
{
    if (transport == 0) return;
    memset(transport, 0, sizeof(*transport));
    transport->read_feature_snapshot = unavailable_read;
    transport->ack_feature_frame = unavailable_ack;
    transport->submit_round_result = unavailable_result;
    transport->emit_diagnostic_event = unavailable_event;
    transport->source_name = "mmio_unavailable";
}
