#include "SleepActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "activities/reader/ReaderUtils.h"
#include "components/SleepingCat.h"
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
  constexpr int textGap = 18;

  renderer.clearScreen();
  const int textHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int groupHeight = SleepingCat::DISPLAY_HEIGHT + textGap + textHeight;
  const int catX = (renderer.getScreenWidth() - SleepingCat::DISPLAY_WIDTH) / 2;
  const int catY = (renderer.getScreenHeight() - groupHeight) / 2;
  SleepingCat::draw(renderer, catX, catY, SleepingCat::BubbleContent::Sleeping);
  renderer.drawCenteredText(UI_10_FONT_ID, catY + SleepingCat::DISPLAY_HEIGHT + textGap, tr(STR_SLEEPING), true,
                            EpdFontFamily::BOLD);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH, false);
}
