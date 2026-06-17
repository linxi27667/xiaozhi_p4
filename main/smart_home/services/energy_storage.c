#include "energy_storage.h"
#include "mqtt_device_model.h"

#include <nvs.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "ENERGY_STORE";
static const char *NS = "energy";
static bool s_save_pending;

static const char *device_key_prefix(const char *id)
{
    if (strcmp(id, "floor1_gate") == 0) return "f1g";
    if (strcmp(id, "floor1_hall_light") == 0) return "f1h";
    if (strcmp(id, "floor2_master_light") == 0) return "f2m";
    if (strcmp(id, "floor2_master_ambient") == 0) return "f2m";
    if (strcmp(id, "floor2_living_light") == 0) return "f2l";
    if (strcmp(id, "floor2_living_ambient") == 0) return "f2l";
    if (strcmp(id, "floor2_toilet_light") == 0) return "f2t";
    if (strcmp(id, "floor2_fan") == 0) return "f2f";
    if (strcmp(id, "floor2_hanger") == 0) return "f2r";
    if (strcmp(id, "floor3_balcony_light") == 0) return "f3b";
    if (strcmp(id, "floor3_left_skylight") == 0) return "f3ls";
    if (strcmp(id, "floor3_right_skylight") == 0) return "f3rs";
    if (strcmp(id, "floor3_hanger") == 0) return "f3r";
    return NULL;
}

static void make_key(char *out, size_t out_size, const char *prefix, const char *suffix)
{
    snprintf(out, out_size, "%s_%s", prefix, suffix);
}

static uint32_t get_u32(nvs_handle_t h, const char *key, uint32_t def)
{
    uint32_t value = def;
    nvs_get_u32(h, key, &value);
    return value;
}

static void set_u32(nvs_handle_t h, const char *key, uint32_t value)
{
    nvs_set_u32(h, key, value);
}

bool energy_storage_load(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    uint32_t tariff = get_u32(h, "tariff", 60);
    device_model_energy_set_tariff((uint16_t)tariff);

    for (uint16_t i = 0; i < device_model_count(); i++) {
        rc_device_t *d = device_model_find_mutable_by_id(device_model_at(i)->id);
        if (!d) continue;
        const char *prefix = device_key_prefix(d->id);
        if (!prefix) continue;

        char key[16];
        make_key(key, sizeof(key), prefix, "pw");
        d->energy.rated_power_w = (uint16_t)get_u32(h, key, d->energy.rated_power_w);
        make_key(key, sizeof(key), prefix, "rt");
        d->energy.runtime_sec = get_u32(h, key, 0);
        make_key(key, sizeof(key), prefix, "trt");
        d->energy.today_runtime_sec = get_u32(h, key, 0);
        make_key(key, sizeof(key), prefix, "wh");
        d->energy.energy_wh_x100 = get_u32(h, key, 0);
        make_key(key, sizeof(key), prefix, "twh");
        d->energy.today_energy_wh_x100 = get_u32(h, key, 0);
        make_key(key, sizeof(key), prefix, "ct");
        d->energy.cost_cent = get_u32(h, key, 0);
        make_key(key, sizeof(key), prefix, "tct");
        d->energy.today_cost_cent = get_u32(h, key, 0);
    }

    nvs_close(h);
    return true;
}

bool energy_storage_save(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return false;

    set_u32(h, "tariff", device_model_energy_get_tariff());

    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t *src = device_model_at(i);
        const char *prefix = device_key_prefix(src->id);
        if (!prefix) continue;

        char key[16];
        make_key(key, sizeof(key), prefix, "pw");
        set_u32(h, key, src->energy.rated_power_w);
        make_key(key, sizeof(key), prefix, "rt");
        set_u32(h, key, src->energy.runtime_sec);
        make_key(key, sizeof(key), prefix, "trt");
        set_u32(h, key, src->energy.today_runtime_sec);
        make_key(key, sizeof(key), prefix, "wh");
        set_u32(h, key, src->energy.energy_wh_x100);
        make_key(key, sizeof(key), prefix, "twh");
        set_u32(h, key, src->energy.today_energy_wh_x100);
        make_key(key, sizeof(key), prefix, "ct");
        set_u32(h, key, src->energy.cost_cent);
        make_key(key, sizeof(key), prefix, "tct");
        set_u32(h, key, src->energy.today_cost_cent);
    }

    err = nvs_commit(h);
    nvs_close(h);
    s_save_pending = false;
    return err == ESP_OK;
}

void energy_storage_schedule_save(void)
{
    s_save_pending = true;
}

bool energy_storage_is_save_pending(void)
{
    return s_save_pending;
}
