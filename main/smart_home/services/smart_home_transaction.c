#include "smart_home_transaction.h"
#include "mqtt_iot_protocol.h"
#include "ui_events.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "SH_TX";

#define SH_TX_MAX 32
static sh_transaction_t s_tx[SH_TX_MAX];
static uint16_t s_next_seq = 1;
static portMUX_TYPE s_tx_lock = portMUX_INITIALIZER_UNLOCKED;

void sh_transaction_init(void)
{
    memset(s_tx, 0, sizeof(s_tx));
    s_next_seq = 1;
}

uint16_t sh_transaction_begin(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index, uint8_t value, uint8_t source)
{
    portENTER_CRITICAL(&s_tx_lock);
    uint16_t seq = s_next_seq++;
    if (s_next_seq == 0) s_next_seq = 1;

    sh_transaction_t *slot = &s_tx[seq % SH_TX_MAX];
    memset(slot, 0, sizeof(*slot));
    slot->seq = seq;
    slot->floor_id = floor_id;
    slot->cmd_type = cmd_type;
    slot->gpio_index = gpio_index;
    slot->value = value;
    slot->source = source;
    slot->state = SH_TX_PENDING;
    slot->started_ms = esp_timer_get_time() / 1000;
    slot->updated_ms = slot->started_ms;
    portEXIT_CRITICAL(&s_tx_lock);

    ui_event_publish(UI_EVENT_MODEL_UPDATED);
    return seq;
}

void sh_transaction_on_ack(const iot_ack_v3_packet_t *ack)
{
    if (!ack) return;
    portENTER_CRITICAL(&s_tx_lock);
    sh_transaction_t *slot = &s_tx[ack->seq % SH_TX_MAX];
    if (slot->seq != ack->seq) { portEXIT_CRITICAL(&s_tx_lock); return; }

    slot->state = (ack->result_code == IOT_RESULT_OK) ? SH_TX_ACKED : SH_TX_FAILED;
    slot->result_code = ack->result_code;
    slot->updated_ms = esp_timer_get_time() / 1000;
    portEXIT_CRITICAL(&s_tx_lock);

    ESP_LOGI(TAG, "ACK seq=%u result=%u", ack->seq, ack->result_code);
    ui_event_publish(UI_EVENT_MODEL_UPDATED);
}

void sh_transaction_on_confirmed(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index, uint8_t value)
{
    portENTER_CRITICAL(&s_tx_lock);
    for (int i = 0; i < SH_TX_MAX; i++) {
        sh_transaction_t *tx = &s_tx[i];
        if (tx->state == SH_TX_ACKED &&
            tx->floor_id == floor_id &&
            tx->cmd_type == cmd_type &&
            tx->gpio_index == gpio_index &&
            tx->value == value) {
            tx->state = SH_TX_CONFIRMED;
            tx->updated_ms = esp_timer_get_time() / 1000;
        }
    }
    portEXIT_CRITICAL(&s_tx_lock);
}

void sh_transaction_poll_timeout(void)
{
    int64_t now = esp_timer_get_time() / 1000;
    portENTER_CRITICAL(&s_tx_lock);
    for (int i = 0; i < SH_TX_MAX; i++) {
        sh_transaction_t *tx = &s_tx[i];
        if (tx->state == SH_TX_PENDING && now - tx->started_ms > 3000) {
            tx->state = SH_TX_TIMEOUT;
            tx->updated_ms = now;
            ESP_LOGW(TAG, "TX timeout seq=%u", tx->seq);
        }
    }
    portEXIT_CRITICAL(&s_tx_lock);
    ui_event_publish(UI_EVENT_MODEL_UPDATED);
}

const sh_transaction_t *sh_transaction_find(uint16_t seq)
{
    portENTER_CRITICAL(&s_tx_lock);
    sh_transaction_t *slot = &s_tx[seq % SH_TX_MAX];
    if (slot->seq != seq) { portEXIT_CRITICAL(&s_tx_lock); return NULL; }
    static sh_transaction_t snapshot;
    snapshot = *slot;
    portEXIT_CRITICAL(&s_tx_lock);
    return &snapshot;
}

uint16_t sh_transaction_latest_seq(void)
{
    portENTER_CRITICAL(&s_tx_lock);
    uint16_t seq = s_next_seq > 1 ? s_next_seq - 1 : 0;
    portEXIT_CRITICAL(&s_tx_lock);
    return seq;
}

const sh_transaction_t *sh_transaction_find_latest_for_device(uint8_t floor, uint8_t cmd, uint8_t index)
{
    static sh_transaction_t snapshot;
    portENTER_CRITICAL(&s_tx_lock);
    const sh_transaction_t *latest = NULL;
    for (int i = 0; i < SH_TX_MAX; i++) {
        sh_transaction_t *tx = &s_tx[i];
        if (tx->seq == 0) continue;
        if (tx->floor_id == floor && tx->cmd_type == cmd && tx->gpio_index == index) {
            if (!latest || tx->seq > latest->seq) {
                latest = tx;
            }
        }
    }
    if (latest) snapshot = *latest;
    portEXIT_CRITICAL(&s_tx_lock);
    return latest ? &snapshot : NULL;
}
