/**
 * @file    sensor_config.h
 * @brief   二楼从机传感器阈值配置
 *
 * 包含：雨滴传感器
 *
 * 阈值设定原则（基于ADC 12位 = 0-4095，对应0-3300mV）：
 * - 雨滴：有水时电阻小，电压低；干燥时电阻大，电压高
 *   干燥 > 2640mV (>80%)，小雨 1320-2640mV (40-80%)，大雨 < 1320mV (<40%)
 */
#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H

#include <stdint.h>
#include "driver/gpio.h"
#include "driver/adc.h"

#define SENSOR_ADC_UNIT         ADC_UNIT_1
#define SENSOR_ADC_NUM          1

#define RAIN_ADC_CHANNEL        ADC1_CHANNEL_0

#define RAIN_ADC_ATTEN          ADC_ATTEN_DB_12

#define ADC_SAMPLE_INTERVAL_MS  500

/* ================= 调试模式 ================= */
#define DEBUG_MODE               0

/* ================= 雨滴传感器阈值 (mV) ================= */
#define RAIN_THRESHOLD_LIGHT     2640
#define RAIN_THRESHOLD_HEAVY     1320

#define RAIN_TRIGGER_COUNT       2

#endif
