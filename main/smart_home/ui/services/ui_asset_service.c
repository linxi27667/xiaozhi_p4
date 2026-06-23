#include "ui_asset_service.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "draw/lv_image_decoder_private.h"
#include "misc/cache/instance/lv_image_cache.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

static const char *TAG = "UI_ASSET";
static lv_fs_drv_t s_drv;
static bool s_inited;
static bool s_preload_done;
static bool s_assets_unavailable;
static bool s_sdcard_missing_logged;
static bool s_tf_override_enabled;
static int s_preload_count;
static uint32_t s_preload_bytes;
static char s_path_buf[160];
static char s_src_buf[96];

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

typedef struct {
    const char *name;
    const uint8_t *embedded_start;
    const uint8_t *embedded_end;
    long legacy_size;
} ui_asset_def_t;

static const ui_asset_def_t s_required_assets[] = {
    {"siyin_logo.png", _binary_siyin_logo_png_start, _binary_siyin_logo_png_end, 9857},
    {"overview_home.png", _binary_overview_home_png_start, _binary_overview_home_png_end, 1838},
    {"weather_guangzhou.png", _binary_weather_guangzhou_png_start, _binary_weather_guangzhou_png_end, 1381},
    {"weather_cloud.png", _binary_weather_cloud_png_start, _binary_weather_cloud_png_end, 1511},
    {"floor_1.png", _binary_floor_1_png_start, _binary_floor_1_png_end, 981},
    {"floor_2.png", _binary_floor_2_png_start, _binary_floor_2_png_end, 999},
    {"floor_3.png", _binary_floor_3_png_start, _binary_floor_3_png_end, 1005},
    {"alarm_siren.png", _binary_alarm_siren_png_start, _binary_alarm_siren_png_end, 1428},
    {"logo_robot.png", _binary_logo_robot_png_start, _binary_logo_robot_png_end, 2271},
    {"scene_lights.png", _binary_scene_lights_png_start, _binary_scene_lights_png_end, 786},
    {"scene_home.png", _binary_scene_home_png_start, _binary_scene_home_png_end, 724},
    {"scene_away.png", _binary_scene_away_png_start, _binary_scene_away_png_end, 671},
    {"scene_sleep.png", _binary_scene_sleep_png_start, _binary_scene_sleep_png_end, 772},
    {"scene_movie.png", _binary_scene_movie_png_start, _binary_scene_movie_png_end, 652},
    {"scene_night.png", _binary_scene_night_png_start, _binary_scene_night_png_end, 831},
    {"scene_rain.png", _binary_scene_rain_png_start, _binary_scene_rain_png_end, 1485},
    {"scene_fire.png", _binary_scene_fire_png_start, _binary_scene_fire_png_end, 2523},
    {"color_wheel_220.png", _binary_color_wheel_220_png_start, _binary_color_wheel_220_png_end, 20412},
};

#define UI_ASSET_COUNT (sizeof(s_required_assets) / sizeof(s_required_assets[0]))

typedef struct {
    const char *name;
    uint8_t *data;
    uint32_t data_size;
    uint8_t *decoded_data;
    uint32_t decoded_size;
    lv_image_dsc_t decoded_dsc;
    long file_size;
    time_t file_mtime;
    bool valid;
    bool decoded_valid;
} ui_asset_cache_entry_t;

static ui_asset_cache_entry_t s_asset_cache[UI_ASSET_COUNT];

typedef struct {
    lv_image_dsc_t dsc;
    uint8_t *data;
    uint32_t data_size;
    char name[64];
    char src[96];
    bool owns_data;
} ui_asset_image_payload_t;

static void build_host_path(const char *path, char *out, size_t out_size)
{
    if (!path) path = "";
    while (*path == '/' || *path == '\\') path++;
    snprintf(out, out_size, "%s/%s", UI_ASSET_ROOT, path);
}

static void *asset_malloc(size_t size)
{
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    return p;
}

