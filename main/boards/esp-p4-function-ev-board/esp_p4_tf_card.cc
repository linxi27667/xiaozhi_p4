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
constexpr char kUiAssetVersionPath[] = "/sdcard/xiaozhi_ui/.xiaozhi_assets_version";
constexpr char kUiAssetVersion[] = "premium-2026-06-v3\n";
constexpr gpio_num_t kSdPowerEnableGpio = GPIO_NUM_45;  // SD_PWRn (active low)
constexpr int kSdLdoChannel = 4;                         // LDO_VO4
constexpr int kSdVoltageMv = 3300;
constexpr int kSdPowerOffDelayMs = 200;
constexpr int kSdPowerOnDelayMs = 500;
constexpr int kSdMountAttempts = 2;
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
extern const uint8_t _binary_weather_xiamen_png_start[] asm("_binary_weather_xiamen_png_start");
extern const uint8_t _binary_weather_xiamen_png_end[] asm("_binary_weather_xiamen_png_end");

struct EmbeddedAsset {
    const char *name;
    const uint8_t *start;
    const uint8_t *end;
};

struct LegacyAssetFingerprint {
    const char *name;
    size_t size;
    uint32_t crc32;
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
    {"weather_xiamen.png", _binary_weather_xiamen_png_start, _binary_weather_xiamen_png_end},
};

