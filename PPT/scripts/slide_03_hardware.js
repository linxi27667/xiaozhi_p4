// slide_03_hardware.js
// 系统实物与硬件架构(简化为左大图+右侧三张小图横排)
const { THEME, newSlide, addImage } = require("./theme");

module.exports.build = function (pres, ctx) {
  const slide = newSlide(pres, {
    title: "系统实物与硬件架构",
    subtitle: "三层楼房实物模型 · P4 中控 · S3 分布式从机",
    pageNum: 3,
  });

  // 左侧:房屋大图
  addImage(slide, "01_house_full.jpg", {
    x: 0.5, y: 1.45, w: 4.5, h: 3.65,
    sizing: { type: "contain", w: 4.5, h: 3.65 },
  });

  // 右侧三张硬件图(竖排)
  // P4 中控
  addImage(slide, "03_esp32p4_lvgl_chat.jpg", {
    x: 5.3, y: 1.45, w: 4.2, h: 1.20,
    sizing: { type: "contain", w: 4.2, h: 1.20 },
  });
  slide.addText("ESP32-P4 中控 · LVGL 可视化交互", {
    x: 5.3, y: 2.68, w: 4.2, h: 0.25,
    fontSize: 10, fontFace: THEME.fontBody, color: THEME.textMuted,
    align: "center", margin: 0,
  });

  // PCB
  addImage(slide, "04_pcb_board.jpg", {
    x: 5.3, y: 2.98, w: 2.0, h: 1.10,
    sizing: { type: "contain", w: 2.0, h: 1.10 },
  });
  slide.addText("自研 PCB", {
    x: 5.3, y: 4.10, w: 2.0, h: 0.22,
    fontSize: 10, fontFace: THEME.fontBody, color: THEME.textMuted,
    align: "center", margin: 0,
  });

  // S3 从机(二楼图)
  addImage(slide, "05_slave_board.jpg", {
    x: 7.5, y: 2.98, w: 2.0, h: 1.10,
    sizing: { type: "contain", w: 2.0, h: 1.10 },
  });
  slide.addText("ESP32-S3 从机 × 3", {
    x: 7.5, y: 4.10, w: 2.0, h: 0.22,
    fontSize: 10, fontFace: THEME.fontBody, color: THEME.textMuted,
    align: "center", margin: 0,
  });

  // 底部架构说明
  slide.addText("P4 负责中控与 AI 交互  ·  S3 负责楼层设备执行  ·  MQTT 双中枢协同", {
    x: 0.5, y: 5.15, w: 9.0, h: 0.25,
    fontSize: 11, fontFace: THEME.fontBody, color: THEME.textDark,
    align: "center", bold: true, margin: 0,
  });
};
