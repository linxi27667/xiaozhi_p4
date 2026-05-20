#include "smart_home_tasks.h"

#include "mqtt_device_model.h"
#include "ui_events.h"
#include "../services/xiaozhi_mqtt.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "SmartHomeTasks";

static TaskHandle_t s_mqtt_task = nullptr;
static bool s_started = false;

static void smart_home_mqtt_task(void* arg) {
    (void)arg;

    vTaskDelay(pdMS_TO_TICKS(5000));

    while (true) {
        if (!mqtt_client_is_connected()) {
            mqtt_client_start();
        } else {
            mqtt_client_poll();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void SmartHomeTasksStart(void) {
    if (s_started) {
        return;
    }
    s_started = true;

    ui_events_init();
    device_model_init();
    mqtt_client_init();

    BaseType_t ret = xTaskCreatePinnedToCore(
        smart_home_mqtt_task,
        "sh_iot_mqtt",
        6144,
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
