#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#define UI_COLOR_BG           lv_color_hex(0xF3F7FD)
#define UI_COLOR_CARD         lv_color_hex(0xFFFFFF)
#define UI_COLOR_CARD_SOFT    lv_color_hex(0xF7FAFF)
#define UI_COLOR_ACCENT       lv_color_hex(0x2F6BFF)
#define UI_COLOR_ACCENT_DARK  lv_color_hex(0x1D4ED8)
#define UI_COLOR_ACCENT_SOFT  lv_color_hex(0xEAF2FF)
#define UI_COLOR_TEXT         lv_color_hex(0x334155)
#define UI_COLOR_TEXT_STRONG  lv_color_hex(0x0B1B3A)
#define UI_COLOR_TEXT_SEC     lv_color_hex(0x64748B)
#define UI_COLOR_GREEN        lv_color_hex(0x22C55E)
#define UI_COLOR_GREEN_SOFT   lv_color_hex(0xE7F8EF)
#define UI_COLOR_ORANGE       lv_color_hex(0xF59E0B)
#define UI_COLOR_RED          lv_color_hex(0xEF4444)
#define UI_COLOR_PURPLE       lv_color_hex(0x6554FF)
#define UI_COLOR_BLUE         lv_color_hex(0x3B82F6)
#define UI_COLOR_BORDER       lv_color_hex(0xC9D8EE)
#define UI_COLOR_INPUT_BG     lv_color_hex(0xEEF4FC)

#define UI_COLOR_SUCCESS      UI_COLOR_GREEN
#define UI_COLOR_WARNING      UI_COLOR_ORANGE
#define UI_COLOR_ERROR        UI_COLOR_RED

#define UI_COLOR_LOGO_BG      lv_color_hex(0x2F6BFF)
#define UI_COLOR_STATUS_BG    lv_color_hex(0xE7F8EF)
#define UI_COLOR_TAB_PRESSED  lv_color_hex(0xDDEAFF)
#define UI_COLOR_HEADER_GRAD  lv_color_hex(0xFFFFFF)
#define UI_COLOR_SHADOW       lv_color_hex(0xAFC4E3)

#define UI_CONTENT_PAD      16
#define UI_CARD_RADIUS      8
#define UI_BTN_RADIUS       12
#define UI_INPUT_RADIUS     10
#define UI_HEADER_HEIGHT    66
#define UI_TAB_HEIGHT       62

void UI_Theme_Init(lv_display_t *disp);

#ifdef __cplusplus
}
#endif
