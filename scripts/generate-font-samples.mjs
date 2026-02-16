#!/usr/bin/env node
/**
 * Generate PNG sample images for converted fonts - e-reader style
 */

import fs from "node:fs";
import path from "node:path";
import sharp from "sharp";

const SAMPLE_WIDTH = 480;
const SAMPLE_HEIGHT = 800;
const LINE_HEIGHT = 32;
const MARGIN_X = 40;
const MARGIN_Y = 50;
const TEXT_WIDTH = SAMPLE_WIDTH - 2 * MARGIN_X;

const SAMPLE_TEXTS = {
  latin: {
    title: "Sample Text",
    text: `The quick brown fox jumps over the lazy dog. This sentence contains every letter of the alphabet and is commonly used to test fonts and keyboards.

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.

Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.

Curabitur pretium tincidunt lacus. Nulla gravida orci a odio. Nullam varius, turpis et commodo pharetra, est eros bibendum elit, nec luctus magna felis sollicitudin mauris.`
  },
  thai: {
    title: "ตัวอย่างข้อความ",
    text: `ภาษาไทยเป็นภาษาที่มีวรรณยุกต์ มีเสียงวรรณยุกต์ห้าเสียง และมีสระมากมาย การอ่านภาษาไทยต้องอาศัยการผสมพยัญชนะ สระ และวรรณยุกต์เข้าด้วยกัน

นิทานเรื่องกระต่ายกับเต่า กาลครั้งหนึ่งนานมาแล้ว มีกระต่ายตัวหนึ่งชอบอวดตัวว่าวิ่งเร็ว วันหนึ่งมันท้าเต่าแข่งวิ่ง เต่าก็รับคำท้า

กระต่ายวิ่งนำหน้าไปไกล แต่เพราะความประมาท มันจึงหยุดนอนพักผ่อน ส่วนเต่าค่อยๆ เดินไปเรื่อยๆ จนแซงกระต่ายไป

เมื่อกระต่ายตื่นขึ้นมา มันพบว่าเต่าถึงเส้นชัยไปแล้ว นิทานเรื่องนี้สอนให้รู้ว่า ความพยายามอยู่ที่ไหน ความสำเร็จอยู่ที่นั่น`
  },
  vietnamese: {
    title: "Văn bản mẫu",
    text: `Tiếng Việt là ngôn ngữ chính thức của Việt Nam. Đây là ngôn ngữ có thanh điệu với sáu thanh khác nhau. Chữ viết tiếng Việt sử dụng bảng chữ cái Latinh với các dấu phụ.

Truyện Kiều là tác phẩm văn học nổi tiếng nhất của Việt Nam, được viết bởi đại thi hào Nguyễn Du. Truyện kể về cuộc đời đầy bi kịch của nàng Thúy Kiều.

Văn hóa Việt Nam rất phong phú và đa dạng, với nhiều lễ hội truyền thống được tổ chức quanh năm. Tết Nguyên Đán là ngày lễ quan trọng nhất trong năm.

Ẩm thực Việt Nam nổi tiếng với phở, bánh mì, và bún chả. Các món ăn thường có sự kết hợp hài hòa giữa các vị chua, cay, mặn, ngọt.`
  }
};

function getSampleText(fontName) {
  if (fontName.includes('thai')) return SAMPLE_TEXTS.thai;
  if (fontName.includes('vn') || fontName.includes('viet')) return SAMPLE_TEXTS.vietnamese;
  return SAMPLE_TEXTS.latin;
}

async function loadGlyphs(folder) {
  const glyphs = {};

  for (const style of ['regular', 'bold', 'italic']) {
    const htmlPath = path.join(folder, `${style}.html`);
    if (!fs.existsSync(htmlPath)) continue;

    const html = fs.readFileSync(htmlPath, 'utf8');
    const glyphMatches = html.matchAll(/<canvas width="(\d+)" height="(\d+)" data-top="(-?\d+)" data-pixels="([^"]+)"[^>]*><\/canvas>\s*<span class="char">([^<]+)<\/span>/g);

    for (const match of glyphMatches) {
      const [, width, height, top, pixels, char] = match;
      glyphs[`${style}_${char}`] = {
        width: parseInt(width),
        height: parseInt(height),
        top: parseInt(top),
        pixels: pixels
      };
    }
  }

  return glyphs;
}

function decodeGlyph(glyph) {
  return Buffer.from(glyph.pixels.split(',')[1], 'base64');
}

function getGlyph(glyphs, char, style) {
  return glyphs[`${style}_${char}`] || glyphs[`regular_${char}`];
}

function measureWord(glyphs, word, style) {
  let width = 0;
  for (const char of word) {
    const g = getGlyph(glyphs, char, style);
    width += g ? g.width + 1 : 7;
  }
  return width;
}

const BASELINE_OFFSET = 24; // Baseline position within line height

