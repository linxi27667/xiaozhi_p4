# 答辩 PPT 制作计划

> 目标产物:`e:\MCU\esp32\p4\xiaozhi-for-p4\PPT\` 目录下,基于 PptxGenJS 的 8 页答辩 PPT,16:9,白底学术风,实拍照片为主。
> 赛事背景:嵌入式芯片设计大赛 · 乐鑫赛道命题 5

---

## 1. 资源现状摸底

### 1.1 现有可复用实物照片(仓库中已存在)

- `tf_card_assets/overview_home.png`、`floor_1.png`、`floor_2.png`、`floor_3.png` —— 房屋/楼层预览(对应 `01_house_full.jpg` / `02_house_floor_detail.jpg` 候选)
- `tf_card_assets/scene_*.png`(home/sleep/movie/rain/fire/lights/night/away) —— 场景页可选素材,可降级替代 7/15/16
- `tf_card_assets/alarm_siren.png`、`weather_*.png`、`color_wheel_220.png` —— 弹窗/色环素材
- `docs/v0/` 与 `docs/v1/` 下有 9+15 张板级参考图(lichuang-s3、magiclick-2p4、waveshare 等),可作为 `04_pcb_board.jpg` / `05_slave_board.jpg` 的占位
- `丝印.png`(项目根目录) —— 备选素材

### 1.2 缺失但命名要求存在的素材

`03_esp32p4_lvgl_chat.jpg`、`06_face_recognition.jpg`、`07_voice_control.jpg`、`08~14` 7 张 LVGL 截图、`15_rain_auto.jpg`、`16_fire_alarm.jpg` 这些图仓库中无现成匹配。

### 1.3 资源策略

| 策略 | 适用 | 操作 |
|---|---|---|
| **A. 真实截图(优先)** | 8 张 LVGL 页面 | 在主机上跑 `idf.py flash monitor`,逐页拍照/截屏导出 |
| **B. 设计稿 + 模板渲染** | 03/06/07/15/16 | 使用现有 `tf_card_assets/scene_*.png`、`alarm_siren.png`、`weather_*.png` 替代,在 PPT 内用文字标注说明 |
| **C. AI 配图 fallback** | 仍缺 | 用 `Pillow` 脚本生成占位框(Pillow 渲染灰底白字占位卡) |
| **D. 工程绘图 fallback** | PCB / 从机 | 从仓库 `丝印.png` 与 `docs/` 导出 |

> 落位原则:全部 16 张图放到 `PPT/images/`,命名严格按清单,缺图位置插入 `PPT/placeholders/<filename>_placeholder.png` 的灰底白字占位卡(后续用真图替换无需改脚本)。

---

## 2. 目录结构

```
PPT\
├── images\                          # 16 张命名照片(原始)
│   ├── 01_house_full.jpg
│   ├── 02_house_floor_detail.jpg
│   ├── 03_esp32p4_lvgl_chat.jpg
│   ├── 04_pcb_board.jpg
│   ├── 05_slave_board.jpg
│   ├── 06_face_recognition.jpg
│   ├── 07_voice_control.jpg
│   ├── 08_lvgl_overview.jpg
│   ├── 09_lvgl_control.jpg
│   ├── 10_lvgl_light.jpg
│   ├── 11_lvgl_scene.jpg
│   ├── 12_lvgl_env.jpg
│   ├── 13_lvgl_network.jpg
│   ├── 14_lvgl_settings.jpg
│   ├── 15_rain_auto.jpg
│   └── 16_fire_alarm.jpg
│
├── placeholders\                    # 缺图占位(Pillow 生成)
│   └── *_placeholder.png
│
├── scripts\                         # Node + Python 脚本
│   ├── build_ppt.js                 # PptxGenJS 主入口
│   ├── theme.js                     # 颜色/字体/常量集中定义
│   ├── slide_01_title.js ~ slide_08_summary.js
│   ├── make_placeholders.py         # 缺图占位生成
│   ├── qa_render.ps1                # PPTX→PDF→JPG 流水线
│   ├── qa_inspect_prompt.md         # subagent 检查提示词
│   └── package.json
│
├── output\                          # 渲染产物
│   ├── xiaozhi_p4_defense.pptx
│   ├── xiaozhi_p4_defense.pdf
│   └── qa\
│       ├── slide-01.jpg ~ slide-08.jpg
│       └── thumbnails.jpg
│
├── README.md                        # 制作/复刻说明
└── CHECKLIST.md                     # 验收清单
```

---

## 3. 设计规范

### 3.1 主题色(`theme.js`)

```javascript
const THEME = {
  bg:         "F7F8FA",  // 背景
  bgCard:     "FFFFFF",  // 卡片
  textDark:   "1F2933",  // 主文字 / 标题
  textMuted:  "5A6470",  // 副文字
  textLight:  "8A95A0",  // 脚注
  accent:     "2F6F6D",  // 蓝绿强调
  accentSoft: "DDE8E7",  // 强调色 12% 透明版
  border:     "E2E6EC",  // 分隔线
  warn:       "C0392B",  // 火警(谨慎使用)
  ok:         "2F6F6D",  // 在线
  fontHead:   "Microsoft YaHei",
  fontBody:   "Microsoft YaHei",
  slideW:     10.0,      // 16:9
  slideH:     5.625,
  margin:     0.5,
  sizeH1:     32,
  sizeH2:     22,
  sizeH3:     16,
  sizeBody:   12,
  sizeCaption:10,
};
```

### 3.2 布局基准

- 16:9 = 10.0" × 5.625"
- 左右页边距 0.5"
- 页码:右下角,`8.6, 5.25, 1.0, 0.25`,`sizeCaption`
- 页眉细线:0.4–9.6, y=0.6, w=9.2, h=0.012,color=`E2E6EC`
- 标题:y=0.32, h=0.6,左对齐,字号 `sizeH1`,颜色 `textDark`
- 副标题:y=0.92, h=0.32,字号 `sizeH3`,颜色 `textMuted`
- 内容区:y=1.40 ~ 5.05

### 3.3 复用工具函数

```javascript
function newSlide(pres, opts={}) { /* 设置 background=bg,加页眉细线、页码 */ }
function addImage(slide, file, box, opts={}) {
  // box = {x,y,w,h};不存在时回退到 placeholders/<basename>_placeholder.png
}
function addCard(slide, x, y, w, h, opts={}) {
  // RECTANGLE 白色 + 0.5pt border + 不带阴影(避免AI味)
}
function addCaption(slide, x, y, w, text) { /* 灰底字图注 */ }
```

---

## 4. 每页布局详细设计(坐标英寸,基于 10×5.625)

### 第 1 页 · 标题页

> 全白底,无装饰线条。左侧 60% 留作文字,右侧 40% 放 `01_house_full.jpg`。

| 元素 | 坐标 / 尺寸 | 字体 / 颜色 |
|---|---|---|
| 主标题"基于 ESP32S3/P4 双中枢架构的 AIoT 多模态家居中控系统" | x=0.6 y=1.7 w=5.5 h=1.6 | 28pt bold, `textDark`, 折行两行 |
| 蓝色细横线(标题下方 4pt 高) | x=0.6 y=3.75 w=1.2 h=0.04 | `accent` |
| 副标题"面向智能家居场景的语音交互、视觉识别、LVGL 中控与安全联动系统" | x=0.6 y=3.85 w=5.5 h=0.4 | 14pt, `textMuted` |
| 赛事:"嵌入式芯片设计大赛 · 乐鑫赛道命题 5" | x=0.6 y=4.30 w=5.5 h=0.4 | 14pt, `textDark` |
| 团队 / 日期 | x=0.6 y=4.85 w=5.5 h=0.3 | 11pt, `textMuted` |
| 房屋图 `01_house_full.jpg` | x=6.2 y=0.7 w=3.3 h=4.2 | 圆角遮罩 0.06,边框 0.5pt `border` |
| 页脚页码 | x=8.6 y=5.25 | 10pt, `textLight` |

### 第 2 页 · 项目背景

| 元素 | 坐标 | 备注 |
|---|---|---|
| 标题"项目背景" | x=0.5 y=0.32 w=9 h=0.6 | sizeH1 |
| 副标题"AIoT 多模态家居中控" | x=0.5 y=0.92 w=9 h=0.32 | sizeH3 muted |
| 左侧文字段(2 段正文) | x=0.5 y=1.4 w=4.5 h=3.0, 12pt | 自然语言 / 多模态 / 多楼层 / 安全自动响应四点列表 |
| 右侧大图 `01_house_full.jpg` | x=5.3 y=1.4 w=4.2 h=3.4 | 圆角遮罩 |
| 底线 | x=0.5 y=5.05 w=9.2 h=0.012 | `border` |

### 第 3 页 · 系统实物与硬件架构

> 左 50% 一张房屋图,右 50% 3 张硬件图上下排列。

| 元素 | 坐标 |
|---|---|
| 标题"系统实物与硬件架构" | x=0.5 y=0.32 w=9 h=0.6 |
| 房屋图 `01_house_full.jpg` | x=0.5 y=1.3 w=4.5 h=3.5, 含 0.5pt 边框 |
| 副标题"三层楼房模型 · 覆盖大门/灯光/风扇/天窗/晾衣杆/火焰/雨滴" | x=0.5 y=4.85 w=4.5 h=0.25, 11pt muted |
| P4 中控图 `03_esp32p4_lvgl_chat.jpg` | x=5.2 y=1.3 w=4.3 h=1.55 |
| 标签"ESP32-P4 中控 · LVGL 界面 + AI 交互" | x=5.2 y=2.85 w=4.3 h=0.25, 10pt |
| PCB 图 `04_pcb_board.jpg` | x=5.2 y=3.15 w=4.3 h=1.05 |
| 标签"自研 PCB · 设备接口 + 电源" | x=5.2 y=4.2 w=4.3 h=0.25, 10pt |
| 从机图 `05_slave_board.jpg` | x=5.2 y=4.45 w=4.3 h=0.55 |
| 标签"ESP32-S3 从机 × 3 (1F/2F/3F)" | x=5.2 y=5.02 w=4.3 h=0.25, 10pt |
| 底部说明"ESP32-P4 负责中控与交互,ESP32-S3 负责分布式执行,双中枢协同" | x=0.5 y=5.30 w=9.0 h=0.25, 10pt |

### 第 4 页 · AI 长链路语音控制

> 顶部 1 行长指令,中部 5 个执行步骤,底部配图。

| 元素 | 坐标 |
|---|---|
| 标题"AI 长链路语音控制" | x=0.5 y=0.32 w=9 h=0.6 |
| 副标题"一句话 → 设备模型 → MQTT → 从机执行" | x=0.5 y=0.92 w=9 h=0.32, 12pt muted |
| 用户语句条(浅灰底) | x=0.5 y=1.4 w=9.0 h=0.55, fill=`F2F4F7`, 文字 13pt |
| 步骤 1 圆点 + "大门开启" | x=0.5 y=2.10 w=9.0 h=0.32,圆点 0.12×0.12,文字 12pt |
| 步骤 2 圆点 + "全屋灯光打开" | x=0.5 y=2.55 |
| 步骤 3 圆点 + "风扇启动" | x=0.5 y=3.00 |
| 步骤 4 圆点 + "三楼天窗打开" | x=0.5 y=3.45 |
| 步骤 5 圆点 + "卧室灯带切换为暖色常亮" | x=0.5 y=3.90 |
| 图 `07_voice_control.jpg`(右下方) | x=7.0 y=4.30 w=2.5 h=0.85 |
| 标签"现场语音控制演示" | x=7.0 y=5.17 w=2.5 h=0.2, 10pt |

### 第 5 页 · AI 意图识别与场景推荐

> 上半 2 个演示卡片(天气 / 观影),下半两张场景图。

| 元素 | 坐标 |
|---|---|
| 标题"AI 意图识别与场景推荐" | x=0.5 y=0.32 |
| 演示 A 卡片(天气) | x=0.5 y=1.4 w=4.4 h=2.4 |
| – 标题"室外下雨 → 关闭天窗、收晾衣杆" | x=0.7 y=1.55 w=4.0 h=0.35, 14pt bold |
| – LLM 输出(等宽 11pt) | x=0.7 y=1.95 w=4.0 h=0.85 |
| – 联动设备列表 | x=0.7 y=2.85 w=4.0 h=0.85, 11pt |
| 演示 B 卡片(观影) | x=5.1 y=1.4 w=4.4 h=2.4 |
| – 标题"语音'我要看电影' → 灯光+窗帘" | x=5.3 y=1.55 w=4.0 h=0.35 |
| – LLM 输出 | x=5.3 y=1.95 w=4.0 h=0.85 |
| – 联动设备列表 | x=5.3 y=2.85 w=4.0 h=0.85 |
| 大图(场景插画,选 `scene_movie.png` 转 jpg) | x=0.5 y=3.95 w=4.4 h=1.10 |
| 大图(场景插画,选 `scene_rain.png` 转 jpg) | x=5.1 y=3.95 w=4.4 h=1.10 |
| 底部说明文字 | x=0.5 y=5.07 w=9 h=0.2, 10pt muted |

### 第 6 页 · LVGL 面板交互(3×3 宫格)

> 9 张图均匀分布,每张图下方 1 行图注。

| 元素 | 坐标 |
|---|---|
| 标题"LVGL 面板交互" | x=0.5 y=0.32 |
| 副标题"9 个核心面板,从语音到设备全链路" | x=0.5 y=0.92 |
| 9 宫格起始位置 | x=0.5 y=1.45;每格 3.0×1.15;列间距 0.15,行间距 0.15 |
| – 单元格 1(AI 聊天) | (0.50, 1.45) 3.00×1.15 图 + 0.30 注 |
| – 单元格 2(总览) | (3.65, 1.45) |
| – 单元格 3(控制) | (6.80, 1.45) |
| – 单元格 4(灯光) | (0.50, 2.75) |
| – 单元格 5(场景) | (3.65, 2.75) |
| – 单元格 6(环境) | (6.80, 2.75) |
| – 单元格 7(网络) | (0.50, 4.05) |
| – 单元格 8(设置) | (3.65, 4.05) |
| – 单元格 9(唤醒) | (6.80, 4.05) |
| 底部说明"界面状态与实际设备联动保持同步" | x=0.5 y=5.10 w=9 h=0.2, 10pt muted |

9 张图与文件对应:
- AI 聊天 → `03_esp32p4_lvgl_chat.jpg`
- 总览 → `08_lvgl_overview.jpg`
- 控制 → `09_lvgl_control.jpg`
- 灯光 → `10_lvgl_light.jpg`
- 场景 → `11_lvgl_scene.jpg`
- 环境 → `12_lvgl_env.jpg`
- 网络 → `13_lvgl_network.jpg`
- 设置 → `14_lvgl_settings.jpg`
- 唤醒 → `06_face_recognition.jpg`(或单独抓图)

### 第 7 页 · 自动模式与安全联动

> 左侧"雨天收衣"演示,右侧"火灾报警"演示。

| 元素 | 坐标 |
|---|---|
| 标题"自动模式与安全联动" | x=0.5 y=0.32 |
| 副标题"传感器 + 规则引擎 + 设备模型,毫秒级自动联动" | x=0.5 y=0.92 |
| 左侧大图 `15_rain_auto.jpg` | x=0.5 y=1.45 w=4.5 h=2.6 |
| 左侧"雨天收衣"小标题 | x=0.5 y=4.10 w=4.5 h=0.30, 14pt bold |
| 左侧文字(触发 / 联动 / 解除) | x=0.5 y=4.45 w=4.5 h=0.75, 11pt |
| 中线 | x=5.05 y=1.45 w=0 h=3.7, line `border` 0.5pt |
| 右侧大图 `16_fire_alarm.jpg` | x=5.2 y=1.45 w=4.3 h=2.6 |
| 右侧"火灾报警"小标题 | x=5.2 y=4.10 w=4.3 h=0.30, 14pt bold |
| 右侧文字 | x=5.2 y=4.45 w=4.3 h=0.75, 11pt |
| 底部说明"传感器检测 → AI/中控判断 → 设备执行 → 界面反馈闭环" | x=0.5 y=5.25 w=9 h=0.2, 10pt muted |

### 第 8 页 · 项目总结

> 上半三点总结(横向三栏),下半留白写金句。

| 元素 | 坐标 |
|---|---|
| 标题"项目总结" | x=0.5 y=0.32 |
| 卡片 1 标题"AI 走向理解式" | x=0.5 y=1.40 w=3.0 h=1.6,标题 18pt bold,正文 11pt |
| 卡片 2 标题"分布式扩展" | x=3.65 y=1.40 w=3.0 h=1.6 |
| 卡片 3 标题"安全与实用" | x=6.80 y=1.40 w=2.7 h=1.6 |
| 收尾金句(居中) | x=0.5 y=3.30 w=9.0 h=1.4, 26pt, `textDark`, `align: center` |
| 收尾副金句(小字) | x=0.5 y=4.55 w=9.0 h=0.4, 14pt, `textMuted`, 居中 |
| 致谢 / 团队 / 联系方式 | x=0.5 y=5.05 w=9.0 h=0.3, 10pt muted |

---

## 5. PptxGenJS 脚本骨架

### 5.1 入口 `scripts/build_ppt.js`

```javascript
const pptxgen = require("pptxgenjs");
const path = require("path");
const { THEME, newSlide, addImage, addCard } = require("./theme");

