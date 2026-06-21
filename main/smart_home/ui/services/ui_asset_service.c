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

static const char *TAG = "UI_ASSET";
static lv_fs_drv_t s_drv;
static bool s_inited;
static bool s_preload_done;
static bool s_assets_unavailable;
static bool s_sdcard_missing_logged;
static int s_preload_count;
static uint32_t s_preload_bytes;
static char s_path_buf[160];
static char s_src_buf[96];

static const char *s_required_assets[] = {
    "siyin_logo.png",
    "overview_home.png",
    "weather_guangzhou.png",
    "weather_cloud.png",
    "floor_1.png",
    "floor_2.png",
    "floor_3.png",
    "alarm_siren.png",
    "logo_robot.png",
    "scene_lights.png",
    "scene_home.png",
    "scene_away.png",
    "scene_sleep.png",
    "scene_movie.png",
    "scene_night.png",
    "scene_rain.png",
    "scene_fire.png",
    "color_wheel_220.png",
};

typedef struct {
    const char *name;
    uint8_t *data;
    uint32_t data_size;
    bool valid;
} ui_asset_cache_entry_t;

static ui_asset_cache_entry_t s_asset_cache[sizeof(s_required_assets) / sizeof(s_required_assets[0])];

typedef struct {
    lv_image_dsc_t dsc;
    uint8_t *data;
    uint32_t data_size;
    char name[64];
    char src[96];
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

static ui_asset_cache_entry_t *find_cache_entry(const char *name)
{
    if (!name || !name[0]) return NULL;
    for (size_t i = 0; i < sizeof(s_asset_cache) / sizeof(s_asset_cache[0]); i++) {
        if (s_asset_cache[i].name && strcmp(s_asset_cache[i].name, name) == 0) {
            return &s_asset_cache[i];
        }
    }
    return NULL;
}

static const ui_asset_cache_entry_t *find_valid_cache_entry(const char *name)
{
    ui_asset_cache_entry_t *entry = find_cache_entry(name);
    return (entry && entry->valid && entry->data && entry->data_size > 0) ? entry : NULL;
}

static bool copy_cached_asset(const char *name, uint8_t **data, uint32_t *data_size)
{
    const ui_asset_cache_entry_t *entry = find_valid_cache_entry(name);
    if (!entry || !data || !data_size) return false;

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
        return false;
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

static void diagnose_possible_nested_path(const char *path, const char *hint)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        ESP_LOGE(TAG, "Found assets at wrong path: %s", path);
        ESP_LOGE(TAG, "-> %s", hint);
    }
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

    if (payload->dsc.data) {
        lv_image_cache_drop(&payload->dsc);
    } else if (payload->src[0]) {
        lv_image_cache_drop(payload->src);
    }
    heap_caps_free(payload->data);
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
    } else {
        ESP_LOGI(TAG, "fs_open OK: %s", host_path);
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
    if (s_assets_unavailable) {
        return 0;
    }

    if (s_preload_done) {
        ESP_LOGI(TAG, "TF assets already preloaded: %d files, %lu bytes",
                 s_preload_count, (unsigned long)s_preload_bytes);
        return s_preload_count;
    }

    for (size_t i = 0; i < sizeof(s_required_assets) / sizeof(s_required_assets[0]); i++) {
        s_asset_cache[i].name = s_required_assets[i];
    }

    if (!ensure_sdcard_mounted()) {
        ESP_LOGE(TAG, "TF asset preload skipped: /sdcard is not mounted");
        s_assets_unavailable = true;
        s_preload_done = true;
        return 0;
    }

    struct stat st;
    if (stat(UI_ASSET_ROOT, &st) != 0) {
        ESP_LOGE(TAG, "TF asset preload skipped: missing %s (errno=%d)", UI_ASSET_ROOT, errno);
        s_assets_unavailable = true;
        s_preload_done = true;
        return 0;
    }

    ESP_LOGI(TAG, "Preloading TF UI assets from %s before Wi-Fi SDIO runtime traffic", UI_ASSET_ROOT);
    for (size_t i = 0; i < sizeof(s_required_assets) / sizeof(s_required_assets[0]); i++) {
        const char *name = s_required_assets[i];
        uint8_t *data = NULL;
        uint32_t data_size = 0;
        if (!read_asset_file(name, &data, &data_size)) {
            ESP_LOGW(TAG, "preload missing/read fail: %s", name);
            continue;
        }

        if (!asset_is_png(data, data_size)) {
            ESP_LOGW(TAG, "preload skip non-png: %s", name);
            heap_caps_free(data);
            continue;
        }

        s_asset_cache[i].data = data;
        s_asset_cache[i].data_size = data_size;
        s_asset_cache[i].valid = true;
        s_preload_count++;
        s_preload_bytes += data_size;
        ESP_LOGI(TAG, "preload OK: %s (%lu bytes)", name, (unsigned long)data_size);
    }

    s_preload_done = true;
    ESP_LOGI(TAG, "TF asset preload complete: %d/%u files, %lu bytes",
             s_preload_count,
             (unsigned)(sizeof(s_required_assets) / sizeof(s_required_assets[0])),
             (unsigned long)s_preload_bytes);
    if (s_preload_count == 0) {
        s_assets_unavailable = true;
        ESP_LOGE(TAG, "TF asset preload produced no usable PNG files; runtime TF reads disabled");
    }
    return s_preload_count;
}

