#include "app_ambient_light.h"
#include "mqtt_iot_protocol.h"

#include "led_strip.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define AMBIENT_BEDROOM_GPIO    GPIO_NUM_18

static const char *TAG = "AMBIENT";

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
} ambient_strip_t;

static ambient_strip_t s_strips[AMBIENT_STRIP_COUNT] = {
    { .strip = NULL, .gpio = AMBIENT_BEDROOM_GPIO, .scene = AMBIENT_SCENE_OFF, .last_applied_scene = AMBIENT_SCENE_OFF, .brightness = 60, .red = 255, .green = 160, .blue = 80, .effect = IOT_LIGHT_EFFECT_STATIC, .on = false, .last_on = false, .rainbow_offset = 0, .breathe_step = 0, .breathe_dir = 1, .warning_toggle = 0 },
};

static uint8_t scale(uint8_t brightness, uint8_t v)
{
    return (uint8_t)((uint16_t)v * brightness / 100);
}

static void fill_rgb(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= AMBIENT_STRIP_COUNT || !s_strips[index].strip) return;
    for (int i = 0; i < AMBIENT_WS2812_COUNT; i++) {
        led_strip_set_pixel(s_strips[index].strip, i,
            scale(s_strips[index].brightness, r),
            scale(s_strips[index].brightness, g),
            scale(s_strips[index].brightness, b));
    }
    led_strip_refresh(s_strips[index].strip);
}

static void apply_scene(uint8_t index)
{
    if (index >= AMBIENT_STRIP_COUNT || !s_strips[index].strip) return;
    ambient_strip_t *s = &s_strips[index];
    if (!s->on) {
        led_strip_clear(s->strip);
        return;
    }
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
    for (int i = 0; i < AMBIENT_WS2812_COUNT; i++) {
        uint8_t hue = (uint8_t)((i * 256 / AMBIENT_WS2812_COUNT) + s->rainbow_offset);
        uint8_t r, g, b;
        hsv_to_rgb(hue, 255, s->brightness, &r, &g, &b);
        led_strip_set_pixel(s->strip, i, r, g, b);
    }
    led_strip_refresh(s->strip);
}

void App_Ambient_Light_Init(void)
{
    for (int i = 0; i < AMBIENT_STRIP_COUNT; i++) {
        led_strip_config_t strip_config = {
            .strip_gpio_num = s_strips[i].gpio,
            .max_leds = AMBIENT_WS2812_COUNT,
        };
        led_strip_rmt_config_t rmt_config = {
            .resolution_hz = 10 * 1000 * 1000,
            .flags.with_dma = false,
        };
        esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strips[i].strip);
        if (ret == ESP_OK) {
            led_strip_clear(s_strips[i].strip);
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
    s_strips[index].on = on;
    if (on && s_strips[index].scene == AMBIENT_SCENE_OFF) {
        s_strips[index].scene = (index == AMBIENT_BEDROOM_INDEX) ? AMBIENT_SCENE_SLEEP : AMBIENT_SCENE_WARM_HOME;
    }
    apply_scene(index);
}

void App_Ambient_Light_Set_RGB(uint8_t index, uint8_t red, uint8_t green, uint8_t blue,
                               uint8_t brightness, uint8_t effect)
{
    if (index >= AMBIENT_STRIP_COUNT) return;
    s_strips[index].red = red;
    s_strips[index].green = green;
    s_strips[index].blue = blue;
    s_strips[index].brightness = brightness;
    s_strips[index].effect = effect;
    s_strips[index].scene = (effect == IOT_LIGHT_EFFECT_RAINBOW) ? AMBIENT_SCENE_RAINBOW : AMBIENT_SCENE_OFF;
    s_strips[index].on = brightness != 0;

    if (effect == IOT_LIGHT_EFFECT_WARNING) {
        s_strips[index].scene = AMBIENT_SCENE_WARNING;
    } else if (effect == IOT_LIGHT_EFFECT_BREATHE) {
        /* 呼吸效果保留用户自定义颜色, scene 设为 OFF 让 apply_scene 显示用户颜色,
           呼吸动画在 Tick 中处理 */
        s_strips[index].scene = AMBIENT_SCENE_OFF;
    }
    apply_scene(index);
}

void App_Ambient_Light_Set_Scene(uint8_t index, ambient_scene_t scene)
{
    if (index >= AMBIENT_STRIP_COUNT) return;
    s_strips[index].scene = scene;
    s_strips[index].on = (scene != AMBIENT_SCENE_OFF);
    apply_scene(index);
}

void App_Ambient_Light_Set_Brightness(uint8_t index, uint8_t brightness)
{
    if (index >= AMBIENT_STRIP_COUNT) return;
    s_strips[index].brightness = brightness;
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
            s_strips[i].warning_toggle++;
            if (s_strips[i].warning_toggle >= 10) {
                s_strips[i].warning_toggle = 0;
            }
            /* 0-4: 红色亮, 5-9: 灭 (每5个tick切换一次) */
            if (s_strips[i].warning_toggle < 5) {
                fill_rgb(i, 255, 0, 0);
            } else {
                led_strip_clear(s_strips[i].strip);
            }
            continue;
        }
    }
}
