"""
make_placeholders.py
为缺失的 16 张命名图片生成灰底白字占位卡。
运行:python make_placeholders.py
"""
from PIL import Image, ImageDraw, ImageFont
import pathlib
import sys

ROOT = pathlib.Path(__file__).parent.parent
IMG_DIR = ROOT / "images"
PH_DIR = ROOT / "placeholders"
PH_DIR.mkdir(exist_ok=True)


def load_font(size):
    candidates = [
        r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\msyh.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/System/Library/Fonts/PingFang.ttc",
    ]
    for p in candidates:
        try:
            return ImageFont.truetype(p, size)
        except Exception:
            continue
    return ImageFont.load_default()


REQUIRED = [
    "01_house_full.jpg", "02_house_floor_detail.jpg", "03_esp32p4_lvgl_chat.jpg",
    "04_pcb_board.jpg", "05_slave_board.jpg", "06_face_recognition.jpg",
    "07_voice_control.jpg", "08_lvgl_overview.jpg", "09_lvgl_control.jpg",
    "10_lvgl_light.jpg", "11_lvgl_scene.jpg", "12_lvgl_env.jpg",
    "13_lvgl_network.jpg", "14_lvgl_settings.jpg", "15_rain_auto.jpg",
    "16_fire_alarm.jpg",
]

def make_placeholder(name: str) -> pathlib.Path:
    img = Image.new("RGB", (1280, 800), "#E8ECF1")
    d = ImageDraw.Draw(img)
    d.rectangle([(8, 8), (1272, 792)], outline="#B6BFC9", width=3)
    f1 = load_font(48)
    f2 = load_font(28)
    f3 = load_font(20)
    d.text((40, 320), "Pending Photo", fill="#9AA4B0", font=f1)
    d.text((40, 400), name, fill="#1F2933", font=f2)
    d.text((40, 440), "Replace with real shot", fill="#5A6470", font=f3)
    out = PH_DIR / name.replace(".", "_placeholder.").replace("jpg", "png")
    img.save(out)
    return out


def main():
    missing = [n for n in REQUIRED if not (IMG_DIR / n).exists()]
    if not missing:
        print("All 16 images present. Nothing to do.")
        return
    print(f"Generating {len(missing)} placeholders...")
    for n in missing:
        p = make_placeholder(n)
        print(f"  {n} -> {p.relative_to(ROOT)}")
    print("Done.")


if __name__ == "__main__":
    main()
