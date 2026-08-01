#pragma once

#include <GfxRenderer.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/LoadingCat.h"

namespace LoadingScreen {

// E-ink is intentionally paced slowly: this is a two-pose activity signal,
// not a high-frame-rate animation. X4 refreshes only the aligned cat window;
// X3's display driver safely promotes that request to a fast full-frame pass.
constexpr unsigned long FRAME_INTERVAL_MS = 1200;
constexpr int TEXT_GAP = 18;

struct State {
  bool active = false;
  bool secondFrame = false;
  unsigned long lastFrameAt = 0;
};

inline void drawCat(const GfxRenderer& renderer, const int x, const int y, const bool secondFrame) {
  const uint16_t* rows = secondFrame ? LoadingCat::FRAME_B : LoadingCat::FRAME_A;

  // Draw contiguous runs rather than 256 individual logical pixels. The masks
  // remain the compact source of truth (32 bytes per pose), while each run is
  // expanded to a chunky 2x2-pixel grid directly in the framebuffer.
  for (int row = 0; row < LoadingCat::LOGICAL_SIZE; row++) {
    const uint16_t mask = rows[row];
    int column = 0;
    while (column < LoadingCat::LOGICAL_SIZE) {
      while (column < LoadingCat::LOGICAL_SIZE && (mask & (0x8000U >> column)) == 0) column++;
      const int runStart = column;
      while (column < LoadingCat::LOGICAL_SIZE && (mask & (0x8000U >> column)) != 0) column++;
      if (runStart < column) {
        renderer.fillRect(x + runStart * LoadingCat::PIXEL_SCALE, y + row * LoadingCat::PIXEL_SCALE,
                          (column - runStart) * LoadingCat::PIXEL_SCALE, LoadingCat::PIXEL_SCALE, true);
      }
    }
  }
}

inline void layout(const GfxRenderer& renderer, int& catX, int& catY) {
  catX = (renderer.getScreenWidth() - LoadingCat::DISPLAY_SIZE) / 2;
  const int textHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int groupHeight = LoadingCat::DISPLAY_SIZE + TEXT_GAP + textHeight;
  catY = (renderer.getScreenHeight() - groupHeight) / 2;
}

inline void drawFrame(GfxRenderer& renderer, const bool secondFrame) {
  int catX;
  int catY;
  layout(renderer, catX, catY);
  renderer.clearScreen();
  drawCat(renderer, catX, catY, secondFrame);
  renderer.drawCenteredText(UI_12_FONT_ID, catY + LoadingCat::DISPLAY_SIZE + TEXT_GAP, tr(STR_LOADING), true,
                            EpdFontFamily::BOLD);
}

inline void show(GfxRenderer& renderer, State* state = nullptr) {
  if (state && state->active) {
    return;
  }

  drawFrame(renderer, false);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);

  if (state) {
    state->active = true;
    state->secondFrame = false;
    state->lastFrameAt = millis();
  }
}

// A chapter-inflate FrameBufferLoan returns the framebuffer cleared to white,
// while the persistent e-ink panel still shows this screen. Rebuild the same
// frame in RAM without refreshing so a later window tick has a valid full-frame
// fallback on X3.
inline void restoreFramebuffer(GfxRenderer& renderer, const State& state) {
  if (state.active && renderer.hasFrameBuffer()) drawFrame(renderer, state.secondFrame);
}

inline void tick(GfxRenderer& renderer, State& state) {
  if (!state.active || !renderer.hasFrameBuffer() || millis() - state.lastFrameAt < FRAME_INTERVAL_MS) return;

  int catX;
  int catY;
  layout(renderer, catX, catY);
  state.secondFrame = !state.secondFrame;
  renderer.fillRect(catX, catY, LoadingCat::DISPLAY_SIZE, LoadingCat::DISPLAY_SIZE, false);
  drawCat(renderer, catX, catY, state.secondFrame);
  renderer.displayWindow(catX, catY, LoadingCat::DISPLAY_SIZE, LoadingCat::DISPLAY_SIZE);
  state.lastFrameAt = millis();
}

inline void finish(State& state) { state.active = false; }

}  // namespace LoadingScreen
