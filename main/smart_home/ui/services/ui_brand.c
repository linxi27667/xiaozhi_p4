#include "ui_brand.h"
#include "ui_theme.h"
#include "ui_font.h"

static void pin(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_remove_style_all(p);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, lv_color_hex(0x1E5A8D), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(p, 1, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *brand_label(lv_obj_t *parent, const char *text, int size,
    lv_color_t color, int x, int y, int w)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, ui_font_cn((uint8_t)size), 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_letter_space(lbl, 0, 0);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_width(lbl, w);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    return lbl;
}

lv_obj_t *ui_brand_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_color_t blue = lv_color_hex(0x1E5A8D);
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_pos(root, x, y);
    lv_obj_set_size(root, w, h);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *chip = lv_obj_create(root);
    lv_obj_remove_style_all(chip);
    lv_obj_set_pos(chip, 0, 1);
    lv_obj_set_size(chip, 32, 32);
    lv_obj_set_style_border_width(chip, 3, 0);
    lv_obj_set_style_border_color(chip, blue, 0);
    lv_obj_set_style_radius(chip, 5, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < 4; i++) {
        pin(root, 6 + i * 7, 0, 2, 7);
        pin(root, 6 + i * 7, 31, 2, 6);
        pin(root, 0, 8 + i * 6, 7, 2);
        pin(root, 26, 8 + i * 6, 7, 2);
    }

    lv_obj_t *n = brand_label(root, "9", 22, blue, 8, 4, 15);
    lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
    brand_label(root, "th", 9, blue, 21, 20, 13);

    brand_label(root,
        "\x41\x49\xE8\xB5\x8B\xE8\x83\xBD\xE8\xAE\xBE\xE8\xAE\xA1\xEF\xBC\x8C\xE8\xAE\xBE\xE8\xAE\xA1\xE7\x82\xB9\xE4\xBA\xAE\x41\x49\x21",
        12, blue, 42, 1, w - 42);
    brand_label(root, "AI for Design & Design for AI !", 10, blue, 42, 20, w - 42);

    return root;
}
