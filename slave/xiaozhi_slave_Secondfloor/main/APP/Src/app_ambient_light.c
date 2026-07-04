#include "app_ambient_light.h"
#include "mqtt_iot_protocol.h"

#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define AMBIENT_BEDROOM_GPIO    GPIO_NUM_4
#define WARNING_TO_FULL_BRIGHT_MS 15000
#define RAIN_NOTIFY_MS          5000
#define DEFAULT_RED             255
#define DEFAULT_GREEN           160
#define DEFAULT_BLUE            80
#define DEFAULT_BRIGHTNESS      60
#define LED_STRIP_RMT_RES_HZ    (10 * 1000 * 1000)

static const char *TAG = "AMBIENT";
static SemaphoreHandle_t s_strip_mutex = NULL;

typedef struct {
    led_strip_handle_t strip;
    gpio_num_t gpio;
    ambient_scene_t scene;
    ambient_scene_t last_applied_scene;
    bool on;
    bool last_on;
    uint8_t brightness;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t effect;
    uint8_t rainbow_offset;
    /* 动画状态 */
    uint8_t breathe_step;       /* 呼吸动画步进 0~255 */
    int8_t breathe_dir;         /* 呼吸方向 +1/-1 */
    uint8_t warning_toggle;     /* 警示闪烁计数器 */
    uint32_t warning_start_ms;   /* 警示效果启动时间 */
    bool restore_pending;
    bool restore_on;
    ambient_scene_t restore_scene;
    uint8_t restore_red;
    uint8_t restore_green;
    uint8_t restore_blue;
    uint8_t restore_brightness;
    uint8_t restore_effect;
} ambient_strip_t;

static ambient_strip_t s_strips[AMBIENT_STRIP_COUNT] = {
    { .strip = NULL, .gpio = AMBIENT_BEDROOM_GPIO, .scene = AMBIENT_SCENE_OFF, .last_applied_scene = AMBIENT_SCENE_OFF, .brightness = 60, .red = 255, .green = 160, .blue = 80, .effect = IOT_LIGHT_EFFECT_STATIC, .on = false, .last_on = false, .rainbow_offset = 0, .breathe_step = 0, .breathe_dir = 1, .warning_toggle = 0 },
};

static uint8_t scale(uint8_t brightness, uint8_t v)
{
    return (uint8_t)((uint16_t)v * brightness / 100);
}

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static bool lock_strip(void)
{
    return s_strip_mutex == NULL || xSemaphoreTake(s_strip_mutex, pdMS_TO_TICKS(100)) == pdTRUE;
}

static void unlock_strip(void)
{
    if (s_strip_mutex) xSemaphoreGive(s_strip_mutex);
}

static esp_err_t configure_strip(gpio_num_t gpio, led_strip_handle_t *out_strip)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio,
        .max_leds = AMBIENT_WS2812_COUNT,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags.with_dma = false,
    };
    return led_strip_new_rmt_device(&strip_config, &rmt_config, out_strip);
}

static void clear_handle_once(led_strip_handle_t strip, gpio_num_t gpio)
{
    if (!strip) return;
    esp_err_t ret = led_strip_clear(strip);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Strip GPIO%d clear failed: %s", gpio, esp_err_to_name(ret));
    }
}

static void clear_strip_once(ambient_strip_t *s)
{
    if (!s || !s->strip) return;
    if (!lock_strip()) {
        ESP_LOGW(TAG, "Strip GPIO%d clear skipped: busy", s->gpio);
        return;
    }
    clear_handle_once(s->strip, s->gpio);
    unlock_strip();
}

static void restore_default_color_if_empty(ambient_strip_t *s)
{
    if (!s) return;
    if (s->brightness == 0) {
        s->brightness = DEFAULT_BRIGHTNESS;
    }
    if (s->red == 0 && s->green == 0 && s->blue == 0) {
        s->red = DEFAULT_RED;
        s->green = DEFAULT_GREEN;
        s->blue = DEFAULT_BLUE;
    }
}

static void fill_rgb(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= AMBIENT_STRIP_COUNT || !s_strips[index].strip) return;
    if (!lock_strip()) {
        ESP_LOGW(TAG, "Strip[%u] refresh skipped: busy", index);
        return;
    }
    for (int i = 0; i < AMBIENT_WS2812_COUNT; i++) {
        uint8_t sr = scale(s_strips[index].brightness, r);
        uint8_t sg = scale(s_strips[index].brightness, g);
        uint8_t sb = scale(s_strips[index].brightness, b);
        led_strip_set_pixel(s_strips[index].strip, i, sr, sg, sb);
    }
    esp_err_t ret = led_strip_refresh(s_strips[index].strip);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Strip[%u] refresh failed: %s", index, esp_err_to_name(ret));
    }
    unlock_strip();
}

