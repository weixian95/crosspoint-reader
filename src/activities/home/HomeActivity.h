#pragma once

#include <string>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class HomeActivity final : public Activity {
  static constexpr int MENU_COUNT = 4;

  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  bool backPressSeen = false;
  std::string resumePath;
  std::string resumeTitle;
  const HomeMenuItem initialMenuItem;

  static int menuItemToIndex(HomeMenuItem item);
  void activateSelection();

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                        HomeMenuItem initialMenuItemValue = HomeMenuItem::NONE)
      : Activity("Home", renderer, mappedInput), initialMenuItem(initialMenuItemValue) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isHomeActivity() const override { return true; }
};
