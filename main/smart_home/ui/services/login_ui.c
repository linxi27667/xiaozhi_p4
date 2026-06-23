#include "login_ui.h"

#include "ui_events.h"
#include "ui_font.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "ui_styles.h"
#include "ui_theme.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "LOGIN_UI";

// 固定密码
#define LOGIN_PASSWORD "0000"
#define PASSWORD_MAX_LEN 4

// 预览尺寸
#define PREVIEW_W 420
#define PREVIEW_H 315
#define PREVIEW_BUF_SIZE (PREVIEW_W * PREVIEW_H * 2)  // RGB565

// 卡片尺寸
#define CARD_W 680
#define CARD_H 440

// 状态
static lv_obj_t *s_overlay = NULL;
static lv_obj_t *s_card = NULL;
static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_back_btn = NULL;
static lv_obj_t *s_content = NULL;  // 内容容器(根据模式切换)
static login_mode_t s_mode = LOGIN_MODE_SELECT;
static bool s_initialized = false;
static bool s_login_completed = false;

// 密码模式状态
static char s_password[PASSWORD_MAX_LEN + 1] = {0};
static int s_password_len = 0;
static lv_obj_t *s_pwd_dots[PASSWORD_MAX_LEN] = {0};

// 人脸模式状态
static lv_obj_t *s_face_canvas = NULL;
static lv_obj_t *s_face_detect_box = NULL;
static lv_obj_t *s_face_hint = NULL;
static uint8_t *s_face_buf = NULL;  // PSRAM 中的预览缓冲

// 翻译辅助
static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

// 创建标签辅助
static lv_obj_t *create_label(lv_obj_t *parent, const char *text, int size,
                              lv_color_t color)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text ? text : "");
    lv_obj_set_style_text_font(lbl, ui_font_cn((uint8_t)size), 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    return lbl;
}

static void set_label_text(lv_obj_t *label, const char *text)
{
    if (!label) return;
    lv_label_set_text(label, text ? text : "");
}

// 清空内容容器
static void clean_content(void)
{
    if (s_content) {
        lv_obj_clean(s_content);
        // 重置子对象指针
        for (int i = 0; i < PASSWORD_MAX_LEN; i++) {
            s_pwd_dots[i] = NULL;
        }
        s_face_canvas = NULL;
        s_face_detect_box = NULL;
        s_face_hint = NULL;
    }
}

// 延迟隐藏定时器回调
static void delayed_hide_cb(lv_timer_t *timer)
{
    login_ui_hide();
    lv_timer_del(timer);
}

// 发布登录成功事件并隐藏弹窗
static void notify_login_success(void)
{
    if (s_login_completed) return;
    s_login_completed = true;
    ESP_LOGI(TAG, "Login success");
    login_ui_set_status(tr("检测通过", "Login success"));
    ui_event_publish(UI_EVENT_LOGIN_SUCCESS);
    ui_events_dispatch_pending();
    // 延迟隐藏,让用户看到成功状态
    lv_timer_t *t = lv_timer_create(delayed_hide_cb, 500, NULL);
    lv_timer_set_repeat_count(t, 1);
}

// 发布登录失败事件
static void notify_login_failed(const char *reason)
{
    ESP_LOGW(TAG, "Login failed: %s", reason);
    login_ui_set_status(reason);
    ui_event_publish(UI_EVENT_LOGIN_FAILED);
    ui_events_dispatch_pending();
}

// 验证密码
static void verify_password(void)
{
    if (s_password_len != PASSWORD_MAX_LEN) {
        notify_login_failed(tr("请输入密码", "Please enter password"));
        return;
    }
    if (strcmp(s_password, LOGIN_PASSWORD) == 0) {
        notify_login_success();
    } else {
        notify_login_failed(tr("密码错误", "Wrong password"));
        // 清空密码
        s_password_len = 0;
        memset(s_password, 0, sizeof(s_password));
        // 更新显示
        for (int i = 0; i < PASSWORD_MAX_LEN; i++) {
            if (s_pwd_dots[i]) {
                lv_obj_set_style_bg_opa(s_pwd_dots[i], LV_OPA_TRANSP, 0);
            }
        }
    }
}