static void apply_scene(uint8_t index)
{
    if (index >= AMBIENT_STRIP_COUNT || !s_strips[index].strip) return;
    ambient_strip_t *s = &s_strips[index];
    if (!s->on) {
        clear_strip_once(s);
        s->last_on = false;
        return;
    }
    s->last_on = true;
    s->last_applied_scene = s->scene;
    if (s->scene == AMBIENT_SCENE_OFF) {
        fill_rgb(index, s->red, s->green, s->blue);
        return;
    }

    if (s->effect == IOT_LIGHT_EFFECT_RAINBOW || s->scene == AMBIENT_SCENE_RAINBOW) return; /* handled in Tick */

    switch (s->scene) {
        case AMBIENT_SCENE_WARM_HOME:   fill_rgb(index, 255, 150, 60); break;
        case AMBIENT_SCENE_READING:     fill_rgb(index, 255, 220, 160); break;
        case AMBIENT_SCENE_MOVIE:       fill_rgb(index, 60, 80, 255); break;
        case AMBIENT_SCENE_SLEEP:       fill_rgb(index, 20, 30, 80); break;
        case AMBIENT_SCENE_WARNING:     fill_rgb(index, 255, 0, 0); break;
        case AMBIENT_SCENE_RAIN:        fill_rgb(index, 0, 120, 255); break;
        case AMBIENT_SCENE_ENERGY_SAVE: fill_rgb(index, 0, 180, 80); break;
        default:                        fill_rgb(index, 255, 150, 60); break;
    }
}

static bool is_rain_notify_color(const ambient_strip_t *s)
{
    return s && s->blue > s->red && s->blue > s->green;
}

static void save_restore_state(ambient_strip_t *s)
{
    if (!s || s->restore_pending) return;
    s->restore_pending = true;
    s->restore_on = s->on;
    s->restore_scene = s->scene;
    s->restore_red = s->red;
    s->restore_green = s->green;
    s->restore_blue = s->blue;
    s->restore_brightness = s->brightness;
    s->restore_effect = s->effect;
}

static void restore_saved_state(uint8_t index)
{
    if (index >= AMBIENT_STRIP_COUNT) return;
    ambient_strip_t *s = &s_strips[index];
    if (!s->restore_pending) return;

    s->on = s->restore_on;
    s->scene = s->restore_scene;
    s->red = s->restore_red;
    s->green = s->restore_green;
    s->blue = s->restore_blue;
    s->brightness = s->restore_brightness;
    s->effect = s->restore_effect;
    s->warning_start_ms = 0;
    s->warning_toggle = 0;
    s->restore_pending = false;

    if (!s->on) {
        clear_strip_once(s);
        s->last_on = false;
        return;
    }
    apply_scene(index);
}

static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region = h / 43;
    uint8_t remainder = (h - region * 43) * 6;
    uint8_t p = (uint16_t)v * (255 - s) / 255;
    uint8_t q = (uint16_t)v * (255 - (uint16_t)s * remainder / 255) / 255;
    uint8_t t = (uint16_t)v * (255 - (uint16_t)s * (255 - remainder) / 255) / 255;
    switch (region) {
        case 0:  *r = v; *g = t; *b = p; break;
        case 1:  *r = q; *g = v; *b = p; break;
        case 2:  *r = p; *g = v; *b = t; break;
        case 3:  *r = p; *g = q; *b = v; break;
        case 4:  *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

static void fill_rainbow(uint8_t index)
{
    if (index >= AMBIENT_STRIP_COUNT || !s_strips[index].strip) return;
    ambient_strip_t *s = &s_strips[index];
    if (!lock_strip()) {
        ESP_LOGW(TAG, "Strip[%u] rainbow skipped: busy", index);
        return;
    }
    for (int i = 0; i < AMBIENT_WS2812_COUNT; i++) {
        uint8_t hue = (uint8_t)((i * 256 / AMBIENT_WS2812_COUNT) + s->rainbow_offset);
        uint8_t r, g, b;
        hsv_to_rgb(hue, 255, s->brightness, &r, &g, &b);
        led_strip_set_pixel(s->strip, i, r, g, b);
    }
    esp_err_t ret = led_strip_refresh(s->strip);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Strip[%u] rainbow refresh failed: %s", index, esp_err_to_name(ret));
    }
    unlock_strip();
}

