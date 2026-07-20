// slide_06_lvgl.js — 三、LVGL 可视化中控面板（极简居中版）
const { THEME, newSlide } = require("./theme");

module.exports.build = function (pres, ctx) {
  const slide = newSlide(pres, {
    title: "三、LVGL 可视化中控面板",
    pageNum: 6,
  });

  slide.addText("设备模型 → 事件驱动 → LVGL渲染 → 7页交互", {
    x: 0.3, y: 2.0, w: 9.4, h: 1.0,
    fontSize: 32, bold: true,
    fontFace: THEME.fontHead, color: THEME.textDark,
    align: "center", valign: "middle",
    margin: 0,
  });

  slide.addText("基于 LVGL 自主开发 7 页中控 UI，支持中英文切换与事件驱动刷新", {
    x: 0.8, y: 3.2, w: 8.4, h: 0.5,
    fontSize: 15,
    fontFace: THEME.fontBody, color: THEME.textMuted,
    align: "center", valign: "top",
  });

  const tags = ["总览页", "控制页", "灯光页", "场景页"];
  const tagW = 1.5, gap = 0.4, startX = (10 - (tagW * 4 + gap * 3)) / 2;
  tags.forEach((t, i) => {
    const x = startX + i * (tagW + gap);
    slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
      x, y: 4.2, w: tagW, h: 0.6,
      rectRadius: 0.08,
      fill: { color: THEME.bgCard },
      line: { color: THEME.border, width: 1 },
    });
    slide.addText(t, {
      x, y: 4.2, w: tagW, h: 0.6,
      fontSize: 15,
      fontFace: THEME.fontBody, color: THEME.textDark,
      align: "center", valign: "middle",
    });
  });
};
