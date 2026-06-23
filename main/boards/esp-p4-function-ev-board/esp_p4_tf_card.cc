#include "esp_p4_tf_card.h"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
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
constexpr int kSdPowerOnDelayMs = 500;
constexpr int kSdMaxOpenFiles = 5;
constexpr size_t kSdAllocationUnitSize = 64 * 1024;

extern const uint8_t _binary_alarm_siren_png_start[] asm("_binary_alarm_siren_png_start");
extern const uint8_t _binary_alarm_siren_png_end[] asm("_binary_alarm_siren_png_end");
extern const uint8_t _binary_color_wheel_220_png_start[] asm("_binary_color_wheel_220_png_start");
extern const uint8_t _binary_color_wheel_220_png_end[] asm("_binary_color_wheel_220_png_end");
extern const uint8_t _binary_floor_1_png_start[] asm("_binary_floor_1_png_start");
extern const uint8_t _binary_floor_1_png_end[] asm("_binary_floor_1_png_end");
extern const uint8_t _binary_floor_2_png_start[] asm("_binary_floor_2_png_start");
extern const uint8_t _binary_floor_2_png_end[] asm("_binary_floor_2_png_end");
extern const uint8_t _binary_floor_3_png_start[] asm("_binary_floor_3_png_start");
extern const uint8_t _binary_floor_3_png_end[] asm("_binary_floor_3_png_end");
extern const uint8_t _binary_logo_robot_png_start[] asm("_binary_logo_robot_png_start");
extern const uint8_t _binary_logo_robot_png_end[] asm("_binary_logo_robot_png_end");
extern const uint8_t _binary_overview_home_png_start[] asm("_binary_overview_home_png_start");
extern const uint8_t _binary_overview_home_png_end[] asm("_binary_overview_home_png_end");
extern const uint8_t _binary_scene_away_png_start[] asm("_binary_scene_away_png_start");
extern const uint8_t _binary_scene_away_png_end[] asm("_binary_scene_away_png_end");
extern const uint8_t _binary_scene_fire_png_start[] asm("_binary_scene_fire_png_start");
extern const uint8_t _binary_scene_fire_png_end[] asm("_binary_scene_fire_png_end");
extern const uint8_t _binary_scene_home_png_start[] asm("_binary_scene_home_png_start");
extern const uint8_t _binary_scene_home_png_end[] asm("_binary_scene_home_png_end");
extern const uint8_t _binary_scene_lights_png_start[] asm("_binary_scene_lights_png_start");
extern const uint8_t _binary_scene_lights_png_end[] asm("_binary_scene_lights_png_end");
extern const uint8_t _binary_scene_movie_png_start[] asm("_binary_scene_movie_png_start");
extern const uint8_t _binary_scene_movie_png_end[] asm("_binary_scene_movie_png_end");
extern const uint8_t _binary_scene_night_png_start[] asm("_binary_scene_night_png_start");
extern const uint8_t _binary_scene_night_png_end[] asm("_binary_scene_night_png_end");
extern const uint8_t _binary_scene_rain_png_start[] asm("_binary_scene_rain_png_start");
extern const uint8_t _binary_scene_rain_png_end[] asm("_binary_scene_rain_png_end");
extern const uint8_t _binary_scene_sleep_png_start[] asm("_binary_scene_sleep_png_start");
extern const uint8_t _binary_scene_sleep_png_end[] asm("_binary_scene_sleep_png_end");
extern const uint8_t _binary_siyin_logo_png_start[] asm("_binary_siyin_logo_png_start");
extern const uint8_t _binary_siyin_logo_png_end[] asm("_binary_siyin_logo_png_end");
extern const uint8_t _binary_weather_cloud_png_start[] asm("_binary_weather_cloud_png_start");
extern const uint8_t _binary_weather_cloud_png_end[] asm("_binary_weather_cloud_png_end");
extern const uint8_t _binary_weather_guangzhou_png_start[] asm("_binary_weather_guangzhou_png_start");
extern const uint8_t _binary_weather_guangzhou_png_end[] asm("_binary_weather_guangzhou_png_end");

struct EmbeddedAsset {
    const char *name;
    const uint8_t *start;
    const uint8_t *end;
};