// 密码按键回调
static void on_pwd_key(lv_event_t *e)
{
    const char *key = (const char *)lv_event_get_user_data(e);
    if (!key) return;

    if (strcmp(key, "C") == 0) {
        // 清除
        if (s_password_len > 0) {
            s_password_len--;
            s_password[s_password_len] = 0;
            if (s_pwd_dots[s_password_len]) {
                lv_obj_set_style_bg_opa(s_pwd_dots[s_password_len], LV_OPA_TRANSP, 0);
            }
        }
        login_ui_set_status("");
    } else if (strcmp(key, "OK") == 0) {
        verify_password();
    } else if (s_password_len < PASSWORD_MAX_LEN) {
        // 数字
        s_password[s_password_len] = key[0];
        s_password_len++;
        s_password[s_password_len] = 0;
        if (s_pwd_dots[s_password_len - 1]) {
            lv_obj_set_style_bg_opa(s_pwd_dots[s_password_len - 1], LV_OPA_COVER, 0);
        }
        login_ui_set_status("");
        // 输入满4位自动验证
        if (s_password_len == PASSWORD_MAX_LEN) {
            verify_password();
        }
    }
}

// 返回按钮回调
static void on_back(lv_event_t *e)
{
    (void)e;
    login_ui_set_mode(LOGIN_MODE_SELECT);
}

// 选择"密码登录"
static void on_select_password(lv_event_t *e)
{
    (void)e;
    login_ui_set_mode(LOGIN_MODE_PASSWORD);
}

// 选择"人脸登录"
static void on_select_face(lv_event_t *e)
{
    (void)e;
    login_ui_set_mode(LOGIN_MODE_FACE);
}

