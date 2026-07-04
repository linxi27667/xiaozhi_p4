#include "smart_home_alarm_ui.h"

#include "mqtt_device_model.h"
#include "ui_asset_service.h"
#include "ui_events.h"
#include "ui_font.h"
#include "ui_icons.h"
#include "ui_styles.h"
#include "ui_theme.h"

#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "ALARM_UI";

static lv_obj_t *s_modal;
static uint8_t s_fire_floor_id;
static bool s_fire_active;
static bool s_fire_acknowledged;
static bool s_initialized;
static lv_timer_t *s_poll_timer;
static lv_timer_t *s_flash_timer;
static uint8_t s_flash_phase;

static const char *floor_name(uint8_t floor_id)
{
    switch (floor_id) {
        case 1: return "\xE4\xB8\x80\xE6\xA5\xBC"; /* 一楼 */
        case 2: return "\xE4\xBA\x8C\xE6\xA5\xBC"; /* 二楼 */
        case 3: return "\xE4\xB8\x89\xE6\xA5\xBC"; /* 三楼 */
        default: return "\xE6\x9C\xAA\xE7\x9F\xA5\xE6\xA5\xBC\xE5\xB1\x82"; /* 未知楼层 */
    }
}

static void close_modal(void)
{
    if (s_flash_timer) {
        lv_timer_delete(s_flash_timer);
        s_flash_timer = NULL;
    }
    s_flash_phase = 0;
    if (s_modal) {
        /* Use async close to safely release msgbox + backdrop from event callbacks */
        lv_msgbox_close_async(s_modal);
        s_modal = NULL;
    }
}

static void fire_flash_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_modal) {
        return;
    }

    s_flash_phase ^= 1;
    lv_obj_set_style_border_width(s_modal, s_flash_phase ? 5 : 3, 0);
    lv_obj_set_style_shadow_opa(s_modal, s_flash_phase ? LV_OPA_70 : LV_OPA_30, 0);
    lv_obj_set_style_shadow_width(s_modal, s_flash_phase ? 36 : 24, 0);
    lv_obj_set_style_bg_opa(s_modal, s_flash_phase ? LV_OPA_COVER : LV_OPA_90, 0);
    lv_obj_invalidate(s_modal);
}

static void start_fire_flash(void)
{
    if (!s_flash_timer) {
        s_flash_timer = lv_timer_create(fire_flash_timer_cb, 240, NULL);
    }
    fire_flash_timer_cb(s_flash_timer);
}

static void on_alarm_ack(lv_event_t *e)
{
    (void)e;
    s_fire_acknowledged = true;
    close_modal();
}