const pres = new pptxgen();
pres.layout = "LAYOUT_16x9";
pres.title  = "ESP32-P4 AIoT 智能家居中控";
pres.author = "xiaozhi-for-p4 team";

[
  require("./slide_01_title"),
  require("./slide_02_background"),
  require("./slide_03_hardware"),
  require("./slide_04_voice"),
  require("./slide_05_intent"),
  require("./slide_06_lvgl"),
  require("./slide_07_safety"),
  require("./slide_08_summary"),
].forEach((mod, i) => mod.build(pres, { THEME, newSlide, addImage, addCard, pageNum: i + 1 }));

const out = path.join(__dirname, "..", "output", "xiaozhi_p4_defense.pptx");
pres.writeFile({ fileName: out }).then(p => console.log("Wrote", p));
```

### 5.2 `theme.js` 关键片段

```javascript
const path = require("path");
const fs = require("fs");
const IMG_DIR = path.join(__dirname, "..", "images");
const PH_DIR  = path.join(__dirname, "..", "placeholders");

function addImage(slide, file, box, opts = {}) {
  let p = path.join(IMG_DIR, file);
  if (!fs.existsSync(p)) p = path.join(PH_DIR, file.replace(/\.\w+$/, "_placeholder.png"));
  slide.addImage({ path: p, ...box, ...opts });
}

