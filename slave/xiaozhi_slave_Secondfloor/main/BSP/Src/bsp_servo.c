/**
 * @file    bsp_servo.c
 * @brief   舵机核心逻辑实现
 */

#include "bsp_servo.h"

void Servo_Init_Device(servo_t *servo) {
    if (servo == NULL) return;
    //servo->Gpio_Config();
    //servo->Tim_Config(servo->pwm_pin.timer, servo->pwm_pin.channel);

    servo->angle = SERVO_MIN_ANGLE;
    
    // 调用底层初始化时，把绑定的定时器资源传进去
    if (servo->Init != NULL) {
        servo->Init(servo->pwm_pin.timer, servo->pwm_pin.channel); 
    }
}

void Servo_Set_Angle(servo_t *servo, float angle) {
    if (servo == NULL) return;

    if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

    servo->angle = angle;

    uint16_t pulse = (uint16_t)((angle - SERVO_MIN_ANGLE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_ANGLE - SERVO_MIN_ANGLE) + SERVO_MIN_PULSEWIDTH_US);

    if (servo->Set_Pulse != NULL) {
        servo->Set_Pulse(servo->pwm_pin.timer, servo->pwm_pin.channel, pulse);
    }
}
