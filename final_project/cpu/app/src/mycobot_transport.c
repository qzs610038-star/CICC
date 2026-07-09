#include "mycobot_transport.h"

static uint16_t ring_advance(uint16_t value)
{
    value++;
    if (value >= MYCOBOT_TRANSPORT_RX_CAPACITY) {
        value = 0u;
    }
    return value;
}

static uint8_t ring_peek(const mycobot_transport_t *transport,
                         uint16_t offset)
{
    uint16_t index = (uint16_t)(transport->tail + offset);

    while (index >= MYCOBOT_TRANSPORT_RX_CAPACITY) {
        index = (uint16_t)(index - MYCOBOT_TRANSPORT_RX_CAPACITY);
    }

    return transport->rx_buf[index];
}

static void ring_drop(mycobot_transport_t *transport, uint16_t len)
{
    while (len && transport->count) {
        transport->tail = ring_advance(transport->tail);
        transport->count--;
        len--;
    }
}

static void count_parse_error(mycobot_transport_t *transport,
                              mycobot_parse_status_t status)
{
    switch (status) {
    case MYCOBOT_PARSE_BAD_HEADER:
        transport->counters.bad_header++;
        break;
    case MYCOBOT_PARSE_BAD_LENGTH:
        transport->counters.bad_length++;
        break;
    case MYCOBOT_PARSE_BAD_FOOTER:
        transport->counters.bad_footer++;
        break;
    case MYCOBOT_PARSE_PAYLOAD_TOO_LONG:
        transport->counters.payload_too_long++;
        break;
    default:
        break;
    }
}

void mycobot_transport_init(mycobot_transport_t *transport)
{
    if (!transport) {
        return;
    }

    transport->head = 0u;
    transport->tail = 0u;
    transport->count = 0u;
    transport->tx_len = 0u;
    transport->tx_pos = 0u;
    mycobot_transport_clear_counters(transport);
}

void mycobot_transport_clear_counters(mycobot_transport_t *transport)
{
    if (!transport) {
        return;
    }

    transport->counters.rx_overflow = 0u;
    transport->counters.noise_bytes = 0u;
    transport->counters.bad_header = 0u;
    transport->counters.bad_length = 0u;
    transport->counters.bad_footer = 0u;
    transport->counters.payload_too_long = 0u;
    transport->counters.frames_ok = 0u;
    transport->counters.tx_frames_queued = 0u;
    transport->counters.tx_bytes_popped = 0u;
    transport->counters.tx_busy_reject = 0u;
    transport->counters.tx_build_failed = 0u;
}

uint16_t mycobot_transport_rx_available(const mycobot_transport_t *transport)
{
    return transport ? transport->count : 0u;
}

uint8_t mycobot_transport_rx_push_byte(mycobot_transport_t *transport,
                                       uint8_t byte)
{
    if (!transport) {
        return 0u;
    }

    if (transport->count >= MYCOBOT_TRANSPORT_RX_CAPACITY) {
        transport->counters.rx_overflow++;
        return 0u;
    }

    transport->rx_buf[transport->head] = byte;
    transport->head = ring_advance(transport->head);
    transport->count++;
    return 1u;
}

uint16_t mycobot_transport_rx_push(mycobot_transport_t *transport,
                                   const uint8_t *bytes,
                                   uint16_t len)
{
    uint16_t accepted = 0u;

    if (!transport || (!bytes && len)) {
        return 0u;
    }

    while (accepted < len) {
        if (!mycobot_transport_rx_push_byte(transport, bytes[accepted])) {
            break;
        }
        accepted++;
    }

    return accepted;
}