function newSlide(pres, { title, subtitle, pageNum }) {
  const s = pres.addSlide();
  s.background = { color: THEME.bg };
  if (title)    s.addText(title,    { x: 0.5, y: 0.32, w: 9, h: 0.6, fontSize: THEME.sizeH1, fontFace: THEME.fontHead, color: THEME.textDark, bold: true, margin: 0 });
  if (subtitle) s.addText(subtitle, { x: 0.5, y: 0.92, w: 9, h: 0.32, fontSize: THEME.sizeH3, fontFace: THEME.fontBody, color: THEME.textMuted, margin: 0 });
  s.addShape(pres.shapes.RECTANGLE, { x: 0.5, y: 1.28, w: 9.2, h: 0.012, fill: { color: THEME.border }, line: { color: THEME.border, width: 0 } });
  s.addText(String(pageNum).padStart(2, "0") + " / 08", { x: 8.6, y: 5.25, w: 1.0, h: 0.25, fontSize: THEME.sizeCaption, fontFace: THEME.fontBody, color: THEME.textLight, align: "right", margin: 0 });
  return s;
}
```

### 5.3 单页模板示例 `slide_06_lvgl.js`(9 宫格)

```javascript
const TILES = [
  { name: "AI 聊天",     file: "03_esp32p4_lvgl_chat.jpg" },
  { name: "总览",        file: "08_lvgl_overview.jpg" },
  { name: "控制",        file: "09_lvgl_control.jpg" },
  { name: "灯光",        file: "10_lvgl_light.jpg" },
  { name: "场景",        file: "11_lvgl_scene.jpg" },
  { name: "环境",        file: "12_lvgl_env.jpg" },
  { name: "网络",        file: "13_lvgl_network.jpg" },
  { name: "设置",        file: "14_lvgl_settings.jpg" },
  { name: "唤醒",        file: "06_face_recognition.jpg" },
];

