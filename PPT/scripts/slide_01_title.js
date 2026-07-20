// slide_01_title.js
// 标题页
const { THEME, newSlide, addImage, addAccentLine } = require("./theme");

module.exports.build = function (pres, ctx) {
  const slide = newSlide(pres, { showHeader: false, pageNum: 1 });

  // 主标题
  slide.addText("基于 ESP32S3/P4 双中枢架构的 AIoT 多模态家居中控系统", {
    x: 0.6, y: 1.50, w: 5.6, h: 1.40,
    fontSize: 26,
    fontFace: THEME.fontHead,
    color: THEME.textDark,
    bold: true,
    align: "left",
    valign: "top",
    margin: 0,
  });

  addAccentLine(slide, pres, 0.6, 3.10, 1.0, 0.05);

  slide.addText("面向智能家居场景的语音交互、视觉识别、LVGL 中控与安全联动系统", {
    x: 0.6, y: 3.22, w: 5.6, h: 0.5,
    fontSize: 13,
    fontFace: THEME.fontBody,
    color: THEME.textMuted,
    margin: 0,
  });

  slide.addText("嵌入式芯片设计大赛 · 乐鑫赛道命题 5", {
    x: 0.6, y: 3.90, w: 5.6, h: 0.4,
    fontSize: 14,
    fontFace: THEME.fontBody,
    color: THEME.textDark,
    bold: true,
    margin: 0,
  });

  slide.addText("ESP32-P4 + ESP32-S3 多节点协同 · 2026", {
    x: 0.6, y: 4.40, w: 5.6, h: 0.3,
    fontSize: 11,
    fontFace: THEME.fontBody,
    color: THEME.textMuted,
    margin: 0,
  });

  addImage(slide, "01_house_full.jpg", {
    x: 6.4, y: 0.5, w: 3.2, h: 4.6,
    sizing: { type: "contain", w: 3.2, h: 4.6 },
  });
};
