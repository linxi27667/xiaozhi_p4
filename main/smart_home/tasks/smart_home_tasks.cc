#include "smart_home_tasks.h"

#include "mqtt_device_model.h"
#include "ui_events.h"
#include "../services/auto_mode.h"
#include "../services/rule_engine.h"
#include "../services/smart_home_event_center.h"
#include "../services/xiaozhi_mqtt.h"
#include "../services/weather_service.h"

#include <esp_log.h>
#include <esp_sntp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "SmartHomeTasks";

static TaskHandle_t s_mqtt_task = nullptr;
static bool s_started = false;
static bool s_sntp_inited = false;

/* SNTP 时间同步初始化(自动模式依赖时间判断) */
static void sntp_start(void) {
    if (s_sntp_inited) return;
    s_sntp_inited = true;

    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "pool.ntp.org");
    esp_sntp_setservername(2, "time.windows.com");
    /* 设置时区为中国标准时间 (CST = UTC+8) */
    setenv("TZ", "CST-8", 1);
    tzset();
    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP started, waiting for time sync...");
}

void SmartHomeTasksNotifyNetworkReady(void) {
    sntp_start();
}

static void smart_home_mqtt_task(void* arg) {
    (void)arg;

    vTaskDelay(pdMS_TO_TICKS(1000));
    uint8_t slow_tick = 0;

    while (true) {
        if (slow_tick == 0) {
            if (!mqtt_client_is_connected()) {
                mqtt_client_start();
            } else {
                mqtt_client_poll();
            }
            weather_service_poll();
            auto_mode_tick();
        }
        mqtt_rgb_blink_tick();
        slow_tick = (uint8_t)((slow_tick + 1) % 4);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void SmartHomeTasksStart(void) {
    if (s_started) {
        return;
    }
    s_started = true;

    ui_events_init();
    device_model_init();
    smart_home_event_center_init();
    auto_mode_init();
    rule_engine_init();
    mqtt_client_init();
    weather_service_init();
    weather_service_start();

    BaseType_t ret = xTaskCreatePinnedToCore(
        smart_home_mqtt_task,
        "sh_iot_mqtt",
        8192,
        nullptr,
        5,
        &s_mqtt_task,
        1);

    if (ret != pdPASS) {
        s_mqtt_task = nullptr;
        ESP_LOGE(TAG, "Failed to create sh_iot_mqtt task");
        return;
    }

    ESP_LOGI(TAG, "Smart home task layer started");
}
