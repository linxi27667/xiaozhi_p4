#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef enum {
    UI_PAGE_DATA = 0,
    UI_PAGE_CTRL = 1,
    UI_PAGE_LIGHT = 2,
    UI_PAGE_SCENE = 3,
    UI_PAGE_ENV   = 4,
    UI_PAGE_NET   = 5,
    UI_PAGE_SET   = 6,
    UI_PAGE_COUNT = 7,
} ui_page_id_t;

void UI_Manager_Init(lv_obj_t *parent);
void UI_Manager_Switch_Page(ui_page_id_t page);
void UI_Manager_Rebuild_Current(void);
void UI_Manager_Poll(void);
ui_page_id_t UI_Manager_Get_Current_Page(void);

#ifdef __cplusplus
}
#endif