static bool read_asset_file(const char *name, uint8_t **data, uint32_t *data_size)
{
    if (!name || !data || !data_size) return false;

    char host_path[160];
    build_host_path(name, host_path, sizeof(host_path));

    FILE *f = fopen(host_path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "asset fopen FAIL: %s (errno=%d)", host_path, errno);
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        ESP_LOGW(TAG, "asset seek end FAIL: %s (errno=%d)", host_path, errno);
        fclose(f);
        return false;
    }

    long size = ftell(f);
    if (size <= 0 || size > 512 * 1024) {
        ESP_LOGW(TAG, "asset invalid size: %s (%ld bytes)", host_path, size);
        fclose(f);
        return false;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        ESP_LOGW(TAG, "asset seek start FAIL: %s (errno=%d)", host_path, errno);
        fclose(f);
        return false;
    }

    uint8_t *buf = (uint8_t *)asset_malloc((size_t)size);
    if (!buf) {
        ESP_LOGE(TAG, "asset malloc FAIL: %s (%ld bytes)", host_path, size);
        fclose(f);
        return false;
    }

    size_t read_len = fread(buf, 1, (size_t)size, f);
    int read_errno = errno;
    fclose(f);

    if (read_len != (size_t)size) {
        ESP_LOGW(TAG, "asset read FAIL: %s (%u/%ld errno=%d)",
                 host_path, (unsigned)read_len, size, read_errno);
        heap_caps_free(buf);
        return false;
    }

    *data = buf;
    *data_size = (uint32_t)size;
    return true;
}

static bool asset_is_png(const uint8_t *data, uint32_t data_size)
{
    static const uint8_t magic[] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    return data && data_size >= sizeof(magic) && memcmp(data, magic, sizeof(magic)) == 0;
}

static bool stat_asset_file(const char *name, struct stat *st)
{
    if (!name || !st) return false;

    char host_path[160];
    build_host_path(name, host_path, sizeof(host_path));
    return stat(host_path, st) == 0 && S_ISREG(st->st_mode);
}

static bool tf_override_marker_exists(void)
{
    struct stat st;
    return stat(UI_ASSET_OVERRIDE_MARKER, &st) == 0 && S_ISREG(st.st_mode);
}

static ui_asset_cache_entry_t *find_cache_entry(const char *name)
{
    if (!name || !name[0]) return NULL;
    for (size_t i = 0; i < UI_ASSET_COUNT; i++) {
        if (s_asset_cache[i].name && strcmp(s_asset_cache[i].name, name) == 0) {
            return &s_asset_cache[i];
        }
    }
    return NULL;
}

static const ui_asset_def_t *find_asset_def(const char *name)
{
    if (!name || !name[0]) return NULL;
    for (size_t i = 0; i < UI_ASSET_COUNT; i++) {
        if (strcmp(s_required_assets[i].name, name) == 0) {
            return &s_required_assets[i];
        }
    }
    return NULL;
}

static void invalidate_cache_entry(ui_asset_cache_entry_t *entry, const char *reason)
{
    if (!entry || !entry->valid) return;

    ESP_LOGI(TAG, "asset cache invalidate: %s (%s)",
             entry->name ? entry->name : "(null)", reason ? reason : "changed");
    if (entry->name) {
        char src[96];
        snprintf(src, sizeof(src), "%c:%s", UI_ASSET_DRIVE_LETTER, entry->name);
        lv_image_cache_drop(src);
    }
    if (entry->decoded_valid && entry->decoded_dsc.data) {
        lv_image_cache_drop(&entry->decoded_dsc);
    }
    heap_caps_free(entry->data);
    heap_caps_free(entry->decoded_data);
    entry->data = NULL;
    entry->data_size = 0;
    entry->decoded_data = NULL;
    entry->decoded_size = 0;
    memset(&entry->decoded_dsc, 0, sizeof(entry->decoded_dsc));
    entry->file_size = 0;
    entry->file_mtime = 0;
    entry->valid = false;
    entry->decoded_valid = false;
}

static bool cache_entry_matches_file(ui_asset_cache_entry_t *entry)
{
    if (!entry || !entry->valid || !entry->name) return false;
    if (entry->file_size < 0) return true;

    struct stat st;
    if (!stat_asset_file(entry->name, &st)) {
        invalidate_cache_entry(entry, "source missing");
        return false;
    }

    if (entry->file_size != (long)st.st_size || entry->file_mtime != st.st_mtime) {
        invalidate_cache_entry(entry, "source replaced");
        return false;
    }
    return true;
}

