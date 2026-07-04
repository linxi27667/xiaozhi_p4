/**
 * @file    iot_control_task.h
 * @brief   一楼设备控制任务 (Flag-driven + Enum 架构)
 *
 * 职责：
 * - 维护设备标志位（枚举类型）
 * - 定时刷新 GPIO 输出
 *
 * 设备配置：1灯(大厅灯) + 1舵机(大门)
 *
 * 架构原则：
 * - 设备状态全部使用枚举类型（易读、易扩展）
 * - 枚举定义在此处，供 mqtt_receive.c 引用能否
 */
#ifndef IOT_CONTROL_TASK_H
#define IOT_CONTROL_TASK_H

#include <stdint.h>

#define LIGHT_COUNT    1
#define RELAY_COUNT    0
#define SERVO_COUNT    1

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

#define LIGHT_1_CHINESE   "一楼大厅灯"

#define SERVO_1_CHINESE   "一楼大门"

typedef struct {
    device_onoff_enum_t main_power;
    device_onoff_enum_t light[LIGHT_COUNT];
    device_onoff_enum_t relay[(RELAY_COUNT > 0) ? RELAY_COUNT : 1];
    servo_angle_enum_t  servo[SERVO_COUNT];
} device_flags_enum_t;

extern volatile device_flags_enum_t g_device_flags;

void Iot_Control_Task_Init(void);

#endif
