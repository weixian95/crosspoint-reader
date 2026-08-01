#include "SdCardFontSystem.h"

#include <Logging.h>

void SdCardFontSystem::begin(GfxRenderer& /*renderer*/) {
  registry_.discover();
  LOG_DBG("SDFS", "Optional SD font registry ready (%d families discovered)", registry_.getFamilyCount());
}

void SdCardFontSystem::ensureLoaded(GfxRenderer& /*renderer*/) {
  if (registryDirty_.exchange(false, std::memory_order_acquire)) {
    registry_.discover();
  }
}
