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

#define TAG "ESP32P4_TF"

namespace {
constexpr char kSdMountPoint[] = "/sdcard";
constexpr char kUiAssetRoot[] = "/sdcard/xiaozhi_ui";
constexpr gpio_num_t kSdPowerEnableGpio = GPIO_NUM_45;  // SD_PWRn (active low)
constexpr int kSdLdoChannel = 4;                         // LDO_VO4
constexpr int kSdVoltageMv = 3300;
constexpr int kSdPowerOnDelayMs = 50;                    // BSP uses 50ms
constexpr int kSdMaxOpenFiles = 5;
constexpr size_t kSdAllocationUnitSize = 64 * 1024;

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

    // Step 1: Enable SD card power (BSP official path: esp_ldo_regulator + SD_PWRn=0)
    esp_err_t ret = EnablePower();
    if (ret != ESP_OK) {
        ReleasePower();
        return ret;
    }

    // Step 2: Configure SDMMC host (1-bit, SDMMC_FREQ_DEFAULT, NO pwr_ctrl_handle)
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    host.flags &= ~(SDMMC_HOST_FLAG_8BIT | SDMMC_HOST_FLAG_4BIT | SDMMC_HOST_FLAG_DDR);
    host.flags |= SDMMC_HOST_FLAG_1BIT;
    // IMPORTANT: Do NOT set host.pwr_ctrl_handle — the BSP official path leaves
    // it NULL. Setting it to sd_pwr_ctrl_by_on_chip_ldo caused ESP_ERR_TIMEOUT
    // because that LDO is for SD3.0 1.8V switching, not 3.3V main power.

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
    ret = esp_vfs_fat_sdmmc_mount(kSdMountPoint, &host, &slot, &mount_config, &card_);
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

esp_err_t EspP4TfCard::EnablePower()
{
    // Acquire LDO_VO4 at 3.3V via the regular LDO regulator API (BSP official path).
    if (ldo_chan_ == nullptr) {
        esp_ldo_channel_config_t ldo_config = {
            .chan_id = kSdLdoChannel,
            .voltage_mv = kSdVoltageMv,
        };
        esp_err_t ret = esp_ldo_acquire_channel(&ldo_config, &ldo_chan_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to acquire LDO_VO%d at %dmV: %s",
                     kSdLdoChannel, kSdVoltageMv, esp_err_to_name(ret));
            return ret;
        }
    }

    // Drive SD_PWRn (GPIO45) low to enable SD card power (active-low enable).
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
    ret = gpio_set_level(kSdPowerEnableGpio, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable SD_PWRn GPIO%d: %s",
                 kSdPowerEnableGpio, esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(kSdPowerOnDelayMs));

    ESP_LOGI(TAG, "SD card power enabled: GPIO45(SD_PWRn)=0, LDO_VO%d=%dmV",
             kSdLdoChannel, kSdVoltageMv);
    return ESP_OK;
}

void EspP4TfCard::ReleasePower()
{
    // Disable SD card power: SD_PWRn=1 (active-low disable), release LDO channel.
    gpio_set_level(kSdPowerEnableGpio, 1);

    if (ldo_chan_ != nullptr) {
        esp_err_t ret = esp_ldo_release_channel(ldo_chan_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to release LDO_VO%d: %s",
                     kSdLdoChannel, esp_err_to_name(ret));
        }
        ldo_chan_ = nullptr;
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