module.exports.build = (pres, ctx) => {
  const { THEME, newSlide, addImage } = ctx;
  const slide = newSlide(pres, { title: "LVGL 面板交互", subtitle: "9 个核心面板,从语音到设备全链路", pageNum: 6 });
  const W = 3.0, H = 1.15, X0 = 0.5, Y0 = 1.45, GAP = 0.15;
  TILES.forEach((t, i) => {
    const col = i % 3, row = Math.floor(i / 3);
    const x = X0 + col * (W + GAP);
    const y = Y0 + row * (H + 0.45);
    addImage(slide, t.file, { x, y, w: W, h: H, sizing: { type: "cover", w: W, h: H } });
    slide.addText(t.name, { x, y: y + H + 0.04, w: W, h: 0.28, fontSize: THEME.sizeCaption, fontFace: THEME.fontBody, color: THEME.textMuted, align: "center", margin: 0 });
  });
};
```

---

## 6. 缺图 fallback 方案

### 6.1 占位图生成器 `scripts/make_placeholders.py`

```python
from PIL import Image, ImageDraw, ImageFont
import pathlib
IMG_DIR = pathlib.Path(__file__).parent.parent / "images"
PH_DIR  = pathlib.Path(__file__).parent.parent / "placeholders"
PH_DIR.mkdir(exist_ok=True)
def font(sz): return ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", sz)

