/**
 * @file    sensor_config.h
 * @brief   三楼从机传感器阈值配置
 *
 * 包含：火焰传感器、雨滴传感器
 *
 * 阈值设定原则（基于ADC 12位 = 0-4095，对应0-3300mV）：
 * - 火焰：接在 GPIO2，对应 ESP32-S3 ADC1_CH1；AO 电压越低表示火焰越近
 * - 雨滴：有水时电阻小，电压低；干燥时电阻大，电压高
 *   干燥 > 2640mV (>80%)，小雨 1320-2640mV (40-80%)，大雨 < 1320mV (<40%)
 */
#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

#define SENSOR_ADC_UNIT         ADC_UNIT_1
#define SENSOR_ADC_NUM          2

#define FLAME_SENSOR_GPIO       GPIO_NUM_2
#define RAIN_SENSOR_GPIO        GPIO_NUM_1

#define FLAME_ADC_CHANNEL       ADC_CHANNEL_1
#define RAIN_ADC_CHANNEL        ADC_CHANNEL_0

#define FLAME_ADC_ATTEN         ADC_ATTEN_DB_12
#define RAIN_ADC_ATTEN          ADC_ATTEN_DB_12
#define SENSOR_ADC_BITWIDTH     ADC_BITWIDTH_12

#define ADC_SAMPLE_INTERVAL_MS  500

/* ================= 调试模式 ================= */
#define DEBUG_MODE               1

/* ================= 火焰传感器阈值 =================
 * 参照 garden 项目：fire_percent = 100 - fire_mv * 100 / 3100。
 * 火焰越近电压越低，百分比越高；超过 60% 连续 3 次确认火灾。
 */
#define SENSOR_ADC_FULL_SCALE_MV 3100
#define FIRE_THRESHOLD_PERCENT   60

#define FIRE_TRIGGER_COUNT       3

#define FIRE_ALERT_INTERVAL_MS   5000
#define FIRE_STARTUP_IGNORE_MS   3000

/* ================= 雨滴传感器阈值 (mV) ================= */
#define RAIN_THRESHOLD_LIGHT     2640

#define RAIN_TRIGGER_COUNT       2

#endif