bool ui_asset_available(const char *name)
{
    if (!name || !name[0]) return false;

    const ui_asset_cache_entry_t *cached = find_valid_cache_entry(name);
    if (cached) {
        ESP_LOGI(TAG, "asset OK(cache): %s (%lu bytes)", name, (unsigned long)cached->data_size);
        return true;
    }

    if (s_assets_unavailable) {
        return false;
    }

    if (s_preload_done) {
        return false;
    }

    ensure_sdcard_mounted();
    build_host_path(name, s_path_buf, sizeof(s_path_buf));
    struct stat st;
    int ret = stat(s_path_buf, &st);
    if (ret != 0) {
        ESP_LOGW(TAG, "asset NOT found: %s (host: %s, stat=%d errno=%d)",
                 name, s_path_buf, ret, errno);
        return false;
    }
    ESP_LOGI(TAG, "asset OK: %s -> %s (%ld bytes)", name, s_path_buf, (long)st.st_size);
    return true;
}

const char *ui_asset_src(const char *name)
{
    if (!name || !name[0] || !ui_asset_available(name)) return NULL;
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

    uint8_t *data = NULL;
    uint32_t data_size = 0;
    if (!copy_cached_asset(name, &data, &data_size) &&
        !read_asset_file(name, &data, &data_size)) {
        return asset_file_image_create(parent, payload, "direct read failed");
    }

    if (!asset_is_png(data, data_size)) {
        ESP_LOGW(TAG, "image_create SKIP (not png): %s", name ? name : "(null)");
        heap_caps_free(data);
        heap_caps_free(payload);
        return NULL;
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
                payload->dsc.data = argb_data;
                payload->dsc.data_size = decoded_size;
                payload->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
                payload->dsc.header.cf = decoded_cf;
                payload->dsc.header.w = decoded_w;
                payload->dsc.header.h = decoded_h;
                payload->dsc.header.stride = decoded_stride;

                lv_image_decoder_close(&decoder_dsc);

                lv_obj_t *img = asset_image_obj_create(parent, payload, &payload->dsc);

                ESP_LOGI(TAG, "asset image pre-decoded: %s size=%ldx%ld bytes=%lu",
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
    payload->dsc.data = payload->data;
    payload->dsc.data_size = payload->data_size;
    payload->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    payload->dsc.header.cf = LV_COLOR_FORMAT_UNKNOWN;
    payload->dsc.header.w = header.w;
    payload->dsc.header.h = header.h;

    lv_obj_t *img = asset_image_obj_create(parent, payload, &payload->dsc);

    ESP_LOGI(TAG, "asset image ready (raw): %s size=%ldx%ld bytes=%lu",
             payload->name, (long)header.w, (long)header.h, (unsigned long)data_size);
    return img;
}

void ui_asset_service_diagnose(void)
{
    ESP_LOGI(TAG, "=== TF Card Asset Diagnosis ===");
    ESP_LOGI(TAG, "UI_ASSET_ROOT = %s", UI_ASSET_ROOT);
    ESP_LOGI(TAG, "Drive letter = %c", UI_ASSET_DRIVE_LETTER);
    ESP_LOGI(TAG, "Preload state: done=%d count=%d bytes=%lu",
             s_preload_done, s_preload_count, (unsigned long)s_preload_bytes);
    ESP_LOGI(TAG, "Unavailable state: %d", s_assets_unavailable);

    if (s_preload_count > 0) {
        ESP_LOGI(TAG, "Using preloaded TF assets; runtime UI will not read TF card");
        for (size_t i = 0; i < sizeof(s_asset_cache) / sizeof(s_asset_cache[0]); i++) {
            ESP_LOGI(TAG, "  cache[%u] %s %s (%lu bytes)",
                     (unsigned)i,
                     s_asset_cache[i].name ? s_asset_cache[i].name : "(null)",
                     s_asset_cache[i].valid ? "OK" : "MISS",
                     (unsigned long)s_asset_cache[i].data_size);
        }
        ESP_LOGI(TAG, "=== Diagnosis Complete ===");
        return;
    }

    if (s_assets_unavailable) {
        ESP_LOGW(TAG, "TF assets unavailable; runtime UI will not read TF card");
        ESP_LOGI(TAG, "=== Diagnosis Complete ===");
        return;
    }

    ensure_sdcard_mounted();

    /* Check /sdcard first. If the asset folder is missing, root listing tells
     * whether the TF card mounted but the xiaozhi_ui folder was copied wrong. */
    diagnose_sdcard_root();

    /* Check if asset root directory exists */
    struct stat st;
    if (stat(UI_ASSET_ROOT, &st) != 0) {
        ESP_LOGE(TAG, "ROOT DIR MISSING: %s (errno=%d)", UI_ASSET_ROOT, errno);
        ESP_LOGE(TAG, "-> Expected TF layout: /sdcard/xiaozhi_ui/*.png");
        diagnose_possible_nested_path("/sdcard/xiaozhi_ui/xiaozhi_ui",
                                      "Copy the inner xiaozhi_ui folder to TF root, not xiaozhi_ui/xiaozhi_ui.");
        diagnose_possible_nested_path("/sdcard/tf_card_assets/xiaozhi_ui",
                                      "Copy tf_card_assets/xiaozhi_ui to TF root as /xiaozhi_ui.");
        return;
    }
    ESP_LOGI(TAG, "Root dir exists: %s", UI_ASSET_ROOT);

    /* Try listing files via opendir */
    DIR *dir = opendir(UI_ASSET_ROOT);
    if (!dir) {
        ESP_LOGE(TAG, "opendir FAIL: %s (errno=%d)", UI_ASSET_ROOT, errno);
        return;
    }
    ESP_LOGI(TAG, "Listing %s:", UI_ASSET_ROOT);
    struct dirent *ent;
    int count = 0;
    /* d_name can be up to 256 bytes; size buffer to fit ROOT + "/" + d_name + NUL */
    char full[300];
    while ((ent = readdir(dir)) != NULL) {
        snprintf(full, sizeof(full), "%s/%s", UI_ASSET_ROOT, ent->d_name);
        struct stat fst;
        long sz = (stat(full, &fst) == 0) ? (long)fst.st_size : -1;
        ESP_LOGI(TAG, "  [%d] %s (%ld bytes)", count, ent->d_name, sz);
        count++;
    }
    closedir(dir);
    ESP_LOGI(TAG, "Total files: %d", count);

    for (size_t i = 0; i < sizeof(s_required_assets) / sizeof(s_required_assets[0]); i++) {
        const char *name = s_required_assets[i];
        build_host_path(name, full, sizeof(full));
        if (stat(full, &st) != 0) {
            ESP_LOGE(TAG, "required asset MISSING: %s (expected %s errno=%d)", name, full, errno);
            continue;
        }

        uint8_t *data = NULL;
        uint32_t data_size = 0;
        if (!read_asset_file(name, &data, &data_size)) {
            ESP_LOGE(TAG, "required asset READ FAIL: %s", name);
            continue;
        }

        if (!asset_is_png(data, data_size)) {
            ESP_LOGE(TAG, "required asset NOT PNG: %s (%lu bytes)", name, (unsigned long)data_size);
        } else {
            ESP_LOGI(TAG, "required asset OK: %s (%lu bytes)", name, (unsigned long)data_size);
        }
        heap_caps_free(data);
    }

    ESP_LOGI(TAG, "=== Diagnosis Complete ===");
}