REQUIRED = ["01_house_full.jpg","02_house_floor_detail.jpg","03_esp32p4_lvgl_chat.jpg",
            "04_pcb_board.jpg","05_slave_board.jpg","06_face_recognition.jpg",
            "07_voice_control.jpg","08_lvgl_overview.jpg","09_lvgl_control.jpg",
            "10_lvgl_light.jpg","11_lvgl_scene.jpg","12_lvgl_env.jpg",
            "13_lvgl_network.jpg","14_lvgl_settings.jpg","15_rain_auto.jpg","16_fire_alarm.jpg"]

for name in REQUIRED:
    if (IMG_DIR / name).exists():
        continue
    img = Image.new("RGB", (1280, 800), "#E8ECF1")
    d = ImageDraw.Draw(img)
    d.rectangle([(8, 8), (1272, 792)], outline="#B6BFC9", width=3)
    d.text((40, 360), "待补充照片", fill="#1F2933", font=font(48))
    d.text((40, 420), name,         fill="#5A6470", font=font(28))
    img.save(PH_DIR / name.replace(".", "_placeholder.").replace("jpg", "png"))
    print("placeholder:", name)
```

### 6.2 降级素材映射

| 缺失 | 实际降级 |
|---|---|
| 03 / 08-14 LVGL 截图 | `tf_card_assets/floor_*.png`、`scene_*.png`、`overview_home.png` 任一裁剪 |
| 06 人脸识别 | `tf_card_assets/logo_robot.png` 替代(注明"角色形象") |
| 07 语音控制 | `丝印.png`(项目根目录) |
| 15 雨天自动 | `tf_card_assets/scene_rain.png` |
| 16 火警 | `tf_card_assets/scene_fire.png` + `alarm_siren.png` 拼图 |

---

## 7. 构建与运行命令

### 7.1 环境准备(一次性)

```bash
npm install -g pptxgenjs
pip install "markitdown[pptx]" Pillow
# LibreOffice (soffice) + Poppler (pdftoppm) 用于 QA 转换
```

### 7.2 构建流程(在 `PPT/scripts/` 目录下)

```bash
python make_placeholders.py       # 缺图占位
node build_ppt.js                  # 生成 PPT
soffice --headless --convert-to pdf ../output/xiaozhi_p4_defense.pptx --outdir ../output
pdftoppm -jpeg -r 150 ../output/xiaozhi_p4_defense.pdf ../output/qa/slide
```

### 7.3 一键 QA 脚本 `scripts/qa_render.ps1`

按上述四步串联,失败立即 exit。

---

## 8. 视觉 QA 流程(必须使用 subagent)

### 8.1 流程

1. 渲染 `output/qa/slide-01.jpg` ~ `slide-08.jpg`
2. 用 `thumbnail.py` 生成 `thumbnails.jpg` 总览
3. 起 1 个 subagent,传 `qa_inspect_prompt.md` 模板,逐页检查:
   - 元素是否重叠
   - 文字是否溢出
   - 颜色对比是否足够
   - 0.3" 最小间距
   - 0.5" 边距
   - 文字框是否过窄
4. 拿到问题清单后修复 → 重渲染 → 再检,直到首轮无新问题

### 8.2 必查项清单

- 第 1 页和第 3 页都用到 `01_house_full.jpg`,确认两次视觉一致
- 第 6 页 9 宫格所有图片同尺寸、间距等宽
- 第 7 页左右两栏图,标签居中对齐
- 颜色:`accent #2F6F6D` 仅出现 1-2 处/页,不要铺满
- 字体:统一 Microsoft YaHei

