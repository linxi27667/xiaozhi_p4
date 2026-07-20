// build_ppt.js
// 主入口:调用 8 个 slide_xx.js 生成最终 PPT
const path = require("path");
const fs = require("fs");
const pptxgen = require("pptxgenjs");
const { THEME } = require("./theme");

const pres = new pptxgen();
pres.layout = "LAYOUT_16x9";
pres.title = "ESP32-P4 AIoT 智能家居中控";
pres.author = "xiaozhi-for-p4 team";
pres.company = "嵌入式芯片设计大赛 · 乐鑫赛道";

const slides = [
  "./slide_01_title",
  "./slide_02_background",
  "./slide_03_hardware",
  "./slide_04_voice",
  "./slide_05_intent",
  "./slide_06_lvgl",
  "./slide_07_safety",
  "./slide_08_summary",
];

const ctx = { THEME };
slides.forEach((p, i) => {
  const mod = require(p);
  mod.build(pres, ctx);
  console.log(`Built slide ${i + 1}: ${path.basename(p)}`);
});

const out = path.join(__dirname, "..", "output", "xiaozhi_p4_defense.pptx");
fs.mkdirSync(path.dirname(out), { recursive: true });
pres
  .writeFile({ fileName: out })
  .then((p) => console.log(`\nWrote: ${p}`))
  .catch((e) => {
    console.error("Failed to write pptx:", e);
    process.exit(1);
  });
