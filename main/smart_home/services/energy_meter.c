#include "energy_meter.h"
#include "energy_storage.h"
#include "mqtt_device_model.h"
#include "ui_events.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "ENERGY";
static const uint16_t DEFAULT_TARIFF_CENT_PER_KWH = 60;
static int64_t s_last_save_ms = 0;

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static uint32_t calc_wh_x100(uint16_t power_w, uint32_t delta_sec)
{
    uint64_t v = (uint64_t)power_w * delta_sec * 100ULL;
    return (uint32_t)(v / 3600ULL);
}

static uint32_t calc_cost_cent(uint32_t wh_x100, uint16_t cent_per_kwh)
{
    uint64_t v = (uint64_t)wh_x100 * cent_per_kwh;
    return (uint32_t)(v / 100000ULL);
}

static void settle_device_until_now(rc_device_t *device)
{
    if (!device || !device->energy.enabled || device->energy.on_since_ms <= 0) {
        return;
    }

    int64_t n = now_ms();
    if (n <= device->energy.on_since_ms) {
        return;
    }

    uint32_t delta_sec = (uint32_t)((n - device->energy.on_since_ms) / 1000);
    if (delta_sec == 0) {
        return;
    }

    uint16_t power_w = device->energy.rated_power_w;
    uint16_t tariff = device_model_energy_get_tariff();
    uint32_t wh = calc_wh_x100(power_w, delta_sec);
    uint32_t cost = calc_cost_cent(wh, tariff);

    device->energy.runtime_sec += delta_sec;
    device->energy.today_runtime_sec += delta_sec;
    device->energy.energy_wh_x100 += wh;
    device->energy.today_energy_wh_x100 += wh;
    device->energy.cost_cent += cost;
    device->energy.today_cost_cent += cost;
    device->energy.on_since_ms = n;
}

void energy_meter_init(void)
{
    if (device_model_energy_get_tariff() == 0) {
        device_model_energy_set_tariff(DEFAULT_TARIFF_CENT_PER_KWH);
    }
    energy_storage_load();
    device_model_energy_recalculate_summary();
    device_model_energy_mark_dirty();
    ESP_LOGI(TAG, "Energy meter initialized, tariff=%u cent/kWh",
             device_model_energy_get_tariff());
}

void energy_meter_on_power_changed(const char *device_id, bool old_on, bool new_on)
{
    rc_device_t *device = device_model_find_mutable_by_id(device_id);
    if (!device || !device->energy.enabled || old_on == new_on) {
        return;
    }

    if (new_on) {
        device->energy.on_since_ms = now_ms();
        device->energy.live_power_w = device->energy.rated_power_w;
    } else {
        settle_device_until_now(device);
        device->energy.on_since_ms = 0;
        device->energy.live_power_w = 0;
        energy_storage_schedule_save();
    }

    device_model_energy_recalculate_summary();
    device_model_energy_mark_dirty();
}

void energy_meter_tick(void)
{
    for (uint16_t i = 0; i < device_model_count(); i++) {
        rc_device_t *d = device_model_find_mutable_by_id(device_model_at(i)->id);
        if (!d) continue;
        if (d->connected && d->power_on && d->energy.on_since_ms > 0) {
            settle_device_until_now(d);
        }
    }

    device_model_energy_recalculate_summary();
    device_model_energy_mark_dirty();

    int64_t n = now_ms();
    if (energy_storage_is_save_pending() && n - s_last_save_ms > 30000) {
        s_last_save_ms = n;
        energy_storage_save();
    }
}

void energy_meter_set_tariff(uint16_t cent_per_kwh)
{
    device_model_energy_set_tariff(cent_per_kwh);
    device_model_energy_recalculate_summary();
    device_model_energy_mark_dirty();
    energy_storage_save();
}

uint16_t energy_meter_get_tariff(void)
{
    return device_model_energy_get_tariff();
}

bool energy_meter_set_device_rated_power(const char *device_id, uint16_t power_w)
{
    bool ok = device_model_energy_set_rated_power(device_id, power_w);
    if (ok) {
        rc_device_t *d = device_model_find_mutable_by_id(device_id);
        if (d) {
            d->energy.live_power_w = (d->connected && d->power_on && d->energy.enabled) ? power_w : 0;
        }
        device_model_energy_recalculate_summary();
        device_model_energy_mark_dirty();
        energy_storage_save();
    }
    return ok;
}

void energy_meter_get_summary(energy_summary_t *out)
{
    if (!out) return;
    const mqtt_device_model_t *m = device_model_get();
    out->total_wh_x100 = m->energy_total_wh_x100;
    out->today_wh_x100 = m->energy_today_wh_x100;
    out->total_cost_cent = m->energy_total_cost_cent;
    out->today_cost_cent = m->energy_today_cost_cent;
    out->live_power_w = m->energy_live_power_w;
    out->tariff_cent_per_kwh = m->energy_tariff_cent_per_kwh;
}

void energy_meter_reset_today(void)
{
    for (uint16_t i = 0; i < device_model_count(); i++) {
        rc_device_t *d = device_model_find_mutable_by_id(device_model_at(i)->id);
        if (!d) continue;
        d->energy.today_runtime_sec = 0;
        d->energy.today_energy_wh_x100 = 0;
        d->energy.today_cost_cent = 0;
    }
    device_model_energy_recalculate_summary();
    device_model_energy_mark_dirty();
    energy_storage_save();
}

void energy_meter_reset_all(void)
{
    for (uint16_t i = 0; i < device_model_count(); i++) {
        rc_device_t *d = device_model_find_mutable_by_id(device_model_at(i)->id);
        if (!d) continue;
        d->energy.runtime_sec = 0;
        d->energy.today_runtime_sec = 0;
        d->energy.energy_wh_x100 = 0;
        d->energy.today_energy_wh_x100 = 0;
        d->energy.cost_cent = 0;
        d->energy.today_cost_cent = 0;
        d->energy.on_since_ms = 0;
        d->energy.live_power_w = 0;
    }
    device_model_energy_recalculate_summary();
    device_model_energy_mark_dirty();
    energy_storage_save();
}

void energy_meter_format_money(uint32_t cent, char *buf, size_t buf_size)
{
    snprintf(buf, buf_size, "\xC2\xA5%lu.%02lu",
             (unsigned long)(cent / 100),
             (unsigned long)(cent % 100));
}

void energy_meter_format_energy(uint32_t wh_x100, char *buf, size_t buf_size)
{
    uint32_t kwh_x1000 = wh_x100 / 100;
    snprintf(buf, buf_size, "%lu.%03lu kWh",
             (unsigned long)(kwh_x1000 / 1000),
             (unsigned long)(kwh_x1000 % 1000));
}

void energy_meter_format_runtime(uint32_t sec, char *buf, size_t buf_size)
{
    uint32_t h = sec / 3600;
    uint32_t m = (sec % 3600) / 60;
    uint32_t s = sec % 60;
    if (h > 0) snprintf(buf, buf_size, "%luh%02lum", (unsigned long)h, (unsigned long)m);
    else if (m > 0) snprintf(buf, buf_size, "%lum%02lus", (unsigned long)m, (unsigned long)s);
    else snprintf(buf, buf_size, "%lus", (unsigned long)s);
}
