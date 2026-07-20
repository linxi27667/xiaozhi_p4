// theme.js
// 集中常量与工具函数。所有颜色均为 6 位 hex,无 # 前缀。

const path = require("path");
const fs = require("fs");

const ROOT = path.join(__dirname, "..");
const IMG_DIR = path.join(ROOT, "images");
const PH_DIR = path.join(ROOT, "placeholders");

const THEME = {
  // 颜色
  bg: "F7F8FA",          // 背景
  bgCard: "FFFFFF",      // 卡片
  bgSoft: "EEF1F5",      // 浅灰条
  textDark: "1F2933",    // 主文字
  textMuted: "5A6470",   // 副文字
  textLight: "8A95A0",   // 脚注
  accent: "2F6F6D",      // 蓝绿强调
  accentSoft: "DDE8E7",  // 强调色淡
  border: "E2E6EC",      // 分隔线
  warn: "C0392B",        // 火警
  ok: "2F6F6D",          // 在线
  // 字体
  fontHead: "Microsoft YaHei",
  fontBody: "Microsoft YaHei",
  // 尺寸(英寸)
  slideW: 10.0,
  slideH: 5.625,
  // 字号
  sizeH1: 30,
  sizeH2: 20,
  sizeH3: 14,
  sizeBody: 12,
  sizeCaption: 10,
  sizeCardTitle: 18,
  sizeBig: 26,
};

// 缺图时回退到 placeholders/<filename>_placeholder.png
function resolveImage(file) {
  const p1 = path.join(IMG_DIR, file);
  if (fs.existsSync(p1)) return p1;
  const baseNoExt = file.replace(/\.[^.]+$/, "");
  const ph = path.join(PH_DIR, baseNoExt + "_placeholder.png");
  if (fs.existsSync(ph)) return ph;
  // 最后兜底:返回 IMG_DIR 路径,让 PptxGenJS 报错时能定位
  return p1;
}

function addImage(slide, file, box, opts = {}) {
  const p = resolveImage(file);
  slide.addImage({ path: p, ...box, ...opts });
}

// 新建一页,自动加背景、页眉细线、页码
function newSlide(pres, { title, subtitle, pageNum, totalPages = 8, showHeader = true } = {}) {
  const s = pres.addSlide();
  s.background = { color: THEME.bg };
  if (showHeader && title) {
    s.addText(title, {
      x: 0.5, y: 0.30, w: 9.0, h: 0.55,
      fontSize: THEME.sizeH1,
      fontFace: THEME.fontHead,
      color: THEME.textDark,
      bold: true,
      margin: 0,
    });
    if (subtitle) {
      s.addText(subtitle, {
        x: 0.5, y: 0.88, w: 9.0, h: 0.32,
        fontSize: THEME.sizeH3,
        fontFace: THEME.fontBody,
        color: THEME.textMuted,
        margin: 0,
      });
    }
    // 标题下细线
    s.addShape(pres.shapes.RECTANGLE, {
      x: 0.5, y: 1.26, w: 9.2, h: 0.012,
      fill: { color: THEME.border },
      line: { color: THEME.border, width: 0 },
    });
  }
  // 页码
  if (pageNum) {
    s.addText(
      String(pageNum).padStart(2, "0") + " / " + String(totalPages).padStart(2, "0"),
      {
        x: 8.4, y: 5.28, w: 1.2, h: 0.25,
        fontSize: THEME.sizeCaption,
        fontFace: THEME.fontBody,
        color: THEME.textLight,
        align: "right",
        margin: 0,
      }
    );
  }
  return s;
}

// 白色卡片(无阴影,符合白底学术风)
function addCard(slide, pres, x, y, w, h) {
  slide.addShape(pres.shapes.RECTANGLE, {
    x, y, w, h,
    fill: { color: THEME.bgCard },
    line: { color: THEME.border, width: 0.5 },
  });
}

// 浅灰条(用于引文/代码块)
function addSoftBar(slide, pres, x, y, w, h) {
  slide.addShape(pres.shapes.RECTANGLE, {
    x, y, w, h,
    fill: { color: THEME.bgSoft },
    line: { color: THEME.bgSoft, width: 0 },
  });
}

// 强调色细横线(标题下方装饰)
function addAccentLine(slide, pres, x, y, w = 0.8, h = 0.04) {
  slide.addShape(pres.shapes.RECTANGLE, {
    x, y, w, h,
    fill: { color: THEME.accent },
    line: { color: THEME.accent, width: 0 },
  });
}

// 强调色小方块(用作列表点)
function addAccentDot(slide, pres, x, y, size = 0.12) {
  slide.addShape(pres.shapes.OVAL, {
    x, y, w: size, h: size,
    fill: { color: THEME.accent },
    line: { color: THEME.accent, width: 0 },
  });
}

module.exports = {
  THEME,
  ROOT,
  IMG_DIR,
  PH_DIR,
  resolveImage,
  addImage,
  newSlide,
  addCard,
  addSoftBar,
  addAccentLine,
  addAccentDot,
};
