#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// 登录弹窗的三种模式
typedef enum {
    LOGIN_MODE_SELECT = 0,    // 选择模式(密码 or 人脸)
    LOGIN_MODE_PASSWORD,      // 密码输入模式
    LOGIN_MODE_FACE,          // 人脸识别模式
} login_mode_t;

// 初始化登录弹窗(订阅事件,可在 UI_Manager_Init 中调用)
void login_ui_init(void);

// 显示登录弹窗(默认 SELECT 模式)
void login_ui_show(void);

// 隐藏登录弹窗
void login_ui_hide(void);

// 是否可见
bool login_ui_is_visible(void);

// 是否处于人脸识别模式
bool login_ui_is_face_mode(void);

// 切换模式
void login_ui_set_mode(login_mode_t mode);

// 设置状态文本(如 "密码错误"、"正在识别..." 等)
void login_ui_set_status(const char *text);

// 更新人脸预览帧(RGB565 数据)
// 注意:此函数在 LVGL 任务中调用
void login_ui_update_face_preview(const uint8_t *rgb565, int width, int height);

// 显示人脸检测框(坐标相对于预览区域)
void login_ui_update_face_detect(int x, int y, int w, int h);

// 显示人脸检测成功状态，保留界面供 LVGL 完成一次可见刷新
// 注意:此函数在持有 LVGL 显示锁时调用
bool login_ui_show_face_success(int x, int y, int w, int h);

// 完成人脸成功界面并进入系统
// 注意:此函数在持有 LVGL 显示锁时调用
bool login_ui_finish_face_success(void);

// 清除人脸检测框
void login_ui_clear_face_detect(void);

#ifdef __cplusplus
}
#endif
