"""
Xiaozhi UI 图标生成器 - 程序化绘制扁平矢量风格图标
- 蓝色主色 #2F6BFF
- 浅色背景圆角卡片
- 真实房子造型 (替换丑陋的菱形)
- 简洁场景图标 (匹配原 scene_* 风格)
"""
from PIL import Image, ImageDraw, ImageFilter
import os

OUT_DIR = r"E:\MCU\esp32\p4\xiaozhi-for-p4\tf_card_assets\xiaozhi_ui"

# 主题色
COL_PRIMARY   = (47, 107, 255, 255)   # #2F6BFF
COL_PRIMARY_D = (35, 80, 200, 255)
COL_BG        = (255, 255, 255, 255)
COL_BORDER    = (225, 232, 245, 255)
COL_TEXT      = (28, 35, 51, 255)
COL_TEXT_SEC  = (120, 132, 160, 255)
COL_GREEN     = (52, 199, 89, 255)
COL_ORANGE    = (255, 149, 0, 255)
COL_PURPLE    = (175, 82, 222, 255)
COL_RED       = (255, 59, 48, 255)
COL_YELLOW    = (255, 204, 0, 255)
COL_INDIGO    = (88, 86, 214, 255)
COL_TEAL      = (90, 200, 250, 255)
COL_SHADOW    = (47, 107, 255, 40)


def new_canvas(w, h, bg=COL_BG, radius=24):
    """创建带圆角的画布"""
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, w - 1, h - 1), radius=radius, fill=bg,
                        outline=COL_BORDER, width=2)
    return img, d


