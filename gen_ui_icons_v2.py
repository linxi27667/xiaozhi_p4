"""
Xiaozhi UI 图标生成器 v2 - 高分辨率 + 精致质感
- 先 8x 尺寸绘制,再高质量缩放,消除像素感
- 渐变填充 + 柔光阴影 + 内发光
- 现代扁平风格,匹配 Cloud White 主题
"""
from PIL import Image, ImageDraw, ImageFilter, ImageFont
import os
import math

OUT_DIR = r"E:\MCU\esp32\p4\xiaozhi-for-p4\tf_card_assets\xiaozhi_ui"
SCALE = 8  # 先画 8 倍大,再缩回去,得到抗锯齿效果

# 主题色
PRIMARY     = (47, 107, 255)
PRIMARY_D   = (30, 80, 210)
PRIMARY_L   = (80, 140, 255)
GREEN       = (52, 199, 89)
GREEN_D     = (36, 160, 70)
ORANGE      = (255, 149, 0)
ORANGE_D    = (230, 120, 0)
PURPLE      = (175, 82, 222)
PURPLE_D    = (140, 50, 190)
RED         = (255, 59, 48)
RED_D       = (220, 40, 35)
YELLOW      = (255, 204, 0)
YELLOW_D    = (230, 180, 0)
TEAL        = (90, 200, 250)
TEAL_D      = (60, 170, 220)
INDIGO      = (88, 86, 214)
INDIGO_D    = (60, 58, 180)
WHITE       = (255, 255, 255)
CARD_BG     = (248, 250, 255)
CARD_BORDER = (220, 230, 245)
SHADOW      = (47, 107, 255, 50)
TEXT_DARK   = (28, 35, 51)


def make_canvas(target_w, target_h):
    """创建高分辨率画布"""
    W, H = target_w * SCALE, target_h * SCALE
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img), W, H


def finalize(img, target_w, target_h):
    """缩放到目标尺寸,高质量抗锯齿"""
    return img.resize((target_w, target_h), Image.LANCZOS)


def draw_rounded_rect(draw, xy, radius, fill=None, outline=None, width=0):
    """画圆角矩形"""
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)


def add_shadow(img, draw_fn, offset=(0, 4), blur=8, opacity=60, color=(47, 107, 255)):
    """给图形加柔和阴影(在下方图层绘制模糊色块)"""
    W, H = img.size
    shadow_layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow_layer)
    # 用颜色+透明度画
    shadow_color = (*color, opacity)
    draw_fn(sd, shadow_color)
    # 模糊
    shadow_layer = shadow_layer.filter(ImageFilter.GaussianBlur(radius=blur))
    # 偏移
    offset_layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    offset_layer.paste(shadow_layer, offset, shadow_layer)
    # 合成
    return Image.alpha_composite(img, offset_layer)


def linear_gradient(start_color, end_color, steps=256, direction="vertical"):
    """生成线性渐变色带"""
    result = []
    for i in range(steps):
        t = i / max(steps - 1, 1)
        r = int(start_color[0] + (end_color[0] - start_color[0]) * t)
        g = int(start_color[1] + (end_color[1] - start_color[1]) * t)
        b = int(start_color[2] + (end_color[2] - start_color[2]) * t)
        result.append((r, g, b, 255))
    return result


def draw_grad_rounded_rect(img, xy, radius, colors, direction="vertical"):
    """画渐变圆角矩形"""
    x1, y1, x2, y2 = xy
    w, h = x2 - x1, y2 - y1
    # 先画一个完整的渐变条
    if direction == "vertical":
        grad = linear_gradient(colors[0], colors[1], steps=int(h))
        grad_img = Image.new("RGBA", (1, int(h)), (0, 0, 0, 0))
        for i in range(int(h)):
            grad_img.putpixel((0, i), grad[i])
        grad_img = grad_img.resize((int(w), int(h)), Image.NEAREST)
    else:
        grad = linear_gradient(colors[0], colors[1], steps=int(w))
        grad_img = Image.new("RGBA", (int(w), 1), (0, 0, 0, 0))
        for i in range(int(w)):
            grad_img.putpixel((i, 0), grad[i])
        grad_img = grad_img.resize((int(w), int(h)), Image.NEAREST)

    # 用圆角矩形作为 mask
    mask = Image.new("L", (int(w), int(h)), 0)
    md = ImageDraw.Draw(mask)
    md.rounded_rectangle((0, 0, int(w) - 1, int(h) - 1), radius=radius, fill=255)

    # 合成到主图
    result = Image.new("RGBA", img.size, (0, 0, 0, 0))
    result.paste(grad_img, (int(x1), int(y1)), mask)
    return Image.alpha_composite(img, result)


