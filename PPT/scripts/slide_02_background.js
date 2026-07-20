// slide_02_background.js
// 项目背景(简洁)
const { THEME, newSlide, addImage } = require("./theme");

module.exports.build = function (pres, ctx) {
  const slide = newSlide(pres, {
    title: "项目背景",
    pageNum: 2,
  });

  slide.addText(
    "随着 AI 大模型发展,智能家居不应只停留在\"开灯关灯\"的单点控制,\n而应具备自然语言理解、场景判断、多设备联动和安全响应能力。",
    {
      x: 0.5, y: 1.55, w: 4.6, h: 1.4,
      fontSize: 15,
      fontFace: THEME.fontBody,
      color: THEME.textDark,
      align: "left",
      valign: "top",
      margin: 0,
      paraSpaceAfter: 8,
    }
  );

  slide.addText(
    "本项目基于 ESP32-P4 与 ESP32-S3,构建三层楼房实物模型,实现语音交互、人脸识别、LVGL 可视化控制与安全联动。",
    {
      x: 0.5, y: 3.10, w: 4.6, h: 1.0,
      fontSize: 13,
      fontFace: THEME.fontBody,
      color: THEME.textMuted,
      margin: 0,
    }
  );

  addImage(slide, "02_house_floor_detail.jpg", {
    x: 5.4, y: 1.45, w: 4.1, h: 3.60,
    sizing: { type: "contain", w: 4.1, h: 3.60 },
  });
};
