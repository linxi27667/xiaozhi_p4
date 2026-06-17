#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_styles.h"
#include "ui_events.h"
#include "ui_font.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "smart_home_alarm_ui.h"
#include "mqtt_device_model.h"
#include "ui_asset_service.h"
#include "pages/page_data.h"
#include "pages/page_ctrl.h"
#include "pages/page_light.h"
#include "pages/page_scene.h"
#include "pages/page_env.h"
#include "pages/page_net.h"
#include "pages/page_set.h"
#include "ui_welcome_popup.h"
#include "esp_log.h"
#include <string.h>
#include <stdint.h>

static const char *TAG = "UI_MGR";

#define NAV_COUNT 7

typedef struct {
    lv_obj_t *content_parent;
    lv_obj_t *nav_btns[NAV_COUNT];
    lv_obj_t *nav_icons[NAV_COUNT];
    lv_obj_t *nav_labels[NAV_COUNT];
    ui_page_id_t current_page;
    volatile int switch_pending;
    ui_page_id_t target_page;
} app_ctx_t;

typedef struct {
    const char *icon;
    const char *label_zh;
    const char *label_en;
    ui_page_id_t page;
} nav_item_t;

static const nav_item_t s_nav_items[NAV_COUNT] = {
    { ICON_HOME,     "\xE6\x80\xBB\xE8\xA7\x88", "Home",     UI_PAGE_DATA },  /* 总览 */
    { ICON_CHART,    "\xE6\x8E\xA7\xE5\x88\xB6", "Control",  UI_PAGE_CTRL },  /* 控制 */
    { ICON_LIGHTBULB, "\xE7\x81\xAF\xE5\x85\x89", "Light",   UI_PAGE_LIGHT }, /* 灯光 */
    { ICON_STAR,     "\xE5\x9C\xBA\xE6\x99\xAF", "Scenes",   UI_PAGE_SCENE }, /* 场景 */
    { ICON_LEAF,     "\xE7\x8E\xAF\xE5\xA2\x83", "Env",      UI_PAGE_ENV },   /* 环境 */
    { ICON_WIFI,     "\xE7\xBD\x91\xE7\xBB\x9C", "Network",  UI_PAGE_NET },   /* 网络 */
    { ICON_SETTINGS, "\xE8\xAE\xBE\xE7\xBD\xAE", "Settings", UI_PAGE_SET },   /* 设置 */
};

static app_ctx_t s_app_ctx;
static lv_timer_t *s_poll_timer;

static void reset_all_indev(void)
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        lv_indev_reset(indev, NULL);
        indev = lv_indev_get_next(indev);
    }
}

static int active_nav_index(ui_page_id_t page)
{
    if (page == UI_PAGE_CTRL) return 1;
    if (page == UI_PAGE_LIGHT) return 2;
    if (page == UI_PAGE_SCENE) return 3;
    if (page == UI_PAGE_ENV) return 4;
    if (page == UI_PAGE_NET) return 5;
    if (page == UI_PAGE_SET) return 6;
    return 0;
}

static const char *nav_label(int index)
{
    if (index < 0 || index >= NAV_COUNT) return "";
    return ui_i18n_get_lang() == UI_LANG_ZH ?
        s_nav_items[index].label_zh : s_nav_items[index].label_en;
}

static void refresh_nav_text(void)
{
    for (int i = 0; i < NAV_COUNT; i++) {
        if (s_app_ctx.nav_labels[i]) {
            lv_label_set_text(s_app_ctx.nav_labels[i], nav_label(i));
        }
    }
}

static void refresh_nav_state(void)
{
    refresh_nav_text();
    int active = active_nav_index(s_app_ctx.current_page);
    for (int i = 0; i < NAV_COUNT; i++) {
        bool on = (i == active);
        lv_obj_t *btn = s_app_ctx.nav_btns[i];
        if (!btn) continue;
        lv_obj_set_style_bg_opa(btn, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, on ? 8 : 0, 0);
        lv_obj_set_style_shadow_opa(btn, on ? LV_OPA_10 : LV_OPA_TRANSP, 0);
        if (s_app_ctx.nav_icons[i]) {
            lv_obj_set_style_text_color(s_app_ctx.nav_icons[i],
                on ? UI_COLOR_ACCENT : UI_COLOR_TEXT_SEC, 0);
        }
        if (s_app_ctx.nav_labels[i]) {
            lv_obj_set_style_text_color(s_app_ctx.nav_labels[i],
                on ? UI_COLOR_ACCENT : UI_COLOR_TEXT_SEC, 0);
        }
    }
}

static void on_language_changed(void *user_data)
{
    (void)user_data;
    refresh_nav_text();
    UI_Manager_Rebuild_Current();
}

