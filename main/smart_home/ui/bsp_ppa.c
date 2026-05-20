#include "bsp_ppa.h"
#include "esp_lv_adapter.h"
#include "esp_log.h"

#ifdef CONFIG_ESP_LCD_PPA_ENABLE
#include "esp_cache.h"
#include "esp_lcd_ppa.h"
#endif

static const char *TAG = "BSP_PPA";
static bool s_ppa_ready = false;

esp_err_t BSP_PPA_Init(void)
{
#ifdef CONFIG_ESP_LCD_PPA_ENABLE
    esp_err_t ret = esp_lcd_ppa_init();
    if (ret == ESP_OK) {
        s_ppa_ready = true;
        ESP_LOGI(TAG, "PPA initialized successfully");
    } else {
        ESP_LOGW(TAG, "PPA init failed (0x%x), falling back to SW render", ret);
        s_ppa_ready = false;
    }
    return ret;
#else
    ESP_LOGI(TAG, "PPA not enabled in menuconfig, using SW render");
    s_ppa_ready = false;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t BSP_PPA_Blend(const bsp_ppa_blend_cfg_t *cfg)
{
#ifdef CONFIG_ESP_LCD_PPA_ENABLE
    if (!s_ppa_ready || !cfg) return ESP_ERR_INVALID_STATE;

    esp_lv_adapter_lock(-1);

    ppa_client_handle_t ppa_client = NULL;
    ppa_client_config_t client_cfg = {
        .oper_type = PPA_OPERATION_SRM,
    };

    esp_err_t ret = ppa_client_register(&client_cfg, &ppa_client);
    if (ret != ESP_OK) {
        esp_lv_adapter_unlock();
        return ret;
    }

    ppa_srm_oper_config_t srm_cfg = {
        .src = {
            .buffer = cfg->src_buf,
            .pic_w = cfg->src_w,
            .pic_h = cfg->src_h,
            .block_w = cfg->src_w,
            .block_h = cfg->src_h,
            .block_offset_x = 0,
            .block_offset_y = 0,
        },
        .dst = {
            .buffer = cfg->dst_buf,
            .pic_w = cfg->dst_w,
            .pic_h = cfg->dst_h,
            .block_offset_x = cfg->dst_x,
            .block_offset_y = cfg->dst_y,
        },
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    ret = ppa_do_srm_operation(ppa_client, &srm_cfg);
    ppa_client_unregister(ppa_client);
    esp_lv_adapter_unlock();
    return ret;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t BSP_PPA_Fill(lv_color_t color, void *dst_buf,
                        uint32_t dst_w, uint32_t dst_h,
                        uint32_t dst_stride, lv_color_format_t dst_cf,
                        int32_t x, int32_t y, int32_t w, int32_t h)
{
#ifdef CONFIG_ESP_LCD_PPA_ENABLE
    if (!s_ppa_ready || !dst_buf) return ESP_ERR_INVALID_STATE;

    esp_lv_adapter_lock(-1);

    ppa_client_handle_t ppa_client = NULL;
    ppa_client_config_t client_cfg = {
        .oper_type = PPA_OPERATION_FILL,
    };

    esp_err_t ret = ppa_client_register(&client_cfg, &ppa_client);
    if (ret != ESP_OK) {
        esp_lv_adapter_unlock();
        return ret;
    }

    ppa_fill_oper_config_t fill_cfg = {
        .dst = {
            .buffer = dst_buf,
            .pic_w = dst_w,
            .pic_h = dst_h,
            .block_offset_x = x,
            .block_offset_y = y,
            .block_w = w,
            .block_h = h,
        },
        .fill_mode = PPA_FILL_MODE_SOLID_COLOR,
        .fill_color = {
            .val = lv_color_to_u32(color),
        },
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    ret = ppa_do_fill_operation(ppa_client, &fill_cfg);
    ppa_client_unregister(ppa_client);
    esp_lv_adapter_unlock();
    return ret;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

bool BSP_PPA_Is_Ready(void)
{
    return s_ppa_ready;
}
