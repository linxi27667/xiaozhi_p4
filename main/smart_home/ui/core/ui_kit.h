#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>

/* 页面标题行:图标 + 主标题 + 副标题 */
lv_obj_t *ui_kit_page_title(lv_obj_t *parent, const char *icon,
                             const char *title, const char *subtitle);

/* 状态胶囊:圆角小标签,带颜色圆点 */
lv_obj_t *ui_kit_status_pill(lv_obj_t *parent, const char *text,
                              lv_color_t dot_color, lv_color_t bg_color);

/* 指标卡:图标 + 标签 + 数值 + 单位,valid=false 时显示"暂无数据" */
lv_obj_t *ui_kit_metric_card(lv_obj_t *parent, const char *icon,
                              const char *label, const char *value,
                              const char *unit, bool valid);

/* 设备卡:图标 + 名称 + 状态文字 + 状态颜色,点击回调 */
lv_obj_t *ui_kit_device_card(lv_obj_t *parent, const char *icon,
                              const char *name, const char *status_text,
                              lv_color_t status_color, bool connected,
                              lv_event_cb_t on_click, void *user_data);

/* 场景卡:图标 + 名称 + 描述,active=true 高亮蓝边框,danger=true 红色警示 */
lv_obj_t *ui_kit_scene_card(lv_obj_t *parent, const char *icon,
                             const char *name, const char *desc,
                             bool active, bool danger,
                             lv_event_cb_t on_click, void *user_data);

/* 设置行:图标 + 标签 + 右侧控件(开关/箭头/自定义) */
lv_obj_t *ui_kit_setting_row(lv_obj_t *parent, const char *icon,
                              const char *label, lv_obj_t *right_widget);

/* 分组标题:小字灰色标题 */
lv_obj_t *ui_kit_group_title(lv_obj_t *parent, const char *text);

/* 快捷芯片:小圆角按钮,active 高亮 */
lv_obj_t *ui_kit_chip(lv_obj_t *parent, const char *text, bool active,
                       lv_event_cb_t on_click, void *user_data);

/* 统一弹窗基类:遮罩 + 卡片 + 标题 + 关闭按钮,body 由调用方填充 */
typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *card;
    lv_obj_t *title_label;
    lv_obj_t *body;
    lv_obj_t *close_btn;
} ui_kit_popup_t;

ui_kit_popup_t ui_kit_popup_create(lv_obj_t *parent, const char *title,
                                    lv_color_t accent_color);
void ui_kit_popup_close(ui_kit_popup_t *popup);

#ifdef __cplusplus
}
#endif
