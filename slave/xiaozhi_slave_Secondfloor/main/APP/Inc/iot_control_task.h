/**
 * @file    iot_control_task.h
 * @brief   二楼设备控制任务 (Flag-driven + Enum 架构)
 *
 * 职责：
 * - 维护设备标志位（枚举类型）
 * - 定时刷新 GPIO 输出
 *
 * 设备配置：3灯 + 3继电器 + 3舵机
 *
 * 架构原则：
 * - 设备状态全部使用枚举类型（易读、易扩展）
 * - 枚举定义在此处，供 mqtt_receive.c 引用
 */
#ifndef IOT_CONTROL_TASK_H
#define IOT_CONTROL_TASK_H

#include <stdint.h>

/* ================= 全局设备数量配置 ================= */
#define LIGHT_COUNT    3
#define RELAY_COUNT    3
#define SERVO_COUNT    3

/* ================= 设备状态枚举 ================= */
typedef enum {
    OFF = 0,
    ON  = 1
} device_onoff_enum_t;

typedef enum {
    SERVO_0   = 0,
    SERVO_45  = 1,
    SERVO_90  = 2,
    SERVO_135 = 3,
    SERVO_180 = 4
} servo_angle_enum_t;

/* ================= 中文设备名称绑定（用于小智意图识别） ================= */
#define LIGHT_1_CHINESE   "二楼主卧灯"
#define LIGHT_2_CHINESE   "二楼客厅灯"
#define LIGHT_3_CHINESE   "二楼厕所灯"

#define RELAY_1_CHINESE   "二楼风扇"
#define RELAY_2_CHINESE   "二楼继电器2"
#define RELAY_3_CHINESE   "二楼继电器3"

#define SERVO_1_CHINESE   "二楼晾衣架"
#define SERVO_2_CHINESE   "二楼舵机2"
#define SERVO_3_CHINESE   "二楼舵机3"

/* ================= 设备标志位结构体 ================= */
typedef struct {
    device_onoff_enum_t light[LIGHT_COUNT];
    device_onoff_enum_t relay[RELAY_COUNT];
    servo_angle_enum_t  servo[SERVO_COUNT];
} device_flags_enum_t;

extern volatile device_flags_enum_t g_device_flags;

void Iot_Control_Task_Init(void);

#endif
