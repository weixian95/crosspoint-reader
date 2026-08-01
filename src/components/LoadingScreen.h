#pragma once

#include <GfxRenderer.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/LoadingCat.h"

namespace LoadingScreen {

constexpr int TEXT_GAP = 18;

struct State {
  bool active = false;
};

inline bool isCatPixelSet(const int row, const int column) {
  return (LoadingCat::BITMAP[row][column / 8] & (0x80U >> (column % 8))) != 0;
}

inline void drawCat(const GfxRenderer& renderer, const int x, const int y) {
  // Draw contiguous runs instead of issuing one fill per logical pixel. The
  // 48x48 source stays packed while rendering as crisp 3x3 hardware pixels.
  for (int row = 0; row < LoadingCat::LOGICAL_HEIGHT; row++) {
    int column = 0;
    while (column < LoadingCat::LOGICAL_WIDTH) {
      while (column < LoadingCat::LOGICAL_WIDTH && !isCatPixelSet(row, column)) column++;
      const int runStart = column;
      while (column < LoadingCat::LOGICAL_WIDTH && isCatPixelSet(row, column)) column++;
      if (runStart < column) {
        renderer.fillRect(x + runStart * LoadingCat::PIXEL_SCALE, y + row * LoadingCat::PIXEL_SCALE,
                          (column - runStart) * LoadingCat::PIXEL_SCALE, LoadingCat::PIXEL_SCALE, true);
      }
    }
  }
}

inline void layout(const GfxRenderer& renderer, int& catX, int& catY) {
  catX = (renderer.getScreenWidth() - LoadingCat::DISPLAY_WIDTH) / 2;
  const int textHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int groupHeight = LoadingCat::DISPLAY_HEIGHT + TEXT_GAP + textHeight;
  catY = (renderer.getScreenHeight() - groupHeight) / 2;
}

inline void drawFrame(GfxRenderer& renderer) {
  int catX;
  int catY;
  layout(renderer, catX, catY);
  renderer.clearScreen();
  drawCat(renderer, catX, catY);
  renderer.drawCenteredText(UI_12_FONT_ID, catY + LoadingCat::DISPLAY_HEIGHT + TEXT_GAP, tr(STR_LOADING), true,
                            EpdFontFamily::BOLD);
}

inline void show(GfxRenderer& renderer, State* state = nullptr) {
  if (state && state->active) {
    return;
  }

  drawFrame(renderer);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);

  if (state) state->active = true;
}

// A chapter-inflate FrameBufferLoan returns the framebuffer cleared to white,
// while the persistent e-ink panel still shows this screen. Rebuild the same
// static frame in RAM without triggering another panel refresh.
inline void restoreFramebuffer(GfxRenderer& renderer, const State& state) {
  if (state.active && renderer.hasFrameBuffer()) drawFrame(renderer);
}

inline void finish(State& state) { state.active = false; }

}  // namespace LoadingScreen
