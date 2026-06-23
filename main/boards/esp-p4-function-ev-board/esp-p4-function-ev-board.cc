#include "wifi_board.h"
#include "audio/codecs/es8311_audio_codec.h"
// Display
#include "display/display.h"
#include "display/lcd_display.h"
#include "lvgl_theme.h"
// Backlight
// PwmBacklight is declared in backlight headers pulled by display/lcd_display includes via lvgl stack

#include "application.h"
#include "button.h"
#include "config.h"
#include "esp_p4_tf_card.h"
#include "esp_video.h"
#include "ui_asset_service.h"

#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_lvgl_port.h>
#include <soc/clk_tree_defs.h>
// MIPI-DSI / LCD vendor includes (library may replace some)
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_touch_gt911.h"

// Library includes
#include "bsp/esp32_p4_function_ev_board.h"
#include "bsp/touch.h"

#define TAG "ESP32P4FuncEV"

class ESP32P4FunctionEvBoard : public WifiBoard
{
private:
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    Button boot_button_;
    LcdDisplay *display_ = nullptr;
    esp_lcd_touch_handle_t tp_ = nullptr;
    lv_indev_t *touch_indev_ = nullptr;
    EspVideo* camera_ = nullptr;
    EspP4TfCard tf_card_;

    void InitializeI2cBuses()
    {
        ESP_ERROR_CHECK(bsp_i2c_init());
        codec_i2c_bus_ = bsp_i2c_get_handle();
    }

    // Touch I2C bus initialization is not required for this board (handled elsewhere)
    void InitializeTouchI2cBus()
    {
        // No implementation needed
    }

    void InitializeLCD()
    {
        bsp_display_config_t config = {
            .hdmi_resolution = BSP_HDMI_RES_NONE,
            .dsi_bus = {
                .phy_clk_src = (mipi_dsi_phy_clock_source_t)SOC_MOD_CLK_PLL_F20M,
                .lane_bit_rate_mbps = 1000,
            },
        };

        bsp_lcd_handles_t handles;
        ESP_ERROR_CHECK(bsp_display_new_with_handles(&config, &handles));

        display_ = new MipiLcdDisplay(handles.io, handles.panel, 1024, 600, 0, 0, true, true, false);
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeTouch()
    {
        ESP_ERROR_CHECK(bsp_touch_new(NULL, &tp_));
        tp_->config.int_gpio_num = GPIO_NUM_NC;
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp_,
        };
        touch_indev_ = lvgl_port_add_touch(&touch_cfg);
        if (touch_indev_ == nullptr) {
            ESP_LOGE(TAG, "Failed to register touch controller with LVGL");
        } else {
            lv_indev_reset(touch_indev_, nullptr);
            ESP_LOGI(TAG, "Touch controller registered with LVGL");
        }
    }

    void InitializeSdCard()
    {
        esp_err_t ret = tf_card_.Mount();
        if (ret == ESP_OK) {
            return;
        }

        ESP_LOGE(TAG, "TF card is unavailable; UI assets will degrade: %s", esp_err_to_name(ret));
    }

    void InitializeCamera()
    {
        ESP_LOGI(TAG, "Initializing camera (MIPI-CSI)");

        /* P4 EV Board 原生支持 MIPI-CSI 摄像头接口(200万像素),
           SCCB 复用已有 I2C 总线(GPIO7/8,与 ES8311 codec/GT911 触摸共用),
           数据通过 MIPI-CSI 专用差分通道传输,不占用普通 GPIO。 */
        esp_video_init_csi_config_t csi_config = {
            .sccb_config = {
                .init_sccb = false,       /* 复用已有 I2C 总线,不重复初始化 */
                .i2c_handle = codec_i2c_bus_,
                .freq = 400000,
            },
            .reset_pin = GPIO_NUM_NC,
            .pwdn_pin  = GPIO_NUM_NC,
        };

        esp_video_init_config_t video_config = {
            .csi = &csi_config,
        };

        auto *camera = new EspVideo(video_config);
        if (camera->IsReady()) {
            camera_ = camera;
            ESP_LOGI(TAG, "MIPI-CSI camera initialized successfully");
        } else {
            delete camera;
            camera_ = nullptr;
            ESP_LOGE(TAG, "MIPI-CSI camera initialization failed");
        }
    }

    void InitializeFonts()
    {
        ESP_LOGI(TAG, "Initializing font support");
        // Font initialization is handled by the Assets system
        // The board supports loading fonts from assets partition
        // Verify that fonts are properly loaded by checking theme
        auto& theme_manager = LvglThemeManager::GetInstance();
        auto current_theme = theme_manager.GetTheme("light");
        if (current_theme != nullptr) {
            auto text_font = current_theme->text_font();
            if (text_font != nullptr && text_font->font() != nullptr) {
                ESP_LOGI(TAG, "Custom font loaded successfully: line_height=%d", text_font->font()->line_height);
            } else {
                ESP_LOGW(TAG, "Custom font not loaded, using built-in font");
            }
        }
    }

public:

    ESP32P4FunctionEvBoard() : boot_button_(GPIO_NUM_35)
    {
        InitializeI2cBuses();
        // Audio is initialized by Es8311AudioCodec
        InitializeLCD();
        InitializeButtons();
        InitializeTouch();
        InitializeSdCard();
        ui_asset_service_preload_required();
        InitializeCamera();
        InitializeFonts();
        GetBacklight()->RestoreBrightness();
    }

    ~ESP32P4FunctionEvBoard()
    {
        if (touch_indev_ != nullptr) {
            lvgl_port_remove_touch(touch_indev_);
            touch_indev_ = nullptr;
        }
        if (tp_ != nullptr) {
            bsp_touch_delete();
            tp_ = nullptr;
        }
        // Clean up display pointer
        delete display_;
        display_ = nullptr;
        // If other resources need cleanup, add here
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, (i2c_port_t)BSP_I2C_NUM, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            BSP_I2S_MCLK, BSP_I2S_SCLK, BSP_I2S_LCLK, BSP_I2S_DOUT, BSP_I2S_DSIN,
            BSP_POWER_AMP_IO, ES8311_CODEC_DEFAULT_ADDR, true, false);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override { return display_; }

    virtual Backlight *GetBacklight() override
    {
        static PwmBacklight backlight(BSP_LCD_BACKLIGHT, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual Camera *GetCamera() override
    {
        return camera_;
    }
};

DECLARE_BOARD(ESP32P4FunctionEvBoard);
