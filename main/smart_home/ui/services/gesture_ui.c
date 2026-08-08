#include "sdkconfig.h"
#include "gesture_ui.h"

#ifdef CONFIG_IDF_TARGET_ESP32P4

#include "gesture_mode.h"
#include "ui_font.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "ui_styles.h"
#include "ui_theme.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define PREVIEW_SCALE 2
#define PREVIEW_DISPLAY_W (GESTURE_PREVIEW_WIDTH * PREVIEW_SCALE)
#define PREVIEW_DISPLAY_H (GESTURE_PREVIEW_HEIGHT * PREVIEW_SCALE)

static const char *TAG = "GESTURE_UI";
static lv_obj_t *s_overlay;
static lv_obj_t *s_canvas;
static lv_obj_t *s_detect_box;
static lv_obj_t *s_preview_result_label;
static lv_obj_t *s_state_label;
static lv_obj_t *s_gesture_label;
static lv_obj_t *s_progress_label;
static lv_obj_t *s_timeout_label;
static lv_obj_t *s_action_label;
static lv_timer_t *s_timer;
static uint8_t *s_canvas_buffer;
static uint32_t s_preview_sequence;

static char s_state_text[96];
static char s_gesture_text[64];
static char s_preview_result_text[64];
static char s_progress_text[64];
static char s_timeout_text[48];
static char s_action_text[72];