function renderGlyph(buffer, g, x, y) {
  if (!g) return 7;

  const glyphData = decodeGlyph(g);
  // Use baseline alignment: top metric indicates distance from baseline to glyph top
  const yOffset = BASELINE_OFFSET - (g.top || 0);

  for (let gy = 0; gy < g.height; gy++) {
    const destY = y + yOffset + gy;
    if (destY < 0 || destY >= SAMPLE_HEIGHT) continue;

    for (let gx = 0; gx < g.width; gx++) {
      const destX = Math.round(x) + gx;
      if (destX < 0 || destX >= SAMPLE_WIDTH) continue;

      const srcIdx = gy * g.width + gx;
      const dstIdx = destY * SAMPLE_WIDTH + destX;

      if (srcIdx < glyphData.length && dstIdx < buffer.length) {
        const alpha = glyphData[srcIdx];
        if (alpha > 0) {
          buffer[dstIdx] = Math.round(buffer[dstIdx] * (1 - alpha / 255));
        }
      }
    }
  }

  return g.width + 1;
}

async function renderSample(folder, outputPath, fontName) {
  const glyphs = await loadGlyphs(folder);
  if (Object.keys(glyphs).length === 0) {
    console.log(`  No glyphs found for ${fontName}`);
    return false;
  }

  const buffer = Buffer.alloc(SAMPLE_WIDTH * SAMPLE_HEIGHT);
  buffer.fill(255);

  // Get sample text based on font type
  const { title, text } = getSampleText(fontName);

  // Get space width
  const spaceG = getGlyph(glyphs, ' ', 'regular');
  const spaceWidth = spaceG ? spaceG.width + 1 : 7;

  let y = MARGIN_Y;

  // Render title centered (use regular style if bold not available)
  const titleStyle = glyphs['bold_A'] ? 'bold' : 'regular';
  const titleWidth = measureWord(glyphs, title, titleStyle);
  let x = MARGIN_X + (TEXT_WIDTH - titleWidth) / 2;
  for (const char of title) {
    const g = getGlyph(glyphs, char, titleStyle);
    x += renderGlyph(buffer, g, x, y);
  }
  y += LINE_HEIGHT * 1.5;

  // Render paragraphs
  const paragraphs = text.split('\n\n');

  for (const para of paragraphs) {
    if (y > SAMPLE_HEIGHT - MARGIN_Y - LINE_HEIGHT) break;

    const words = para.trim().split(/\s+/);

    // Build lines
    const lines = [];
    let lineWords = [];
    let lineWidth = 0;

    for (const word of words) {
      const wordWidth = measureWord(glyphs, word, 'regular');

      if (lineWords.length > 0 && lineWidth + spaceWidth + wordWidth > TEXT_WIDTH) {
        lines.push({ words: lineWords, width: lineWidth });
        lineWords = [word];
        lineWidth = wordWidth;
      } else {
        if (lineWords.length > 0) lineWidth += spaceWidth;
        lineWords.push(word);
        lineWidth += wordWidth;
      }
    }
    if (lineWords.length > 0) {
      lines.push({ words: lineWords, width: lineWidth, last: true });
    }

    // Render lines with justification
    for (const line of lines) {
      if (y > SAMPLE_HEIGHT - MARGIN_Y - LINE_HEIGHT) break;

      x = MARGIN_X;

      // Calculate word spacing for justification
      let wordGap = spaceWidth;
      if (!line.last && line.words.length > 1) {
        const totalWordWidth = line.width - (line.words.length - 1) * spaceWidth;
        wordGap = (TEXT_WIDTH - totalWordWidth) / (line.words.length - 1);
      }

      for (let i = 0; i < line.words.length; i++) {
        for (const char of line.words[i]) {
          const g = getGlyph(glyphs, char, 'regular');
          x += renderGlyph(buffer, g, x, y);
        }
        if (i < line.words.length - 1) {
          x += wordGap;
        }
      }

      y += LINE_HEIGHT;
    }

    y += LINE_HEIGHT * 0.5; // Paragraph gap
  }

  await sharp(buffer, { raw: { width: SAMPLE_WIDTH, height: SAMPLE_HEIGHT, channels: 1 } })
    .png()
    .toFile(outputPath);

  console.log(`  Created: ${outputPath}`);
  return true;
}

async function main() {
  const samplesDir = path.resolve(process.argv[2] || 'theme_font_samples');

  if (!fs.existsSync(samplesDir)) {
    console.error(`Directory not found: ${samplesDir}`);
    process.exit(1);
  }

  const fonts = fs.readdirSync(samplesDir)
    .filter(f => fs.statSync(path.join(samplesDir, f)).isDirectory() && f.endsWith('-16'));

  console.log(`Generating samples for ${fonts.length} fonts...`);

  for (const fontDir of fonts) {
    const fontName = fontDir.replace('-16', '');
    console.log(`\n${fontName}:`);

    await renderSample(path.join(samplesDir, fontDir), path.join(samplesDir, `${fontName}-sample.png`), fontName);
  }

  console.log('\nDone!');
}

main().catch(console.error);
