#pragma once

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/SleepingCat.h"
#include "fontIds.h"

namespace LoadingScreen {

constexpr int TEXT_GAP = 18;

struct State {
  bool active = false;
};

inline void layout(const GfxRenderer& renderer, int& catX, int& catY) {
  catX = (renderer.getScreenWidth() - SleepingCat::DISPLAY_WIDTH) / 2;
  const int textHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int groupHeight = SleepingCat::DISPLAY_HEIGHT + TEXT_GAP + textHeight;
  catY = (renderer.getScreenHeight() - groupHeight) / 2;
}

inline void drawFrame(GfxRenderer& renderer) {
  int catX;
  int catY;
  layout(renderer, catX, catY);
  renderer.clearScreen();
  SleepingCat::draw(renderer, catX, catY, SleepingCat::BubbleContent::Loading);
  renderer.drawCenteredText(UI_12_FONT_ID, catY + SleepingCat::DISPLAY_HEIGHT + TEXT_GAP, tr(STR_LOADING), true,
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