static void update_cache_metadata(ui_asset_cache_entry_t *entry)
{
    if (!entry || !entry->name) return;

    struct stat st;
    if (stat_asset_file(entry->name, &st)) {
        entry->file_size = (long)st.st_size;
        entry->file_mtime = st.st_mtime;
    }
}

static bool decode_asset_to_cache(ui_asset_cache_entry_t *entry,
                                  const uint8_t *data,
                                  uint32_t data_size)
{
    if (!entry || !data || data_size == 0 || !asset_is_png(data, data_size)) {
        return false;
    }

    lv_image_dsc_t tmp_dsc;
    memset(&tmp_dsc, 0, sizeof(tmp_dsc));
    tmp_dsc.data = data;
    tmp_dsc.data_size = data_size;
    tmp_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    tmp_dsc.header.cf = LV_COLOR_FORMAT_UNKNOWN;

    lv_image_decoder_dsc_t decoder_dsc;
    memset(&decoder_dsc, 0, sizeof(decoder_dsc));
    lv_image_decoder_args_t decoder_args = {
        .no_cache = true,
    };

    lv_result_t open_res = lv_image_decoder_open(&decoder_dsc, &tmp_dsc, &decoder_args);
    if (open_res != LV_RESULT_OK || decoder_dsc.decoded == NULL) {
        if (open_res == LV_RESULT_OK) {
            lv_image_decoder_close(&decoder_dsc);
        }
        return false;
    }

    const lv_draw_buf_t *decoded_buf = decoder_dsc.decoded;
    const uint32_t decoded_size = decoded_buf->data_size;
    const uint8_t *decoded_data = decoded_buf->data;
    if (decoded_size == 0 || decoded_data == NULL) {
        lv_image_decoder_close(&decoder_dsc);
        return false;
    }

    uint8_t *cached = (uint8_t *)asset_malloc(decoded_size);
    if (!cached) {
        ESP_LOGW(TAG, "decode cache malloc FAIL: %s (%lu bytes)",
                 entry->name ? entry->name : "(null)", (unsigned long)decoded_size);
        lv_image_decoder_close(&decoder_dsc);
        return false;
    }

    memcpy(cached, decoded_data, decoded_size);
    if (entry->decoded_valid && entry->decoded_dsc.data) {
        lv_image_cache_drop(&entry->decoded_dsc);
    }
    heap_caps_free(entry->decoded_data);

    entry->decoded_data = cached;
    entry->decoded_size = decoded_size;
    memset(&entry->decoded_dsc, 0, sizeof(entry->decoded_dsc));
    entry->decoded_dsc.data = cached;
    entry->decoded_dsc.data_size = decoded_size;
    entry->decoded_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    entry->decoded_dsc.header.cf = decoded_buf->header.cf;
    entry->decoded_dsc.header.w = decoded_buf->header.w;
    entry->decoded_dsc.header.h = decoded_buf->header.h;
    entry->decoded_dsc.header.stride = decoded_buf->header.stride;
    entry->decoded_valid = true;

    lv_image_decoder_close(&decoder_dsc);
    ESP_LOGD(TAG, "asset decoded cache ready: %s size=%ldx%ld bytes=%lu",
             entry->name ? entry->name : "(null)",
             (long)entry->decoded_dsc.header.w,
             (long)entry->decoded_dsc.header.h,
             (unsigned long)decoded_size);
    return true;
}

static bool decode_embedded_asset_to_cache(ui_asset_cache_entry_t *entry)
{
    if (!entry || !entry->name) return false;

    const ui_asset_def_t *def = find_asset_def(entry->name);
    if (!def || !def->embedded_start || !def->embedded_end ||
        def->embedded_end <= def->embedded_start) {
        return false;
    }

    const uint32_t size = (uint32_t)(def->embedded_end - def->embedded_start);
    if (!decode_asset_to_cache(entry, def->embedded_start, size)) {
        ESP_LOGW(TAG, "embedded asset decode FAIL: %s", entry->name);
        return false;
    }

    heap_caps_free(entry->data);
    entry->data = NULL;
    entry->data_size = 0;
    entry->file_size = -1;
    entry->file_mtime = 0;
    entry->valid = true;
    return true;
}

