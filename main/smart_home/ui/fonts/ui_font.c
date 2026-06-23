#include "ui_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

LV_FONT_DECLARE(ui_font_cn_16);
LV_FONT_DECLARE(ui_font_cn_20);
LV_FONT_DECLARE(ui_font_cn_30);
LV_FONT_DECLARE(font_awesome_16_4);
LV_FONT_DECLARE(font_awesome_20_4);
LV_FONT_DECLARE(font_awesome_30_4);

static lv_font_t s_cn_16_fallback;
static lv_font_t s_cn_20_fallback;
static lv_font_t s_cn_30_fallback;
static bool s_cn_fallback_ready = false;

static void init_cn_fallbacks(void)
{
    if (s_cn_fallback_ready) return;

    s_cn_16_fallback = ui_font_cn_16;
    s_cn_20_fallback = ui_font_cn_20;
    s_cn_30_fallback = ui_font_cn_30;
    s_cn_16_fallback.fallback = &lv_font_montserrat_14;
    s_cn_20_fallback.fallback = &lv_font_montserrat_14;
    s_cn_30_fallback.fallback = &lv_font_montserrat_14;
    s_cn_fallback_ready = true;
}

#if LV_USE_TINY_TTF

#include "esp_log.h"

static const char *TAG = "ui_font";

static lv_font_t *s_fa_solid_16 = NULL;
static lv_font_t *s_fa_solid_20 = NULL;
static lv_font_t *s_fa_solid_24 = NULL;

static void *load_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "failed to open %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return NULL; }
    void *data = malloc(len);
    if (!data) { fclose(f); return NULL; }
    fread(data, 1, len, f);
    fclose(f);
    *out_size = len;
    return data;
}

void ui_font_init(void)
{
    init_cn_fallbacks();

    /* Load FontAwesome from SPIFFS */
    size_t fa_size = 0;
    void *fa_data = load_file("/spiffs/fa-solid-900.ttf", &fa_size);

    if (!fa_data) {
        ESP_LOGW(TAG, "FontAwesome TTF not found in SPIFFS");
    } else {
        s_fa_solid_16 = lv_tiny_ttf_create_data(fa_data, fa_size, 16);
        s_fa_solid_20 = lv_tiny_ttf_create_data(fa_data, fa_size, 20);
        s_fa_solid_24 = lv_tiny_ttf_create_data(fa_data, fa_size, 24);
        ESP_LOGI(TAG, "FontAwesome loaded OK");
    }
}

const lv_font_t *ui_font_cn(uint8_t size)
{
    init_cn_fallbacks();
    if (size <= 18) return &s_cn_16_fallback;
    if (size <= 24) return &s_cn_20_fallback;
    return &s_cn_30_fallback;
}

const lv_font_t *ui_font_icon(uint8_t size)
{
    if (size <= 18 && s_fa_solid_16) return s_fa_solid_16;
    if (size <= 22 && s_fa_solid_20) return s_fa_solid_20;
    if (s_fa_solid_24) return s_fa_solid_24;
    if (size <= 18) return &font_awesome_16_4;
    if (size <= 24) return &font_awesome_20_4;
    return &font_awesome_30_4;
}

#else

void ui_font_init(void) { init_cn_fallbacks(); }

const lv_font_t *ui_font_cn(uint8_t size)
{
    init_cn_fallbacks();
    if (size <= 18) return &s_cn_16_fallback;
    if (size <= 24) return &s_cn_20_fallback;
    return &s_cn_30_fallback;
}

const lv_font_t *ui_font_icon(uint8_t size)
{
    if (size <= 18) return &font_awesome_16_4;
    if (size <= 24) return &font_awesome_20_4;
    return &font_awesome_30_4;
}

#endif
