#include "ui_theme.h"
#include "ui_font.h"
#include "esp_log.h"

static const char *TAG = "UI_THEME";

static lv_font_t g_font_title;
static lv_font_t g_font_subtitle;
static lv_font_t g_font_body;
static lv_font_t g_font_caption;
static lv_font_t g_font_value;

static void Font_Init(void)
{
    ui_font_init();

    g_font_title = *ui_font_cn(24);
    g_font_title.fallback = &lv_font_montserrat_14;

    g_font_subtitle = *ui_font_cn(16);
    g_font_subtitle.fallback = &lv_font_montserrat_14;

    g_font_body = *ui_font_cn(16);
    g_font_body.fallback = &lv_font_montserrat_14;

    g_font_caption = *ui_font_cn(14);
    g_font_caption.fallback = &lv_font_montserrat_14;

    g_font_value = *ui_font_cn(20);
    g_font_value.fallback = &lv_font_montserrat_14;

    ESP_LOGI(TAG, "Fonts initialized with Chinese fallback");
}

void UI_Theme_Init(lv_display_t *disp)
{
    Font_Init();

    lv_theme_t *th = lv_theme_default_init(
        disp,
        UI_COLOR_ACCENT,
        UI_COLOR_BLUE,
        false,
        &g_font_body
    );
    lv_disp_set_theme(disp, th);

    ESP_LOGI(TAG, "Cloud White theme initialized");
}