static bool asset_is_legacy_default_size(const char *name, long size)
{
    const ui_asset_def_t *def = find_asset_def(name);
    return def && def->legacy_size > 0 && def->legacy_size == size;
}

static bool copy_cached_asset(const char *name, uint8_t **data, uint32_t *data_size)
{
    ui_asset_cache_entry_t *entry = find_cache_entry(name);
    if (!cache_entry_matches_file(entry)) return false;
    if (!entry || !data || !data_size || !entry->data || entry->data_size == 0) return false;

    uint8_t *copy = (uint8_t *)asset_malloc(entry->data_size);
    if (!copy) {
        ESP_LOGE(TAG, "cache copy malloc FAIL: %s (%lu bytes)",
                 name, (unsigned long)entry->data_size);
        return false;
    }

    memcpy(copy, entry->data, entry->data_size);
    *data = copy;
    *data_size = entry->data_size;
    return true;
}

static bool ensure_sdcard_mounted(void)
{
    if (s_assets_unavailable) {
        struct stat sd = {0};
        struct stat root = {0};
        if (stat("/sdcard", &sd) == 0 && stat(UI_ASSET_ROOT, &root) == 0 && S_ISDIR(root.st_mode)) {
            ESP_LOGI(TAG, "TF assets became available again: %s", UI_ASSET_ROOT);
            s_assets_unavailable = false;
            s_preload_done = false;
        } else {
            return false;
        }
    }

    struct stat st;
    if (stat("/sdcard", &st) == 0) {
        s_sdcard_missing_logged = false;
        return true;
    }

    if (!s_sdcard_missing_logged) {
        ESP_LOGW(TAG, "/sdcard is not mounted; board initialization owns TF card mounting");
        s_sdcard_missing_logged = true;
    }
    return false;
}

static void diagnose_sdcard_root(void)
{
    struct stat st;
    if (stat("/sdcard", &st) != 0) {
        ESP_LOGE(TAG, "/sdcard mount point MISSING (errno=%d)", errno);
        return;
    }

    ESP_LOGI(TAG, "/sdcard mount point OK");

    DIR *dir = opendir("/sdcard");
    if (!dir) {
        ESP_LOGE(TAG, "opendir FAIL: /sdcard (errno=%d)", errno);
        return;
    }

    ESP_LOGI(TAG, "Listing /sdcard root:");
    struct dirent *ent;
    int count = 0;
    while ((ent = readdir(dir)) != NULL && count < 24) {
        ESP_LOGI(TAG, "  [%d] %s", count, ent->d_name);
        count++;
    }
    closedir(dir);
    ESP_LOGI(TAG, "/sdcard root entries shown: %d", count);
}

static void asset_image_delete_cb(lv_event_t *e)
{
    ui_asset_image_payload_t *payload =
        (ui_asset_image_payload_t *)lv_event_get_user_data(e);
    if (!payload) return;

    if (payload->owns_data && payload->dsc.data) {
        lv_image_cache_drop(&payload->dsc);
    } else if (payload->owns_data && payload->src[0]) {
        lv_image_cache_drop(payload->src);
    }
    if (payload->owns_data) {
        heap_caps_free(payload->data);
    }
    heap_caps_free(payload);
}

static ui_asset_image_payload_t *asset_payload_create(const char *name)
{
    ui_asset_image_payload_t *payload =
        (ui_asset_image_payload_t *)heap_caps_calloc(1, sizeof(ui_asset_image_payload_t),
                                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!payload) {
        payload = (ui_asset_image_payload_t *)heap_caps_calloc(1, sizeof(ui_asset_image_payload_t),
                                                               MALLOC_CAP_8BIT);
    }
    if (!payload) {
        ESP_LOGE(TAG, "image_create payload malloc FAIL: %s", name ? name : "(null)");
        return NULL;
    }

    snprintf(payload->name, sizeof(payload->name), "%s", name ? name : "");
    snprintf(payload->src, sizeof(payload->src), "%c:%s", UI_ASSET_DRIVE_LETTER, payload->name);
    return payload;
}

