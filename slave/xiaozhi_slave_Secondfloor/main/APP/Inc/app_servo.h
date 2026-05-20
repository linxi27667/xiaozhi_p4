/**
 * @file    app_servo.h
 * @brief   舵机应用层声明
 */
#ifndef __SERVO_H__
#define __SERVO_H__

#include "bsp_servo.h"

extern servo_t My_Servo_1;
extern servo_t My_Servo_2;
extern servo_t My_Servo_3;

void App_Servo_System_Init(void);

#endif