// 创建密码输入界面
static void build_password_ui(void)
{
    clean_content();

    // 密码圆点显示(4个圆点)
    int dot_size = 16;
    int dot_gap = 24;
    int total_w = PASSWORD_MAX_LEN * dot_size + (PASSWORD_MAX_LEN - 1) * dot_gap;
    int start_x = (CARD_W - total_w) / 2;
    for (int i = 0; i < PASSWORD_MAX_LEN; i++) {
        lv_obj_t *dot = lv_obj_create(s_content);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, dot_size, dot_size);
        lv_obj_set_pos(dot, start_x + i * (dot_size + dot_gap), 10);
        lv_obj_set_style_radius(dot, dot_size / 2, 0);
        lv_obj_set_style_border_width(dot, 2, 0);
        lv_obj_set_style_border_color(dot, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_bg_color(dot, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        s_pwd_dots[i] = dot;
    }

    // 数字键盘 3x4
    const char *keys[12] = {
        "1", "2", "3",
        "4", "5", "6",
        "7", "8", "9",
        "C", "0", "OK"
    };
    int key_w = 70;
    int key_h = 50;
    int key_gap = 10;
    int pad_x = (CARD_W - 3 * key_w - 2 * key_gap) / 2;
    int pad_y = 60;
    for (int i = 0; i < 12; i++) {
        int row = i / 3;
        int col = i % 3;
        lv_obj_t *btn = lv_btn_create(s_content);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, key_w, key_h);
        lv_obj_set_pos(btn, pad_x + col * (key_w + key_gap), pad_y + row * (key_h + key_gap));
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_bg_color(btn, UI_COLOR_INPUT_BG, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, UI_COLOR_BORDER, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        // 按键文字
        lv_color_t txt_color = UI_COLOR_TEXT_STRONG;
        if (strcmp(keys[i], "C") == 0) {
            txt_color = UI_COLOR_RED;
        } else if (strcmp(keys[i], "OK") == 0) {
            txt_color = UI_COLOR_GREEN;
        }
        lv_obj_t *lbl = create_label(btn, keys[i], 18, txt_color);
        lv_obj_center(lbl);

        // 存储 key 字符串(使用静态字符串避免悬空指针)
        static char key_storage[12][4];
        strncpy(key_storage[i], keys[i], 3);
        key_storage[i][3] = 0;
        lv_obj_add_event_cb(btn, on_pwd_key, LV_EVENT_CLICKED, key_storage[i]);
    }

    // 重置密码状态
    s_password_len = 0;
    memset(s_password, 0, sizeof(s_password));
    login_ui_set_status(tr("请输入密码", "Please enter password"));
}

// 创建人脸识别界面
static void build_face_ui(void)
{
    clean_content();

    // 分配预览缓冲(PSRAM)
    if (!s_face_buf) {
        s_face_buf = (uint8_t *)heap_caps_calloc(1, PREVIEW_BUF_SIZE,
                                                   MALLOC_CAP_SPIRAM);
        if (!s_face_buf) {
            ESP_LOGE(TAG, "Failed to allocate face preview buffer");
            login_ui_set_status(tr("内存不足", "Out of memory"));
            return;
        }
    }

    // 创建画布
    s_face_canvas = lv_canvas_create(s_content);
    lv_canvas_set_buffer(s_face_canvas, s_face_buf, PREVIEW_W, PREVIEW_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_canvas_fill_bg(s_face_canvas, lv_color_black(), LV_OPA_COVER);
    int canvas_x = (CARD_W - PREVIEW_W) / 2;
    lv_obj_set_pos(s_face_canvas, canvas_x, 10);

    // 检测框(初始隐藏)
    s_face_detect_box = lv_obj_create(s_content);
    lv_obj_remove_style_all(s_face_detect_box);
    lv_obj_set_size(s_face_detect_box, 0, 0);
    lv_obj_set_style_border_color(s_face_detect_box, UI_COLOR_GREEN, 0);
    lv_obj_set_style_border_width(s_face_detect_box, 2, 0);
    lv_obj_set_style_bg_opa(s_face_detect_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(s_face_detect_box, 0, 0);
    lv_obj_add_flag(s_face_detect_box, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(s_face_detect_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_face_detect_box, LV_OBJ_FLAG_HIDDEN);

    // 提示文字
    s_face_hint = create_label(s_content,
        tr("检测中...", "Recognizing..."),
        14, UI_COLOR_TEXT_SEC);
    lv_obj_set_width(s_face_hint, PREVIEW_W);
    lv_label_set_long_mode(s_face_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_face_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_face_hint, (CARD_W - PREVIEW_W) / 2, 10 + PREVIEW_H + 16);

    login_ui_set_status(tr("请面向屏幕", "Please face the screen"));
}

// 创建选择界面
static void build_select_ui(void)
{
    clean_content();

    int btn_w = 240;
    int btn_h = 200;
    int gap = 48;
    int total_w = 2 * btn_w + gap;
    int start_x = (CARD_W - total_w) / 2;
    int start_y = 40;

    // 密码登录按钮
    lv_obj_t *btn_pwd = lv_btn_create(s_content);
    lv_obj_remove_style_all(btn_pwd);
    lv_obj_set_size(btn_pwd, btn_w, btn_h);
    lv_obj_set_pos(btn_pwd, start_x, start_y);
    lv_obj_set_style_radius(btn_pwd, 12, 0);
    lv_obj_set_style_bg_color(btn_pwd, UI_COLOR_CARD_SOFT, 0);
    lv_obj_set_style_bg_opa(btn_pwd, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_pwd, 2, 0);
    lv_obj_set_style_border_color(btn_pwd, UI_COLOR_ACCENT, 0);
    lv_obj_clear_flag(btn_pwd, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(btn_pwd, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_pwd, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn_pwd, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn_pwd, 12, 0);
    lv_obj_add_event_cb(btn_pwd, on_select_password, LV_EVENT_CLICKED, NULL);
    ui_apply_press_feedback(btn_pwd, UI_COLOR_ACCENT);

    lv_obj_t *ic1 = ui_create_icon(btn_pwd, ICON_LOCK, ui_font_icon(40), UI_COLOR_ACCENT);
    (void)ic1;
    create_label(btn_pwd, tr("密码", "Password"), 18, UI_COLOR_TEXT_STRONG);
    create_label(btn_pwd, tr("使用数字密码", "Use numeric password"), 12, UI_COLOR_TEXT_SEC);

    // 人脸登录按钮
    lv_obj_t *btn_face = lv_btn_create(s_content);
    lv_obj_remove_style_all(btn_face);
    lv_obj_set_size(btn_face, btn_w, btn_h);
    lv_obj_set_pos(btn_face, start_x + btn_w + gap, start_y);
    lv_obj_set_style_radius(btn_face, 12, 0);
    lv_obj_set_style_bg_color(btn_face, UI_COLOR_CARD_SOFT, 0);
    lv_obj_set_style_bg_opa(btn_face, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_face, 2, 0);
    lv_obj_set_style_border_color(btn_face, UI_COLOR_GREEN, 0);
    lv_obj_clear_flag(btn_face, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(btn_face, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_face, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn_face, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn_face, 12, 0);
    lv_obj_add_event_cb(btn_face, on_select_face, LV_EVENT_CLICKED, NULL);
    ui_apply_press_feedback(btn_face, UI_COLOR_GREEN);

    lv_obj_t *ic2 = ui_create_icon(btn_face, ICON_USER, ui_font_icon(40), UI_COLOR_GREEN);
    (void)ic2;
    create_label(btn_face, tr("面部", "Face ID"), 18, UI_COLOR_TEXT_STRONG);
    create_label(btn_face, tr("使用面部检测", "Use face detection"), 12, UI_COLOR_TEXT_SEC);

    login_ui_set_status(tr("请选择", "Please select login method"));
}

// 构建内容区域
static void build_content(void)
{
    if (!s_content) return;
    clean_content();

    switch (s_mode) {
        case LOGIN_MODE_SELECT:
            build_select_ui();
            break;
        case LOGIN_MODE_PASSWORD:
            build_password_ui();
            break;
        case LOGIN_MODE_FACE:
            build_face_ui();
            break;
    }

    // 返回按钮在 SELECT 模式下隐藏
    if (s_back_btn) {
        if (s_mode == LOGIN_MODE_SELECT) {
            lv_obj_add_flag(s_back_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_back_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// 事件回调:登录要求
static void on_login_required(void *user_data)
{
    (void)user_data;
    login_ui_show();
}

// 事件回调:人脸识别成功
static void on_face_recognized(void *user_data)
{
    (void)user_data;
    login_ui_set_status(tr("检测通过", "Face recognized"));
    notify_login_success();
}

// 事件回调:人脸识别失败
static void on_face_not_recognized(void *user_data)
{
    (void)user_data;
    login_ui_set_status(tr("检测未通过", "Face not recognized, please retry"));
}

// 事件回调:检测到人脸
static void on_face_detected(void *user_data)
{
    (void)user_data;
    login_ui_set_status(tr("检测中...", "Recognizing..."));
}

void login_ui_init(void)
{
    if (s_initialized) return;
    s_initialized = true;

    ui_event_subscribe(UI_EVENT_LOGIN_REQUIRED, on_login_required, NULL);
    ui_event_subscribe(UI_EVENT_FACE_RECOGNIZED, on_face_recognized, NULL);
    ui_event_subscribe(UI_EVENT_FACE_NOT_RECOGNIZED, on_face_not_recognized, NULL);
    ui_event_subscribe(UI_EVENT_FACE_DETECTED, on_face_detected, NULL);

    ESP_LOGI(TAG, "Login UI initialized");
}

void login_ui_show(void)
{
    if (s_overlay) {
        // 已经显示
        return;
    }

    lv_obj_t *scr = lv_screen_active();
    if (!scr) {
        ESP_LOGE(TAG, "No active screen");
        return;
    }

    // 创建全屏遮罩
    s_overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_80, 0);
    lv_obj_set_pos(s_overlay, 0, 0);

    // 创建卡片
    s_card = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_card);
    lv_obj_set_size(s_card, CARD_W, CARD_H);
    lv_obj_center(s_card);
    lv_obj_set_style_bg_color(s_card, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(s_card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_card, 16, 0);
    lv_obj_set_style_border_width(s_card, 1, 0);
    lv_obj_set_style_border_color(s_card, UI_COLOR_BORDER, 0);
    lv_obj_set_style_shadow_width(s_card, 20, 0);
    lv_obj_set_style_shadow_opa(s_card, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(s_card, UI_COLOR_SHADOW, 0);
    lv_obj_set_style_pad_all(s_card, 20, 0);
    lv_obj_clear_flag(s_card, LV_OBJ_FLAG_SCROLLABLE);

    // 标题
    s_title_label = create_label(s_card, tr("系统", "System Login"),
                                  22, UI_COLOR_TEXT_STRONG);
    lv_obj_set_pos(s_title_label, 0, 0);

    // 返回按钮(右上角)
    s_back_btn = lv_btn_create(s_card);
    lv_obj_remove_style_all(s_back_btn);
    lv_obj_set_size(s_back_btn, 36, 36);
    lv_obj_align(s_back_btn, LV_ALIGN_TOP_RIGHT, 0, -4);
    lv_obj_set_style_radius(s_back_btn, 8, 0);
    lv_obj_set_style_bg_color(s_back_btn, UI_COLOR_INPUT_BG, 0);
    lv_obj_set_style_bg_opa(s_back_btn, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(s_back_btn, on_back, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(s_back_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *back_icon = ui_create_icon(s_back_btn, ICON_BACK,
                                          ui_font_icon(16), UI_COLOR_TEXT_SEC);
    lv_obj_center(back_icon);

    // 状态标签
    s_status_label = create_label(s_card, "", 14, UI_COLOR_TEXT_SEC);
    lv_obj_set_width(s_status_label, CARD_W - 40);
    lv_obj_set_height(s_status_label, 34);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_status_label, 0, 36);

    // 内容容器
    s_content = lv_obj_create(s_card);
    lv_obj_remove_style_all(s_content);
    lv_obj_set_size(s_content, CARD_W - 40, CARD_H - 80);
    lv_obj_set_pos(s_content, 0, 64);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, 0);

    // 默认选择模式
    s_mode = LOGIN_MODE_SELECT;
    s_login_completed = false;
    build_content();

    ESP_LOGI(TAG, "Login UI shown");
}

void login_ui_hide(void)
{
    if (!s_overlay) return;

    lv_obj_delete(s_overlay);
    s_overlay = NULL;
    s_card = NULL;
    s_title_label = NULL;
    s_status_label = NULL;
    s_back_btn = NULL;
    s_content = NULL;
    for (int i = 0; i < PASSWORD_MAX_LEN; i++) {
        s_pwd_dots[i] = NULL;
    }
    s_face_canvas = NULL;
    s_face_detect_box = NULL;
    s_face_hint = NULL;
    // 注意:s_face_buf 保留,下次显示时复用

    // 重置密码状态
    s_password_len = 0;
    memset(s_password, 0, sizeof(s_password));
    s_login_completed = false;

    ESP_LOGI(TAG, "Login UI hidden");
}

bool login_ui_is_visible(void)
{
    return s_overlay != NULL;
}

bool login_ui_is_face_mode(void)
{
    return s_overlay != NULL && s_mode == LOGIN_MODE_FACE;
}

void login_ui_set_mode(login_mode_t mode)
{
    if (!s_overlay) {
        ESP_LOGW(TAG, "Login UI not visible, cannot set mode");
        return;
    }
    if (s_mode == mode) return;
    s_mode = mode;
    build_content();
    ESP_LOGI(TAG, "Mode set to %d", (int)mode);
}

void login_ui_set_status(const char *text)
{
    set_label_text(s_status_label, text);
    if (s_mode == LOGIN_MODE_FACE) {
        set_label_text(s_face_hint, text);
    }
}

void login_ui_update_face_preview(const uint8_t *rgb565, int width, int height)
{
    if (!s_face_canvas || !s_face_buf) return;

    // 只处理匹配尺寸的帧
    if (width != PREVIEW_W || height != PREVIEW_H) {
        ESP_LOGD(TAG, "Preview size mismatch: %dx%d (expected %dx%d)",
                 width, height, PREVIEW_W, PREVIEW_H);
        return;
    }

    // 复制数据到画布缓冲
    memcpy(s_face_buf, rgb565, PREVIEW_BUF_SIZE);
    lv_obj_invalidate(s_face_canvas);
}

void login_ui_update_face_detect(int x, int y, int w, int h)
{
    if (!s_face_detect_box || !s_face_canvas) return;

    // 坐标相对于预览画布
    int canvas_x = (CARD_W - PREVIEW_W) / 2;
    int canvas_y = 10;  // build_face_ui 中的 y 偏移

    lv_obj_clear_flag(s_face_detect_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(s_face_detect_box, w, h);
    lv_obj_set_pos(s_face_detect_box, canvas_x + x, canvas_y + y);
}

void login_ui_clear_face_detect(void)
{
    if (!s_face_detect_box) return;
    lv_obj_add_flag(s_face_detect_box, LV_OBJ_FLAG_HIDDEN);
}