def draw_card(img, d, W, H, radius=None):
    """画卡片背景(白底 + 细边框 + 柔和底部阴影)"""
    if radius is None:
        radius = int(H * 0.32)

    # 阴影
    shadow_h = int(H * 0.08)
    shadow_w = int(W * 0.92)
    sx = (W - shadow_w) // 2
    sy = int(H * 0.88)
    shadow_layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow_layer)
    sd.ellipse((sx, sy, sx + shadow_w, sy + shadow_h * 3), fill=(*PRIMARY, 25))
    shadow_layer = shadow_layer.filter(ImageFilter.GaussianBlur(radius=6 * SCALE // 8))
    img = Image.alpha_composite(img, shadow_layer)

    # 卡片白底(带细微渐变)
    img = draw_grad_rounded_rect(img, (2, 2, W - 3, H - 3), radius,
                                  [CARD_BG, (240, 245, 255)], direction="vertical")

    # 细边框
    d.rounded_rectangle((2, 2, W - 3, H - 3), radius=radius,
                        outline=CARD_BORDER, width=2)
    return img, d


def draw_house_v2(img, d, cx, cy, w, h, floors, highlight_idx, accent_color, accent_dark):
    """画精致小楼(分层渐变 + 阴影 + 精致窗户)"""
    floor_h = h / (floors + 0.5)
    body_top = cy - h * 0.35
    body_bot = cy + h * 0.5
    body_w = w
    body_l = cx - body_w / 2
    body_r = cx + body_w / 2

    # --- 阴影 ---
    shadow_w = int(body_w * 0.8)
    shadow_h = int(floor_h * 0.15)
    sx = cx - shadow_w // 2
    sy = int(body_bot - floor_h * 0.05)
    shadow_layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow_layer)
    sd.ellipse((sx, sy, sx + shadow_w, sy + shadow_h * 3), fill=(*PRIMARY, 35))
    shadow_layer = shadow_layer.filter(ImageFilter.GaussianBlur(radius=4 * SCALE // 8))
    img = Image.alpha_composite(img, shadow_layer)

    # --- 屋顶 ---
    roof_h = floor_h * 0.7
    roof_overhang = body_w * 0.08
    # 屋顶渐变三角形
    roof_top = body_top - roof_h
    roof_pts = [
        (body_l - roof_overhang, body_top + floor_h * 0.05),
        (body_r + roof_overhang, body_top + floor_h * 0.05),
        (cx, roof_top),
    ]
    # 屋顶主体
    d.polygon(roof_pts, fill=accent_dark)
    # 屋顶亮边
    d.line(roof_pts + [roof_pts[0]], fill=accent_color, width=int(SCALE * 0.8))
    # 屋檐小横条
    d.rectangle((body_l - roof_overhang, body_top - floor_h * 0.02,
                 body_r + roof_overhang, body_top + floor_h * 0.08),
                fill=accent_dark)

    # --- 楼层 ---
    for i in range(floors):
        y1 = body_top + i * floor_h
        y2 = y1 + floor_h - floor_h * 0.05
        is_highlight = (i == highlight_idx)

        if is_highlight:
            # 高亮层: 渐变填充
            img = draw_grad_rounded_rect(img, (body_l, y1, body_r, y2),
                                          radius=0,
                                          colors=[accent_color,
                                                  (int(accent_color[0] * 0.85),
                                                   int(accent_color[1] * 0.85),
                                                   int(accent_color[2] * 0.9))])
            window_fill = (255, 248, 220, 255)
            window_frame = (200, 170, 80, 255)
        else:
            # 普通层: 浅白渐变
            img = draw_grad_rounded_rect(img, (body_l, y1, body_r, y2),
                                          radius=0,
                                          colors=[(250, 252, 255), (240, 245, 252)])
            window_fill = (210, 220, 240, 255)
            window_frame = (170, 185, 210, 255)

        # 楼层分隔线
        if i > 0:
            d.line((body_l + 2, y1, body_r - 2, y1), fill=(220, 230, 245, 255), width=1)

        # 窗户(居中)
        win_w = body_w * 0.22
        win_h = floor_h * 0.55
        win_x = cx - win_w / 2
        win_y = y1 + (floor_h - win_h) / 2

        # 窗框
        d.rounded_rectangle((win_x - 1, win_y - 1, win_x + win_w + 1, win_y + win_h + 1),
                            radius=int(win_h * 0.1), fill=window_frame)
        # 窗玻璃
        d.rounded_rectangle((win_x + 1, win_y + 1, win_x + win_w - 1, win_y + win_h - 1),
                            radius=int(win_h * 0.08), fill=window_fill)
        # 十字窗格
        d.line((cx, win_y + 2, cx, win_y + win_h - 2), fill=window_frame, width=max(1, int(SCALE * 0.4)))
        d.line((win_x + 2, y1 + floor_h / 2 - 1, win_x + win_w - 2, y1 + floor_h / 2 - 1),
               fill=window_frame, width=max(1, int(SCALE * 0.4)))

        # 高亮层加窗内发光
        if is_highlight:
            glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
            gd = ImageDraw.Draw(glow)
            gd.ellipse((win_x - win_w * 0.3, win_y - win_h * 0.2,
                        win_x + win_w * 1.3, win_y + win_h * 1.3),
                       fill=(255, 230, 150, 40))
            glow = glow.filter(ImageFilter.GaussianBlur(radius=6 * SCALE // 8))
            img = Image.alpha_composite(img, glow)

    # 左右侧边阴影
    left_shade = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ls = ImageDraw.Draw(left_shade)
    ls.rectangle((body_l, body_top, body_l + body_w * 0.06, body_bot),
                 fill=(0, 0, 0, 15))
    img = Image.alpha_composite(img, left_shade)
    right_shade = Image.new("RGBA", img.size, (0, 0, 0, 0))
    rs = ImageDraw.Draw(right_shade)
    rs.rectangle((body_r - body_w * 0.04, body_top, body_r, body_bot),
                 fill=(255, 255, 255, 20))
    img = Image.alpha_composite(img, right_shade)

    # --- 门(仅最底层) ---
    door_w = body_w * 0.18
    door_h = floor_h * 0.7
    door_x = cx - door_w / 2
    door_y = body_bot - door_h
    # 门框
    d.rounded_rectangle((door_x - 1, door_y - 1, door_x + door_w + 1, body_bot),
                        radius=int(door_w * 0.3), fill=PRIMARY_D)
    d.rounded_rectangle((door_x + 1, door_y + 1, door_x + door_w - 1, body_bot - 2),
                        radius=int(door_w * 0.25), fill=PRIMARY)
    # 门把手
    knob_r = max(2, int(door_w * 0.12))
    d.ellipse((door_x + door_w * 0.65 - knob_r, door_y + door_h * 0.5 - knob_r,
               door_x + door_w * 0.65 + knob_r, door_y + door_h * 0.5 + knob_r),
              fill=(255, 215, 100))

    return img, d


def make_overview_home():
    """首页图标: 大智控房子 + WiFi + 小房子"""
    TW, TH = 148, 82
    img, d, W, H = make_canvas(TW, TH)
    img, d = draw_card(img, d, W, H)

    # 左侧大三层房子 (高亮第一层)
    img, d = draw_house_v2(img, d, cx=int(W * 0.32), cy=int(H * 0.58),
                            w=int(W * 0.38), h=int(H * 0.72),
                            floors=3, highlight_idx=0,
                            accent_color=PRIMARY_L, accent_dark=PRIMARY_D)

    # 中间连接线 + 绿点
    line_y = int(H * 0.42)
    line_x1 = int(W * 0.52)
    line_x2 = int(W * 0.63)
    d.line((line_x1, line_y, line_x2, line_y), fill=PRIMARY, width=max(2, SCALE))
    dot_r = int(H * 0.06)
    d.ellipse((line_x2 - dot_r, line_y - dot_r, line_x2 + dot_r, line_y + dot_r),
              fill=GREEN)

    # WiFi 信号 (三道弧)
    wifi_cx = int(W * 0.74)
    wifi_cy = int(H * 0.42)
    for i, r in enumerate([int(H * 0.18), int(H * 0.13), int(H * 0.07)]):
        d.arc((wifi_cx - r, wifi_cy - r, wifi_cx + r, wifi_cy + r),
              start=210, end=330, fill=PRIMARY, width=max(2, SCALE))
    d.ellipse((wifi_cx - 3, wifi_cy - 3, wifi_cx + 3, wifi_cy + 3), fill=PRIMARY)

    # 右侧小房子
    img, d = draw_house_v2(img, d, cx=int(W * 0.85), cy=int(H * 0.65),
                            w=int(W * 0.2), h=int(H * 0.42),
                            floors=2, highlight_idx=1,
                            accent_color=ORANGE, accent_dark=ORANGE_D)

    img = finalize(img, TW, TH)
    img.save(os.path.join(OUT_DIR, "overview_home.png"))
    print(f"  overview_home.png  {TW}x{TH}")


def make_floor(idx, floors, highlight_idx, accent_color, accent_dark):
    """楼层图标"""
    TW, TH = 82, 56
    img, d, W, H = make_canvas(TW, TH)
    img, d = draw_card(img, d, W, H)

    img, d = draw_house_v2(img, d, cx=W // 2, cy=int(H * 0.55),
                            w=int(W * 0.62), h=int(H * 0.72),
                            floors=floors, highlight_idx=highlight_idx,
                            accent_color=accent_color, accent_dark=accent_dark)

    # 右上角 "1F"/"2F"/"3F" 标签
    try:
        font = ImageFont.truetype("arialbd.ttf", int(H * 0.22))
    except Exception:
        try:
            font = ImageFont.truetype("arial.ttf", int(H * 0.22))
        except Exception:
            font = ImageFont.load_default()

    label = f"{idx}F"
    bbox = d.textbbox((0, 0), label, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    pad_x, pad_y = int(tw * 0.35), int(th * 0.25)
    lbl_w = tw + pad_x * 2
    lbl_h = th + pad_y * 2
    lx = W - lbl_w - int(W * 0.06)
    ly = int(H * 0.08)

    # 标签背景(渐变圆角胶囊)
    lbl_img = Image.new("RGBA", img.size, (0, 0, 0, 0))
    lbl_d = ImageDraw.Draw(lbl_img)
    lbl_img = draw_grad_rounded_rect(lbl_img, (lx, ly, lx + lbl_w, ly + lbl_h),
                                      radius=int(lbl_h / 2),
                                      colors=[accent_color, accent_dark])
    img = Image.alpha_composite(img, lbl_img)

    # 文字
    d = ImageDraw.Draw(img)
    d.text((lx + pad_x, ly + pad_y - 1), label, fill=WHITE, font=font)

    img = finalize(img, TW, TH)
    img.save(os.path.join(OUT_DIR, f"floor_{idx}.png"))
    print(f"  floor_{idx}.png  {TW}x{TH}")


def make_scene(name, draw_fn):
    """场景图标: 统一卡片风格"""
    TW, TH = 82, 56
    img, d, W, H = make_canvas(TW, TH)
    img, d = draw_card(img, d, W, H, radius=int(H * 0.35))
    d2 = ImageDraw.Draw(img)
    draw_fn(img, d2, W, H)
    img = finalize(img, TW, TH)
    img.save(os.path.join(OUT_DIR, f"scene_{name}.png"))
    print(f"  scene_{name}.png  {TW}x{TH}")


def scene_home(img, d, W, H):
    """在家: 蓝色小房子"""
    cx, cy = W // 2, int(H * 0.55)
    bw, bh = int(W * 0.35), int(H * 0.28)
    roof_h = int(H * 0.2)

    # 阴影
    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.ellipse((cx - bw * 0.6, cy + bh * 0.8, cx + bw * 0.6, cy + bh * 1.2),
               fill=(*PRIMARY, 30))
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=4 * SCALE // 8))
    img_paste(img, shadow)
    d = ImageDraw.Draw(img)

    # 屋顶
    roof_pts = [(cx - bw * 0.55, cy - bh * 0.1),
                (cx + bw * 0.55, cy - bh * 0.1),
                (cx, cy - bh * 0.1 - roof_h)]
    d.polygon(roof_pts, fill=PRIMARY_D)
    # 屋檐
    d.rectangle((cx - bw * 0.58, cy - bh * 0.15, cx + bw * 0.58, cy - bh * 0.02),
                fill=PRIMARY_D)

    # 房子主体(渐变)
    img2 = draw_grad_rounded_rect(img,
        (cx - bw // 2, cy - bh * 0.1, cx + bw // 2, cy + bh * 0.7),
        radius=int(bw * 0.05),
        colors=[(245, 250, 255), (235, 242, 255)])
    d = ImageDraw.Draw(img2)

    # 门
    dw, dh = int(bw * 0.28), int(bh * 0.55)
    d.rounded_rectangle((cx - dw // 2, cy + bh * 0.15, cx + dw // 2, cy + bh * 0.7),
                        radius=int(dw * 0.3), fill=PRIMARY)
    # 门把手
    kr = max(2, int(dw * 0.1))
    d.ellipse((cx + dw * 0.2 - kr, cy + bh * 0.45 - kr,
               cx + dw * 0.2 + kr, cy + bh * 0.45 + kr),
              fill=(255, 220, 100))

    # 窗户(门两边)
    for side in [-1, 1]:
        wx = cx + side * bw * 0.35
        wy = cy + bh * 0.08
        ww, wh = int(bw * 0.15), int(bh * 0.3)
        d.rounded_rectangle((wx - ww // 2, wy, wx + ww // 2, wy + wh),
                            radius=int(wh * 0.1), fill=PRIMARY_L, outline=PRIMARY_D, width=1)
    return img2


def img_paste(dst, src):
    """原地 alpha composite"""
    # 这个函数直接修改 dst,但 PIL 的 paste 带 mask 不够灵活
    # 我们用 alpha_composite 返回新图
    pass


def scene_sleep(img, d, W, H):
    """睡眠: 弯月+星星"""
    cx, cy = W // 2, int(H * 0.5)
    mr = int(H * 0.28)

    # 月牙(两个圆相减)
    moon_layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    md = ImageDraw.Draw(moon_layer)
    # 外圆
    md.ellipse((cx - mr, cy - mr, cx + mr, cy + mr), fill=INDIGO)
    # 内圆(挖空)
    inner_r = int(mr * 0.72)
    inner_cx = int(cx + mr * 0.35)
    inner_cy = int(cy - mr * 0.15)
    md.ellipse((inner_cx - inner_r, inner_cy - inner_r,
                inner_cx + inner_r, inner_cy + inner_r),
               fill=(0, 0, 0, 0))
    # 发光
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((cx - mr * 1.3, cy - mr * 1.3, cx + mr * 1.3, cy + mr * 1.3),
               fill=(*INDIGO, 25))
    glow = glow.filter(ImageFilter.GaussianBlur(radius=6 * SCALE // 8))
    img = Image.alpha_composite(img, glow)
    img = Image.alpha_composite(img, moon_layer)

    # 星星
    d = ImageDraw.Draw(img)
    stars = [(-0.45, -0.55, 0.08), (0.5, -0.45, 0.06), (0.55, 0.3, 0.07), (-0.5, 0.35, 0.055)]
    for sx, sy, sr in stars:
        x = cx + int(sx * W * 0.5)
        y = cy + int(sy * H * 0.5)
        r = int(sr * H)
        draw_star(d, x, y, r, INDIGO)

    return img


def draw_star(d, cx, cy, r, color):
    """画四角星"""
    pts = []
    for i in range(8):
        angle = math.pi / 4 * i - math.pi / 2
        rr = r if i % 2 == 0 else r * 0.35
        pts.append((cx + rr * math.cos(angle), cy + rr * math.sin(angle)))
    d.polygon(pts, fill=color)


def scene_movie(img, d, W, H):
    """观影: 显示器 + 播放键"""
    cx, cy = W // 2, int(H * 0.5)
    bw, bh = int(W * 0.5), int(H * 0.32)

    # 屏幕阴影
    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.ellipse((cx - bw * 0.5, cy + bh * 0.7, cx + bw * 0.5, cy + bh * 1.1),
               fill=(*PRIMARY, 25))
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=4 * SCALE // 8))
    img = Image.alpha_composite(img, shadow)

    # 显示器(渐变)
    img = draw_grad_rounded_rect(img, (cx - bw // 2, cy - bh // 2, cx + bw // 2, cy + bh // 2),
                                  radius=int(bh * 0.12),
                                  colors=[PRIMARY_L, PRIMARY_D])
    d = ImageDraw.Draw(img)

    # 播放三角
    tri_s = int(bh * 0.4)
    tri_pts = [(cx - tri_s * 0.3, cy - tri_s * 0.5),
               (cx + tri_s * 0.5, cy),
               (cx - tri_s * 0.3, cy + tri_s * 0.5)]
    d.polygon(tri_pts, fill=WHITE)

    # 底座
    base_w = int(bw * 0.25)
    base_h = int(bh * 0.2)
    d.rounded_rectangle((cx - base_w // 2, cy + bh // 2,
                         cx + base_w // 2, cy + bh // 2 + base_h),
                        radius=int(base_h * 0.3), fill=PRIMARY_D)
    # 底座底盘
    chassis_w = int(bw * 0.4)
    chassis_h = int(bh * 0.08)
    d.rounded_rectangle((cx - chassis_w // 2, cy + bh // 2 + base_h,
                         cx + chassis_w // 2, cy + bh // 2 + base_h + chassis_h),
                        radius=int(chassis_h * 0.4), fill=PRIMARY_D)
    return img


def scene_night(img, d, W, H):
    """夜间: 深蓝夜空 + 月亮 + 星星"""
    cx, cy = W // 2, int(H * 0.5)
    r = int(H * 0.3)

    # 夜空圆(渐变深蓝)
    img = draw_grad_rounded_rect(img, (cx - r, cy - r, cx + r, cy + r),
                                  radius=r,
                                  colors=[(20, 30, 80), (50, 60, 130)])
    d = ImageDraw.Draw(img)

    # 月亮
    mr = int(r * 0.45)
    mcx = int(cx - r * 0.15)
    mcy = int(cy - r * 0.1)
    # 月光晕
    moon_glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    mg = ImageDraw.Draw(moon_glow)
    mg.ellipse((mcx - mr * 1.5, mcy - mr * 1.5, mcx + mr * 1.5, mcy + mr * 1.5),
               fill=(255, 240, 180, 40))
    moon_glow = moon_glow.filter(ImageFilter.GaussianBlur(radius=6 * SCALE // 8))
    img = Image.alpha_composite(img, moon_glow)
    d = ImageDraw.Draw(img)
    d.ellipse((mcx - mr, mcy - mr, mcx + mr, mcy + mr), fill=(255, 240, 180))
    # 月亮暗面
    inner_r = int(mr * 0.75)
    icx = mcx + int(mr * 0.3)
    icy = mcy - int(mr * 0.1)
    d.ellipse((icx - inner_r, icy - inner_r, icx + inner_r, icy + inner_r),
              fill=(50, 60, 130))

    # 星星
    d = ImageDraw.Draw(img)
    stars = [(-0.4, -0.4, 0.1), (0.35, -0.5, 0.08), (0.45, 0.15, 0.09), (-0.3, 0.45, 0.07)]
    for sx, sy, sr in stars:
        x = cx + int(sx * r * 1.6)
        y = cy + int(sy * r * 1.6)
        if (x - cx) ** 2 + (y - cy) ** 2 < (r * 0.95) ** 2:
            draw_star(d, x, y, int(sr * r), (255, 245, 200))

    return img


def scene_lights(img, d, W, H):
    """灯光: 灯泡 + 光芒"""
    cx, cy = W // 2, int(H * 0.48)
    br = int(H * 0.22)

    # 光芒(8 道)
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    for i in range(8):
        angle = math.pi * 2 / 8 * i - math.pi / 2
        x1 = cx + br * 1.1 * math.cos(angle)
        y1 = cy + br * 1.1 * math.sin(angle)
        x2 = cx + br * 1.7 * math.cos(angle)
        y2 = cy + br * 1.7 * math.sin(angle)
        gd.line((x1, y1, x2, y2), fill=(*YELLOW, 200), width=int(SCALE * 1.5))
    glow = glow.filter(ImageFilter.GaussianBlur(radius=2 * SCALE // 8))
    img = Image.alpha_composite(img, glow)

    # 光晕
    halo = Image.new("RGBA", img.size, (0, 0, 0, 0))
    hd = ImageDraw.Draw(halo)
    hd.ellipse((cx - br * 1.4, cy - br * 1.4, cx + br * 1.4, cy + br * 1.4),
               fill=(*YELLOW, 35))
    halo = halo.filter(ImageFilter.GaussianBlur(radius=6 * SCALE // 8))
    img = Image.alpha_composite(img, halo)

    # 灯泡玻璃(渐变)
    img = draw_grad_rounded_rect(img,
        (cx - br, cy - br * 1.1, cx + br, cy + br * 0.5),
        radius=br,
        colors=[(255, 245, 180), YELLOW])
    d = ImageDraw.Draw(img)

    # 高光
    hl_x = cx - int(br * 0.35)
    hl_y = cy - int(br * 0.5)
    hl_w, hl_h = int(br * 0.18), int(br * 0.5)
    d.ellipse((hl_x - hl_w // 2, hl_y - hl_h // 2, hl_x + hl_w // 2, hl_y + hl_h // 2),
              fill=(255, 255, 240, 180))

    # 灯座
    base_w = int(br * 0.75)
    base_h = int(br * 0.45)
    base_top = cy + br * 0.4
    # 灯座上部
    d.rounded_rectangle((cx - base_w // 2, base_top, cx + base_w // 2, base_top + base_h * 0.5),
                        radius=int(base_h * 0.15), fill=(180, 185, 195))
    # 灯座下部
    d.rounded_rectangle((cx - base_w * 0.6, base_top + base_h * 0.5,
                         cx + base_w * 0.6, base_top + base_h),
                        radius=int(base_h * 0.2), fill=(140, 145, 155))
    # 触点
    d.rounded_rectangle((cx - base_w * 0.2, base_top + base_h * 0.92,
                         cx + base_w * 0.2, base_top + base_h * 1.05),
                        radius=int(base_h * 0.08), fill=(100, 105, 115))
    return img


def scene_fire(img, d, W, H):
    """火警: 火焰"""
    cx, cy = W // 2, int(H * 0.52)
    fw, fh = int(W * 0.28), int(H * 0.5)

    # 外焰阴影/光晕
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((cx - fw * 0.8, cy - fh * 0.3, cx + fw * 0.8, cy + fh * 1.1),
               fill=(*RED, 30))
    glow = glow.filter(ImageFilter.GaussianBlur(radius=6 * SCALE // 8))
    img = Image.alpha_composite(img, glow)

    # 外焰(红色,尖顶水滴形)
    outer_pts = [
        (cx, cy - fh * 0.95),
        (cx + fw * 0.55, cy - fh * 0.2),
        (cx + fw * 0.7, cy + fh * 0.4),
        (cx + fw * 0.5, cy + fh * 0.85),
        (cx, cy + fh * 0.95),
        (cx - fw * 0.5, cy + fh * 0.85),
        (cx - fw * 0.7, cy + fh * 0.4),
        (cx - fw * 0.55, cy - fh * 0.2),
    ]
    # 外焰渐变(用垂直线条近似)
    fire_layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    fd = ImageDraw.Draw(fire_layer)
    steps = 60
    for i in range(steps):
        t = i / steps
        y = cy - fh * 0.95 + t * fh * 1.9
        # 火焰宽度曲线
        if t < 0.15:
            ww = fw * 0.3 * (t / 0.15)
        elif t < 0.5:
            ww = fw * (0.3 + 0.7 * ((t - 0.15) / 0.35))
        elif t < 0.8:
            ww = fw * 0.7
        else:
            ww = fw * (0.7 - 0.2 * ((t - 0.8) / 0.2))
        # 颜色渐变
        if t < 0.3:
            col = RED_D
        elif t < 0.6:
            col = RED
        else:
            col = ORANGE
        fd.line((cx - ww, y, cx + ww, y), fill=col, width=max(2, int(SCALE * 0.8)))

    # 内焰(黄亮)
    inner_h = fh * 0.55
    inner_top = cy - fh * 0.25
    inner_steps = 40
    for i in range(inner_steps):
        t = i / inner_steps
        y = inner_top + t * inner_h * 1.2
        if t < 0.3:
            ww = fw * 0.15 * (t / 0.3)
        elif t < 0.7:
            ww = fw * (0.15 + 0.25 * ((t - 0.3) / 0.4))
        else:
            ww = fw * (0.4 - 0.15 * ((t - 0.7) / 0.3))
        if t < 0.4:
            col = YELLOW
        elif t < 0.75:
            col = (255, 180, 50)
        else:
            col = ORANGE
        fd.line((cx - ww, y, cx + ww, y), fill=col, width=max(2, int(SCALE * 0.8)))

    fire_layer = fire_layer.filter(ImageFilter.GaussianBlur(radius=1 * SCALE // 8))
    img = Image.alpha_composite(img, fire_layer)

    # 底座
    d = ImageDraw.Draw(img)
    base_w = int(fw * 0.9)
    base_h = int(fh * 0.12)
    d.rounded_rectangle((cx - base_w // 2, cy + fh * 0.85,
                         cx + base_w // 2, cy + fh * 0.85 + base_h),
                        radius=int(base_h * 0.4), fill=(180, 80, 30))
    return img


def scene_rain(img, d, W, H):
    """雨天: 云 + 雨滴"""
    cx, cy = W // 2, int(H * 0.45)
    cw, ch = int(W * 0.5), int(H * 0.3)

    # 云阴影
    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.ellipse((cx - cw * 0.4, cy + ch * 1.2, cx + cw * 0.4, cy + ch * 1.6),
               fill=(*TEAL, 20))
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=4 * SCALE // 8))
    img = Image.alpha_composite(img, shadow)

    # 云(三个圆叠加)
    cloud_layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    cd = ImageDraw.Draw(cloud_layer)
    # 主云体(渐变蓝灰)
    cd.ellipse((cx - cw * 0.5, cy - ch * 0.3, cx + cw * 0.5, cy + ch * 0.7),
               fill=(200, 215, 235))
    cd.ellipse((cx - cw * 0.25, cy - ch * 0.6, cx + cw * 0.35, cy + ch * 0.5),
               fill=(220, 230, 245))
    cd.ellipse((cx + cw * 0.1, cy - ch * 0.4, cx + cw * 0.55, cy + ch * 0.6),
               fill=(210, 225, 240))

    # 云底部
    cd.ellipse((cx - cw * 0.45, cy + ch * 0.1, cx + cw * 0.45, cy + ch * 0.9),
               fill=(190, 205, 230))

    cloud_layer = cloud_layer.filter(ImageFilter.GaussianBlur(radius=1 * SCALE // 8))
    img = Image.alpha_composite(img, cloud_layer)

    # 雨滴(4 滴)
    d = ImageDraw.Draw(img)
    drops = [(-0.3, 0.8), (-0.08, 0.85), (0.12, 0.82), (0.32, 0.88)]
    for dx, dy in drops:
        x = cx + int(dx * cw)
        y = cy + int(dy * ch)
        dw, dh = int(cw * 0.07), int(ch * 0.28)
        # 雨滴(水滴形)
        drop_pts = [
            (x, y - dh // 2),
            (x + dw // 2, y),
            (x, y + dh // 2),
            (x - dw // 2, y),
        ]
        # 渐变水滴
        drop_img = Image.new("RGBA", img.size, (0, 0, 0, 0))
        dd = ImageDraw.Draw(drop_img)
        for i in range(int(dh)):
            ty = y - dh // 2 + i
            t = i / dh
            if t < 0.5:
                ww = dw * (t / 0.5) * 0.5
            else:
                ww = dw * 0.5 * (1 - (t - 0.5) / 0.5)
            col = TEAL if t < 0.6 else TEAL_D
            dd.line((x - ww, ty, x + ww, ty), fill=col, width=max(1, int(SCALE * 0.6)))
        drop_img = drop_img.filter(ImageFilter.GaussianBlur(radius=0.5 * SCALE // 8))
        img = Image.alpha_composite(img, drop_img)
        d = ImageDraw.Draw(img)

    return img


def scene_away(img, d, W, H):
    """离家: 盾牌 + 锁"""
    cx, cy = W // 2, int(H * 0.5)
    sw, sh = int(W * 0.4), int(H * 0.6)

    # 盾牌阴影
    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.ellipse((cx - sw * 0.4, cy + sh * 0.5, cx + sw * 0.4, cy + sh * 0.85),
               fill=(*PRIMARY, 25))
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=4 * SCALE // 8))
    img = Image.alpha_composite(img, shadow)

    # 盾牌形状
    shield_layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shield_layer)
    # 上半部矩形 + 下半部尖
    shield_pts = [
        (cx - sw // 2, cy - sh * 0.4),
        (cx + sw // 2, cy - sh * 0.4),
        (cx + sw // 2, cy + sh * 0.1),
        (cx, cy + sh * 0.5),
        (cx - sw // 2, cy + sh * 0.1),
    ]
    # 渐变填充盾牌
    steps = 80
    for i in range(steps):
        t = i / steps
        y = cy - sh * 0.4 + t * sh * 0.9
        # 宽度
        if t < 0.55:
            ww = sw // 2
        else:
            ww = sw // 2 * (1 - (t - 0.55) / 0.45)
        # 颜色
        if t < 0.3:
            col = PRIMARY_L
        elif t < 0.7:
            col = PRIMARY
        else:
            col = PRIMARY_D
        sd.line((cx - ww, y, cx + ww, y), fill=col, width=max(2, int(SCALE * 0.8)))
    shield_layer = shield_layer.filter(ImageFilter.GaussianBlur(radius=0.5 * SCALE // 8))
    img = Image.alpha_composite(img, shield_layer)

    # 锁(白色)
    d = ImageDraw.Draw(img)
    lk_w = int(sw * 0.4)
    lk_h = int(sh * 0.38)
    lk_x = cx - lk_w // 2
    lk_y = cy - sh * 0.1
    # 锁体
    d.rounded_rectangle((lk_x, lk_y, lk_x + lk_w, lk_y + lk_h),
                        radius=int(lk_h * 0.2), fill=WHITE)
    # 锁梁(上半圆弧)
    beam_r = int(lk_w * 0.3)
    beam_w = max(3, int(SCALE * 1.2))
    d.arc((cx - beam_r, lk_y - beam_r, cx + beam_r, lk_y + beam_r),
          start=180, end=0, fill=WHITE, width=beam_w)
    # 钥匙孔
    kr = max(2, int(lk_w * 0.08))
    d.ellipse((cx - kr, lk_y + lk_h * 0.3 - kr,
               cx + kr, lk_y + lk_h * 0.3 + kr),
              fill=PRIMARY)
    # 钥匙孔下方小槽
    d.rounded_rectangle((cx - max(1, int(SCALE * 0.3)), lk_y + lk_h * 0.35,
                         cx + max(1, int(SCALE * 0.3)), lk_y + lk_h * 0.6),
                        radius=1, fill=PRIMARY)

    return img


def main():
    print("=== 正在生成精致 UI 图标 v2 (高分辨率 + 渐变 + 柔光) ===")
    make_overview_home()
    make_floor(1, floors=2, highlight_idx=0, accent_color=GREEN, accent_dark=GREEN_D)
    make_floor(2, floors=2, highlight_idx=1, accent_color=ORANGE, accent_dark=ORANGE_D)
    make_floor(3, floors=3, highlight_idx=2, accent_color=PURPLE, accent_dark=PURPLE_D)
    print("--- 8 张场景图 ---")
    make_scene("home",   scene_home)
    make_scene("sleep",  scene_sleep)
    make_scene("movie",  scene_movie)
    make_scene("night",  scene_night)
    make_scene("lights", scene_lights)
    make_scene("fire",   scene_fire)
    make_scene("rain",   scene_rain)
    make_scene("away",   scene_away)
    print("=== 完成 ===")


if __name__ == "__main__":
    main()