constexpr EmbeddedAsset kDefaultUiAssets[] = {
    {"alarm_siren.png", _binary_alarm_siren_png_start, _binary_alarm_siren_png_end},
    {"color_wheel_220.png", _binary_color_wheel_220_png_start, _binary_color_wheel_220_png_end},
    {"floor_1.png", _binary_floor_1_png_start, _binary_floor_1_png_end},
    {"floor_2.png", _binary_floor_2_png_start, _binary_floor_2_png_end},
    {"floor_3.png", _binary_floor_3_png_start, _binary_floor_3_png_end},
    {"logo_robot.png", _binary_logo_robot_png_start, _binary_logo_robot_png_end},
    {"overview_home.png", _binary_overview_home_png_start, _binary_overview_home_png_end},
    {"scene_away.png", _binary_scene_away_png_start, _binary_scene_away_png_end},
    {"scene_fire.png", _binary_scene_fire_png_start, _binary_scene_fire_png_end},
    {"scene_home.png", _binary_scene_home_png_start, _binary_scene_home_png_end},
    {"scene_lights.png", _binary_scene_lights_png_start, _binary_scene_lights_png_end},
    {"scene_movie.png", _binary_scene_movie_png_start, _binary_scene_movie_png_end},
    {"scene_night.png", _binary_scene_night_png_start, _binary_scene_night_png_end},
    {"scene_rain.png", _binary_scene_rain_png_start, _binary_scene_rain_png_end},
    {"scene_sleep.png", _binary_scene_sleep_png_start, _binary_scene_sleep_png_end},
    {"siyin_logo.png", _binary_siyin_logo_png_start, _binary_siyin_logo_png_end},
    {"weather_cloud.png", _binary_weather_cloud_png_start, _binary_weather_cloud_png_end},
    {"weather_guangzhou.png", _binary_weather_guangzhou_png_start, _binary_weather_guangzhou_png_end},
};

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

bool IsPngHeader(const uint8_t *data, size_t size)
{
    static constexpr uint8_t kPngMagic[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    return data && size >= sizeof(kPngMagic) && memcmp(data, kPngMagic, sizeof(kPngMagic)) == 0;
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

    // Step 2: Configure SDMMC host (1-bit, conservative frequency, NO pwr_ctrl_handle)
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_PROBING;
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
    LogCardInfo();
    EnsureUiAssets();
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

void EspP4TfCard::LogCardInfo() const
{
    if (card_ == nullptr) {
        return;
    }

    const uint64_t bytes = (uint64_t)card_->csd.capacity * card_->csd.sector_size;
    const uint32_t mib = (uint32_t)(bytes / (1024ULL * 1024ULL));
    const uint32_t bus_width = 1U << card_->log_bus_width;
    ESP_LOGI(TAG, "TF card: name=%s speed=%.2fMHz size=%uMiB sector=%u capacity=%u bus_width=%u",
             card_->cid.name,
             card_->real_freq_khz / 1000.0f,
             mib,
             (unsigned)card_->csd.sector_size,
             (unsigned)card_->csd.capacity,
             (unsigned)bus_width);
}

bool EspP4TfCard::EnsureUiAssets() const
{
    struct stat st;
    if (stat(kUiAssetRoot, &st) != 0) {
        if (mkdir(kUiAssetRoot, 0775) != 0 && errno != EEXIST) {
            ESP_LOGW(TAG, "Failed to create UI asset folder %s (errno=%d)", kUiAssetRoot, errno);
            return false;
        }
        ESP_LOGI(TAG, "Created UI asset folder: %s", kUiAssetRoot);
        vTaskDelay(pdMS_TO_TICKS(50));
    } else if (!S_ISDIR(st.st_mode)) {
        ESP_LOGW(TAG, "UI asset path exists but is not a directory: %s", kUiAssetRoot);
        return false;
    }

    int written = 0;
    int preserved = 0;
    for (const auto &asset : kDefaultUiAssets) {
        const size_t size = (size_t)(asset.end - asset.start);
        char path[160];
        snprintf(path, sizeof(path), "%s/%s", kUiAssetRoot, asset.name);

        bool should_write = true;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
            should_write = false;
            preserved++;
        }
        if (!should_write) {
            continue;
        }

        if (!IsPngHeader(asset.start, size)) {
            ESP_LOGW(TAG, "Embedded default asset is not PNG: %s", asset.name);
            continue;
        }

        FILE *f = fopen(path, "wb");
        if (!f) {
            ESP_LOGW(TAG, "Failed to create default UI asset %s (errno=%d)", path, errno);
            continue;
        }
        const size_t n = fwrite(asset.start, 1, size, f);
        const int close_ret = fclose(f);
        if (n != size || close_ret != 0) {
            ESP_LOGW(TAG, "Failed to write complete UI asset %s (%u/%u, close=%d errno=%d)",
                     asset.name, (unsigned)n, (unsigned)size, close_ret, errno);
            continue;
        }
        written++;
    }

    ESP_LOGI(TAG, "UI asset bootstrap complete: written=%d preserved=%d total=%u",
             written, preserved, (unsigned)(sizeof(kDefaultUiAssets) / sizeof(kDefaultUiAssets[0])));
    return true;
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
