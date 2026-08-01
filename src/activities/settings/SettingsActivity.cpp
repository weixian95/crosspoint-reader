#include "SettingsActivity.h"

#include <GfxRenderer.h>

#include <cstdio>
#include <memory>

#include "MappedInputManager.h"
#include "activities/util/IntervalSelectionActivity.h"
#include "components/UITheme.h"

namespace {
constexpr int SETTING_COUNT = 2;
}  // namespace

void SettingsActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void SettingsActivity::openSleepTimeoutPicker() {
  startActivityForResult(
      std::make_unique<IntervalSelectionActivity>(
          renderer, mappedInput, "SleepTimeoutInterval", StrId::STR_TIME_TO_SLEEP, SETTINGS.sleepTimeoutMinutes,
          CrossPointSettings::MIN_SLEEP_TIMEOUT_MINUTES, CrossPointSettings::MAX_SLEEP_TIMEOUT_MINUTES, 1, 5,
          StrId::STR_SLEEP_TIMER_VALUE_FORMAT, false, true, StrId::STR_SLEEP_NEVER),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          SETTINGS.sleepTimeoutMinutes = static_cast<uint8_t>(std::get<IntervalResult>(result.data).value);
          SETTINGS.saveToFile();
        }
        requestUpdate();
      });
}

void SettingsActivity::activateSelection() {
  if (selectedIndex == 0) {
    openSleepTimeoutPicker();
    return;
  }

  SETTINGS.sideButtonLayout = SETTINGS.sideButtonLayout == CrossPointSettings::PREV_NEXT
                                  ? CrossPointSettings::NEXT_PREV
                                  : CrossPointSettings::PREV_NEXT;
  SETTINGS.saveToFile();
  requestUpdate();
}

void SettingsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    onGoHome();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight =
      renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  int touched = selectedIndex;
  const auto listTouch = handleListTouch(touched, SETTING_COUNT, contentTop, contentHeight, false);
  if (listTouch != ListTouchResult::None) {
    selectedIndex = touched;
    if (listTouch == ListTouchResult::Activated) activateSelection();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, SETTING_COUNT);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, SETTING_COUNT);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) activateSelection();
}

void SettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SETTINGS_TITLE),
                 CROSSPOINT_VERSION);

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, SETTING_COUNT, selectedIndex,
      [](const int index) { return std::string(index == 0 ? tr(STR_TIME_TO_SLEEP) : tr(STR_SIDE_BTN_LAYOUT)); },
      nullptr, nullptr,
      [](const int index) {
        if (index == 1) {
          return std::string(SETTINGS.sideButtonLayout == CrossPointSettings::PREV_NEXT ? tr(STR_PREV_NEXT)
                                                                                        : tr(STR_NEXT_PREV));
        }
        if (SETTINGS.sleepTimeoutMinutes >= CrossPointSettings::SLEEP_TIMEOUT_NEVER_MINUTES) {
          return std::string(tr(STR_SLEEP_NEVER));
        }
        char value[32];
        snprintf(value, sizeof(value), tr(STR_SLEEP_TIMER_VALUE_FORMAT),
                 static_cast<unsigned int>(SETTINGS.sleepTimeoutMinutes));
        return std::string(value);
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