static void show_fire_modal(void)
{
    if (!s_fire_active || s_fire_acknowledged) {
        close_modal();
        return;
    }

    if (s_modal) {
        return;  /* already showing */
    }

    /* Create modal msgbox on active screen (parent=NULL => modal with backdrop) */
    s_modal = lv_msgbox_create(NULL);
    lv_obj_set_size(s_modal, 560, 318);
    lv_obj_center(s_modal);

    /* Title: 火灾警报 */
    /* UTF-8: 火灾警报 = E781ABE781BEE8ADA6E68AA5 */
    lv_msgbox_add_title(s_modal, "\xE7\x81\xAB\xE7\x81\xBE\xE8\xAD\xA6\xE6\x8A\xA5");

    /* Add flame icon to header, move before title for visual hierarchy.
     * 优先加载 TF 卡中的 alarm_siren.png,缺失时回退到 Font Awesome ICON_FIRE 图标。 */
    lv_obj_t *header = lv_msgbox_get_header(s_modal);
    if (header) {
        lv_obj_t *icon = ui_asset_image_create(header, "alarm_siren.png");
        if (!icon) {
            icon = ui_create_icon(header, ICON_FIRE, ui_font_icon(36), lv_color_white());
        }
        if (icon) {
            lv_obj_move_to_index(icon, 0);
        }
    }

    /* Body text: 检测到%s发生火灾风险，请立即检查火焰传感器和现场设备。 */
    char body[160];
    lv_snprintf(body, sizeof(body),
        "检测到%s发生火灾风险，请立即检查火焰传感器和现场设备。",
        floor_name(s_fire_floor_id));
    lv_obj_t *body_label = lv_msgbox_add_text(s_modal, body);

    /* Footer button: 紧急处理 */
    /* UTF-8: 紧急处理 = E7B4A7E680A5E5A484E79086 */
    lv_obj_t *btn = lv_msgbox_add_footer_button(s_modal, "\xE7\xB4\xA7\xE6\x80\xA5\xE5\xA4\x84\xE7\x90\x86");
    if (btn) {
        lv_obj_add_event_cb(btn, on_alarm_ack, LV_EVENT_CLICKED, NULL);
    }

    /* Style: striking red background */
    lv_color_t red = lv_color_hex(0xC62828);
    lv_color_t dark_red = lv_color_hex(0xB71C1C);

    lv_obj_set_style_bg_color(s_modal, red, 0);
    lv_obj_set_style_bg_opa(s_modal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_modal, dark_red, 0);
    lv_obj_set_style_border_width(s_modal, 3, 0);
    lv_obj_set_style_radius(s_modal, 12, 0);
    lv_obj_set_style_pad_all(s_modal, 28, 0);
    lv_obj_set_style_shadow_color(s_modal, lv_color_hex(0xFF3B30), 0);
    lv_obj_set_style_shadow_width(s_modal, 24, 0);
    lv_obj_set_style_shadow_spread(s_modal, 4, 0);
    lv_obj_set_style_shadow_opa(s_modal, LV_OPA_30, 0);

    /* Style header: transparent bg, white title */
    if (header) {
        lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_column(header, 8, 0);
        lv_obj_t *title_label = lv_msgbox_get_title(s_modal);
        if (title_label) {
            lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
            lv_obj_set_style_text_font(title_label, ui_font_cn(32), 0);
        }
    }

    /* Style content: transparent bg, white text */
    lv_obj_t *content = lv_msgbox_get_content(s_modal);
    if (content) {
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_ver(content, 16, 0);
        uint32_t cnt = lv_obj_get_child_count(content);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_obj_t *child = lv_obj_get_child(content, i);
            lv_obj_set_style_text_color(child, lv_color_white(), 0);
            lv_obj_set_style_text_font(child, ui_font_cn(19), 0);
            lv_obj_set_style_text_line_space(child, 8, 0);
        }
    }
    if (body_label) {
        lv_obj_set_width(body_label, 500);
        lv_label_set_long_mode(body_label, LV_LABEL_LONG_WRAP);
    }

    /* Style footer: transparent bg, white button with red text */
    lv_obj_t *footer = lv_msgbox_get_footer(s_modal);
    if (footer) {
        lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
        uint32_t cnt = lv_obj_get_child_count(footer);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_obj_t *child = lv_obj_get_child(footer, i);
            lv_obj_set_style_bg_color(child, lv_color_white(), 0);
            lv_obj_set_style_bg_opa(child, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(child, red, 0);
            lv_obj_set_style_text_font(child, ui_font_cn(18), 0);
            lv_obj_set_style_radius(child, 6, 0);
            lv_obj_set_style_pad_hor(child, 34, 0);
            lv_obj_set_style_pad_ver(child, 14, 0);
        }
    }

    start_fire_flash();
    ESP_LOGW(TAG, "Fire modal shown for floor %d", s_fire_floor_id);
}

static void on_fire_alarm_event(void *user_data)
{
    (void)user_data;
    show_fire_modal();
}

/* ---- Polling: check device model for fire status every 500ms ---- */
static void poll_fire_status(lv_timer_t *timer)
{
    (void)timer;
    if (s_fire_acknowledged) {
        /* Re-arm when fire clears */
        const mqtt_device_model_t *m = device_model_get();
        bool any_fire = false;
        for (int i = 0; i < 3; i++) {
            if (m->fire_floor_valid[i] && m->fire_floor_status[i] >= 2) {
                any_fire = true;
                break;
            }
        }
        if (!any_fire) {
            s_fire_acknowledged = false;
            s_fire_active = false;
        }
        return;
    }

    /* Check all floors for fire status >= 2 (alert) */
    const mqtt_device_model_t *m = device_model_get();
    for (int i = 0; i < 3; i++) {
        if (m->fire_floor_valid[i] && m->fire_floor_status[i] >= 2) {
            if (!s_fire_active || s_fire_floor_id != (uint8_t)(i + 1)) {
                /* Close existing modal first so text refreshes with new floor */
                if (s_fire_active) {
                    close_modal();
                }
                s_fire_floor_id = (uint8_t)(i + 1);
                s_fire_active = true;
                ESP_LOGW(TAG, "Fire detected on floor %d, showing global modal", s_fire_floor_id);
                show_fire_modal();
            }
            return;
        }
    }

    /* No fire detected — clear state and close modal if open */
    if (s_fire_active) {
        s_fire_active = false;
        close_modal();
        ESP_LOGI(TAG, "Fire cleared, modal closed");
    }
}

void smart_home_alarm_ui_init(void)
{
    if (s_initialized) {
        return;
    }
    s_initialized = true;
    ui_event_subscribe(UI_EVENT_FIRE_ALARM, on_fire_alarm_event, NULL);

    /* Start polling timer: checks device model every 500ms (<=500ms requirement) */
    s_poll_timer = lv_timer_create(poll_fire_status, 500, NULL);
    ESP_LOGI(TAG, "Fire alarm UI initialized with 500ms polling");
}

void smart_home_alarm_ui_set_fire(uint8_t floor_id, bool active)
{
    s_fire_floor_id = floor_id;
    if (!active) {
        s_fire_active = false;
        s_fire_acknowledged = false;
        ui_event_publish(UI_EVENT_FIRE_ALARM);
        return;
    }

    s_fire_active = true;
    ui_event_publish(UI_EVENT_FIRE_ALARM);
}