---

## 9. 必须遵守的 PptxGenJS 陷阱

按 `pptxgenjs.md` 的"Common Pitfalls"逐条规避:

1. **颜色无 `#`** —— 全部走 6 位 hex,封装 `THEME` 常量
2. **阴影不用 8 位 hex** —— 用 `opacity` 字段
3. **项目符号** —— 用 `bullet: true`,禁用 "•"
4. **多行** —— 用 `breakLine: true`
5. **列表与 lineSpacing 冲突** —— 改用 `paraSpaceAfter`
6. **不复用 pptxgen 实例** —— 每次 build 重新 `new pptxgen()`
7. **不复用 option 对象** —— 尤其 `shadow` 字段,封装 `makeShadow()` 工厂
8. **不要 ROUNDED_RECTANGLE 加矩形高亮条** —— 卡片统一用 RECTANGLE

---

## 10. 关键风险与对策

| 风险 | 等级 | 对策 |
|---|---|---|
| 16 张照片全部缺失 | 高 | 优先用真实截图;次之降级映射;最差用占位图 + 文字解释 |
| `idf.py flash` 失败导致 8 张 LVGL 截图无法采集 | 中 | 用 `tools/mqtt_iot_simulator.py` 模拟从机心跳,使页面有数据可截 |
| 中文 Microsoft YaHei 在 macOS/Linux 评审机缺失 | 中 | PPT 文本框全部 `fontFace: "Microsoft YaHei"`,另外保存 PDF 作为兜底 |
| LibreOffice 转换 PDF 失真 | 中 | 高分辨率 `pdftoppm -r 150`;最终交付保留 `.pptx` 而非仅 PDF |
| 缺图时脚本崩溃 | 低 | `addImage` 工具函数内置 fallback,任何缺图都自动用占位 |
| 评审现场字号太小 | 低 | 标题 ≥ 28pt,正文 ≥ 11pt,图注 ≥ 10pt |
| 答辩前临时改图 | 低 | 改图后只需重跑 `node build_ppt.js`,脚本不需改 |
| 现场 `npm` 不可用 | 低 | 同时把 `node_modules/pptxgenjs` 提交一份到 `scripts/vendor/`,离线可跑 |

