// slide_05_intent.js — 二、AI 意图理解与人脸识别（极简居中版）
const { THEME, newSlide } = require("./theme");

module.exports.build = function (pres, ctx) {
  const slide = newSlide(pres, {
    title: "二、AI 意图理解与人脸识别",
    pageNum: 5,
  });

  slide.addText("人脸识别 → 身份确认 → 场景联动 → 回家/离家", {
    x: 0.3, y: 2.0, w: 9.4, h: 1.0,
    fontSize: 30, bold: true,
    fontFace: THEME.fontHead, color: THEME.textDark,
    align: "center", valign: "middle",
    margin: 0,
  });

  slide.addText("门口摄像头识别人脸身份，自动触发回家/离家/欢迎场景，实现无感智能交互", {
    x: 0.8, y: 3.2, w: 8.4, h: 0.5,
    fontSize: 15,
    fontFace: THEME.fontBody, color: THEME.textMuted,
    align: "center", valign: "top",
  });

  const tags = ["人脸识别", "回家模式", "离家布防"];
  const tagW = 1.8, gap = 0.5, startX = (10 - (tagW * 3 + gap * 2)) / 2;
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
