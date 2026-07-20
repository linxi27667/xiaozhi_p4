// slide_07_safety.js — 四、自动模式与安全联动（极简居中版）
const { THEME, newSlide } = require("./theme");

module.exports.build = function (pres, ctx) {
  const slide = newSlide(pres, {
    title: "四、自动模式与安全联动",
    pageNum: 7,
  });

  slide.addText("传感器采集 → 规则判断 → 自动执行 → 安全告警", {
    x: 0.3, y: 2.0, w: 9.4, h: 1.0,
    fontSize: 30, bold: true,
    fontFace: THEME.fontHead, color: THEME.textDark,
    align: "center", valign: "middle",
    margin: 0,
  });

  slide.addText("雨滴传感器自动关窗收衣，烟雾/火灾传感器触发火警弹窗并联动全屋灯光警示", {
    x: 0.8, y: 3.2, w: 8.4, h: 0.5,
    fontSize: 15,
    fontFace: THEME.fontBody, color: THEME.textMuted,
    align: "center", valign: "top",
  });

  const tags = [
    { text: "雨天关窗", danger: false },
    { text: "火灾告警", danger: true },
    { text: "紧急联动", danger: true },
    { text: "睡眠模式", danger: false },
  ];
  const tagW = 1.5, gap = 0.4, startX = (10 - (tagW * 4 + gap * 3)) / 2;
  tags.forEach((t, i) => {
    const x = startX + i * (tagW + gap);
    slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
      x, y: 4.2, w: tagW, h: 0.6,
      rectRadius: 0.08,
      fill: { color: t.danger ? "FFF0F0" : THEME.bgCard },
      line: { color: t.danger ? "E74C3C" : THEME.border, width: 1 },
    });
    slide.addText(t.text, {
      x, y: 4.2, w: tagW, h: 0.6,
      fontSize: 15, bold: t.danger,
      fontFace: THEME.fontBody, color: t.danger ? "C0392B" : THEME.textDark,
      align: "center", valign: "middle",
    });
  });
};
