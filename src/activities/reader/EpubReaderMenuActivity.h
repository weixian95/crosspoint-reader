#pragma once

#include <I18n.h>

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class EpubReaderMenuActivity final : public Activity {
 public:
  enum class MenuAction { SELECT_CHAPTER, GO_HOME };

  explicit EpubReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  struct MenuItem {
    MenuAction action;
    StrId labelId;
  };

  void closeCancelled();

  const std::vector<MenuItem> menuItems = {{MenuAction::SELECT_CHAPTER, StrId::STR_SELECT_CHAPTER},
                                           {MenuAction::GO_HOME, StrId::STR_GO_HOME_BUTTON}};
  ButtonNavigator buttonNavigator;
  std::string title;
  int selectedIndex = 0;
};