def draw_house_base(d, cx, cy, w, h, body_color, roof_color=COL_PRIMARY,
                    window_color=(255, 245, 220, 255), door_color=COL_PRIMARY_D,
                    highlight_floor=None):
    """绘制一座房子的基座(梯形身体 + 三角形屋顶 + 窗户 + 门)"""
    # 阴影
    d.ellipse((cx - w // 2 + 8, cy + h // 2 - 4, cx + w // 2 + 8, cy + h // 2 + 8),
              fill=COL_SHADOW)

    # 主体矩形(梯形)
    body_top = cy - h // 4
    body_bot = cy + h // 2
    inset = 6
    d.polygon([
        (cx - w // 2 + inset, body_top),
        (cx + w // 2 - inset, body_top),
        (cx + w // 2, body_bot),
        (cx - w // 2, body_bot),
    ], fill=body_color)

    # 屋顶三角形
    roof_h = h // 3
    d.polygon([
        (cx - w // 2 - 4, body_top + 2),
        (cx + w // 2 + 4, body_top + 2),
        (cx, body_top - roof_h),
    ], fill=roof_color)

    # 屋顶边沿小条
    d.rectangle((cx - w // 2 - 4, body_top, cx + w // 2 + 4, body_top + 3),
                fill=roof_color)


def draw_house_with_floors(d, cx, cy, w, h, num_floors, highlight_idx,
                           body_color=(245, 248, 255, 255)):
    """绘制多层小楼,highlight_idx 那一层高亮"""
    floor_h = h // (num_floors + 1)
    body_top = cy - h // 4
    body_bot = cy + h // 2
    body_w = w

    # 阴影
    d.ellipse((cx - body_w // 2 + 8, body_bot - 4, cx + body_w // 2 + 8, body_bot + 8),
              fill=(47, 107, 255, 30))

    # 屋顶
    roof_h = floor_h
    d.polygon([
        (cx - body_w // 2 - 6, body_top + 2),
        (cx + body_w // 2 + 6, body_top + 2),
        (cx, body_top - roof_h),
    ], fill=COL_PRIMARY)
    d.rectangle((cx - body_w // 2 - 6, body_top, cx + body_w // 2 + 6, body_top + 3),
                fill=COL_PRIMARY)

    # 每层
    colors = [COL_GREEN, COL_ORANGE, COL_PURPLE]
    for i in range(num_floors):
        y1 = body_top + 3 + i * floor_h
        y2 = y1 + floor_h - 2
        if i == highlight_idx:
            # 高亮层:用对应主题色填充,加发光窗
            floor_fill = colors[i]
            alpha = 230
            floor_fill = (*floor_fill[:3], alpha)
        else:
            floor_fill = (245, 248, 255, 255)

        # 楼层矩形
        d.rectangle((cx - body_w // 2, y1, cx + body_w // 2, y2),
                    fill=floor_fill, outline=(220, 228, 245, 255), width=1)

        # 窗户(中间)
        win_w = body_w // 5
        win_h = floor_h // 2
        win_x = cx - win_w // 2
        win_y = y1 + (floor_h - win_h) // 2
        if i == highlight_idx:
            # 高亮层:亮黄窗户
            win_color = (255, 245, 200, 255)
        else:
            win_color = (180, 200, 230, 255)
        d.rectangle((win_x, win_y, win_x + win_w, win_y + win_h),
                    fill=win_color, outline=(150, 170, 200, 255), width=1)
        # 十字窗
        d.line((win_x + win_w // 2, win_y, win_x + win_w // 2, win_y + win_h),
               fill=(150, 170, 200, 255), width=1)
        d.line((win_x, win_y + win_h // 2, win_x + win_w, win_y + win_h // 2),
               fill=(150, 170, 200, 255), width=1)

    # 门(最下层)
    door_w = body_w // 6
    door_h = floor_h // 2
    door_x = cx - door_w // 2
    door_y = body_bot - door_h
    d.rectangle((door_x, door_y, door_x + door_w, body_bot - 1),
                fill=COL_PRIMARY_D)
    d.ellipse((door_x + door_w - 4, door_y + door_h // 2 - 2,
               door_x + door_w - 1, door_y + door_h // 2 + 1),
              fill=(255, 220, 100, 255))


def make_overview_home():
    """首页图标: 148x82 - 智慧中控房子(分层)"""
    W, H = 148, 82
    img, d = new_canvas(W, H)

    # 左侧大房子
    draw_house_with_floors(d, cx=45, cy=42, w=50, h=68, num_floors=3,
                           highlight_idx=0)

    # 中间连接线
    d.line((70, 35, 85, 35), fill=COL_PRIMARY, width=2)
    d.ellipse((83, 32, 88, 37), fill=COL_GREEN)

    # 右侧: WiFi 信号 + 智能图标
    # WiFi 三道弧
    for i, r in enumerate([12, 8, 4]):
        d.arc((105 - r, 38 - r, 105 + r, 38 + r),
              start=200, end=340, fill=COL_PRIMARY, width=2)
    d.ellipse((103, 36, 107, 40), fill=COL_PRIMARY)

    # 小屋图标右下
    draw_house_with_floors(d, cx=125, cy=58, w=28, h=38, num_floors=2,
                           highlight_idx=1)

    img.save(os.path.join(OUT_DIR, "overview_home.png"))
    print(f"  overview_home.png  {W}x{H}")


def make_floor(idx, num_floors, highlight_idx, label_color):
    """楼层图标: 82x56 - 高亮某一层的房子"""
    W, H = 82, 56
    img, d = new_canvas(W, H)
    draw_house_with_floors(d, cx=41, cy=30, w=44, h=46, num_floors=num_floors,
                           highlight_idx=highlight_idx)
    # 标签 "1F" / "2F" / "3F"
    label = f"{idx}F"
    # 圆角小标签(右上角)
    try:
        from PIL import ImageFont
        font = ImageFont.truetype("arial.ttf", 9)
    except Exception:
        font = ImageFont.load_default()
    bbox = d.textbbox((0, 0), label, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    pad_x, pad_y = 5, 3
    bx1, by1 = W - tw - pad_x * 2 - 4, 4
    bx2, by2 = bx1 + tw + pad_x * 2, by1 + th + pad_y * 2
    d.rounded_rectangle((bx1, by1, bx2, by2), radius=8, fill=label_color)
    d.text((bx1 + pad_x, by1 + pad_y - 1), label, fill=(255, 255, 255, 255), font=font)
    img.save(os.path.join(OUT_DIR, f"floor_{idx}.png"))
    print(f"  floor_{idx}.png  {W}x{H}")


def make_scene(name, draw_fn, label):
    """场景图标: 跟原 scene_* 风格一致的圆形/方形 + 几何图标"""
    W, H = 82, 56
    img, d = new_canvas(W, H, radius=20)
    # 居中绘制
    draw_fn(d, W // 2, H // 2)
    img.save(os.path.join(OUT_DIR, f"scene_{name}.png"))
    print(f"  scene_{name}.png  {W}x{H}")


def scene_home(d, cx, cy):
    # 房子(简化)
    d.polygon([
        (cx - 16, cy + 2), (cx + 16, cy + 2), (cx, cy - 14),
    ], fill=COL_PRIMARY)
    d.rectangle((cx - 13, cy + 2, cx + 13, cy + 16), fill=(230, 240, 255, 255),
                outline=COL_PRIMARY, width=2)
    d.rectangle((cx - 4, cy + 7, cx + 4, cy + 16), fill=COL_PRIMARY)


def scene_sleep(d, cx, cy):
    # 月亮
    d.ellipse((cx - 14, cy - 14, cx + 14, cy + 14), fill=COL_INDIGO)
    d.ellipse((cx - 8, cy - 14, cx + 18, cy + 14), fill=COL_BG)
    # 星星
    for sx, sy, sr in [(-16, -8, 1.5), (12, -10, 1), (15, 5, 1.5), (-14, 8, 1)]:
        d.ellipse((cx + sx - sr, cy + sy - sr, cx + sx + sr, cy + sy + sr),
                  fill=COL_INDIGO)


def scene_movie(d, cx, cy):
    # TV 屏幕
    d.rounded_rectangle((cx - 18, cy - 12, cx + 18, cy + 8), radius=3,
                        fill=COL_PRIMARY, outline=COL_PRIMARY_D, width=2)
    # 播放三角
    d.polygon([
        (cx - 4, cy - 6), (cx - 4, cy + 4), (cx + 6, cy - 1),
    ], fill=(255, 255, 255, 255))
    # 支架
    d.rectangle((cx - 4, cy + 8, cx + 4, cy + 12), fill=COL_PRIMARY_D)
    d.rounded_rectangle((cx - 8, cy + 12, cx + 8, cy + 15), radius=2, fill=COL_PRIMARY_D)


def scene_night(d, cx, cy):
    # 夜晚天空圆
    d.ellipse((cx - 16, cy - 16, cx + 16, cy + 16), fill=(20, 30, 80, 255))
    # 月亮
    d.ellipse((cx - 8, cy - 8, cx + 8, cy + 8), fill=(255, 240, 180, 255))
    d.ellipse((cx - 4, cy - 8, cx + 12, cy + 8), fill=(20, 30, 80, 255))
    # 星星
    for sx, sy in [(-12, -10), (10, -10), (-10, 8), (11, 6)]:
        d.ellipse((cx + sx - 1, cy + sy - 1, cx + sx + 1, cy + sy + 1),
                  fill=(255, 255, 200, 255))


def scene_lights(d, cx, cy):
    # 灯泡
    d.ellipse((cx - 10, cy - 12, cx + 10, cy + 6), fill=COL_YELLOW,
              outline=COL_ORANGE, width=2)
    # 底座
    d.rectangle((cx - 6, cy + 5, cx + 6, cy + 10), fill=(180, 180, 180, 255))
    d.rounded_rectangle((cx - 7, cy + 10, cx + 7, cy + 14), radius=2,
                        fill=(120, 120, 120, 255))
    # 光芒
    for ang_deg in [0, 45, 90, 135, 180]:
        import math
        a = math.radians(ang_deg - 90)
        x1 = cx + 13 * math.cos(a)
        y1 = cy - 3 + 13 * math.sin(a)
        x2 = cx + 18 * math.cos(a)
        y2 = cy - 3 + 18 * math.sin(a)
        d.line((x1, y1, x2, y2), fill=COL_YELLOW, width=2)


def scene_fire(d, cx, cy):
    # 火苗
    d.polygon([
        (cx, cy - 16), (cx + 8, cy - 2), (cx + 5, cy + 10),
        (cx, cy + 14), (cx - 5, cy + 10), (cx - 8, cy - 2),
    ], fill=COL_RED)
    # 内层亮
    d.polygon([
        (cx, cy - 8), (cx + 4, cy), (cx, cy + 8), (cx - 4, cy),
    ], fill=(255, 200, 80, 255))
    # 底座
    d.rectangle((cx - 9, cy + 12, cx + 9, cy + 15), fill=(180, 80, 30, 255))


def scene_rain(d, cx, cy):
    # 云
    d.ellipse((cx - 16, cy - 12, cx + 16, cy + 4), fill=(180, 200, 220, 255),
              outline=(120, 150, 180, 255), width=2)
    d.ellipse((cx - 8, cy - 16, cx + 12, cy + 2), fill=(180, 200, 220, 255),
              outline=(120, 150, 180, 255), width=2)
    # 雨滴
    for dx in [-10, -3, 4, 11]:
        d.polygon([
            (cx + dx, cy + 4),
            (cx + dx - 2, cy + 12),
            (cx + dx + 2, cy + 12),
        ], fill=COL_TEAL)


def scene_away(d, cx, cy):
    # 盾牌 + 锁
    d.polygon([
        (cx, cy - 16), (cx + 12, cy - 10), (cx + 12, cy + 4),
        (cx, cy + 14), (cx - 12, cy + 4), (cx - 12, cy - 10),
    ], fill=COL_PRIMARY)
    # 锁体
    d.rounded_rectangle((cx - 6, cy - 2, cx + 6, cy + 10), radius=2,
                        fill=(255, 255, 255, 255))
    # 锁梁
    d.arc((cx - 4, cy - 8, cx + 4, cy), start=0, end=180,
          fill=(255, 255, 255, 255), width=2)
    # 钥匙孔
    d.ellipse((cx - 1.5, cy + 1, cx + 1.5, cy + 4), fill=COL_PRIMARY)


def main():
    print("=== 正在生成 UI 图标 ===")
    make_overview_home()
    # 一楼: 2 层小楼, 1F 高亮(绿)
    make_floor(1, num_floors=2, highlight_idx=0, label_color=COL_GREEN)
    # 二楼: 2 层小楼, 2F 高亮(橙)
    make_floor(2, num_floors=2, highlight_idx=1, label_color=COL_ORANGE)
    # 三楼: 3 层别墅, 3F 高亮(紫)
    make_floor(3, num_floors=3, highlight_idx=2, label_color=COL_PURPLE)
    print("--- 8 张场景图 ---")
    make_scene("home",  scene_home,  "在家")
    make_scene("sleep", scene_sleep, "睡眠")
    make_scene("movie", scene_movie, "观影")
    make_scene("night", scene_night, "夜间")
    make_scene("lights", scene_lights, "灯光")
    make_scene("fire",  scene_fire,  "火警")
    make_scene("rain",  scene_rain,  "雨天")
    make_scene("away",  scene_away,  "离家")
    print("=== 完成 ===")


if __name__ == "__main__":
    main()