mycobot_transport_status_t mycobot_transport_next_frame(mycobot_transport_t *transport,
                                                        mycobot_frame_t *frame)
{
    uint8_t frame_len;
    uint8_t payload_len;
    uint16_t total_len;
    uint16_t i;
    uint16_t consumed = 0u;
    mycobot_parse_status_t parsed;
    uint8_t raw[MYCOBOT_MAX_PAYLOAD + MYCOBOT_FRAME_OVERHEAD];

    if (!transport || !frame) {
        return MYCOBOT_TRANSPORT_INVALID_ARG;
    }

    while (transport->count) {
        if (ring_peek(transport, 0u) != MYCOBOT_FRAME_HEADER) {
            ring_drop(transport, 1u);
            transport->counters.noise_bytes++;
            continue;
        }

        if (transport->count < 2u) {
            return MYCOBOT_TRANSPORT_NO_FRAME;
        }

        if (ring_peek(transport, 1u) != MYCOBOT_FRAME_HEADER) {
            ring_drop(transport, 1u);
            transport->counters.bad_header++;
            continue;
        }

        if (transport->count < 3u) {
            return MYCOBOT_TRANSPORT_NO_FRAME;
        }

        frame_len = ring_peek(transport, 2u);
        if (frame_len < 2u) {
            ring_drop(transport, 1u);
            transport->counters.bad_length++;
            continue;
        }

        payload_len = (uint8_t)(frame_len - 2u);
        if (payload_len > MYCOBOT_MAX_PAYLOAD) {
            ring_drop(transport, 1u);
            transport->counters.payload_too_long++;
            continue;
        }

        total_len = (uint16_t)(payload_len + MYCOBOT_FRAME_OVERHEAD);
        if (transport->count < total_len) {
            return MYCOBOT_TRANSPORT_NO_FRAME;
        }

        for (i = 0u; i < total_len; ++i) {
            raw[i] = ring_peek(transport, i);
        }

        parsed = mycobot_parse_frame(raw, total_len, frame, &consumed);
        if (parsed == MYCOBOT_PARSE_OK && consumed == total_len) {
            ring_drop(transport, total_len);
            transport->counters.frames_ok++;
            return MYCOBOT_TRANSPORT_OK;
        }

        count_parse_error(transport, parsed);
        ring_drop(transport, 1u);
    }

    return MYCOBOT_TRANSPORT_NO_FRAME;
}

mycobot_transport_status_t mycobot_transport_tx_queue_frame(mycobot_transport_t *transport,
                                                            uint8_t command,
                                                            const uint8_t *payload,
                                                            uint8_t payload_len)
{
    uint8_t len;

    if (!transport || (payload_len && !payload)) {
        return MYCOBOT_TRANSPORT_INVALID_ARG;
    }

    if (transport->tx_pos < transport->tx_len) {
        transport->counters.tx_busy_reject++;
        return MYCOBOT_TRANSPORT_BUSY;
    }

    len = mycobot_build_frame_ex(command,
                                 payload,
                                 payload_len,
                                 transport->tx_buf,
                                 MYCOBOT_TRANSPORT_TX_CAPACITY);
    if (!len) {
        transport->tx_len = 0u;
        transport->tx_pos = 0u;
        transport->counters.tx_build_failed++;
        return MYCOBOT_TRANSPORT_BUFFER_TOO_SMALL;
    }

    transport->tx_len = len;
    transport->tx_pos = 0u;
    transport->counters.tx_frames_queued++;
    return MYCOBOT_TRANSPORT_OK;
}

uint8_t mycobot_transport_tx_busy(const mycobot_transport_t *transport)
{
    if (!transport) {
        return 0u;
    }

    return (uint8_t)(transport->tx_pos < transport->tx_len);
}

uint16_t mycobot_transport_tx_pending(const mycobot_transport_t *transport)
{
    if (!transport || transport->tx_pos >= transport->tx_len) {
        return 0u;
    }

    return (uint16_t)(transport->tx_len - transport->tx_pos);
}

mycobot_transport_status_t mycobot_transport_tx_pop_byte(mycobot_transport_t *transport,
                                                         uint8_t *byte_out)
{
    if (!transport || !byte_out) {
        return MYCOBOT_TRANSPORT_INVALID_ARG;
    }

    if (transport->tx_pos >= transport->tx_len) {
        transport->tx_len = 0u;
        transport->tx_pos = 0u;
        return MYCOBOT_TRANSPORT_NO_FRAME;
    }

    *byte_out = transport->tx_buf[transport->tx_pos];
    transport->tx_pos++;
    transport->counters.tx_bytes_popped++;

    if (transport->tx_pos >= transport->tx_len) {
        transport->tx_len = 0u;
        transport->tx_pos = 0u;
    }

    return MYCOBOT_TRANSPORT_OK;
}

void mycobot_transport_tx_abort(mycobot_transport_t *transport)
{
    if (!transport) {
        return;
    }

    transport->tx_len = 0u;
    transport->tx_pos = 0u;
}

const mycobot_transport_counters_t *mycobot_transport_get_counters(const mycobot_transport_t *transport)
{
    return transport ? &transport->counters : 0;
}
