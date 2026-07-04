/**
 * @file    sensor_adc_task.h
 * @brief   三楼从机统一传感器任务头文件
 */
#ifndef SENSOR_ADC_TASK_H
#define SENSOR_ADC_TASK_H

#include <stdint.h>

void Sensor_ADC_Task_Init(void);
void Sensor_ADC_Get_Snapshot(uint16_t *flame_mv, uint16_t *rain_mv,
                             uint8_t *fire_status, uint8_t *rain_status);

#endif
