"""
prepare_images.py
将 output/image/ 下的真实照片复制到 images/ 目录,并去除水印。
P4-P7 已改为纯文字页,不再使用 UI 截图。
只需要处理 P1-P3 用到的图片:
  - 01_house_full.jpg: 房子完整图片(无水印,直接复制)
  - 02_house_floor_detail.jpg: 一楼局部(无水印,直接复制)
  - 03_esp32p4_lvgl_chat.jpg: P4中控照片(裁掉顶部外框含水印)
  - 04_pcb_board.jpg: 自研PCB(裁掉右侧水印)
  - 05_slave_board.jpg: 二楼/S3从机(无水印,直接复制)
"""
from PIL import Image, ImageDraw
import pathlib
import shutil

ROOT = pathlib.Path(__file__).parent.parent
SRC = ROOT / "output" / "image"
DST = ROOT / "images"
DST.mkdir(exist_ok=True)

def main():
    for f in DST.glob("*"):
        if f.is_file():
            f.unlink()

    # 1. 房子完整图片(3072x4096竖图) - 直接复制
    shutil.copy(SRC / "房子完整图片.jpg", DST / "01_house_full.jpg")
    print("OK: 01_house_full.jpg (direct copy)")

    # 2. 一楼局部(1706x1279) - 直接复制(无水印)
    shutil.copy(SRC / "一楼.jpg", DST / "02_house_floor_detail.jpg")
    print("OK: 02_house_floor_detail.jpg (direct copy)")

    # 3. ESP32P4中控(1440x1080) - 裁掉顶部220px外框+水印,底部30px
    img = Image.open(SRC / "ESP32P4中控.jpg").convert("RGB")
    w, h = img.size
    img = img.crop((0, 220, w, h - 30))
    img.save(DST / "03_esp32p4_lvgl_chat.jpg", "JPEG", quality=92)
    print(f"OK: 03_esp32p4_lvgl_chat.jpg {img.size}")

    # 4. 自研PCB(4096x3072) - 裁掉右侧26%水印
    img = Image.open(SRC / "自研pcb.jpg").convert("RGB")
    w, h = img.size
    img = img.crop((0, 0, int(w * 0.74), h))
    img.save(DST / "04_pcb_board.jpg", "JPEG", quality=92)
    print(f"OK: 04_pcb_board.jpg {img.size}")

    # 5. 二楼/S3从机(1706x1279) - 直接复制(无水印)
    shutil.copy(SRC / "二楼.jpg", DST / "05_slave_board.jpg")
    print("OK: 05_slave_board.jpg (direct copy)")

if __name__ == "__main__":
    main()
