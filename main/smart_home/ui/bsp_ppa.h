#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "esp_err.h"

typedef enum {
    BSP_PPA_OP_BLEND = 0,
    BSP_PPA_OP_FILL,
    BSP_PPA_OP_COPY,
} bsp_ppa_op_t;

typedef struct {
    void *src_buf;
    uint32_t src_w;
    uint32_t src_h;
    uint32_t src_stride;
    lv_color_format_t src_cf;
    void *dst_buf;
    uint32_t dst_w;
    uint32_t dst_h;
    uint32_t dst_stride;
    lv_color_format_t dst_cf;
    int32_t dst_x;
    int32_t dst_y;
    uint8_t opa;
} bsp_ppa_blend_cfg_t;

esp_err_t BSP_PPA_Init(void);
esp_err_t BSP_PPA_Blend(const bsp_ppa_blend_cfg_t *cfg);
esp_err_t BSP_PPA_Fill(lv_color_t color, void *dst_buf,
                        uint32_t dst_w, uint32_t dst_h,
                        uint32_t dst_stride, lv_color_format_t dst_cf,
                        int32_t x, int32_t y, int32_t w, int32_t h);
bool BSP_PPA_Is_Ready(void);

#ifdef __cplusplus
}
#endif
