#pragma once

#include <SdCardFontRegistry.h>

#include <atomic>

class GfxRenderer;

/// Compatibility facade for the web font-file manager. Reader/UI fallback is
/// built into flash in the focused profile, so this class only maintains the
/// optional SD registry and never loads a runtime font.
class SdCardFontSystem {
 public:
  SdCardFontSystem() = default;
  SdCardFontSystem(const SdCardFontSystem&) = delete;
  SdCardFontSystem& operator=(const SdCardFontSystem&) = delete;
  /// Discover optional legacy SD card fonts. Call once during setup.
  void begin(GfxRenderer& renderer);

  /// Re-discover after an optional web upload/delete. No font is loaded.
  void ensureLoaded(GfxRenderer& renderer);

  /// Access the registry (e.g. for settings UI to enumerate available fonts).
  const SdCardFontRegistry& registry() const { return registry_; }

  /// Non-const access to the registry (for FontInstaller).
  SdCardFontRegistry& registry() { return registry_; }

  /// Mark the registry as needing re-discovery.
  /// Thread-safe: can be called from the web server task.
  void markRegistryDirty() { registryDirty_.store(true, std::memory_order_release); }

  /// If the registry is dirty, re-scan the SD card now and clear the flag.
  /// Used by the web UI so uploaded/deleted fonts appear in the list
  /// without waiting for the reader activity to run ensureLoaded().
  void refreshIfDirty() {
    if (registryDirty_.exchange(false, std::memory_order_acquire)) {
      registry_.discover();
    }
  }

 private:
  SdCardFontRegistry registry_;
  std::atomic<bool> registryDirty_{false};
};

// Global SD card font system instance (defined in main.cpp).
extern SdCardFontSystem sdFontSystem;