static lv_obj_t *asset_image_obj_create(lv_obj_t *parent, ui_asset_image_payload_t *payload, const void *src)
{
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, src);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(img, asset_image_delete_cb, LV_EVENT_DELETE, payload);
    return img;
}

static lv_obj_t *asset_file_image_create(lv_obj_t *parent, ui_asset_image_payload_t *payload, const char *reason)
{
    lv_obj_t *img = asset_image_obj_create(parent, payload, payload->src);
    ESP_LOGW(TAG, "asset image using TF file source: %s src=%s reason=%s",
             payload->name, payload->src, reason ? reason : "fallback");
    return img;
}

static bool fs_ready_cb(lv_fs_drv_t *drv)
{
    (void)drv;
    if (s_assets_unavailable) {
        return false;
    }

    struct stat st;
    ensure_sdcard_mounted();
    return stat(UI_ASSET_ROOT, &st) == 0;
}

static void *fs_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv;
    char host_path[160];
    build_host_path(path, host_path, sizeof(host_path));

    const char *flags = "rb";
    if (mode == LV_FS_MODE_WR) flags = "wb";
    else if (mode == (LV_FS_MODE_RD | LV_FS_MODE_WR)) flags = "rb+";

    FILE *f = fopen(host_path, flags);
    if (!f) {
        ESP_LOGW(TAG, "fs_open FAIL: %s (mode=%d errno=%d)", host_path, mode, errno);
    }
    return f;
}

