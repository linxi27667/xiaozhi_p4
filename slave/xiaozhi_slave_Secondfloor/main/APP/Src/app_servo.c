/**
 * @file    app_servo.c
 * @brief   舵机硬件底层绑定代码 (ESP32 LEDC PWM 版本)
 */

#include "app_servo.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "SERVO";

/* ================= 1. 编写通用的硬件底层函数 (HW_ 前缀) ================= */

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_14_BIT
#define LEDC_FREQUENCY          50
#define LEDC_CLK_CFG            LEDC_AUTO_CLK

static bool g_ledc_initialized = false;

static void HW_Ledc_Global_Init(void) {
    if (g_ledc_initialized) return;

    ledc_timer_config_t timer_conf = {
        .speed_mode      = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num       = LEDC_TIMER,
        .freq_hz         = LEDC_FREQUENCY,
        .clk_cfg         = LEDC_CLK_CFG,
    };
    ledc_timer_config(&timer_conf);

    g_ledc_initialized = true;
    ESP_LOGI(TAG, "LEDC timer initialized (50Hz, 14-bit)");
}

static int8_t HW_Servo_Init(void *timer, uint32_t channel) {
    HW_Ledc_Global_Init();

    servo_pwm_t *pwm = (servo_pwm_t *)timer;
    if (pwm == NULL) return -1;

    ledc_channel_config_t ledc_conf = {
        .gpio_num   = (gpio_num_t)pwm->channel,
        .speed_mode = LEDC_MODE,
        .channel    = (ledc_channel_t)channel,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    esp_err_t ret = ledc_channel_config(&ledc_conf);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Servo LEDC channel %d initialized on GPIO %d", 
                 channel, pwm->channel);
        return 0;
    }
    return -1;
}

static void HW_Servo_Set_Pulse(void *timer, uint32_t channel, uint16_t pulse) {
    servo_pwm_t *pwm = (servo_pwm_t *)timer;
    if (pwm == NULL) return;

    uint32_t duty = (pulse * 16384) / 20000;

    ledc_set_duty(LEDC_MODE, (ledc_channel_t)channel, duty);
    ledc_update_duty(LEDC_MODE, (ledc_channel_t)channel);
}

/* ================= 2. 实例化对象并拼装硬件资源 ================= */

static servo_pwm_t servo1_pwm = { .timer = NULL, .channel = GPIO_NUM_8 };
static servo_pwm_t servo2_pwm = { .timer = NULL, .channel = GPIO_NUM_11 };
static servo_pwm_t servo3_pwm = { .timer = NULL, .channel = GPIO_NUM_14 };

servo_t My_Servo_1 = {
    .pwm_pin = { &servo1_pwm, 0 },
    .Init      = HW_Servo_Init,
    .Set_Pulse = HW_Servo_Set_Pulse
};

servo_t My_Servo_2 = {
    .pwm_pin = { &servo2_pwm, 1 },
    .Init      = HW_Servo_Init,
    .Set_Pulse = HW_Servo_Set_Pulse
};

servo_t My_Servo_3 = {
    .pwm_pin = { &servo3_pwm, 2 },
    .Init      = HW_Servo_Init,
    .Set_Pulse = HW_Servo_Set_Pulse
};

/* ================= 3. 提供给 main.c 的统一切入点 ================= */

void App_Servo_System_Init(void) {
    Servo_Init_Device(&My_Servo_1);
    Servo_Set_Angle(&My_Servo_1, 90.0f);

    Servo_Init_Device(&My_Servo_2);
    Servo_Set_Angle(&My_Servo_2, 90.0f);

    Servo_Init_Device(&My_Servo_3);
    Servo_Set_Angle(&My_Servo_3, 90.0f);

    ESP_LOGI(TAG, "Servo system initialized (3 servos: GPIO8, GPIO11, GPIO14)");
}
