/**
 * @file    sensor_adc_task.h
 * @brief   二楼从机传感器任务头文件
 */
#ifndef SENSOR_ADC_TASK_H
#define SENSOR_ADC_TASK_H

#include <stdint.h>

void Sensor_ADC_Task_Init(void);
void Sensor_ADC_Get_Snapshot(uint16_t *rain_mv, uint8_t *rain_status);

#endif