static lv_fs_res_t fs_close_cb(lv_fs_drv_t *drv, void *file_p)
{
    (void)drv;
    if (!file_p) return LV_FS_RES_INV_PARAM;
    return fclose((FILE *)file_p) == 0 ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t fs_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf,
                              uint32_t btr, uint32_t *br)
{
    (void)drv;
    if (!file_p || !buf) return LV_FS_RES_INV_PARAM;
    size_t n = fread(buf, 1, btr, (FILE *)file_p);
    if (br) *br = (uint32_t)n;
    return ferror((FILE *)file_p) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek_cb(lv_fs_drv_t *drv, void *file_p,
                              uint32_t pos, lv_fs_whence_t whence)
{
    (void)drv;
    if (!file_p) return LV_FS_RES_INV_PARAM;
    int origin = SEEK_SET;
    if (whence == LV_FS_SEEK_CUR) origin = SEEK_CUR;
    else if (whence == LV_FS_SEEK_END) origin = SEEK_END;
    return fseek((FILE *)file_p, (long)pos, origin) == 0 ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t fs_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    (void)drv;
    if (!file_p || !pos_p) return LV_FS_RES_INV_PARAM;
    long pos = ftell((FILE *)file_p);
    if (pos < 0) return LV_FS_RES_FS_ERR;
    *pos_p = (uint32_t)pos;
    return LV_FS_RES_OK;
}

void ui_asset_service_init(void)
{
    if (s_inited) return;

    lv_fs_drv_init(&s_drv);
    s_drv.letter = UI_ASSET_DRIVE_LETTER;
    s_drv.cache_size = 64 * 1024;
    s_drv.ready_cb = fs_ready_cb;
    s_drv.open_cb = fs_open_cb;
    s_drv.close_cb = fs_close_cb;
    s_drv.read_cb = fs_read_cb;
    s_drv.seek_cb = fs_seek_cb;
    s_drv.tell_cb = fs_tell_cb;
    lv_fs_drv_register(&s_drv);

    s_inited = true;
    ESP_LOGI(TAG, "LVGL asset drive %c: mapped to %s", UI_ASSET_DRIVE_LETTER, UI_ASSET_ROOT);
}

int ui_asset_service_preload_required(void)
{
    if (s_assets_unavailable && !ensure_sdcard_mounted()) {
        return 0;
    }

    if (s_preload_done) {
        ESP_LOGI(TAG, "TF assets already preloaded: %d files, %lu bytes",
                 s_preload_count, (unsigned long)s_preload_bytes);
        return s_preload_count;
    }

    for (size_t i = 0; i < UI_ASSET_COUNT; i++) {
        s_asset_cache[i].name = s_required_assets[i].name;
    }

    bool tf_assets_ready = ensure_sdcard_mounted();
    struct stat st;
    if (!tf_assets_ready || stat(UI_ASSET_ROOT, &st) != 0 || !S_ISDIR(st.st_mode)) {
        ESP_LOGW(TAG, "TF asset folder unavailable; using embedded default UI assets");
        tf_assets_ready = false;
    } else if (!tf_override_marker_exists()) {
        ESP_LOGI(TAG, "TF UI override marker missing; using embedded default UI assets");
        tf_assets_ready = false;
    }
    s_tf_override_enabled = tf_assets_ready;

    ESP_LOGI(TAG, "Preloading UI assets (%s preferred, embedded fallback)",
             tf_assets_ready ? UI_ASSET_ROOT : "embedded");
    s_preload_count = 0;
    s_preload_bytes = 0;
    for (size_t i = 0; i < UI_ASSET_COUNT; i++) {
        const char *name = s_required_assets[i].name;
        uint8_t *data = NULL;
        uint32_t data_size = 0;

        bool use_embedded = true;
        if (tf_assets_ready && stat_asset_file(name, &st)) {
            use_embedded = asset_is_legacy_default_size(name, (long)st.st_size);
            if (use_embedded) {
                ESP_LOGI(TAG, "preload uses embedded upgrade for legacy asset: %s", name);
            } else if (read_asset_file(name, &data, &data_size)) {
                use_embedded = false;
            } else {
                ESP_LOGW(TAG, "preload TF read fail, fallback embedded: %s", name);
                use_embedded = true;
            }
        }

        if (use_embedded) {
            if (!decode_embedded_asset_to_cache(&s_asset_cache[i])) {
                continue;
            }
            const uint32_t embedded_size =
                (uint32_t)(s_required_assets[i].embedded_end - s_required_assets[i].embedded_start);
            s_preload_count++;
            s_preload_bytes += embedded_size;
            continue;
        }

        if (!asset_is_png(data, data_size)) {
            ESP_LOGW(TAG, "preload skip non-png: %s", name);
            heap_caps_free(data);
            if (decode_embedded_asset_to_cache(&s_asset_cache[i])) {
                const uint32_t embedded_size =
                    (uint32_t)(s_required_assets[i].embedded_end - s_required_assets[i].embedded_start);
                s_preload_count++;
                s_preload_bytes += embedded_size;
            }
            continue;
        }

        heap_caps_free(s_asset_cache[i].data);
        s_asset_cache[i].data = NULL;
        s_asset_cache[i].data_size = 0;
        if (decode_asset_to_cache(&s_asset_cache[i], data, data_size)) {
            heap_caps_free(data);
        } else {
            s_asset_cache[i].data = data;
            s_asset_cache[i].data_size = data_size;
            ESP_LOGW(TAG, "preload kept raw PNG fallback: %s", name);
        }
        s_asset_cache[i].valid = true;
        update_cache_metadata(&s_asset_cache[i]);
        s_preload_count++;
        s_preload_bytes += data_size;
        ESP_LOGD(TAG, "preload OK: %s (%lu bytes)", name, (unsigned long)data_size);
    }

    s_preload_done = true;
    ESP_LOGI(TAG, "UI asset preload complete: %d/%u files, %lu bytes",
             s_preload_count, (unsigned)UI_ASSET_COUNT, (unsigned long)s_preload_bytes);
    if (s_preload_count == 0) {
        s_preload_done = false;
        ESP_LOGE(TAG, "TF asset preload produced no usable PNG files; will retry later");
    }
    return s_preload_count;
}

bool ui_asset_available(const char *name)
{
    if (!name || !name[0]) return false;

    ui_asset_cache_entry_t *cached = find_cache_entry(name);
    if (cache_entry_matches_file(cached)) {
        ESP_LOGD(TAG, "asset OK(cache): %s (%lu bytes)", name, (unsigned long)cached->data_size);
        return true;
    }

    if (s_assets_unavailable && !ensure_sdcard_mounted()) {
        return false;
    }

    if (!s_tf_override_enabled && find_asset_def(name)) {
        return true;
    }

    ensure_sdcard_mounted();
    build_host_path(name, s_path_buf, sizeof(s_path_buf));
    struct stat st;
    int ret = stat(s_path_buf, &st);
    if (ret != 0) {
        if (find_asset_def(name)) {
            ESP_LOGW(TAG, "asset TF missing, embedded fallback available: %s", name);
            return true;
        }
        ESP_LOGW(TAG, "asset NOT found: %s (host: %s, stat=%d errno=%d)",
                 name, s_path_buf, ret, errno);
        return false;
    }
    if (asset_is_legacy_default_size(name, (long)st.st_size)) {
        ESP_LOGI(TAG, "asset legacy default on TF, embedded upgrade available: %s", name);
        return true;
    }
    ESP_LOGI(TAG, "asset OK: %s -> %s (%ld bytes)", name, s_path_buf, (long)st.st_size);
    return true;
}

const char *ui_asset_src(const char *name)
{
    if (!name || !name[0] || !ui_asset_available(name)) return NULL;
    if (!s_tf_override_enabled) return NULL;
    snprintf(s_src_buf, sizeof(s_src_buf), "%c:%s", UI_ASSET_DRIVE_LETTER, name);
    return s_src_buf;
}

lv_obj_t *ui_asset_image_create(lv_obj_t *parent, const char *name)
{
    if (!ui_asset_available(name)) {
        return NULL;
    }

    ui_asset_image_payload_t *payload = asset_payload_create(name);
    if (!payload) {
        return NULL;
    }

    ui_asset_cache_entry_t *entry = find_cache_entry(name);
    if (cache_entry_matches_file(entry) && entry->decoded_valid) {
        return asset_image_obj_create(parent, payload, &entry->decoded_dsc);
    }

    if (!s_tf_override_enabled && entry && decode_embedded_asset_to_cache(entry)) {
        return asset_image_obj_create(parent, payload, &entry->decoded_dsc);
    }

    struct stat st;
    if (entry && stat_asset_file(name, &st) &&
        asset_is_legacy_default_size(name, (long)st.st_size) &&
        decode_embedded_asset_to_cache(entry)) {
        return asset_image_obj_create(parent, payload, &entry->decoded_dsc);
    }

    uint8_t *data = NULL;
    uint32_t data_size = 0;
    if (!copy_cached_asset(name, &data, &data_size) &&
        !read_asset_file(name, &data, &data_size)) {
        if (entry && decode_embedded_asset_to_cache(entry)) {
            return asset_image_obj_create(parent, payload, &entry->decoded_dsc);
        }
        return asset_file_image_create(parent, payload, "direct read failed");
    }

    if (!asset_is_png(data, data_size)) {
        ESP_LOGW(TAG, "image_create SKIP (not png): %s", name ? name : "(null)");
        heap_caps_free(data);
        heap_caps_free(payload);
        return NULL;
    }

    if (entry && decode_asset_to_cache(entry, data, data_size)) {
        entry->valid = true;
        update_cache_metadata(entry);
        heap_caps_free(data);
        return asset_image_obj_create(parent, payload, &entry->decoded_dsc);
    }

    /* Set up a temporary dsc with raw PNG data for decoding.
     * LV_COLOR_FORMAT_UNKNOWN tells LVGL to detect format from image data. */
    lv_image_dsc_t tmp_dsc;
    tmp_dsc.data = data;
    tmp_dsc.data_size = data_size;
    tmp_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    tmp_dsc.header.cf = LV_COLOR_FORMAT_UNKNOWN;

    lv_image_header_t header;
    if (lv_image_decoder_get_info(&tmp_dsc, &header) != LV_RESULT_OK) {
        heap_caps_free(data);
        return asset_file_image_create(parent, payload, "decode header failed");
    }

    /* Pre-decode PNG to ARGB8888 once at load time. This eliminates the
     * per-frame lodepng software decode during rendering, which is the
     * primary cause of UI frame rate drops on page switches. */
    lv_image_decoder_dsc_t decoder_dsc;
    lv_image_decoder_args_t decoder_args = {
        .no_cache = true,
    };
    lv_result_t open_res = lv_image_decoder_open(&decoder_dsc, &tmp_dsc, &decoder_args);

    if (open_res == LV_RESULT_OK && decoder_dsc.decoded != NULL) {
        const lv_draw_buf_t *decoded_buf = decoder_dsc.decoded;
        uint32_t decoded_size = decoded_buf->data_size;
        const uint8_t *decoded_data = decoded_buf->data;
        lv_color_format_t decoded_cf = decoded_buf->header.cf;
        uint32_t decoded_w = decoded_buf->header.w;
        uint32_t decoded_h = decoded_buf->header.h;
        uint32_t decoded_stride = decoded_buf->header.stride;

        if (decoded_size > 0 && decoded_data != NULL) {
            uint8_t *argb_data = (uint8_t *)asset_malloc(decoded_size);
            if (argb_data) {
                memcpy(argb_data, decoded_data, decoded_size);

                /* Release original PNG bytes; decoded ARGB8888 replaces it. */
                heap_caps_free(data);
                data = NULL;

                payload->data = argb_data;
                payload->data_size = decoded_size;
                payload->owns_data = true;
                payload->dsc.data = argb_data;
                payload->dsc.data_size = decoded_size;
                payload->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
                payload->dsc.header.cf = decoded_cf;
                payload->dsc.header.w = decoded_w;
                payload->dsc.header.h = decoded_h;
                payload->dsc.header.stride = decoded_stride;

                lv_image_decoder_close(&decoder_dsc);

                lv_obj_t *img = asset_image_obj_create(parent, payload, &payload->dsc);

                ESP_LOGD(TAG, "asset image pre-decoded: %s size=%ldx%ld bytes=%lu",
                         payload->name, (long)decoded_w, (long)decoded_h, (unsigned long)decoded_size);
                return img;
            }
            ESP_LOGW(TAG, "image_create ARGB malloc FAIL, fallback to raw: %s", payload->name);
        } else {
            ESP_LOGW(TAG, "image_create decode empty, fallback to raw: %s", payload->name);
        }
        lv_image_decoder_close(&decoder_dsc);
    } else {
        ESP_LOGW(TAG, "image_create decode FAIL, fallback to raw: %s", payload->name);
    }

    /* Fallback: keep raw PNG data and let LVGL decode per-frame (original path). */
    payload->data = data;
    payload->data_size = data_size;
    payload->owns_data = true;
    payload->dsc.data = payload->data;
    payload->dsc.data_size = payload->data_size;
    payload->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    payload->dsc.header.cf = LV_COLOR_FORMAT_UNKNOWN;
    payload->dsc.header.w = header.w;
    payload->dsc.header.h = header.h;

    lv_obj_t *img = asset_image_obj_create(parent, payload, &payload->dsc);

    ESP_LOGD(TAG, "asset image ready (raw): %s size=%ldx%ld bytes=%lu",
             payload->name, (long)header.w, (long)header.h, (unsigned long)data_size);
    return img;
}

void ui_asset_service_diagnose(void)
{
    ESP_LOGI(TAG, "Asset service: preload=%d files, %lu bytes, unavailable=%d",
             s_preload_count, (unsigned long)s_preload_bytes, s_assets_unavailable);

    if (s_assets_unavailable || s_preload_count == 0) {
        /* Only print detailed diagnosis when something is wrong */
        ESP_LOGW(TAG, "=== TF Card Asset Diagnosis (problems detected) ===");
        ESP_LOGI(TAG, "UI_ASSET_ROOT = %s", UI_ASSET_ROOT);
        ESP_LOGI(TAG, "Drive letter = %c", UI_ASSET_DRIVE_LETTER);
        ESP_LOGI(TAG, "Preload state: done=%d count=%d bytes=%lu",
                 s_preload_done, s_preload_count, (unsigned long)s_preload_bytes);
        ESP_LOGI(TAG, "Unavailable state: %d", s_assets_unavailable);
        diagnose_sdcard_root();
        ESP_LOGI(TAG, "=== Diagnosis Complete ===");
        return;
    }

    /* Normal case: just log a summary, skip per-file listing */
    ESP_LOGI(TAG, "All %d UI assets preloaded OK", s_preload_count);
}
