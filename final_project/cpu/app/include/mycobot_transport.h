#ifndef MYCOBOT_TRANSPORT_H
#define MYCOBOT_TRANSPORT_H

#include <stdint.h>

#include "mycobot_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MYCOBOT_TRANSPORT_RX_CAPACITY 128u
#define MYCOBOT_TRANSPORT_TX_CAPACITY (MYCOBOT_MAX_PAYLOAD + MYCOBOT_FRAME_OVERHEAD)

typedef enum {
    MYCOBOT_TRANSPORT_OK = 0,
    MYCOBOT_TRANSPORT_NO_FRAME,
    MYCOBOT_TRANSPORT_BUSY,
    MYCOBOT_TRANSPORT_BUFFER_TOO_SMALL,
    MYCOBOT_TRANSPORT_INVALID_ARG
} mycobot_transport_status_t;

typedef struct {
    uint32_t rx_overflow;
    uint32_t noise_bytes;
    uint32_t bad_header;
    uint32_t bad_length;
    uint32_t bad_footer;
    uint32_t payload_too_long;
    uint32_t frames_ok;
    uint32_t tx_frames_queued;
    uint32_t tx_bytes_popped;
    uint32_t tx_busy_reject;
    uint32_t tx_build_failed;
} mycobot_transport_counters_t;

typedef struct {
    uint8_t rx_buf[MYCOBOT_TRANSPORT_RX_CAPACITY];
    uint8_t tx_buf[MYCOBOT_TRANSPORT_TX_CAPACITY];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint16_t tx_len;
    uint16_t tx_pos;
    mycobot_transport_counters_t counters;
} mycobot_transport_t;

void mycobot_transport_init(mycobot_transport_t *transport);
void mycobot_transport_clear_counters(mycobot_transport_t *transport);
uint16_t mycobot_transport_rx_available(const mycobot_transport_t *transport);
uint8_t mycobot_transport_rx_push_byte(mycobot_transport_t *transport,
                                       uint8_t byte);
uint16_t mycobot_transport_rx_push(mycobot_transport_t *transport,
                                   const uint8_t *bytes,
                                   uint16_t len);
mycobot_transport_status_t mycobot_transport_next_frame(mycobot_transport_t *transport,
                                                        mycobot_frame_t *frame);
mycobot_transport_status_t mycobot_transport_tx_queue_frame(mycobot_transport_t *transport,
                                                            uint8_t command,
                                                            const uint8_t *payload,
                                                            uint8_t payload_len);
uint8_t mycobot_transport_tx_busy(const mycobot_transport_t *transport);
uint16_t mycobot_transport_tx_pending(const mycobot_transport_t *transport);
mycobot_transport_status_t mycobot_transport_tx_pop_byte(mycobot_transport_t *transport,
                                                         uint8_t *byte_out);
void mycobot_transport_tx_abort(mycobot_transport_t *transport);
const mycobot_transport_counters_t *mycobot_transport_get_counters(const mycobot_transport_t *transport);

#ifdef __cplusplus
}
#endif

#endif /* MYCOBOT_TRANSPORT_H */
