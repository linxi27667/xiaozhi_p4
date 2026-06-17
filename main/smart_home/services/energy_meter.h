#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    ENERGY_SOURCE_ESTIMATED = 0,
    ENERGY_SOURCE_MEASURED = 1,
} energy_source_t;

typedef struct {
    uint16_t rated_power_w;
    uint16_t live_power_w;
    uint32_t runtime_sec;
    uint32_t today_runtime_sec;
    uint32_t energy_wh_x100;
    uint32_t today_energy_wh_x100;
    uint32_t cost_cent;
    uint32_t today_cost_cent;
    int64_t on_since_ms;
    energy_source_t source;
    bool enabled;
} rc_energy_t;

typedef struct {
    uint32_t total_wh_x100;
    uint32_t today_wh_x100;
    uint32_t total_cost_cent;
    uint32_t today_cost_cent;
    uint16_t live_power_w;
    uint16_t tariff_cent_per_kwh;
} energy_summary_t;

void energy_meter_init(void);
void energy_meter_on_power_changed(const char *device_id, bool old_on, bool new_on);
void energy_meter_tick(void);
void energy_meter_set_tariff(uint16_t cent_per_kwh);
uint16_t energy_meter_get_tariff(void);
bool energy_meter_set_device_rated_power(const char *device_id, uint16_t power_w);
void energy_meter_get_summary(energy_summary_t *out);
void energy_meter_reset_today(void);
void energy_meter_reset_all(void);
void energy_meter_format_money(uint32_t cent, char *buf, size_t buf_size);
void energy_meter_format_energy(uint32_t wh_x100, char *buf, size_t buf_size);
void energy_meter_format_runtime(uint32_t sec, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif
