/**
 * @file    iot_control_task.c
 * @brief   三楼设备控制任务 (Flag-driven + Enum 架构)
 *
 * 閼卞矁鐭楅敍? * - 缂佸瓨濮㈢拋鎯ь槵閺嶅洤绻旀担宥忕礄閺嬫矮濡囩猾璇茬€烽敍? * - 鐎规碍妞傞崚閿嬫煀 GPIO 鏉堟挸鍤?
 * - 闂嗐劌銇夐懛顏勫З閺€鎯般€傞張宥夆偓鏄忕帆
 * - 閻忣偆浼ㄩ幎銉劅閾氬倿绂忛崳銊﹀付閸? *
 * 鐠佹儳顦柊宥囩枂閿?閻?+ 3缂佈呮暩閸?+ 3閼稿灚婧€ + 閻戠喖娴樻导鐘冲妳閸?+ 闂嗐劍娣导鐘冲妳閸?+ 濮瑰倸濮幐澶愭暛
 *
 * 閺嬭埖鐎崢鐔峰灟閿? * - 鐠佹儳顦悩鑸碘偓浣稿弿闁劋濞囬悽銊︾亣娑撳墽琚崹瀣剁礄閺勬捁顕伴妴浣规閹碘晛鐫嶉敍? * - 閸欘亣顕伴崣鏍ㄧ垼韫囨ぞ缍呴獮璺哄煕閺?GPIO
 * - 娑撳秴顦╅悶鍡曟崲娴?ESP-NOW 闁槒绶?
 * - 閺嶅洤绻旀担宥囨暠 esp_now_receive.c 閸?sensor_adc_task.c 閺囧瓨鏌?
 */
#include "iot_control_task.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DEBUG_MODE               0
#define DEBUG_BTN_GPIO          GPIO_NUM_47
#define DEBUG_STEP_DEGREES      45

static const char* TAG = "IOT_CTRL";

/* ================= 1. 绾兛娆㈡惔鏇炵湴閸戣姤鏆?(HW_ 閸撳秶绱? ================= */
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_14_BIT
#define LEDC_FREQUENCY          50
#define LEDC_CLK_CFG            LEDC_AUTO_CLK

#define SERVO_MIN_PULSEWIDTH_US 500
#define SERVO_MAX_PULSEWIDTH_US 2500
#define SERVO_MAX_ANGLE         180

static ledc_channel_t g_servo_channels[SERVO_COUNT] = {
    LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2
};

static void HW_Gpio_Init(gpio_num_t gpio) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);
}

static void HW_Gpio_Write(gpio_num_t gpio, uint8_t level) {
    if (gpio == GPIO_NUM_48) {
        gpio_set_level(gpio, level ? 0 : 1);
    } else {
        gpio_set_level(gpio, level);
    }
}

static void HW_Servo_Init(gpio_num_t gpio, ledc_channel_t channel) {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_CLK_CFG
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .speed_mode = LEDC_MODE,
        .channel = channel,
        .gpio_num = gpio,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ch);

    ESP_LOGI(TAG, "Servo initialized on GPIO %d, channel %d", gpio, channel);
}

static void HW_Servo_Set_Angle(ledc_channel_t channel, int16_t angle) {
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;
    if (angle < 0) angle = 0;

    float pulse_width = SERVO_MIN_PULSEWIDTH_US +
        (float)(angle) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_ANGLE);

    uint32_t duty = (uint32_t)((pulse_width * 16384) / 20000);

    ledc_set_duty(LEDC_MODE, channel, duty);
    ledc_update_duty(LEDC_MODE, channel);
}

/* ================= 2. GPIO 绾兒娆㈢€规矮绠?(娴滃搫浼愰崣顖涙暭) ================= */
#define LIGHT_GPIO_1     GPIO_NUM_2   /* 阳台灯 */

#define SERVO_GPIO_1     GPIO_NUM_18  /* 左天窗 */
#define SERVO_GPIO_2     GPIO_NUM_21  /* 右天窗 */
#define SERVO_GPIO_3     GPIO_NUM_40  /* 晾衣杆 */

#define REFRESH_INTERVAL_MS     100

/* ================= 3. 舵机角度映射表 ================= */
#define SERVO_ANGLE_0     0
#define SERVO_ANGLE_25    25
#define SERVO_ANGLE_70    70
#define SERVO_ANGLE_135   135
#define SERVO_ANGLE_180   180



#if DEBUG_MODE != 1
static const int16_t g_servo_angle_map[5] = {
    [SERVO_0]   = SERVO_ANGLE_0,
    [SERVO_25]  = SERVO_ANGLE_25,
    [SERVO_70]  = SERVO_ANGLE_70,
    [SERVO_135] = SERVO_ANGLE_135,
    [SERVO_180] = SERVO_ANGLE_180
};
#endif