static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, uint8_t size,
                              lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text != NULL ? text : "");
    lv_obj_set_style_text_font(label, ui_font_cn(size), 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

static void update_static_label(lv_obj_t *label, char *storage, size_t storage_size,
                                const char *text)
{
    if (label == NULL || storage == NULL || storage_size == 0 || text == NULL) return;
    if (strncmp(storage, text, storage_size) == 0) return;
    lv_obj_invalidate(label);
    snprintf(storage, storage_size, "%s", text);
    lv_label_set_text_static(label, storage);
    lv_obj_invalidate(label);
}

static const char *gesture_display_name(const char *name)
{
    if (name == NULL || name[0] == '\0' || strcmp(name, "no_hand") == 0) {
        return tr("等待", "WAITING");
    }
    if (strcmp(name, "ok") == 0) return tr("确认", "OK");
    if (strcmp(name, "five") == 0) return tr("全开", "OPEN PALM");
    if (strcmp(name, "no_gesture") == 0) return tr("握拳", "FIST");
    if (strcmp(name, "one") == 0) return tr("数字一", "ONE");
    if (strcmp(name, "two") == 0) return tr("数字2", "TWO");
    if (strcmp(name, "three") == 0) return tr("数字3", "THREE");
    if (strcmp(name, "four") == 0) return tr("数字4", "FOUR");
    if (strcmp(name, "like") == 0) return tr("点赞", "LIKE");
    if (strcmp(name, "call") == 0) return tr("呼唤", "CALL");
    if (strcmp(name, "dislike") == 0) return tr("倒赞", "DISLIKE");
    return tr("无效", name);
}

static bool is_actionable_gesture(const char *name)
{
    return name != NULL &&
           (strcmp(name, "no_gesture") == 0 || strcmp(name, "ok") == 0 ||
            strcmp(name, "five") == 0 || strcmp(name, "two") == 0 ||
            strcmp(name, "three") == 0 || strcmp(name, "four") == 0 ||
            strcmp(name, "one") == 0 || strcmp(name, "like") == 0 ||
            strcmp(name, "dislike") == 0);
}

static float gesture_action_threshold(const char *name)
{
    if (name == NULL) return 1.0f;
    if (strcmp(name, "no_gesture") == 0) return 0.90f;
    if (strcmp(name, "two") == 0 || strcmp(name, "three") == 0 ||
        strcmp(name, "one") == 0 || strcmp(name, "like") == 0 ||
        strcmp(name, "dislike") == 0) return 0.90f;
    return 0.85f;
}

static void on_exit_clicked(lv_event_t *event)
{
    (void)event;
    ESP_LOGI(TAG, "Exit button clicked");
    if (s_overlay != NULL) {
        /* Immediate visual acknowledgement; actual deletion stays queued on
           the normal LVGL/application path and is safe to call repeatedly. */
        lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    gesture_mode_stop(GESTURE_STOP_USER);
}

static void update_detection_box(const gesture_mode_snapshot_t *snapshot)
{
    if (s_detect_box == NULL || snapshot == NULL ||
        snapshot->box_width <= 0 || snapshot->box_height <= 0) {
        if (s_detect_box != NULL) lv_obj_add_flag(s_detect_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    int mirrored_x = GESTURE_PREVIEW_WIDTH -
                     (snapshot->box_x + snapshot->box_width);
    if (mirrored_x < 0) mirrored_x = 0;
    int x = mirrored_x * PREVIEW_SCALE;
    int y = snapshot->box_y * PREVIEW_SCALE;
    int width = snapshot->box_width * PREVIEW_SCALE;
    int height = snapshot->box_height * PREVIEW_SCALE;
    if (x + width > PREVIEW_DISPLAY_W) width = PREVIEW_DISPLAY_W - x;
    if (y + height > PREVIEW_DISPLAY_H) height = PREVIEW_DISPLAY_H - y;
    if (width <= 0 || height <= 0) {
        lv_obj_add_flag(s_detect_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_set_pos(s_detect_box, x, y);
    lv_obj_set_size(s_detect_box, width, height);
    lv_obj_clear_flag(s_detect_box, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_ui(lv_timer_t *timer)
{
    (void)timer;
    gesture_mode_snapshot_t snapshot = {0};
    if (!gesture_mode_get_snapshot(&snapshot)) return;

    if (!snapshot.active && snapshot.state == GESTURE_STATE_STOPPED) {
        gesture_ui_hide();
        return;
    }

    /* Shell page switches move their containers to the foreground. Keep this
       modal overlay above every normal page while gesture mode is active. */
    if (s_overlay != NULL) lv_obj_move_foreground(s_overlay);

    if (s_canvas_buffer != NULL &&
        gesture_mode_copy_preview(s_canvas_buffer, GESTURE_PREVIEW_BYTES,
                                  &s_preview_sequence)) {
        lv_obj_invalidate(s_canvas);
    }
    update_detection_box(&snapshot);

    update_static_label(s_state_label, s_state_text, sizeof(s_state_text),
                        snapshot.status[0] != '\0' ? snapshot.status :
                        tr("手控模式运行中", "Gesture mode running"));

    char text[96];
    const char *display_name = gesture_display_name(snapshot.gesture);
    const bool waiting = snapshot.gesture[0] == '\0' ||
                         strcmp(snapshot.gesture, "no_hand") == 0;
    if (waiting) {
        snprintf(text, sizeof(text), "%s", display_name);
    } else {
        snprintf(text, sizeof(text), "%s  %.0f%%",
                 display_name, snapshot.confidence * 100.0f);
    }
    update_static_label(s_preview_result_label, s_preview_result_text,
                        sizeof(s_preview_result_text), text);

    const bool actionable = is_actionable_gesture(snapshot.gesture) &&
                            snapshot.confidence >=
                                gesture_action_threshold(snapshot.gesture);
    lv_color_t result_color = waiting
                                  ? lv_color_hex(0xB8C7DA)
                                  : (actionable ? UI_COLOR_GREEN : UI_COLOR_ORANGE);
    lv_obj_set_style_text_color(s_preview_result_label, result_color, 0);
    lv_obj_set_style_border_color(s_preview_result_label, result_color, 0);

    snprintf(text, sizeof(text), "%s: %s  %.0f%%",
             tr("当前", "Current"), display_name,
             snapshot.confidence * 100.0f);
    update_static_label(s_gesture_label, s_gesture_text, sizeof(s_gesture_text), text);

    const unsigned confirmation_required =
        snapshot.confirmation_required > 0 ? snapshot.confirmation_required : 3;
    snprintf(text, sizeof(text), "%s: %u / %u",
             tr("确认进度", "Confirm"),
             (unsigned)snapshot.confirmation_count, confirmation_required);
    update_static_label(s_progress_label, s_progress_text, sizeof(s_progress_text), text);

    snprintf(text, sizeof(text), "%s",
             tr("返回 点击右上角", "Exit: tap the top-right button"));
    update_static_label(s_timeout_label, s_timeout_text, sizeof(s_timeout_text), text);

    snprintf(text, sizeof(text), "%s: %s",
             tr("场景", "Last action"),
             snapshot.last_action[0] != '\0' ? snapshot.last_action : tr("暂无", "None"));
    update_static_label(s_action_label, s_action_text, sizeof(s_action_text), text);

    lv_color_t state_color = UI_COLOR_GREEN;
    if (snapshot.state == GESTURE_STATE_PAUSED ||
        snapshot.state == GESTURE_STATE_LOADING ||
        snapshot.state == GESTURE_STATE_STOPPING) {
        state_color = UI_COLOR_ORANGE;
    } else if (snapshot.state == GESTURE_STATE_ERROR) {
        state_color = UI_COLOR_RED;
    }
    lv_obj_set_style_text_color(s_state_label, state_color, 0);
}

void gesture_ui_show(void)
{
    if (s_overlay != NULL) return;

    lv_obj_t *top_layer = lv_layer_top();
    if (top_layer == NULL) {
        ESP_LOGE(TAG, "No LVGL top layer");
        gesture_mode_stop(GESTURE_STOP_ERROR);
        return;
    }

    s_canvas_buffer = (uint8_t *)heap_caps_aligned_alloc(
        64, GESTURE_PREVIEW_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_canvas_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate preview canvas");
        gesture_mode_stop(GESTURE_STOP_ERROR);
        return;
    }
    memset(s_canvas_buffer, 0, GESTURE_PREVIEW_BYTES);
    s_preview_sequence = 0;
    memset(s_state_text, 0, sizeof(s_state_text));
    memset(s_gesture_text, 0, sizeof(s_gesture_text));
    memset(s_preview_result_text, 0, sizeof(s_preview_result_text));
    memset(s_progress_text, 0, sizeof(s_progress_text));
    memset(s_timeout_text, 0, sizeof(s_timeout_text));
    memset(s_action_text, 0, sizeof(s_action_text));

    s_overlay = lv_obj_create(top_layer);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x07111F), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);

    lv_obj_t *title = create_label(s_overlay, tr("手控模式", "Gesture Control"),
                                   30, lv_color_white());
    lv_obj_set_pos(title, 28, 18);

    lv_obj_t *subtitle = create_label(
        s_overlay,
        tr("握拳=离家 / 全开=回家 / 确认=观影\n数字2=开门 / 数字3=关门 / 数字4=明亮",
           "FIST=Away / PALM=Home / OK=Movie\n2=Open / 3=Close / 4=Bright"),
        16, lv_color_hex(0x9FB3CC));
    lv_obj_set_pos(subtitle, 28, 57);

    lv_obj_t *exit_button = lv_btn_create(s_overlay);
    lv_obj_remove_style_all(exit_button);
    lv_obj_set_size(exit_button, 112, 46);
    lv_obj_align(exit_button, LV_ALIGN_TOP_RIGHT, -24, 20);
    lv_obj_set_style_radius(exit_button, 12, 0);
    lv_obj_set_style_bg_color(exit_button, lv_color_hex(0x24364D), 0);
    lv_obj_set_style_bg_opa(exit_button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(exit_button, 1, 0);
    lv_obj_set_style_border_color(exit_button, lv_color_hex(0x526C8D), 0);
    lv_obj_add_event_cb(exit_button, on_exit_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(exit_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *exit_label = create_label(exit_button, tr("返回", "Exit"), 20, lv_color_white());
    lv_obj_center(exit_label);

    lv_obj_t *preview = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(preview);
    lv_obj_set_size(preview, PREVIEW_DISPLAY_W, PREVIEW_DISPLAY_H);
    lv_obj_set_pos(preview, 28, 102);
    lv_obj_set_style_bg_color(preview, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(preview, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(preview, 2, 0);
    lv_obj_set_style_border_color(preview, lv_color_hex(0x2F6BFF), 0);
    lv_obj_set_style_radius(preview, 12, 0);
    lv_obj_set_style_clip_corner(preview, true, 0);
    lv_obj_clear_flag(preview, LV_OBJ_FLAG_SCROLLABLE);

    s_canvas = lv_canvas_create(preview);
    lv_canvas_set_buffer(s_canvas, s_canvas_buffer,
                         GESTURE_PREVIEW_WIDTH, GESTURE_PREVIEW_HEIGHT,
                         LV_COLOR_FORMAT_RGB565);
    lv_canvas_fill_bg(s_canvas, lv_color_black(), LV_OPA_COVER);
    lv_image_set_pivot(s_canvas, 0, 0);
    lv_image_set_scale(s_canvas, 256 * PREVIEW_SCALE);
    lv_obj_set_pos(s_canvas, 0, 0);

    s_detect_box = lv_obj_create(preview);
    lv_obj_remove_style_all(s_detect_box);
    lv_obj_set_style_bg_opa(s_detect_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_detect_box, 3, 0);
    lv_obj_set_style_border_color(s_detect_box, UI_COLOR_GREEN, 0);
    lv_obj_set_style_radius(s_detect_box, 4, 0);
    lv_obj_add_flag(s_detect_box, LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_detect_box, LV_OBJ_FLAG_SCROLLABLE);

    s_preview_result_label = create_label(preview, tr("等待", "WAITING"),
                                          30, lv_color_white());
    lv_obj_set_style_bg_color(s_preview_result_label, lv_color_hex(0x07111F), 0);
    lv_obj_set_style_bg_opa(s_preview_result_label, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_preview_result_label, 2, 0);
    lv_obj_set_style_border_color(s_preview_result_label, lv_color_hex(0xB8C7DA), 0);
    lv_obj_set_style_radius(s_preview_result_label, 12, 0);
    lv_obj_set_style_pad_hor(s_preview_result_label, 18, 0);
    lv_obj_set_style_pad_ver(s_preview_result_label, 8, 0);
    lv_obj_align(s_preview_result_label, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_flag(s_preview_result_label, LV_OBJ_FLAG_FLOATING);

    lv_obj_t *panel = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 312, 430);
    lv_obj_set_pos(panel, 684, 102);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x111F31), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x2C425F), 0);
    lv_obj_set_style_radius(panel, 14, 0);
    lv_obj_set_style_pad_all(panel, 22, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    s_state_label = create_label(panel, "", 20, UI_COLOR_GREEN);
    lv_obj_set_width(s_state_label, 268);
    lv_label_set_long_mode(s_state_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(s_state_label, 0, 2);

    s_gesture_label = create_label(panel, "", 20, lv_color_white());
    lv_obj_set_pos(s_gesture_label, 0, 94);
    s_progress_label = create_label(panel, "", 16, lv_color_hex(0xB8C7DA));
    lv_obj_set_pos(s_progress_label, 0, 146);
    s_timeout_label = create_label(panel, "", 16, lv_color_hex(0xB8C7DA));
    lv_obj_set_pos(s_timeout_label, 0, 192);
    s_action_label = create_label(panel, "", 16, lv_color_hex(0xB8C7DA));
    lv_obj_set_width(s_action_label, 268);
    lv_label_set_long_mode(s_action_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(s_action_label, 0, 238);

    lv_obj_t *hint = create_label(
        panel,
        tr("小鑫已暂停 点击返回恢复",
           "Xiaoxin is offline in gesture mode. Voice returns after Exit releases the model."),
        16, lv_color_hex(0x7F95AF));
    lv_obj_set_width(hint, 268);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(hint, 0, 318);

    s_timer = lv_timer_create(refresh_ui, 100, NULL);
    ESP_LOGI(TAG, "Gesture overlay shown");
    refresh_ui(NULL);
}

void gesture_ui_hide(void)
{
    bool had_ui = s_timer != NULL || s_overlay != NULL || s_canvas_buffer != NULL;
    if (s_timer != NULL) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_overlay != NULL) {
        lv_obj_delete(s_overlay);
        s_overlay = NULL;
    }
    s_canvas = NULL;
    s_detect_box = NULL;
    s_preview_result_label = NULL;
    s_state_label = NULL;
    s_gesture_label = NULL;
    s_progress_label = NULL;
    s_timeout_label = NULL;
    s_action_label = NULL;
    if (s_canvas_buffer != NULL) {
        heap_caps_free(s_canvas_buffer);
        s_canvas_buffer = NULL;
    }
    s_preview_sequence = 0;
    if (had_ui) {
        ESP_LOGI(TAG, "Gesture overlay hidden");
    }
}

bool gesture_ui_is_visible(void)
{
    return s_overlay != NULL;
}

#else

void gesture_ui_show(void) {}
void gesture_ui_hide(void) {}
bool gesture_ui_is_visible(void) { return false; }

#endif
