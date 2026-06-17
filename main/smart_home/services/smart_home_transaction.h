#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "../../shared/mqtt_iot_protocol.h"

typedef enum {
    SH_TX_IDLE = 0,
    SH_TX_PENDING,
    SH_TX_ACKED,
    SH_TX_CONFIRMED,
    SH_TX_FAILED,
    SH_TX_TIMEOUT,
} sh_transaction_state_t;

typedef struct {
    uint16_t seq;
    uint8_t floor_id;
    uint8_t cmd_type;
    uint8_t gpio_index;
    uint8_t value;
    uint8_t source;
    sh_transaction_state_t state;
    uint8_t result_code;
    int64_t started_ms;
    int64_t updated_ms;
} sh_transaction_t;

void sh_transaction_init(void);
uint16_t sh_transaction_begin(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index, uint8_t value, uint8_t source);
void sh_transaction_on_ack(const iot_ack_v3_packet_t *ack);
void sh_transaction_on_confirmed(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index, uint8_t value);
void sh_transaction_poll_timeout(void);
const sh_transaction_t *sh_transaction_find(uint16_t seq);
uint16_t sh_transaction_latest_seq(void);
const sh_transaction_t *sh_transaction_find_latest_for_device(uint8_t floor, uint8_t cmd, uint8_t index);

#ifdef __cplusplus
}
#endif
