"""
豆包 Seedream 4.5 图标生成器 v3 - 图生图模式 (Ark SDK)
- 使用最新 doubao-seedream-4-5-251128 模型
- 以现有图标作为参考图(base64), 通过精准中文提示词重新生成
- 生成 2K 高清图, 再缩放到目标尺寸
- 匹配 Cloud White 主题风格
"""
import os
import time
import base64
import requests
from PIL import Image
from io import BytesIO
from volcenginesdkarkruntime import Ark

# ============ 配置 ============
ARK_API_KEY = "57f01057-1e3a-4f27-96d2-d2af44f2ecf9"
BASE_URL = "https://ark.cn-beijing.volces.com/api/v3"
MODEL = "doubao-seedream-4-5-251128"  # Seedream 4.5 最新版
OUT_DIR = r"E:\MCU\esp32\p4\xiaozhi-for-p4\tf_card_assets\xiaozhi_ui"
GEN_SIZE = "2K"  # 2K 高清

client = Ark(base_url=BASE_URL, api_key=ARK_API_KEY)


def image_to_base64(image_path):
    """将本地图片转为 base64 字符串"""
    with open(image_path, "rb") as f:
        return base64.b64encode(f.read()).decode("utf-8")


# ============ 图标定义 ============
# name: (filename, target_w, target_h, prompt, use_ref_image)
ICONS = {
    "logo_robot": (
        "logo_robot.png", 34, 34,
        "智能机器人图标：画面中央是一个可爱的圆头小机器人，"
        "头部为圆角方形，有两个蓝色发光圆形眼睛，"
        "头顶有一根小天线，天线顶部有小圆球，"
        "身体为简洁的圆角矩形，胸前有一个蓝色圆形按钮，"
        "两只小手臂在身体两侧，"
        "整体为蓝色#2F6BFF主色调，白色高光细节，"
        "背景必须是纯白色#FFFFFF，无任何渐变或阴影，"
        "扁平化UI图标风格，无文字，无水印，简洁几何造型，矢量质感",
        True
    ),
}


def generate_icon(name, filename, target_w, target_h, prompt, use_ref_image):
    """调用 Seedream 4.5 生成单张图标(图生图模式)"""
    out_path = os.path.join(OUT_DIR, filename)
    ref_path = os.path.join(OUT_DIR, filename)  # 参考图路径(同名文件)
    print(f"  生成 {filename} ({target_w}x{target_h}) ...")

    try:
        kwargs = {
            "model": MODEL,
            "prompt": prompt,
            "size": GEN_SIZE,
            "response_format": "url",
            "watermark": False,
            "sequential_image_generation": "disabled",
        }

        # 图生图模式: 传入参考图 data URI (base64)
        if use_ref_image and os.path.exists(ref_path):
            ref_b64 = image_to_base64(ref_path)
            # Ark SDK 需要 data URI 格式
            data_uri = f"data:image/png;base64,{ref_b64}"
            kwargs["image"] = data_uri
            print(f"    [图生图] 使用参考图: {filename}")

        response = client.images.generate(**kwargs)
        image_url = response.data[0].url
        print(f"    URL: {image_url[:80]}...")

        # 下载图片
        img_resp = requests.get(image_url, timeout=120)
        img_resp.raise_for_status()
        img = Image.open(BytesIO(img_resp.content)).convert("RGBA")

        # 缩放到目标尺寸 (高质量)
        img_resized = img.resize((target_w, target_h), Image.LANCZOS)
        img_resized.save(out_path, "PNG")
        file_size = os.path.getsize(out_path)
        print(f"    -> 已保存 {filename} ({target_w}x{target_h}, {file_size} bytes)")
        return True

    except Exception as e:
        print(f"    !! 生成失败: {e}")
        return False


def main():
    print(f"=== 豆包 Seedream 4.5 图标生成器 v3 (图生图模式) ===")
    print(f"模型: {MODEL}")
    print(f"生成尺寸: {GEN_SIZE}")
    print(f"输出目录: {OUT_DIR}")
    print(f"图标数量: {len(ICONS)}")
    print()

    os.makedirs(OUT_DIR, exist_ok=True)

    success = 0
    fail = 0
    for name, (filename, tw, th, prompt, use_ref) in ICONS.items():
        if generate_icon(name, filename, tw, th, prompt, use_ref):
            success += 1
        else:
            fail += 1
        # 避免触发限流
        time.sleep(3)

    print(f"\n=== 完成: {success} 成功, {fail} 失败 ===")


if __name__ == "__main__":
    main()