constexpr LegacyAssetFingerprint kLegacyDefaultAssets[] = {
    {"alarm_siren.png", 1428, 0xf960d25d},
    {"color_wheel_220.png", 20412, 0xbc165748},
    {"floor_1.png", 981, 0x78525619},
    {"floor_2.png", 999, 0xbd955424},
    {"floor_3.png", 1005, 0x5e49d0b8},
    {"logo_robot.png", 5188, 0x7a3bc676},
    {"overview_home.png", 1838, 0xb83c74b6},
    {"scene_away.png", 671, 0x53a9beb0},
    {"scene_fire.png", 2523, 0xebadac3d},
    {"scene_home.png", 724, 0x10f4a231},
    {"scene_lights.png", 786, 0x9d3c00d9},
    {"scene_movie.png", 652, 0x9ab003ca},
    {"scene_night.png", 831, 0x28d5a025},
    {"scene_rain.png", 1485, 0x836c3bc8},
    {"scene_sleep.png", 772, 0xa52d7812},
    {"siyin_logo.png", 9857, 0xe6aeff9a},
    {"weather_cloud.png", 1511, 0x6472981f},
    {"weather_xiamen.png", 1381, 0x205aa4b5},
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

uint32_t Crc32Update(uint32_t crc, const uint8_t *data, size_t size)
{
    while (size--) {
        crc ^= *data++;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

const LegacyAssetFingerprint *FindLegacyAsset(const char *name)
{
    for (const auto &asset : kLegacyDefaultAssets) {
        if (strcmp(asset.name, name) == 0) {
            return &asset;
        }
    }
    return nullptr;
}

bool FileCrc32(const char *path, uint32_t *out_crc)
{
    if (out_crc == nullptr) {
        return false;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }

    uint8_t buf[512];
    uint32_t crc = 0xFFFFFFFFU;
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        crc = Crc32Update(crc, buf, n);
    }
    const bool ok = ferror(f) == 0;
    fclose(f);
    if (!ok) {
        return false;
    }

    *out_crc = ~crc;
    return true;
}

bool IsLegacyDefaultAssetFile(const char *path, const char *name, size_t size)
{
    const auto *legacy = FindLegacyAsset(name);
    if (legacy == nullptr || legacy->size != size) {
        return false;
    }

    uint32_t crc = 0;
    return FileCrc32(path, &crc) && crc == legacy->crc32;
}

bool UiAssetVersionCurrent()
{
    FILE *f = fopen(kUiAssetVersionPath, "rb");
    if (!f) {
        return false;
    }

    char buf[64] = {};
    const size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    return n == strlen(kUiAssetVersion) && strcmp(buf, kUiAssetVersion) == 0;
}

void WriteUiAssetVersion()
{
    FILE *f = fopen(kUiAssetVersionPath, "wb");
    if (!f) {
        ESP_LOGW(TAG, "Failed to write UI asset version marker (errno=%d)", errno);
        return;
    }

    const size_t size = strlen(kUiAssetVersion);
    const size_t n = fwrite(kUiAssetVersion, 1, size, f);
    const int close_ret = fclose(f);
    if (n != size || close_ret != 0) {
        ESP_LOGW(TAG, "Failed to persist UI asset version marker (%u/%u close=%d errno=%d)",
                 (unsigned)n, (unsigned)size, close_ret, errno);
    }
}

bool WriteEmbeddedAssetFile(const char *path, const EmbeddedAsset &asset)
{
    const size_t size = (size_t)(asset.end - asset.start);
    char tmp_path[192];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    for (int attempt = 1; attempt <= 3; attempt++) {
        FILE *f = fopen(tmp_path, "wb");
        if (!f) {
            ESP_LOGW(TAG, "Failed to open temp UI asset %s (attempt=%d errno=%d)",
                     tmp_path, attempt, errno);
            vTaskDelay(pdMS_TO_TICKS(150));
            continue;
        }

        const size_t n = fwrite(asset.start, 1, size, f);
        const int flush_ret = fflush(f);
        const int close_ret = fclose(f);
        if (n == size && flush_ret == 0 && close_ret == 0) {
            remove(path);
            if (rename(tmp_path, path) == 0) {
                vTaskDelay(pdMS_TO_TICKS(80));
                return true;
            }
            ESP_LOGW(TAG, "Failed to replace UI asset %s (attempt=%d errno=%d)",
                     path, attempt, errno);
        } else {
            ESP_LOGW(TAG, "Failed to write temp UI asset %s (%u/%u flush=%d close=%d attempt=%d errno=%d)",
                     tmp_path, (unsigned)n, (unsigned)size, flush_ret, close_ret, attempt, errno);
        }

        remove(tmp_path);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    return false;
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

    // Configure SDMMC host (1-bit, conservative frequency, NO pwr_ctrl_handle).
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

    esp_err_t ret = ESP_FAIL;
    for (int attempt = 1; attempt <= kSdMountAttempts; ++attempt) {
        ret = EnablePower();
        if (ret == ESP_OK) {
            card_ = nullptr;
            ret = esp_vfs_fat_sdmmc_mount(kSdMountPoint, &host, &slot,
                                          &mount_config, &card_);
        }
        if (ret == ESP_OK) {
            break;
        }

        ESP_LOGW(TAG, "TF card mount attempt %d/%d failed: %s",
                 attempt, kSdMountAttempts, esp_err_to_name(ret));
        card_ = nullptr;
        ReleasePower();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TF card SDMMC mount failed after %d attempts: %s",
                 kSdMountAttempts, esp_err_to_name(ret));
        return ret;
    }

    mounted_ = true;
    ESP_LOGI(TAG, "TF card mounted successfully at %s (SDMMC slot0, 1-bit)", kSdMountPoint);
    LogCardInfo();
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
    // A CPU reset does not necessarily remove power from the external TF card.
    // Force SD_PWRn high first so a card left in a bad transfer state receives
    // the same hardware reset it would get from a full board power cycle.
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

    // Acquire LDO_VO4 at 3.3V via the regular LDO regulator API (BSP official path).
    if (ldo_chan_ == nullptr) {
        esp_ldo_channel_config_t ldo_config = {
            .chan_id = kSdLdoChannel,
            .voltage_mv = kSdVoltageMv,
        };
        ret = esp_ldo_acquire_channel(&ldo_config, &ldo_chan_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to acquire LDO_VO%d at %dmV: %s",
                     kSdLdoChannel, kSdVoltageMv, esp_err_to_name(ret));
            return ret;
        }
    }

    ret = gpio_set_level(kSdPowerEnableGpio, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable SD_PWRn GPIO%d: %s",
                 kSdPowerEnableGpio, esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(kSdPowerOnDelayMs));

    ESP_LOGI(TAG, "SD warm-reset recovery complete: GPIO45 off=%dms on=%dms, LDO_VO%d=%dmV",
             kSdPowerOffDelayMs, kSdPowerOnDelayMs, kSdLdoChannel, kSdVoltageMv);
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

    const bool assets_current = UiAssetVersionCurrent();
    int written = 0;
    int upgraded = 0;
    int preserved = 0;
    int custom_preserved = 0;
    bool all_writes_ok = true;
    for (const auto &asset : kDefaultUiAssets) {
        const size_t size = (size_t)(asset.end - asset.start);
        char path[160];
        snprintf(path, sizeof(path), "%s/%s", kUiAssetRoot, asset.name);

        bool should_write = true;
        bool is_upgrade = false;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
            if (!assets_current && IsLegacyDefaultAssetFile(path, asset.name, (size_t)st.st_size)) {
                should_write = true;
                is_upgrade = true;
            } else {
                should_write = false;
                preserved++;
                if (!assets_current) {
                    custom_preserved++;
                }
            }
        }
        if (!should_write) {
            continue;
        }

        if (!IsPngHeader(asset.start, size)) {
            ESP_LOGW(TAG, "Embedded default asset is not PNG: %s", asset.name);
            continue;
        }

        if (!WriteEmbeddedAssetFile(path, asset)) {
            ESP_LOGW(TAG, "Failed to persist default UI asset %s", asset.name);
            all_writes_ok = false;
            continue;
        }
        if (is_upgrade) {
            upgraded++;
        } else {
            written++;
        }
    }

    if (all_writes_ok) {
        WriteUiAssetVersion();
    } else {
        ESP_LOGW(TAG, "UI asset version marker not updated because one or more writes failed");
    }
    ESP_LOGI(TAG, "UI asset bootstrap complete: written=%d upgraded=%d preserved=%d custom=%d total=%u",
             written, upgraded, preserved, custom_preserved,
             (unsigned)(sizeof(kDefaultUiAssets) / sizeof(kDefaultUiAssets[0])));
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