static void do_switch_page(void)
{
    ui_page_id_t page = s_app_ctx.target_page;
    s_app_ctx.switch_pending = 0;

    if (s_app_ctx.content_parent) {
        reset_all_indev();
        lv_obj_clean(s_app_ctx.content_parent);
        lv_obj_scroll_to(s_app_ctx.content_parent, 0, 0, LV_ANIM_OFF);

        switch (page) {
            case UI_PAGE_DATA: page_data_create(s_app_ctx.content_parent); break;
            case UI_PAGE_CTRL: page_ctrl_create(s_app_ctx.content_parent); break;
            case UI_PAGE_LIGHT: page_light_create(s_app_ctx.content_parent); break;
            case UI_PAGE_SCENE: page_scene_create(s_app_ctx.content_parent); break;
            case UI_PAGE_ENV: page_env_create(s_app_ctx.content_parent); break;
            case UI_PAGE_NET:  page_net_create(s_app_ctx.content_parent); break;
            case UI_PAGE_SET:  page_set_create(s_app_ctx.content_parent); break;
            default: break;
        }
    }
    s_app_ctx.current_page = page;
    refresh_nav_state();
    ui_event_publish(UI_EVENT_PAGE_SWITCHED);
}

static void on_nav_clicked(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= NAV_COUNT) return;
    UI_Manager_Switch_Page(s_nav_items[idx].page);
}

static void poll_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    UI_Manager_Poll();
}

static void create_sidebar(lv_obj_t *scr)
{
    lv_obj_t *side = lv_obj_create(scr);
    lv_obj_remove_style_all(side);
    lv_obj_set_size(side, 72, lv_pct(100));
    lv_obj_set_style_bg_color(side, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(side, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(side, 1, 0);
    lv_obj_set_style_border_side(side, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(side, UI_COLOR_BORDER, 0);
    lv_obj_set_style_pad_top(side, 8, 0);
    lv_obj_set_style_pad_hor(side, 7, 0);
    lv_obj_set_style_pad_row(side, 8, 0);
    lv_obj_set_layout(side, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(side, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(side, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(side, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < NAV_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(side);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, 58, 56);
        lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT_SOFT, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_shadow_color(btn, UI_COLOR_SHADOW, 0);
        lv_obj_set_style_shadow_offset_y(btn, 2, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(btn, 3, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
        lv_obj_add_event_cb(btn, on_nav_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        ui_apply_press_feedback(btn, UI_COLOR_ACCENT);

        s_app_ctx.nav_btns[i] = btn;
        s_app_ctx.nav_icons[i] = ui_create_icon(btn, s_nav_items[i].icon,
            ui_font_icon(20), UI_COLOR_TEXT_SEC);
        s_app_ctx.nav_labels[i] = ui_create_label(btn, nav_label(i),
            ui_font_cn(11), UI_COLOR_TEXT_SEC);
    }
}

static void create_content_area(lv_obj_t *scr)
{
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_remove_style_all(content);
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_height(content, lv_pct(100));
    lv_obj_set_style_bg_color(content, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
    s_app_ctx.content_parent = content;
}

static void create_embedded_content_area(lv_obj_t *scr)
{
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_remove_style_all(content);
    int32_t width = lv_obj_get_width(scr);
    int32_t height = lv_obj_get_height(scr);
    if (width <= 1) width = LV_HOR_RES;
    if (height <= 1) height = LV_VER_RES;
    lv_obj_set_size(content, width, height);
    lv_obj_set_style_bg_color(content, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
    s_app_ctx.content_parent = content;
}

void UI_Manager_Init(lv_obj_t *parent)
{
    memset(&s_app_ctx, 0, sizeof(s_app_ctx));
    s_app_ctx.current_page = UI_PAGE_DATA;

    ui_events_init();
    ui_styles_init();
    ui_i18n_init();
    ui_asset_service_init();
    device_model_init();
    smart_home_alarm_ui_init();
    ui_welcome_popup_init();

    lv_obj_t *scr = parent ? parent : lv_screen_active();
    bool embedded = parent != NULL;
    lv_obj_clean(scr);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_pad_gap(scr, 0, 0);

    if (embedded) {
        lv_obj_set_layout(scr, LV_LAYOUT_NONE);
        create_embedded_content_area(scr);
    } else {
        lv_obj_set_layout(scr, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_ROW);
        create_sidebar(scr);
        create_content_area(scr);
    }
    ui_event_subscribe(UI_EVENT_LANG_CHANGED, on_language_changed, NULL);

    s_app_ctx.switch_pending = 1;
    s_app_ctx.target_page = s_app_ctx.current_page;
    refresh_nav_state();

    /* Timer for deferred page switching */
    if (!s_poll_timer) {
        s_poll_timer = lv_timer_create(poll_timer_cb, 50, NULL);
    }

    ESP_LOGI(TAG, "%s", embedded ?
        "UI initialized in embedded content mode, 7 pages" :
        "UI initialized in standalone sidebar layout, 7 pages");
}

void UI_Manager_Switch_Page(ui_page_id_t page)
{
    if ((int)page < 0 || (int)page >= UI_PAGE_COUNT) return;
    if (s_app_ctx.switch_pending && s_app_ctx.target_page == page) return;
    s_app_ctx.switch_pending = 1;
    s_app_ctx.target_page = page;
}

void UI_Manager_Rebuild_Current(void)
{
    s_app_ctx.switch_pending = 1;
    s_app_ctx.target_page = s_app_ctx.current_page;
}

void UI_Manager_Poll(void)
{
    ui_events_dispatch_pending();
    if (s_app_ctx.switch_pending) {
        do_switch_page();
    }
}
