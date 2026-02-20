#include "HomeView.h"

#include <CoverHelpers.h>

#include <algorithm>
#include <cstdio>

namespace ui {

void render(const GfxRenderer& r, const Theme& t, const HomeView& v) {
  // Art background: HomeState draws sumi-e art directly to framebuffer
  // No art: clear screen only if no cover (HomeState handles clear when cover present)
  if (!v.useArtBackground && !v.hasCoverBmp) {
    r.clearScreen(t.backgroundColor);
  }

  const int pageWidth = r.getScreenWidth();
  const int pageHeight = r.getScreenHeight();

  // "SUMI" brand - small bold, top-left with padding from screen edge
  r.drawText(t.uiFontId, 10, 8, "SUMI", t.primaryTextBlack, EpdFontFamily::BOLD);

  // Battery indicator - top right
  battery(r, t, pageWidth - 80, 10, v.batteryPercent);

  // Book card dimensions (matched to art template)
  const auto card = CardDimensions::calculate(pageWidth, pageHeight);
  const int cardX = card.x;
  const int cardY = card.y;
  const int cardWidth = card.width;
  const int cardHeight = card.height;

  const bool hasCover = v.coverData != nullptr || v.hasCoverBmp;

  // Resolve font for title/author/buttons
  const int titleFontId = (v.hasBook && v.titleFontId >= 0) ? v.titleFontId : t.uiFontId;
  const int titleLineHeight = r.getLineHeight(titleFontId);

  // Layout below cover (matched to art template):
  //   y=543..546: cover bottom border (in art)
  //   y=548..648: info area (title + author) - 100px
  //   y=648:      separator line (in art)
  //   y=664+:     progress bar area
  const int infoTopY = cardY + cardHeight + 4;   // ~548
  const int infoBottomY = infoTopY + 100;        // ~648

  // === COVER AREA ===
  if (v.hasBook) {
    const auto coverArea = card.getCoverArea();
    if (v.coverData != nullptr && v.coverWidth > 0 && v.coverHeight > 0) {
      const auto rect = CoverHelpers::calculateCenteredRect(v.coverWidth, v.coverHeight, coverArea.x, coverArea.y,
                                                            coverArea.width, coverArea.height);
      r.drawImage(v.coverData, rect.x, rect.y, v.coverWidth, v.coverHeight);
    }
    if (!hasCover) {
      bookPlaceholder(r, t, coverArea.x, coverArea.y, coverArea.width, coverArea.height);
    }
  } else {
    // No book - show hint in cover area
    if (!v.useArtBackground) {
      // Hand-drawn sketchy cover frame
      r.drawRect(cardX, cardY, cardWidth, cardHeight, t.primaryTextBlack);
      r.drawRect(cardX + 1, cardY + 1, cardWidth - 2, cardHeight - 2, t.primaryTextBlack);
    }
    const int lineHeight = r.getLineHeight(t.uiFontId);
    const int centerY = cardY + cardHeight / 2;
    const char* noBookText = "No book open";
    const int nbw = r.getTextWidth(t.uiFontId, noBookText);
    r.drawText(t.uiFontId, cardX + (cardWidth - nbw) / 2, centerY - lineHeight, noBookText, t.primaryTextBlack);
    const char* hintText = "Press \"Files\" to browse";
    const int hw = r.getTextWidth(t.uiFontId, hintText);
    r.drawText(t.uiFontId, cardX + (cardWidth - hw) / 2, centerY + lineHeight / 2, hintText, t.secondaryTextBlack);
  }

  // === BOOK INFO (title + author, centered in info area) ===
  if (v.hasBook) {
    // Top separator line - hand-drawn sketchy style (only when no art)
    if (!v.useArtBackground) {
      r.drawLine(cardX, infoTopY, cardX + cardWidth - 2, infoTopY, t.primaryTextBlack);
      r.drawLine(cardX + 1, infoTopY + 1, cardX + cardWidth, infoTopY + 1, t.primaryTextBlack);
      r.drawPixel(cardX + cardWidth - 1, infoTopY - 1, t.primaryTextBlack);
    }

    constexpr int textPad = 6;
    int textY = infoTopY + textPad;
    const int maxTitleLines = std::max(1, (infoBottomY - textY - titleLineHeight) / titleLineHeight);
    const auto titleLines =
        r.wrapTextWithHyphenation(titleFontId, v.bookTitle, cardWidth - 10, std::min(3, maxTitleLines));
    for (const auto& line : titleLines) {
      const int lw = r.getTextWidth(titleFontId, line.c_str());
      r.drawText(titleFontId, cardX + (cardWidth - lw) / 2, textY, line.c_str(), t.primaryTextBlack);
      textY += titleLineHeight;
    }

    if (v.bookAuthor[0] != '\0') {
      textY += titleLineHeight / 6;
      const auto trunc = r.truncatedText(titleFontId, v.bookAuthor, cardWidth - 10);
      const int aw = r.getTextWidth(titleFontId, trunc.c_str());
      r.drawText(titleFontId, cardX + (cardWidth - aw) / 2, textY, trunc.c_str(), t.secondaryTextBlack);
    }

    // Bottom separator line - hand-drawn sketchy style (only when no art)
    if (!v.useArtBackground) {
      // Draw 2 slightly offset lines for a hand-drawn feel
      r.drawLine(cardX + 2, infoBottomY, cardX + cardWidth - 3, infoBottomY, t.primaryTextBlack);
      r.drawLine(cardX, infoBottomY + 1, cardX + cardWidth - 1, infoBottomY + 1, t.primaryTextBlack);
      // Small ink blob at left end
      r.drawPixel(cardX + 1, infoBottomY - 1, t.primaryTextBlack);
      r.drawPixel(cardX - 1, infoBottomY + 2, t.primaryTextBlack);
    }
  }

  // === PROGRESS BAR (hand-drawn style, replaces old button area) ===
  if (v.hasBook && v.bookProgress >= 0) {
    const int barY = infoBottomY + 16;
    constexpr int barH = 14;
    constexpr int barPad = 20;  // Inset from card edges
    const int barX = cardX + barPad;
    const int barW = cardWidth - 2 * barPad;

    // Hand-drawn border: double-line for thick sketchy feel
    r.drawRect(barX, barY, barW, barH, t.primaryTextBlack);
    r.drawRect(barX + 1, barY + 1, barW - 2, barH - 2, t.primaryTextBlack);
    // Corner ink blobs
    r.drawPixel(barX - 1, barY - 1, t.primaryTextBlack);
    r.drawPixel(barX + barW, barY - 1, t.primaryTextBlack);
    r.drawPixel(barX - 1, barY + barH, t.primaryTextBlack);
    r.drawPixel(barX + barW, barY + barH, t.primaryTextBlack);

    // Fill portion (inside the double border)
    const int fillMax = barW - 6;
    int fillW = v.bookProgress * fillMax / 100;
    if (fillW < 1 && v.bookProgress > 0) fillW = 1;
    if (fillW > 0) {
      r.fillRect(barX + 3, barY + 3, fillW, barH - 6, t.primaryTextBlack);
    }

    // Progress text below bar
    const int textY = barY + barH + 6;
    char progressText[32];
    if (v.bookTotalPages > 0) {
      if (v.isChapterBased) {
        snprintf(progressText, sizeof(progressText), "Chapter %d of %d", v.bookCurrentPage, v.bookTotalPages);
      } else {
        snprintf(progressText, sizeof(progressText), "Page %d of %d", v.bookCurrentPage, v.bookTotalPages);
      }
    } else {
      // Just show percentage when we don't have page counts
      snprintf(progressText, sizeof(progressText), "%d%% complete", v.bookProgress);
    }
    const int ptw = r.getTextWidth(t.uiFontId, progressText);
    r.drawText(t.uiFontId, cardX + (cardWidth - ptw) / 2, textY, progressText, t.secondaryTextBlack);
  }

  // === LIBRARY CAROUSEL (dots at bottom showing position) ===
  if (v.recentBookCount > 0) {
    const int carouselY = pageHeight - 35;
    
    // Draw carousel dots showing position
    const int totalDots = v.recentBookCount + 1;  // +1 for current book
    constexpr int dotSpacing = 16;
    constexpr int dotRadius = 4;
    const int dotsWidth = (totalDots - 1) * dotSpacing;
    const int dotsStartX = (pageWidth - dotsWidth) / 2;
    
    for (int i = 0; i < totalDots; i++) {
      const int dotX = dotsStartX + i * dotSpacing;
      const bool isSelected = (i == v.selectedBookIndex);
      
      if (isSelected) {
        // Filled dot for selected
        r.fillRect(dotX - dotRadius, carouselY - dotRadius, dotRadius * 2, dotRadius * 2, t.primaryTextBlack);
      } else {
        // Hollow dot for unselected
        r.drawRect(dotX - dotRadius, carouselY - dotRadius, dotRadius * 2, dotRadius * 2, t.primaryTextBlack);
      }
    }
  }

  // Note: displayBuffer() is NOT called here; HomeState will call it
  // after rendering the cover image on top of the card area
}

void render(const GfxRenderer& r, const Theme& t, const FileListView& v) {
  r.clearScreen(t.backgroundColor);

  // Title with path
  title(r, t, t.screenMarginTop, "Files");

  // Current path (truncated if needed)
  const int pathY = 40;
  const int maxPathW = r.getScreenWidth() - 2 * t.screenMarginSide - 16;
  const auto truncPath = r.truncatedText(t.smallFontId, v.currentPath, maxPathW);
  r.drawText(t.smallFontId, t.screenMarginSide + 8, pathY, truncPath.c_str(), t.secondaryTextBlack);

  // File list
  const int listStartY = 65;
  const int pageStart = v.getPageStart();
  const int pageEnd = v.getPageEnd();

  for (int i = pageStart; i < pageEnd; i++) {
    const int y = listStartY + (i - pageStart) * (t.itemHeight + t.itemSpacing);
    fileEntry(r, t, y, v.files[i].name, v.files[i].isDirectory, i == v.selected);
  }

  // Page indicator
  if (v.getPageCount() > 1) {
    char pageStr[16];
    snprintf(pageStr, sizeof(pageStr), "%d/%d", v.page + 1, v.getPageCount());
    const int pageY = r.getScreenHeight() - 50;
    centeredText(r, t, pageY, pageStr);
  }

  r.displayBuffer();
}

void render(const GfxRenderer& r, const Theme& t, ChapterListView& v) {
  r.clearScreen(t.backgroundColor);

  title(r, t, t.screenMarginTop, "Chapters");

  constexpr int listStartY = 60;
  const int availableHeight = r.getScreenHeight() - listStartY - 50;
  const int itemHeight = t.itemHeight + t.itemSpacing;
  const int visibleCount = availableHeight / itemHeight;

  v.ensureVisible(visibleCount);

  const int end = std::min(v.scrollOffset + visibleCount, static_cast<int>(v.chapterCount));
  for (int i = v.scrollOffset; i < end; i++) {
    const int y = listStartY + (i - v.scrollOffset) * itemHeight;
    chapterItem(r, t, t.uiFontId, y, v.chapters[i].title, v.chapters[i].depth, i == v.selected, i == v.currentChapter);
  }

  r.displayBuffer();
}

}  // namespace ui