void App_Ambient_Light_Init(void)
{
    if (s_strip_mutex == NULL) {
        s_strip_mutex = xSemaphoreCreateMutex();
        if (s_strip_mutex == NULL) {
            ESP_LOGW(TAG, "Strip mutex create failed; continuing without lock");
        }
    }

    for (int i = 0; i < AMBIENT_STRIP_COUNT; i++) {
        s_strips[i].on = false;
        s_strips[i].last_on = false;
        s_strips[i].scene = AMBIENT_SCENE_OFF;
        s_strips[i].last_applied_scene = AMBIENT_SCENE_OFF;
        s_strips[i].effect = IOT_LIGHT_EFFECT_STATIC;
        s_strips[i].warning_start_ms = 0;

        esp_err_t ret = configure_strip(s_strips[i].gpio, &s_strips[i].strip);
        if (ret == ESP_OK) {
            clear_strip_once(&s_strips[i]);
            ESP_LOGI(TAG, "Strip[%d] initialized on GPIO%d", i, s_strips[i].gpio);
        } else {
            ESP_LOGE(TAG, "Strip[%d] init failed: %s", i, esp_err_to_name(ret));
            s_strips[i].strip = NULL;
        }
    }
}

void App_Ambient_Light_Set_Power(uint8_t index, bool on)
{
    if (index >= AMBIENT_STRIP_COUNT) return;
    ambient_strip_t *s = &s_strips[index];
    if (!on) {
        bool was_on = s->on || s->last_on;
        s->on = false;
        s->scene = AMBIENT_SCENE_OFF;
        s->effect = IOT_LIGHT_EFFECT_STATIC;
        s->warning_start_ms = 0;
        s->restore_pending = false;
        if (was_on) clear_strip_once(s);
        s->last_on = false;
        return;
    }
    restore_default_color_if_empty(s);
    if (s->on && s->last_on) return;
    s_strips[index].on = on;
    apply_scene(index);
}

void App_Ambient_Light_Set_RGB(uint8_t index, uint8_t red, uint8_t green, uint8_t blue,
                               uint8_t brightness, uint8_t effect)
{
    if (index >= AMBIENT_STRIP_COUNT) return;
    ambient_strip_t *s = &s_strips[index];
    bool was_on = s->on || s->last_on;
    bool rain_notify = effect == IOT_LIGHT_EFFECT_WARNING &&
                       blue > red && blue > green;
    if (rain_notify) {
        save_restore_state(s);
    } else {
        s->restore_pending = false;
    }
    s->red = red;
    s->green = green;
    s->blue = blue;
    s->brightness = brightness;
    s->effect = effect;
    s->scene = (effect == IOT_LIGHT_EFFECT_RAINBOW) ? AMBIENT_SCENE_RAINBOW : AMBIENT_SCENE_OFF;
    s->on = brightness != 0;
    s->warning_start_ms = 0;

    if (effect == IOT_LIGHT_EFFECT_WARNING) {
        s->scene = AMBIENT_SCENE_WARNING;
        s->warning_toggle = 0;
        s->warning_start_ms = now_ms();
    } else if (effect == IOT_LIGHT_EFFECT_BREATHE) {
        /* 呼吸效果保留用户自定义颜色, scene 设为 OFF 让 apply_scene 显示用户颜色,
           呼吸动画在 Tick 中处理 */
        s->scene = AMBIENT_SCENE_OFF;
    }

    if (!s->on && was_on) {
        s->restore_pending = false;
        clear_strip_once(s);
        s->last_on = false;
        return;
    }
    apply_scene(index);
}

void App_Ambient_Light_Set_Scene(uint8_t index, ambient_scene_t scene)
{
    if (index >= AMBIENT_STRIP_COUNT) return;
    ambient_strip_t *s = &s_strips[index];
    bool was_on = s->on || s->last_on;
    s->scene = scene;
    s->on = (scene != AMBIENT_SCENE_OFF);
    if (!s->on && was_on) {
        s->effect = IOT_LIGHT_EFFECT_STATIC;
        s->warning_start_ms = 0;
        s->restore_pending = false;
        clear_strip_once(s);
        s->last_on = false;
        return;
    }
    apply_scene(index);
}

void App_Ambient_Light_Set_Brightness(uint8_t index, uint8_t brightness)
{
    if (index >= AMBIENT_STRIP_COUNT) return;
    ambient_strip_t *s = &s_strips[index];
    s->brightness = brightness;
    if (brightness == 0) {
        s->on = false;
        s->effect = IOT_LIGHT_EFFECT_STATIC;
        s->warning_start_ms = 0;
        s->restore_pending = false;
        clear_strip_once(s);
        s->last_on = false;
        return;
    }
    s->on = true;
    apply_scene(index);
}

