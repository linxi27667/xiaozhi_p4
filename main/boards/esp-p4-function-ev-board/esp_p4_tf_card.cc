#include "esp_p4_tf_card.h"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

#include <driver/gpio.h>
#include <driver/sdmmc_host.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "bsp/esp32_p4_function_ev_board.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

#define TAG "ESP32P4_TF"

namespace {
constexpr char kSdMountPoint[] = "/sdcard";
constexpr char kUiAssetRoot[] = "/sdcard/xiaozhi_ui";
constexpr gpio_num_t kSdPowerEnableGpio = GPIO_NUM_45;
constexpr int kSdLdoChannel = 4;
constexpr int kSdVoltageMv = 3300;
constexpr int kSdPowerOffDelayMs = 150;
constexpr int kSdPowerOnDelayMs = 800;
constexpr int kSdMaxOpenFiles = 8;
constexpr size_t kSdAllocationUnitSize = 16 * 1024;

esp_vfs_fat_sdmmc_mount_config_t MakeMountConfig()
{
    return {
        .format_if_mount_failed = false,
        .max_files = kSdMaxOpenFiles,
        .allocation_unit_size = kSdAllocationUnitSize,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };
}
}

EspP4TfCard::~EspP4TfCard()
{
    Unmount();
}

esp_err_t EspP4TfCard::Mount()
{
    if (mounted_) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing TF card");

    esp_err_t ret = EnsurePowerController();
    if (ret != ESP_OK) {
        ReleasePower();
        return ret;
    }

    PrepareSdPins();

    ret = PowerCycleCard();
    if (ret != ESP_OK) {
        ReleasePower();
        return ret;
    }

    ret = MountSdmmc();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TF card SDMMC mount failed: %s", esp_err_to_name(ret));
        ReleasePower();
        return ret;
    }

    mounted_ = true;
    ESP_LOGI(TAG, "TF card mounted successfully at %s (SDMMC slot0, 1-bit)", kSdMountPoint);
    sdmmc_card_print_info(stdout, card_);
    VerifyAssetFolder();
    return ESP_OK;
}

void EspP4TfCard::Unmount()
{
    if (card_ != nullptr) {
        esp_err_t ret = esp_vfs_fat_sdcard_unmount(kSdMountPoint, card_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to unmount TF card: %s", esp_err_to_name(ret));
        }
        card_ = nullptr;
    }

    mounted_ = false;
    ReleasePower();
}

esp_err_t EspP4TfCard::EnsurePowerController()
{
    if (pwr_ctrl_ != nullptr) {
        return ESP_OK;
    }

    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = kSdLdoChannel,
    };

    esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create SD LDO_VO%d power controller: %s",
                 kSdLdoChannel, esp_err_to_name(ret));
        return ret;
    }

    ret = sd_pwr_ctrl_set_io_voltage(pwr_ctrl_, kSdVoltageMv);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to pre-bias SD LDO_VO%d to %dmV: %s",
                 kSdLdoChannel, kSdVoltageMv, esp_err_to_name(ret));
        ReleasePower();
        return ret;
    }

    ESP_LOGI(TAG, "SD LDO_VO%d prepared at %dmV", kSdLdoChannel, kSdVoltageMv);
    return ESP_OK;
}

esp_err_t EspP4TfCard::PowerCycleCard()
{
    const gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << kSdPowerEnableGpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure SD_PWRn GPIO%d: %s",
                 kSdPowerEnableGpio, esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_set_level(kSdPowerEnableGpio, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable SD_PWRn GPIO%d: %s",
                 kSdPowerEnableGpio, esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(kSdPowerOffDelayMs));

    ret = gpio_set_level(kSdPowerEnableGpio, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable SD_PWRn GPIO%d: %s",
                 kSdPowerEnableGpio, esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(kSdPowerOnDelayMs));

    ESP_LOGI(TAG, "TF power cycled: GPIO45(SD_PWRn)=0, LDO_VO%d=%dmV",
             kSdLdoChannel, kSdVoltageMv);
    return ESP_OK;
}

void EspP4TfCard::PrepareSdPins()
{
    auto configure_line = [](gpio_num_t gpio, bool pullup) {
        if (gpio == GPIO_NUM_NC) {
            return;
        }

        gpio_pulldown_dis(gpio);
        if (pullup) {
            gpio_pullup_en(gpio);
        } else {
            gpio_pullup_dis(gpio);
        }
        gpio_set_drive_capability(gpio, GPIO_DRIVE_CAP_3);
    };

    configure_line(BSP_SD_CLK, false);
    configure_line(BSP_SD_CMD, true);
    configure_line(BSP_SD_D0, true);
    configure_line(BSP_SD_D1, true);
    configure_line(BSP_SD_D2, true);
    configure_line(BSP_SD_D3, true);
}

esp_err_t EspP4TfCard::MountSdmmc()
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_PROBING;
    host.flags &= ~(SDMMC_HOST_FLAG_8BIT | SDMMC_HOST_FLAG_4BIT | SDMMC_HOST_FLAG_DDR);
    host.flags |= SDMMC_HOST_FLAG_1BIT;
    host.pwr_ctrl_handle = pwr_ctrl_;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.clk = BSP_SD_CLK;
    slot.cmd = BSP_SD_CMD;
    slot.d0 = BSP_SD_D0;
    slot.d1 = BSP_SD_D1;
    slot.d2 = BSP_SD_D2;
    slot.d3 = BSP_SD_D3;
    slot.cd = SDMMC_SLOT_NO_CD;
    slot.wp = SDMMC_SLOT_NO_WP;
    slot.width = 1;
    slot.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    auto mount_config = MakeMountConfig();
    ESP_LOGI(TAG, "Mounting TF card via SDMMC slot 0, 1-bit, %dkHz: CLK=%d CMD=%d D0=%d",
             host.max_freq_khz, BSP_SD_CLK, BSP_SD_CMD, BSP_SD_D0);

    card_ = nullptr;
    return esp_vfs_fat_sdmmc_mount(kSdMountPoint, &host, &slot, &mount_config, &card_);
}

void EspP4TfCard::ReleasePower()
{
    gpio_set_level(kSdPowerEnableGpio, 1);

    if (pwr_ctrl_ != nullptr) {
        esp_err_t ret = sd_pwr_ctrl_del_on_chip_ldo(pwr_ctrl_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to release SD LDO_VO%d: %s",
                     kSdLdoChannel, esp_err_to_name(ret));
        }
        pwr_ctrl_ = nullptr;
    }
}

void EspP4TfCard::LogRootEntries() const
{
    DIR *dir = opendir(kSdMountPoint);
    if (dir == nullptr) {
        ESP_LOGW(TAG, "Cannot list %s after mount (errno=%d)", kSdMountPoint, errno);
        return;
    }

    ESP_LOGI(TAG, "TF card root listing:");
    struct dirent *ent = nullptr;
    int count = 0;
    while ((ent = readdir(dir)) != nullptr && count < 16) {
        ESP_LOGI(TAG, "  /sdcard/%s", ent->d_name);
        count++;
    }
    closedir(dir);
}

void EspP4TfCard::VerifyAssetFolder() const
{
    struct stat st;
    if (stat(kUiAssetRoot, &st) != 0) {
        ESP_LOGW(TAG, "TF card mounted, but UI asset folder is missing: %s (errno=%d)",
                 kUiAssetRoot, errno);
        ESP_LOGW(TAG, "Expected TF layout: /sdcard/xiaozhi_ui/*.png");
        LogRootEntries();
        return;
    }

    ESP_LOGI(TAG, "UI asset folder found: %s", kUiAssetRoot);
}
