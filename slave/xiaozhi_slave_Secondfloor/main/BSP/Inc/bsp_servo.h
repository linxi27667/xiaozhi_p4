/**
 * @file    bsp_servo.h
 * @brief   舵机核心协议层 (完全对象化，跨平台)
 */

#ifndef __BSP_SERVO_H__
#define __BSP_SERVO_H__

#include <stdint.h>
#include <stddef.h>

#define SERVO_MIN_ANGLE    0.0f
#define SERVO_MAX_ANGLE    180.0f
#define SERVO_MIN_PULSEWIDTH_US  500
#define SERVO_MAX_PULSEWIDTH_US  2500

/* ================= 1. 定义抽象硬件资源 ================= */

/**
 * @brief 抽象 PWM 资源对象
 * @note  ESP32 版本：timer 存储指向 servo_pwm_t 的指针，channel 存储 LEDC 通道号
 */
typedef struct {
    void     *timer;    // 指向包含 GPIO 信息的结构体指针
    uint32_t channel;   // LEDC 通道号 或 GPIO 引脚号
} servo_pwm_t;

/* ================= 2. 核心类定义 ================= */

/**
 * @brief 舵机对象结构体
 */
typedef struct servo_dev {
    servo_pwm_t pwm_pin;

    float angle;

    void   (*Gpio_Config)(void);
    void   (*Tim_Config)(void);
    int8_t (*Init)(void *timer, uint32_t channel);
    void   (*Set_Pulse)(void *timer, uint32_t channel, uint16_t pulse);
} servo_t;

/* ================= 3. 通用控制 API ================= */
void Servo_Init_Device(servo_t *servo);
void Servo_Set_Angle(servo_t *servo, float angle);

#endif
