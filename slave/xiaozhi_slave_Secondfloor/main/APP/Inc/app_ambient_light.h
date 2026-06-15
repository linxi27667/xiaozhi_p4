#ifndef APP_AMBIENT_LIGHT_H
#define APP_AMBIENT_LIGHT_H

#include <stdint.h>
#include <stdbool.h>

#define AMBIENT_STRIP_COUNT     1
#define AMBIENT_BEDROOM_INDEX   0
#define AMBIENT_WS2812_COUNT    8

typedef enum {
    AMBIENT_SCENE_OFF = 0,
    AMBIENT_SCENE_WARM_HOME = 1,
    AMBIENT_SCENE_READING = 2,
    AMBIENT_SCENE_MOVIE = 3,
    AMBIENT_SCENE_SLEEP = 4,
    AMBIENT_SCENE_WARNING = 5,
    AMBIENT_SCENE_RAIN = 6,
    AMBIENT_SCENE_ENERGY_SAVE = 7,
    AMBIENT_SCENE_RAINBOW = 8,
} ambient_scene_t;

void App_Ambient_Light_Init(void);
void App_Ambient_Light_Set_Power(uint8_t index, bool on);
void App_Ambient_Light_Set_RGB(uint8_t index, uint8_t red, uint8_t green, uint8_t blue,
                               uint8_t brightness, uint8_t effect);
void App_Ambient_Light_Set_Scene(uint8_t index, ambient_scene_t scene);
void App_Ambient_Light_Set_Brightness(uint8_t index, uint8_t brightness);
ambient_scene_t App_Ambient_Light_Get_Scene(uint8_t index);
bool App_Ambient_Light_Is_On(uint8_t index);
uint8_t App_Ambient_Light_Get_Red(uint8_t index);
uint8_t App_Ambient_Light_Get_Green(uint8_t index);
uint8_t App_Ambient_Light_Get_Blue(uint8_t index);
uint8_t App_Ambient_Light_Get_Brightness(uint8_t index);
uint8_t App_Ambient_Light_Get_Effect(uint8_t index);
void App_Ambient_Light_Tick(void);

#endif
