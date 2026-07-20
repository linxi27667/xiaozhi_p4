// slide_08_summary.js — 项目总结（极简居中版）
const { THEME, newSlide, addAccentLine } = require("./theme");

module.exports.build = function (pres, ctx) {
  const slide = newSlide(pres, {
    title: "项目总结",
    pageNum: 8,
  });

  // 三大核心能力
  const items = [
    "AI 长链路语音控制",
    "P4+S3 双中枢分布式架构",
    "传感器闭环安全联动",
  ];
  const CW = 2.7, GAP = 0.35, startX = (10 - (CW * 3 + GAP * 2)) / 2;
  items.forEach((t, i) => {
    const x = startX + i * (CW + GAP);
    slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
      x, y: 1.8, w: CW, h: 0.9,
      rectRadius: 0.08,
      fill: { color: THEME.bgCard },
      line: { color: THEME.border, width: 1 },
    });
    slide.addText(t, {
      x, y: 1.8, w: CW, h: 0.9,
      fontSize: 15, bold: true,
      fontFace: THEME.fontHead, color: THEME.textDark,
      align: "center", valign: "middle", margin: 0,
    });
  });

  // 核心价值大字
  slide.addText("听得懂  ·  看得见  ·  能联动  ·  会响应", {
    x: 0.5, y: 3.2, w: 9.0, h: 0.8,
    fontSize: 32, bold: true,
    fontFace: THEME.fontHead, color: THEME.accent,
    align: "center", valign: "middle", margin: 0,
  });

  addAccentLine(slide, pres, 4.5, 4.15, 1.0, 0.04);

  slide.addText("感谢各位评委老师指正", {
    x: 0.5, y: 4.4, w: 9.0, h: 0.4,
    fontSize: 16, fontFace: THEME.fontBody, color: THEME.textMuted,
    align: "center", margin: 0,
  });
};
