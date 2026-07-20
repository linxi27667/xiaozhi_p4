# xiaozhi-for-p4 答辩 PPT

> 基于 ESP32S3/P4 双中枢架构的 AIoT 多模态家居中控系统
> 嵌入式芯片设计大赛 · 乐鑫赛道命题 5

本目录包含答辩 PPT 的全部资源、脚本与渲染产物。

## 目录结构

```
PPT\
├── images\          16 张命名照片(原始,已用降级映射填充)
├── placeholders\    缺图时由 Pillow 自动生成的占位卡
├── scripts\         PptxGenJS 主脚本 + Python 工具
├── output\          渲染产物
│   ├── xiaozhi_p4_defense.pptx
│   ├── xiaozhi_p4_defense.pdf
│   └── qa\          每页 JPG(用于视觉 QA)
├── README.md
└── CHECKLIST.md
```

## 风格

- 16:9 比例
- 背景 `#F7F8FA` / 卡片 `#FFFFFF` / 标题 `#1F2933` / 强调 `#2F6F6D`
- 字体 `Microsoft YaHei`(思源黑体回退)
- 无渐变、无发光、无科技线条

## 重新生成

```bash
cd PPT/scripts
python make_placeholders.py        # 缺图占位
node build_ppt.js                   # 生成 PPT
soffice --headless --convert-to pdf ../output/xiaozhi_p4_defense.pptx --outdir ../output
pdftoppm -jpeg -r 110 ../output/xiaozhi_p4_defense.pdf ../output/qa/slide
```

## 替换图片

16 张图片命名严格,直接覆盖 `PPT/images/01_*.jpg` ~ `16_*.jpg` 即可,无需改脚本。

## 设计稿与真实数据纠错(避免做稿错误)

| 设计稿常见错误 | 真实情况 |
|---|---|
| 一楼含"客厅灯" | 一楼仅大门+大厅灯,客厅灯在二楼 |
| 二楼含"窗帘/新风/扫地/热水器" | 二楼仅客厅灯/厕所灯/风扇/晾衣杆/主卧 RGB |
| 灯光含"暖白/日落/自然/冷白" | 效果仅 4 种:静态/呼吸/彩虹/警示 |
| 场景含"聚会/自动模式" | 协议无此场景,已删除 |
| 环境含"光照 320 lux" | 无光照传感器,已删除 |
| 设置 MQTT 写 "10.1.1.50" | 实际 broker `8.134.167.240:1883` |
| 总览 1F=18 设备 | 实际 1F=2 / 2F=6 / 3F=4 |

## 依赖

- Node.js ≥ 18
- pptxgenjs ^3.12
- Python ≥ 3.8 + Pillow
- LibreOffice (`soffice` 命令)
- Poppler (`pdftoppm` 命令)