/* ================= 4. 对象实例化 ================= */
static gpio_num_t g_light_gpios[LIGHT_COUNT] = {
    LIGHT_GPIO_1
};

static gpio_num_t g_relay_gpios[(RELAY_COUNT > 0) ? RELAY_COUNT : 1] = {0};

static gpio_num_t g_servo_gpios[SERVO_COUNT] = {
    SERVO_GPIO_1, SERVO_GPIO_2, SERVO_GPIO_3
};

volatile device_flags_enum_t g_device_flags = {
    .light = {OFF},
    .relay = {OFF},
    .servo = {SERVO_25, SERVO_25, SERVO_25},
    .fire_status = FIRE_STATUS_NORMAL,
    .rain_status = RAIN_STATUS_DRY,
    .help_status = HELP_STATUS_INACTIVE
};

#if DEBUG_MODE == 1
static uint8_t g_debug_servo_current_angle = 0;

static void HW_Button_Init(gpio_num_t gpio) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);
}

static bool HW_Button_Is_Pressed(gpio_num_t gpio) {
    return gpio_get_level(gpio) == 0;
}

static void Debug_Servo_Step(void) {
    g_debug_servo_current_angle = (g_debug_servo_current_angle + DEBUG_STEP_DEGREES) % 225;

    for (int i = 0; i < SERVO_COUNT; i++) {
        HW_Servo_Set_Angle(g_servo_channels[i], g_debug_servo_current_angle);
        ESP_LOGI(TAG, "DEBUG: Servo[%d] -> %d deg", i, g_debug_servo_current_angle);
    }

    if (g_debug_servo_current_angle == 0) {
        ESP_LOGI(TAG, "=== DEBUG: 0->45->90->135->180->0 cycle completed ===");
    }
}
#endif

/* ================= 5. 鐠佹儳顦崚閿嬫煀娴犺濮?================= */
static void Device_Refresh_Task(void* arg) {
    ESP_LOGI(TAG, "Device refresh task started");

#if DEBUG_MODE == 1
    HW_Button_Init(DEBUG_BTN_GPIO);
    ESP_LOGI(TAG, "DEBUG MODE ENABLED (GPIO%d, step=%d deg)", DEBUG_BTN_GPIO, DEBUG_STEP_DEGREES);
#endif

    for (int i = 0; i < LIGHT_COUNT; i++) {
        HW_Gpio_Init(g_light_gpios[i]);
        ESP_LOGI(TAG, "Light[%d] -> GPIO%d", i, g_light_gpios[i]);
    }

    for (int i = 0; i < RELAY_COUNT; i++) {
        HW_Gpio_Init(g_relay_gpios[i]);
        ESP_LOGI(TAG, "Relay[%d] -> GPIO%d", i, g_relay_gpios[i]);
    }

    for (int i = 0; i < SERVO_COUNT; i++) {
        HW_Servo_Init(g_servo_gpios[i], g_servo_channels[i]);
        ESP_LOGI(TAG, "Servo[%d] -> GPIO%d", i, g_servo_gpios[i]);
    }

    static uint8_t last_btn_state = 1;
    static uint8_t last_fire_status = FIRE_STATUS_NORMAL;

    while (1) {
#if DEBUG_MODE == 1
        uint8_t btn_state = HW_Button_Is_Pressed(DEBUG_BTN_GPIO);
        if (last_btn_state == 1 && btn_state == 0) {
            Debug_Servo_Step();
        }
        last_btn_state = btn_state;
#endif

        if (last_fire_status != g_device_flags.fire_status) {
            last_fire_status = g_device_flags.fire_status;
            ESP_LOGI(TAG, "[FIRE] Status changed to: %d", last_fire_status);
        }

        for (int i = 0; i < LIGHT_COUNT; i++) {
            HW_Gpio_Write(g_light_gpios[i], g_device_flags.light[i]);
        }

        for (int i = 0; i < RELAY_COUNT; i++) {
            HW_Gpio_Write(g_relay_gpios[i], g_device_flags.relay[i]);
        }

        for (int i = 0; i < SERVO_COUNT; i++) {
#if DEBUG_MODE != 1
            HW_Servo_Set_Angle(g_servo_channels[i], g_servo_angle_map[g_device_flags.servo[i]]);
#endif
        }

        vTaskDelay(pdMS_TO_TICKS(REFRESH_INTERVAL_MS));
    }
}

/* ================= 6. 任务初始化函数 ================= */
void Iot_Control_Task_Init(void) {
    BaseType_t ret = xTaskCreate(
        Device_Refresh_Task,
        "iot_ctrl",
        4096,
        NULL,
        3,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create device refresh task");
    } else {
        ESP_LOGI(TAG, "Device control initialized: %d lights, %d relays, %d servos",
                 LIGHT_COUNT, RELAY_COUNT, SERVO_COUNT);
    }
}