uint8_t App_Ambient_Light_Get_Red(uint8_t index)
{
    if (index >= AMBIENT_STRIP_COUNT) return 0;
    return s_strips[index].red;
}

uint8_t App_Ambient_Light_Get_Green(uint8_t index)
{
    if (index >= AMBIENT_STRIP_COUNT) return 0;
    return s_strips[index].green;
}

uint8_t App_Ambient_Light_Get_Blue(uint8_t index)
{
    if (index >= AMBIENT_STRIP_COUNT) return 0;
    return s_strips[index].blue;
}

uint8_t App_Ambient_Light_Get_Brightness(uint8_t index)
{
    if (index >= AMBIENT_STRIP_COUNT) return 0;
    return s_strips[index].brightness;
}

uint8_t App_Ambient_Light_Get_Effect(uint8_t index)
{
    if (index >= AMBIENT_STRIP_COUNT) return 0;
    return s_strips[index].effect;
}

ambient_scene_t App_Ambient_Light_Get_Scene(uint8_t index)
{
    if (index >= AMBIENT_STRIP_COUNT) return AMBIENT_SCENE_OFF;
    return s_strips[index].scene;
}

bool App_Ambient_Light_Is_On(uint8_t index)
{
    if (index >= AMBIENT_STRIP_COUNT) return false;
    return s_strips[index].on;
}

void App_Ambient_Light_Tick(void)
{
    for (int i = 0; i < AMBIENT_STRIP_COUNT; i++) {
        if (!s_strips[i].on || !s_strips[i].strip) continue;

        /* 彩虹效果: 每次偏移色相 */
        if (s_strips[i].scene == AMBIENT_SCENE_RAINBOW ||
            s_strips[i].effect == IOT_LIGHT_EFFECT_RAINBOW) {
            s_strips[i].rainbow_offset += 4;
            fill_rainbow(i);
            continue;
        }

        /* 呼吸效果: 亮度在 0~brightness 之间正弦变化 */
        if (s_strips[i].effect == IOT_LIGHT_EFFECT_BREATHE) {
            s_strips[i].breathe_step += s_strips[i].breathe_dir * 8;
            if (s_strips[i].breathe_step >= 200) {
                s_strips[i].breathe_step = 200;
                s_strips[i].breathe_dir = -1;
            } else if (s_strips[i].breathe_step <= 0) {
                s_strips[i].breathe_step = 0;
                s_strips[i].breathe_dir = 1;
            }
            /* 动态亮度 = 基础亮度 * (breathe_step / 200) */
            uint8_t dyn_brightness = (uint8_t)((uint16_t)s_strips[i].brightness * s_strips[i].breathe_step / 200);
            uint8_t saved = s_strips[i].brightness;
            s_strips[i].brightness = dyn_brightness;
            fill_rgb(i, s_strips[i].red, s_strips[i].green, s_strips[i].blue);
            s_strips[i].brightness = saved;
            continue;
        }

        /* 警示效果: 红色闪烁 (每5次tick切换一次) */
        if (s_strips[i].effect == IOT_LIGHT_EFFECT_WARNING ||
            s_strips[i].scene == AMBIENT_SCENE_WARNING) {
            if (is_rain_notify_color(&s_strips[i]) &&
                s_strips[i].warning_start_ms != 0 &&
                now_ms() - s_strips[i].warning_start_ms >= RAIN_NOTIFY_MS) {
                restore_saved_state(i);
                continue;
            }
            if (s_strips[i].warning_start_ms != 0 &&
                now_ms() - s_strips[i].warning_start_ms >= WARNING_TO_FULL_BRIGHT_MS) {
                s_strips[i].red = 255;
                s_strips[i].green = 255;
                s_strips[i].blue = 255;
                s_strips[i].brightness = 100;
                s_strips[i].effect = IOT_LIGHT_EFFECT_STATIC;
                s_strips[i].scene = AMBIENT_SCENE_OFF;
                s_strips[i].warning_start_ms = 0;
                fill_rgb(i, 255, 255, 255);
                continue;
            }
            s_strips[i].warning_toggle++;
            if (s_strips[i].warning_toggle >= 10) {
                s_strips[i].warning_toggle = 0;
            }
            /* 0-4: 目标警示色亮, 5-9: 灭 (每5个tick切换一次) */
            if (s_strips[i].warning_toggle < 5) {
                fill_rgb(i, s_strips[i].red, s_strips[i].green, s_strips[i].blue);
            } else {
                clear_strip_once(&s_strips[i]);
            }
            continue;
        }
    }
}
