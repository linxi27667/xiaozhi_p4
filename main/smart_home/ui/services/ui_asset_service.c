#include "ui_asset_service.h"

#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "UI_ASSET";
static lv_fs_drv_t s_drv;
static bool s_inited;
static char s_path_buf[160];
static char s_src_buf[96];

static void build_host_path(const char *path, char *out, size_t out_size)
{
    if (!path) path = "";
    while (*path == '/' || *path == '\\') path++;
    snprintf(out, out_size, "%s/%s", UI_ASSET_ROOT, path);
}

static bool fs_ready_cb(lv_fs_drv_t *drv)
{
    (void)drv;
    struct stat st;
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

    return fopen(host_path, flags);
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

bool ui_asset_available(const char *name)
{
    if (!name || !name[0]) return false;
    build_host_path(name, s_path_buf, sizeof(s_path_buf));
    struct stat st;
    return stat(s_path_buf, &st) == 0;
}

const char *ui_asset_src(const char *name)
{
    if (!name || !name[0] || !ui_asset_available(name)) return NULL;
    snprintf(s_src_buf, sizeof(s_src_buf), "%c:%s", UI_ASSET_DRIVE_LETTER, name);
    return s_src_buf;
}

lv_obj_t *ui_asset_image_create(lv_obj_t *parent, const char *name)
{
    const char *src = ui_asset_src(name);
    if (!src) return NULL;

    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, src);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return img;
}
