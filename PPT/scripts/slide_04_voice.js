// slide_04_voice.js — 一、AI 长链路语音控制（极简居中版）
const { THEME, newSlide } = require("./theme");

module.exports.build = function (pres, ctx) {
  const slide = newSlide(pres, {
    title: "一、AI 长链路语音控制",
    pageNum: 4,
  });

  // ===== 居中核心流程大字 =====
  slide.addText("一句话 → 设备模型 → MQTT → 从机执行", {
    x: 0.3, y: 2.0, w: 9.4, h: 1.0,
    fontSize: 34, bold: true,
    fontFace: THEME.fontHead, color: THEME.textDark,
    align: "center", valign: "middle",
    margin: 0,
  });

  // 说明文字
  slide.addText("自然语言指令经小智 AI 引擎拆解为多个设备动作，通过 MQTT 下发到三层从机协同执行", {
    x: 0.8, y: 3.2, w: 8.4, h: 0.5,
    fontSize: 15,
    fontFace: THEME.fontBody, color: THEME.textMuted,
    align: "center", valign: "top",
  });

  // 三个关键能力标签
  const tags = ["语音唤醒", "意图理解", "MQTT 下发"];
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
