#include "SleepActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "activities/reader/ReaderUtils.h"
#include "components/UITheme.h"
#include "fontIds.h"

void SleepActivity::onEnter() {
  Activity::onEnter();

  // The minimal reader never decodes a cover, wallpaper, or previous framebuffer
  // while sleeping. This keeps all book imagery confined to EPUB page rendering.
  if (APP_STATE.lastSleepFromReader) {
    ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
    GUI.drawPopup(renderer, tr(STR_ENTERING_SLEEP));
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  } else {
    GUI.drawPopup(renderer, tr(STR_ENTERING_SLEEP));
  }

  renderDefaultSleepScreen();
}

void SleepActivity::renderDefaultSleepScreen() const {
  renderer.clearScreen();
  renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_SLEEPING), true, EpdFontFamily::BOLD);
  renderer.invertScreen();
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
