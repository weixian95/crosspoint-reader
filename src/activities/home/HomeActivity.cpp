#include "HomeActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <array>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"

namespace {
constexpr std::array<UIIcon, 4> MENU_ICONS = {Book, Folder, Transfer, Settings};
}  // namespace

int HomeActivity::menuItemToIndex(const HomeMenuItem item) {
  switch (item) {
    case HomeMenuItem::FILE_BROWSER:
      return 1;
    case HomeMenuItem::FILE_TRANSFER:
      return 2;
    case HomeMenuItem::SETTINGS_MENU:
      return 3;
    case HomeMenuItem::RECENTS:
    case HomeMenuItem::OPDS_BROWSER:
    case HomeMenuItem::NONE:
    default:
      return 0;
  }
}

void HomeActivity::onEnter() {
  Activity::onEnter();

  if (RECENT_BOOKS.pruneMissing()) RECENT_BOOKS.saveToFile();
  const auto& books = RECENT_BOOKS.getBooks();
  if (!books.empty()) {
    resumePath = books.front().path;
    resumeTitle = books.front().title;
    if (resumeTitle.empty()) resumeTitle = resumePath.substr(resumePath.find_last_of('/') + 1);
  }

  selectorIndex = menuItemToIndex(initialMenuItem);
  if (selectorIndex == 0 && resumePath.empty()) selectorIndex = 1;
  requestUpdate();
}

void HomeActivity::activateSelection() {
  switch (selectorIndex) {
    case 0:
      if (!resumePath.empty()) activityManager.goToReader(resumePath);
      break;
    case 1:
      activityManager.goToFileBrowser();
      break;
    case 2:
      activityManager.goToFileTransfer();
      break;
    case 3:
      activityManager.goToSettings();
      break;
    default:
      break;
  }
}

void HomeActivity::loop() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight =
      renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) backPressSeen = true;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && backPressSeen && !resumePath.empty()) {
    activityManager.goToReader(resumePath);
    return;
  }

  int touched = selectorIndex;
  const auto listTouch = handleListTouch(touched, MENU_COUNT, contentTop, contentHeight, false);
  if (listTouch != ListTouchResult::None) {
    selectorIndex = touched;
    if (listTouch == ListTouchResult::Activated) activateSelection();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, MENU_COUNT);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, MENU_COUNT);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) activateSelection();
}

void HomeActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, nullptr);

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, MENU_COUNT, selectorIndex,
      [](const int index) {
        switch (index) {
          case 0:
            return std::string(tr(STR_CONTINUE_READING));
          case 1:
            return std::string(tr(STR_BROWSE_FILES));
          case 2:
            return std::string(tr(STR_FILE_TRANSFER));
          default:
            return std::string(tr(STR_SETTINGS_TITLE));
        }
      },
      nullptr, [](const int index) { return MENU_ICONS[index]; },
      [this](const int index) { return index == 0 ? resumeTitle : std::string(); }, false,
      [this](const int index) { return index == 0 && resumePath.empty(); });

  const auto labels =
      mappedInput.mapLabels(resumePath.empty() ? "" : tr(STR_RESUME), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