---

## 11. 验证清单(`PPT/CHECKLIST.md`)

### 11.1 资源

- [ ] `images/` 下 16 个文件全部存在,或相应占位图已生成
- [ ] 所有图分辨率 ≥ 1280×800

### 11.2 构建

- [ ] `python make_placeholders.py` 退出码 0
- [ ] `node build_ppt.js` 退出码 0,输出 > 1MB
- [ ] PDF 转换成功,8 页全有

### 11.3 视觉

- [ ] 每页元素无重叠
- [ ] 文字不溢出
- [ ] 颜色对比度足(深色字 #1F2933 在 #F7F8FA / #FFFFFF 上)
- [ ] 9 宫格尺寸完全一致
- [ ] 整本 16:9 比例无变形

### 11.4 内容(对照 `project_rules.md` 设计稿纠错表)

- [ ] 标题页包含完整项目名 + 赛事全称
- [ ] 没有出现"lorem/ipsum/xxxx"占位文字
- [ ] 没有"AI 风格"装饰(发光/渐变/霓虹)
- [ ] 没有出现与项目无关的硬件(如一楼"空调"、二楼"窗帘")
- [ ] 没有出现"暖白/日落/自然/聚会"等设计稿纠错项
- [ ] 没有出现"光照 320lux"等虚构传感器
- [ ] 实际设备清单 1F=2 个、2F=6 个、3F=4 个(若提及数量)

### 11.5 答辩预演

- [ ] PPT 5 分钟讲完,8 页平均 30-45 秒
- [ ] 字号在 1080P 投影下清晰可读
- [ ] 视频/动画无(纯静态,符合"白底学术风")

---

## 12. 推荐实施顺序

1. **第 1 阶段** · 建目录 + `theme.js` + 占位图脚本 → 跑通空 PPT 8 页
2. **第 2 阶段** · 实物照片采集:用 P4 真机 + 模拟器,逐页截 8 张 LVGL
3. **第 3 阶段** · 写 8 个 slide_xx.js,逐页替换占位 → subagent 视觉 QA
4. **第 4 阶段** · 内容校对(对比 `project_rules.md` 设计稿纠错表)
5. **第 5 阶段** · 输出最终 `.pptx` + `.pdf` + QA 报告,归档到 `output/`

---

## 13. 文件交付清单

执行完成后应产生:

- `PPT/scripts/theme.js`
- `PPT/scripts/build_ppt.js`
- `PPT/scripts/slide_01_title.js` ~ `slide_08_summary.js`
- `PPT/scripts/make_placeholders.py`
- `PPT/scripts/qa_render.ps1` + `qa_inspect_prompt.md`
- `PPT/scripts/package.json`
- `PPT/README.md` + `PPT/CHECKLIST.md`
- `PPT/images/`(16 张原图,可缺)
- `PPT/placeholders/`(占位图,缺图时生成)
- `PPT/output/xiaozhi_p4_defense.pptx` + `.pdf`
- `PPT/output/qa/slide-01.jpg` ~ `slide-08.jpg`
