#pragma once

#include <I18n.h>

#include <vector>

#include "CrossPointSettings.h"
#include "activities/settings/SettingsActivity.h"

// The minimal-reader profile intentionally exposes only choices that affect
// basic device operation. Rendering parameters are fixed so page caches remain
// valid and a stale settings.json cannot re-enable expensive reader features.
inline std::vector<SettingInfo> getSettingsList() {
  return {
      SettingInfo::Value(
          StrId::STR_TIME_TO_SLEEP, &CrossPointSettings::sleepTimeoutMinutes,
          {CrossPointSettings::MIN_SLEEP_TIMEOUT_MINUTES, CrossPointSettings::MAX_SLEEP_TIMEOUT_MINUTES, 1},
          "sleepTimeoutMinutes", StrId::STR_CAT_SYSTEM),
      SettingInfo::Enum(StrId::STR_SIDE_BTN_LAYOUT, &CrossPointSettings::sideButtonLayout,
                        {StrId::STR_PREV_NEXT, StrId::STR_NEXT_PREV}, "sideButtonLayout", StrId::STR_CAT_CONTROLS),
  };
}
