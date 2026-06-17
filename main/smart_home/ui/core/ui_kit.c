#include "ui_kit.h"
#include "ui_styles.h"
#include "ui_theme.h"
#include "ui_font.h"
#include "ui_i18n.h"

#include "lvgl.h"
#include <string.h>

/* "暂无数据" UTF-8: E6 9A 82 E6 97 A0 E6 95 B0 E6 8D AE */
#define UI_KIT_NO_DATA_ZH "\xE6\x9A\x82\xE6\x97\xA0\xE6\x95\xB0\xE6\x8D\xAE"
#define UI_KIT_NO_DATA_EN "No data"

static const char *no_data_text(void)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? UI_KIT_NO_DATA_ZH : UI_KIT_NO_DATA_EN;
}

/* 内部辅助:创建一个 flex 容器(无样式) */
static lv_obj_t *create_flex_row(lv_obj_t *parent, lv_flex_align_t main_align)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

/* 内部辅助:创建一个 flex 列容器(无样式) */
static lv_obj_t *create_flex_col(lv_obj_t *parent, lv_flex_align_t main_align)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_width(col, lv_pct(100));
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_layout(col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, main_align, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    return col;
}

/* 内部辅助:创建标准卡片(白底 + 边框 + 圆角 + 阴影) */
static lv_obj_t *create_card(lv_obj_t *parent, lv_coord_t pad)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(card, UI_COLOR_CARD_SOFT, 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(card, UI_CARD_RADIUS, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card, pad, 0);
    lv_obj_set_style_pad_gap(card, 8, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_card_shadow(card);
    return card;
}

/* ---- 页面标题行 ---- */
lv_obj_t *ui_kit_page_title(lv_obj_t *parent, const char *icon,
                             const char *title, const char *subtitle)
{
    lv_obj_t *row = create_flex_row(parent, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_style_pad_left(row, 4, 0);

    if (icon) {
        lv_obj_t *badge = lv_obj_create(row);
        lv_obj_remove_style_all(badge);
        lv_obj_set_size(badge, 34, 34);
        lv_obj_set_style_bg_color(badge, UI_COLOR_ACCENT_SOFT, 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(badge, 8, 0);
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *icon_obj = ui_create_icon(badge, icon, ui_font_icon(20), UI_COLOR_ACCENT);
        lv_obj_center(icon_obj);
    }

    lv_obj_t *text_col = create_flex_col(row, LV_FLEX_ALIGN_START);
    lv_obj_set_flex_grow(text_col, 1);
    lv_obj_set_style_pad_row(text_col, 2, 0);

    if (title) {
        ui_create_label(text_col, title, ui_font_cn(22), UI_COLOR_TEXT_STRONG);
    }
    if (subtitle) {
        ui_create_label(text_col, subtitle, ui_font_cn(13), UI_COLOR_TEXT_SEC);
    }
    return row;
}

/* ---- 状态胶囊 ---- */
lv_obj_t *ui_kit_status_pill(lv_obj_t *parent, const char *text,
                              lv_color_t dot_color, lv_color_t bg_color)
{
    lv_obj_t *pill = lv_obj_create(parent);
    lv_obj_remove_style_all(pill);
    lv_obj_set_height(pill, 26);
    lv_obj_set_width(pill, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pill, bg_color, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pill, 12, 0);
    lv_obj_set_style_pad_hor(pill, 12, 0);
    lv_obj_set_style_pad_ver(pill, 3, 0);
    lv_obj_set_layout(pill, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pill, 6, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_dot(pill, dot_color, 6);
    if (text) {
        ui_create_label(pill, text, ui_font_cn(12), UI_COLOR_TEXT_STRONG);
    }
    return pill;
}

/* ---- 指标卡 ---- */
lv_obj_t *ui_kit_metric_card(lv_obj_t *parent, const char *icon,
                              const char *label, const char *value,
                              const char *unit, bool valid)
{
    lv_obj_t *card = create_card(parent, 14);
    lv_obj_set_style_min_height(card, 104, 0);

    if (label) {
        lv_obj_t *lbl = ui_create_label(card, label, ui_font_cn(13), UI_COLOR_TEXT_SEC);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, lv_pct(100));
    }

    if (!valid) {
        lv_obj_t *row = create_flex_row(card, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_column(row, 6, 0);
        if (icon) {
            ui_create_icon(row, icon, ui_font_icon(16), UI_COLOR_TEXT_SEC);
        }
        ui_create_label(row, no_data_text(), ui_font_cn(15), UI_COLOR_TEXT_SEC);
        return card;
    }

    lv_obj_t *row = create_flex_row(card, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (icon) {
        ui_create_icon(row, icon, ui_font_icon(18), UI_COLOR_ACCENT);
    }

    if (value) {
        ui_create_label(row, value, ui_font_cn(24), UI_COLOR_TEXT_STRONG);
    }
    if (unit) {
        ui_create_label(row, unit, ui_font_cn(12), UI_COLOR_TEXT_SEC);
    }
    return card;
}

/* ---- 设备卡 ---- */
lv_obj_t *ui_kit_device_card(lv_obj_t *parent, const char *icon,
                              const char *name, const char *status_text,
                              lv_color_t status_color, bool connected,
                              lv_event_cb_t on_click, void *user_data)
{
    lv_obj_t *card = create_card(parent, 14);
    lv_obj_set_style_min_height(card, 86, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 12, 0);

    if (on_click) {
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, on_click, LV_EVENT_CLICKED, user_data);
        ui_apply_press_feedback(card, UI_COLOR_ACCENT);
    }

    /* 图标 */
    lv_color_t icon_color = connected ? status_color : UI_COLOR_TEXT_SEC;
    if (icon) {
        lv_obj_t *badge = lv_obj_create(card);
        lv_obj_remove_style_all(badge);
        lv_obj_set_size(badge, 38, 38);
        lv_obj_set_style_bg_color(badge, connected ? lv_color_mix(icon_color, UI_COLOR_CARD, 18) : UI_COLOR_INPUT_BG, 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(badge, 8, 0);
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *ico = ui_create_icon(badge, icon, ui_font_icon(20), icon_color);
        lv_obj_center(ico);
    }

    /* 名称 + 状态 列 */
    lv_obj_t *col = create_flex_col(card, LV_FLEX_ALIGN_START);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_style_pad_row(col, 4, 0);

    if (name) {
        lv_color_t name_color = connected ? UI_COLOR_TEXT_STRONG : UI_COLOR_TEXT_SEC;
        lv_obj_t *name_lbl = ui_create_label(col, name, ui_font_cn(16), name_color);
        lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name_lbl, lv_pct(100));
    }
    if (status_text) {
        ui_create_label(col, status_text, ui_font_cn(12),
            connected ? status_color : UI_COLOR_TEXT_SEC);
    }
    return card;
}

/* ---- 场景卡 ---- */
lv_obj_t *ui_kit_scene_card(lv_obj_t *parent, const char *icon,
                             const char *name, const char *desc,
                             bool active, bool danger,
                             lv_event_cb_t on_click, void *user_data)
{
    lv_obj_t *card = create_card(parent, 14);
    lv_obj_set_style_min_height(card, 92, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 12, 0);

    lv_color_t accent = danger ? UI_COLOR_RED : UI_COLOR_ACCENT;

    if (active) {
        lv_obj_set_style_border_color(card, accent, 0);
        lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_bg_color(card, lv_color_mix(accent, UI_COLOR_CARD, 12), 0);
    }

    if (on_click) {
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, on_click, LV_EVENT_CLICKED, user_data);
        ui_apply_press_feedback(card, accent);
    }

    if (icon) {
        lv_obj_t *badge = lv_obj_create(card);
        lv_obj_remove_style_all(badge);
        lv_obj_set_size(badge, 40, 40);
        lv_obj_set_style_bg_color(badge, lv_color_mix(accent, UI_COLOR_CARD, danger ? 28 : 18), 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(badge, 8, 0);
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *ico = ui_create_icon(badge, icon, ui_font_icon(20), accent);
        lv_obj_center(ico);
    }

    lv_obj_t *col = create_flex_col(card, LV_FLEX_ALIGN_START);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_style_pad_row(col, 4, 0);

    if (name) {
        lv_obj_t *name_lbl = ui_create_label(col, name, ui_font_cn(17), UI_COLOR_TEXT_STRONG);
        lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name_lbl, lv_pct(100));
    }
    if (desc) {
        lv_obj_t *desc_lbl = ui_create_label(col, desc, ui_font_cn(12), UI_COLOR_TEXT_SEC);
        lv_label_set_long_mode(desc_lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(desc_lbl, lv_pct(100));
    }
    return card;
}

/* ---- 设置行 ---- */
lv_obj_t *ui_kit_setting_row(lv_obj_t *parent, const char *icon,
                              const char *label, lv_obj_t *right_widget)
{
    lv_obj_t *row = create_card(parent, 10);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_min_height(row, 48, 0);
    lv_obj_set_style_pad_ver(row, 10, 0);
    lv_obj_set_style_pad_column(row, 12, 0);

    /* 左侧:图标 + 标签 */
    lv_obj_t *left = create_flex_row(row, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(left, 10, 0);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (icon) {
        ui_create_icon(left, icon, ui_font_icon(16), UI_COLOR_TEXT_SEC);
    }
    if (label) {
        ui_create_label(left, label, ui_font_cn(15), UI_COLOR_TEXT_STRONG);
    }

    /* 右侧:自定义控件 */
    if (right_widget) {
        lv_obj_set_parent(right_widget, row);
    }
    return row;
}

/* ---- 分组标题 ---- */
lv_obj_t *ui_kit_group_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *row = create_flex_row(parent, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_left(row, 4, 0);
    lv_obj_set_style_pad_top(row, 6, 0);
    lv_obj_set_style_pad_bottom(row, 2, 0);

    if (text) {
        ui_create_label(row, text, ui_font_cn(15), UI_COLOR_TEXT_STRONG);
    }
    return row;
}

/* ---- 快捷芯片 ---- */
lv_obj_t *ui_kit_chip(lv_obj_t *parent, const char *text, bool active,
                       lv_event_cb_t on_click, void *user_data)
{
    lv_obj_t *chip = lv_obj_create(parent);
    lv_obj_remove_style_all(chip);
    lv_obj_set_height(chip, 34);
    lv_obj_set_width(chip, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(chip, active ? UI_COLOR_ACCENT : UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(chip, 16, 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_set_style_border_color(chip, active ? UI_COLOR_ACCENT : UI_COLOR_BORDER, 0);
    lv_obj_set_style_pad_hor(chip, 16, 0);
    lv_obj_set_style_pad_ver(chip, 6, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);

    if (text) {
        ui_create_label(chip, text, ui_font_cn(13),
            active ? lv_color_white() : UI_COLOR_TEXT_SEC);
    }

    if (on_click) {
        lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(chip, on_click, LV_EVENT_CLICKED, user_data);
        ui_apply_press_feedback(chip, UI_COLOR_ACCENT);
    }
    return chip;
}

/* ---- 弹窗 ---- */
static void on_popup_close_btn(lv_event_t *e)
{
    lv_obj_t *overlay = (lv_obj_t *)lv_event_get_user_data(e);
    if (overlay && lv_obj_is_valid(overlay)) {
        lv_obj_delete(overlay);
    }
}

ui_kit_popup_t ui_kit_popup_create(lv_obj_t *parent, const char *title,
                                    lv_color_t accent_color)
{
    ui_kit_popup_t popup;
    memset(&popup, 0, sizeof(popup));

    /* 遮罩层:全屏 20% 黑 */
    popup.overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(popup.overlay);
    lv_obj_set_size(popup.overlay, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(popup.overlay, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(popup.overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(popup.overlay, LV_OPA_20, 0);
    lv_obj_clear_flag(popup.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(popup.overlay);

    /* 卡片容器 */
    popup.card = lv_obj_create(popup.overlay);
    lv_obj_remove_style_all(popup.card);
    lv_obj_set_size(popup.card, 380, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(popup.card, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(popup.card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(popup.card, UI_CARD_RADIUS, 0);
    lv_obj_set_style_border_width(popup.card, 1, 0);
    lv_obj_set_style_border_color(popup.card, UI_COLOR_BORDER, 0);
    lv_obj_set_style_pad_all(popup.card, 20, 0);
    lv_obj_set_style_pad_gap(popup.card, 12, 0);
    lv_obj_set_layout(popup.card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(popup.card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(popup.card, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(popup.card, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_card_shadow(popup.card);
    lv_obj_center(popup.card);

    /* 标题行:标题 + 关闭按钮 */
    lv_obj_t *title_row = create_flex_row(popup.card, LV_FLEX_ALIGN_SPACE_BETWEEN);
    lv_obj_set_style_pad_column(title_row, 8, 0);

    if (title) {
        popup.title_label = ui_create_label(title_row, title,
            ui_font_cn(20), accent_color);
    }

    /* 关闭按钮:点击删除 overlay(所有子对象随之销毁) */
    popup.close_btn = lv_obj_create(title_row);
    lv_obj_remove_style_all(popup.close_btn);
    lv_obj_set_size(popup.close_btn, 28, 28);
    lv_obj_set_style_bg_color(popup.close_btn, UI_COLOR_INPUT_BG, 0);
    lv_obj_set_style_bg_opa(popup.close_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(popup.close_btn, 14, 0);
    lv_obj_clear_flag(popup.close_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(popup.close_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(popup.close_btn, on_popup_close_btn,
        LV_EVENT_CLICKED, popup.overlay);
    ui_apply_press_feedback(popup.close_btn, accent_color);
    ui_create_label(popup.close_btn, "X", ui_font_cn(14), UI_COLOR_TEXT_SEC);

    /* body 容器:调用方填充 */
    popup.body = create_flex_col(popup.card, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(popup.body, 8, 0);

    return popup;
}

void ui_kit_popup_close(ui_kit_popup_t *popup)
{
    if (!popup || !popup->overlay) return;
    if (lv_obj_is_valid(popup->overlay)) {
        lv_obj_delete(popup->overlay);
    }
    popup->overlay = NULL;
    popup->card = NULL;
    popup->title_label = NULL;
    popup->body = NULL;
    popup->close_btn = NULL;
}
