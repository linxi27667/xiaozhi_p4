/**
 * @file    iot_control_task.h
 * @brief   三楼设备控制任务 (Flag-driven + Enum 架构)
 *
 * 职责：
 * - 维护设备标志位（枚举类型）
 * - 定时刷新 GPIO 输出
 *
 * 设备配置：1灯(阳台灯) + 0继电器 + 3舵机(左天窗/右天窗/晾衣杆) + 烟雾传感器 + 雨滴传感器 + 求助按键
 *
 * 架构原则：
 * - 设备状态全部使用枚举类型（易读、易扩展）
 * - 枚举定义在此处，供其他模块引用
 */
#ifndef IOT_CONTROL_TASK_H
#define IOT_CONTROL_TASK_H

#include <stdint.h>

/* ================= 全局设备数量配置 ================= */
#define LIGHT_COUNT    1
#define RELAY_COUNT    0
#define SERVO_COUNT    3

/* ================= 设备状态枚举 ================= */
typedef enum {
    OFF = 0,
    ON  = 1
} device_onoff_enum_t;

typedef enum {
    SERVO_0   = 0,
    SERVO_25  = 1,
    SERVO_70  = 2,
    SERVO_135 = 3,
    SERVO_180 = 4
} servo_angle_enum_t;

typedef enum {
    FIRE_STATUS_NORMAL = 0,
    FIRE_STATUS_WARNING = 1,
    FIRE_STATUS_CONFIRMED = 2
} fire_status_enum_t;

typedef enum {
    RAIN_STATUS_DRY = 0,
    RAIN_STATUS_RAINING = 1
} rain_status_enum_t;

typedef enum {
    HELP_STATUS_INACTIVE = 0,
    HELP_STATUS_ACTIVE = 1
} help_status_enum_t;

/* ================= 中文设备名称绑定（与主机MCP描述一致） ================= */
#define LIGHT_1_CHINESE   "三楼阳台灯"

#define SERVO_1_CHINESE   "三楼左天窗"
#define SERVO_2_CHINESE   "三楼右天窗"
#define SERVO_3_CHINESE   "三楼晾衣架"

/* ================= 设备标志位结构体 ================= */
typedef struct {
    device_onoff_enum_t light[LIGHT_COUNT];
    device_onoff_enum_t relay[(RELAY_COUNT > 0) ? RELAY_COUNT : 1];  /* 避免零长度数组 */
    servo_angle_enum_t  servo[SERVO_COUNT];
    fire_status_enum_t  fire_status;
    rain_status_enum_t  rain_status;
    help_status_enum_t  help_status;
} device_flags_enum_t;

extern volatile device_flags_enum_t g_device_flags;

void Iot_Control_Task_Init(void);

#endif
