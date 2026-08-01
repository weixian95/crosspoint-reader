#include "TxtReaderActivity.h"

#include <BidiUtils.h>
#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Serialization.h>
#include <Utf8.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "ProgressFile.h"
#include "ReaderUtils.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr size_t CHUNK_SIZE = 8 * 1024;  // 8KB chunk for reading
// Cache file magic and version
constexpr uint32_t CACHE_MAGIC = 0x54585449;  // "TXTI"
constexpr uint8_t CACHE_VERSION = 6;          // v6: incremental page index with completion marker

// Advance to the next decoded UTF-8 codepoint. Using the shared decoder here is
// important: it also advances safely over malformed input instead of treating a
// run of stray continuation bytes as one enormous character.
size_t nextUtf8Offset(const std::string& text, const size_t offset) {
  if (offset >= text.size()) return text.size();

  const auto* cursor = reinterpret_cast<const uint8_t*>(text.c_str() + offset);
  utf8NextCodepoint(&cursor);
  const size_t next = static_cast<size_t>(reinterpret_cast<const char*>(cursor) - text.c_str());
  return (next > offset && next <= text.size()) ? next : offset + 1;
}

// Return the largest prefix that fits the viewport, preferring the last ASCII
// space within that prefix. The old implementation started at text.size() and
// remeasured after removing one codepoint at a time. A long Chinese paragraph
// therefore performed millions of repeated glyph lookups per visual line.
//
// Probe eight codepoints at a time, then refine only the final group. This keeps
// measurements bounded to short prefixes regardless of how much of the 8KB read
// buffer remains in the source paragraph.
size_t findWrapPosition(const GfxRenderer& renderer, const int fontId, const std::string& text, const int maxWidth) {
  if (text.empty()) return 0;

  // Ordinary short source lines are overwhelmingly likely to fit and are
  // cheapest to decide with one measurement.
  constexpr size_t DIRECT_MEASURE_BYTES = 256;
  if (text.size() <= DIRECT_MEASURE_BYTES &&
      renderer.getTextAdvanceX(fontId, text.c_str(), EpdFontFamily::REGULAR) <= maxWidth) {
    return text.size();
  }

  constexpr int CODEPOINTS_PER_PROBE = 8;
  size_t lastFit = 0;
  size_t lastSpaceAtFit = std::string::npos;
  size_t latestSpaceSeen = std::string::npos;
  size_t probeEnd = 0;
  int codepointsInProbe = 0;

  while (probeEnd < text.size()) {
    const size_t codepointStart = probeEnd;
    probeEnd = nextUtf8Offset(text, probeEnd);
    if (codepointStart > 0 && text[codepointStart] == ' ') latestSpaceSeen = codepointStart;
    codepointsInProbe++;

    if (codepointsInProbe < CODEPOINTS_PER_PROBE && probeEnd < text.size()) continue;

    const std::string prefix = text.substr(0, probeEnd);
    if (renderer.getTextAdvanceX(fontId, prefix.c_str(), EpdFontFamily::REGULAR) <= maxWidth) {
      lastFit = probeEnd;
      lastSpaceAtFit = latestSpaceSeen;
      codepointsInProbe = 0;
      continue;
    }

    // Only the most recent group can straddle the viewport edge. Refine that
    // group one codepoint at a time (at most eight additional measurements).
    size_t refinedEnd = lastFit;
    size_t refinedSpace = lastSpaceAtFit;
    while (refinedEnd < probeEnd) {
      const size_t codepointStart = refinedEnd;
      const size_t candidateEnd = nextUtf8Offset(text, refinedEnd);
      const std::string candidate = text.substr(0, candidateEnd);
      if (renderer.getTextAdvanceX(fontId, candidate.c_str(), EpdFontFamily::REGULAR) > maxWidth) break;
      refinedEnd = candidateEnd;
      if (codepointStart > 0 && text[codepointStart] == ' ') refinedSpace = codepointStart;
    }

    if (refinedSpace != std::string::npos) return refinedSpace;
    if (refinedEnd > 0) return refinedEnd;
    // Even one glyph is wider than the viewport: consume that complete glyph
    // rather than splitting its UTF-8 byte sequence.
    return nextUtf8Offset(text, 0);
  }

  return text.size();
}
}  // namespace

void TxtReaderActivity::onEnter() {
  Activity::onEnter();

  if (!txt) {
    return;
  }

  ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);

  txt->setupCacheDir();

  // Save current txt as last opened file and add to recent books
  auto filePath = txt->getPath();
  auto fileName = filePath.substr(filePath.rfind('/') + 1);
  APP_STATE.openEpubPath = filePath;
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(filePath, fileName, "");

  // Trigger first update
  requestUpdate();
}

void TxtReaderActivity::onExit() {
  Activity::onExit();

  // Reset orientation back to portrait for the rest of the UI
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  if (txt && initialized && indexDirty) savePageIndexCache();
  pageOffsets.clear();
  currentPageLines.clear();
  APP_STATE.readerActivityLoadCount = 0;
  APP_STATE.saveToFile();
  txt.reset();
}

void TxtReaderActivity::loop() {
  if (ReaderUtils::handleBackNavigation(mappedInput, activityManager, txt ? txt->getPath().c_str() : "",
                                        {this, [](void* ctx) { static_cast<TxtReaderActivity*>(ctx)->onGoHome(); }})) {
    return;
  }

  const auto touch = ReaderUtils::detectTouchPageTurn(renderer, mappedInput);
  auto [prevTriggered, nextTriggered, fromTilt] = ReaderUtils::detectPageTurn(mappedInput);
  prevTriggered = prevTriggered || touch.prev;
  nextTriggered = nextTriggered || touch.next;
  if (!prevTriggered && !nextTriggered) {
    return;
  }

  if (prevTriggered && currentPage > 0) {
    currentPage--;
    requestUpdate();
  } else if (nextTriggered) {
    if (currentPage < totalPages - 1) {
      currentPage++;
      requestUpdate();
    } else if (!indexComplete && !pageOffsets.empty()) {
      std::vector<std::string> ignoredLines;
      size_t nextOffset = pageOffsets.back();
      const size_t currentOffset = pageOffsets.back();
      if (loadPageAtOffset(currentOffset, ignoredLines, nextOffset) && nextOffset > currentOffset &&
          nextOffset < txt->getFileSize()) {
        pageOffsets.push_back(nextOffset);
        totalPages = static_cast<int>(pageOffsets.size());
        currentPage++;
        indexDirty = true;
        // Persist occasionally without turning every page into several FAT operations.
        if ((pageOffsets.size() % 32U) == 0U) {
          savePageIndexCache();
          indexDirty = false;
        }
        requestUpdate();
      } else {
        indexComplete = true;
        indexDirty = true;
        savePageIndexCache();
        indexDirty = false;
        onGoHome();
      }
    } else {
      onGoHome();
    }
  }
}

void TxtReaderActivity::initializeReader() {
  if (initialized) {
    return;
  }

  // Store current settings for cache validation
  cachedFontId = SETTINGS.getReaderFontId();
  cachedScreenMargin = SETTINGS.screenMargin;
  cachedParagraphAlignment = SETTINGS.paragraphAlignment;

  // Calculate viewport dimensions
  renderer.getOrientedViewableTRBL(&cachedOrientedMarginTop, &cachedOrientedMarginRight, &cachedOrientedMarginBottom,
                                   &cachedOrientedMarginLeft);
  cachedOrientedMarginTop +=
      std::max(cachedScreenMargin, static_cast<uint8_t>(renderer.getGlobalBatteryOverlayHeight()));
  cachedOrientedMarginLeft += cachedScreenMargin;
  cachedOrientedMarginRight += cachedScreenMargin;
  cachedOrientedMarginBottom +=
      std::max(cachedScreenMargin, static_cast<uint8_t>(UITheme::getInstance().getStatusBarHeight()));

  viewportWidth = renderer.getScreenWidth() - cachedOrientedMarginLeft - cachedOrientedMarginRight;
  const int viewportHeight = renderer.getScreenHeight() - cachedOrientedMarginTop - cachedOrientedMarginBottom;
  const int lineHeight = renderer.getLineHeight(cachedFontId);

  linesPerPage = viewportHeight / lineHeight;
  if (linesPerPage < 1) linesPerPage = 1;

  LOG_DBG("TRS", "Viewport: %dx%d, lines per page: %d", viewportWidth, viewportHeight, linesPerPage);

  // Load only the offsets discovered during earlier reading. A new book starts
  // immediately at byte zero; later pages are indexed on demand during turns.
  if (!loadPageIndexCache()) {
    pageOffsets.clear();
    if (txt->getFileSize() > 0) pageOffsets.push_back(0);
    totalPages = static_cast<int>(pageOffsets.size());
    indexComplete = txt->getFileSize() == 0;
    indexDirty = true;
  }

  // Load saved progress
  loadProgress();

  initialized = true;
}

bool TxtReaderActivity::loadPageAtOffset(size_t offset, std::vector<std::string>& outLines, size_t& nextOffset) {
  outLines.clear();
  const size_t fileSize = txt->getFileSize();

  if (offset >= fileSize) {
    return false;
  }

  // Read a chunk from file
  size_t chunkSize = std::min(CHUNK_SIZE, fileSize - offset);
  auto* buffer = static_cast<uint8_t*>(malloc(chunkSize + 1));
  if (!buffer) {
    LOG_ERR("TRS", "Failed to allocate %zu bytes", chunkSize);
    return false;
  }

  if (!txt->readContent(buffer, offset, chunkSize)) {
    free(buffer);
    return false;
  }
  buffer[chunkSize] = '\0';

  // Prime the SD card font's advance table with this chunk's codepoints.
  // Without this, every getTextAdvanceX() call in the wrap loop below triggers
  // on-demand glyph loads through the 8-slot overflow ring buffer, which
  // thrashes for any text with more than 8 unique chars (i.e. all English),
  // floods the heap with short-lived bitmap allocations, and eventually
  // corrupts FreeRTOS state. The advance table persists across calls per
  // font, so the cost amortizes to ~ASCII-size after the first chunk.
  if (renderer.isSdCardFont(cachedFontId)) {
    renderer.ensureSdCardFontReady(cachedFontId, reinterpret_cast<const char*>(buffer), /*styleMask=*/0x01);
  }

  // Parse lines from buffer
  size_t pos = 0;

  while (pos < chunkSize && static_cast<int>(outLines.size()) < linesPerPage) {
    // Find end of line
    size_t lineEnd = pos;
    while (lineEnd < chunkSize && buffer[lineEnd] != '\n') {
      lineEnd++;
    }

    // Check if we have a complete line
    bool lineComplete = (lineEnd < chunkSize) || (offset + lineEnd >= fileSize);

    if (!lineComplete && static_cast<int>(outLines.size()) > 0) {
      // Incomplete line and we already have some lines, stop here
      break;
    }

    // Calculate the actual length of line content in the buffer (excluding newline)
    size_t lineContentLen = lineEnd - pos;

    // Check for carriage return
    bool hasCR = (lineContentLen > 0 && buffer[pos + lineContentLen - 1] == '\r');
    size_t displayLen = hasCR ? lineContentLen - 1 : lineContentLen;

    // Extract line content for display (without CR/LF)
    std::string line(reinterpret_cast<char*>(buffer + pos), displayLen);

    // Track position within this source line (in bytes from pos)
    size_t lineBytePos = 0;

    // Emit at least one visual line for each source line (including blank lines),
    // then continue with wrapping when needed.
    do {
      if (line.empty()) {
        outLines.emplace_back();
        break;
      }

      const size_t breakPos = findWrapPosition(renderer, cachedFontId, line, viewportWidth);

      if (breakPos >= line.size()) {
        outLines.push_back(line);
        lineBytePos = displayLen;  // Consumed entire display content
        line.clear();
        break;
      }

      outLines.push_back(line.substr(0, breakPos));

      // Skip space at break point
      size_t skipChars = breakPos;
      if (breakPos < line.length() && line[breakPos] == ' ') {
        skipChars++;
      }
      lineBytePos += skipChars;
      line = line.substr(skipChars);
    } while (!line.empty() && static_cast<int>(outLines.size()) < linesPerPage);

    // Determine how much of the source buffer we consumed
    if (line.empty()) {
      // Fully consumed this source line, move past the newline
      pos = lineEnd + 1;
    } else {
      // Partially consumed - page is full mid-line
      // Move pos to where we stopped in the line (NOT past the line)
      pos = pos + lineBytePos;
      break;
    }
  }

  // Ensure we make progress even if calculations go wrong
  if (pos == 0 && !outLines.empty()) {
    // Fallback: at minimum, consume something to avoid infinite loop
    pos = 1;
  }

  nextOffset = offset + pos;

  // Make sure we don't go past the file
  if (nextOffset > fileSize) {
    nextOffset = fileSize;
  }

  free(buffer);

  return !outLines.empty();
}

void TxtReaderActivity::render(RenderLock&&) {
  if (!txt) {
    return;
  }

  // Initialize reader if not done
  if (!initialized) {
    initializeReader();
  }

  if (pageOffsets.empty()) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_EMPTY_FILE), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  // Bounds check
  if (currentPage < 0) currentPage = 0;
  if (currentPage >= totalPages) currentPage = totalPages - 1;

  // Load current page content
  size_t offset = pageOffsets[currentPage];
  size_t nextOffset;
  currentPageLines.clear();
  loadPageAtOffset(offset, currentPageLines, nextOffset);
  renderedNextOffset = nextOffset;

  renderer.clearScreen();
  renderPage();

  // Save progress
  saveProgress();
}

void TxtReaderActivity::renderPage() {
  const int lineHeight = renderer.getLineHeight(cachedFontId);
  const int contentWidth = viewportWidth;

  // Render text lines with alignment
  auto renderLines = [&]() {
    int y = cachedOrientedMarginTop;
    for (const auto& line : currentPageLines) {
      if (!line.empty()) {
        int x = cachedOrientedMarginLeft;
        const bool lineIsRtl = BidiUtils::startsWithRtl(line.c_str(), BidiUtils::RTL_PARAGRAPH_PROBE_DEPTH);
        uint8_t effectiveAlignment = cachedParagraphAlignment;
        if (lineIsRtl && (effectiveAlignment == CrossPointSettings::LEFT_ALIGN ||
                          effectiveAlignment == CrossPointSettings::JUSTIFIED)) {
          effectiveAlignment = CrossPointSettings::RIGHT_ALIGN;
        }
        const int textWidth = renderer.getTextAdvanceX(cachedFontId, line.c_str(), EpdFontFamily::REGULAR);

        // Apply text alignment
        switch (effectiveAlignment) {
          case CrossPointSettings::LEFT_ALIGN:
          default:
            // x already set to left margin
            break;
          case CrossPointSettings::CENTER_ALIGN: {
            x = cachedOrientedMarginLeft + (contentWidth - textWidth) / 2;
            break;
          }
          case CrossPointSettings::RIGHT_ALIGN: {
            x = cachedOrientedMarginLeft + contentWidth - textWidth;
            break;
          }
          case CrossPointSettings::JUSTIFIED:
            // For plain text, justified is treated as left-aligned
            // (true justification would require word spacing adjustments)
            break;
        }

        renderer.drawText(cachedFontId, x, y, line.c_str());
      }
      y += lineHeight;
    }
  };

  // Font prewarm: scan pass accumulates text, then prewarm, then real render
  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope();
  renderLines();  // scan pass — text accumulated, no drawing
  scope.endScanAndPrewarm();

  // BW rendering
  renderLines();
  renderStatusBar();

  ReaderUtils::displayWithRefreshCycle(renderer, pagesUntilFullRefresh);

  // scope destructor clears font cache via FontCacheManager
}

void TxtReaderActivity::renderStatusBar() const {
  const size_t fileSize = txt->getFileSize();
  const size_t pageEnd = std::min(renderedNextOffset, fileSize);
  const float progress = fileSize > 0 ? static_cast<float>(pageEnd) * 100.0f / static_cast<float>(fileSize) : 0;

  int displayTotalPages = totalPages;
  if (!indexComplete && pageEnd > 0 && currentPage >= 0) {
    const size_t pagesRead = static_cast<size_t>(currentPage + 1);
    const size_t estimated = (fileSize * pagesRead + pageEnd - 1) / pageEnd;
    displayTotalPages = std::max(displayTotalPages, static_cast<int>(estimated));
  }

  GUI.drawStatusBar(renderer, progress, currentPage + 1, displayTotalPages, txt->getTitle(), 0, 0, true, false,
                    !indexComplete);
}

void TxtReaderActivity::saveProgress() const {
  uint8_t data[4];
  data[0] = currentPage & 0xFF;
  data[1] = (currentPage >> 8) & 0xFF;
  data[2] = 0;
  data[3] = 0;
  if (!ProgressFile::writeAtomic(txt->getCachePath(), data, sizeof(data))) {
    LOG_ERR("TRS", "Failed to save progress: page %d", currentPage);
  }
}

void TxtReaderActivity::loadProgress() {
  HalFile f;
  if (Storage.openFileForRead("TRS", txt->getCachePath() + "/progress.bin", f)) {
    uint8_t data[4];
    if (f.read(data, 4) == 4) {
      currentPage = data[0] + (data[1] << 8);
      if (currentPage >= totalPages) {
        currentPage = totalPages - 1;
      }
      if (currentPage < 0) {
        currentPage = 0;
      }
      LOG_DBG("TRS", "Loaded progress: page %d/%d", currentPage, totalPages);
    }
  }
}

bool TxtReaderActivity::loadPageIndexCache() {
  // Cache file format (using serialization module):
  // - uint32_t: magic "TXTI"
  // - uint8_t: cache version
  // - uint32_t: file size (to validate cache)
  // - int32_t: viewport width
  // - int32_t: lines per page
  // - int32_t: font ID (to invalidate cache on font change)
  // - int32_t: screen margin (to invalidate cache on margin change)
  // - uint8_t: paragraph alignment (to invalidate cache on alignment change)
  // - uint8_t: whether the final page has been discovered
  // - uint32_t: discovered page count
  // - N * uint32_t: page offsets

  std::string cachePath = txt->getCachePath() + "/index.bin";
  HalFile f;
  if (!Storage.openFileForRead("TRS", cachePath, f)) {
    LOG_DBG("TRS", "No page index cache found");
    return false;
  }

  const auto readExact = [&f](auto& value) {
    return f.read(reinterpret_cast<uint8_t*>(&value), sizeof(value)) == sizeof(value);
  };

  uint32_t magic = 0;
  if (!readExact(magic)) return false;
  if (magic != CACHE_MAGIC) {
    LOG_DBG("TRS", "Cache magic mismatch, rebuilding");
    return false;
  }

  uint8_t version = 0;
  if (!readExact(version)) return false;
  if (version != CACHE_VERSION) {
    LOG_DBG("TRS", "Cache version mismatch (%d != %d), rebuilding", version, CACHE_VERSION);
    return false;
  }

  uint32_t fileSize = 0;
  if (!readExact(fileSize)) return false;
  if (fileSize != txt->getFileSize()) {
    LOG_DBG("TRS", "Cache file size mismatch, rebuilding");
    return false;
  }

  int32_t cachedWidth = 0;
  if (!readExact(cachedWidth)) return false;
  if (cachedWidth != viewportWidth) {
    LOG_DBG("TRS", "Cache viewport width mismatch, rebuilding");
    return false;
  }

  int32_t cachedLines = 0;
  if (!readExact(cachedLines)) return false;
  if (cachedLines != linesPerPage) {
    LOG_DBG("TRS", "Cache lines per page mismatch, rebuilding");
    return false;
  }

  int32_t fontId = 0;
  if (!readExact(fontId)) return false;
  if (fontId != cachedFontId) {
    LOG_DBG("TRS", "Cache font ID mismatch (%d != %d), rebuilding", fontId, cachedFontId);
    return false;
  }

  int32_t margin = 0;
  if (!readExact(margin)) return false;
  if (margin != cachedScreenMargin) {
    LOG_DBG("TRS", "Cache screen margin mismatch, rebuilding");
    return false;
  }

  uint8_t alignment = 0;
  if (!readExact(alignment)) return false;
  if (alignment != cachedParagraphAlignment) {
    LOG_DBG("TRS", "Cache paragraph alignment mismatch, rebuilding");
    return false;
  }

  uint8_t complete = 0;
  uint32_t numPages = 0;
  if (!readExact(complete) || !readExact(numPages)) return false;

  const uint32_t sourceSize = static_cast<uint32_t>(txt->getFileSize());
  if (complete > 1 || numPages == 0 || numPages > sourceSize + 1U) {
    LOG_ERR("TRS", "Invalid page index header");
    return false;
  }

  // Read page offsets
  pageOffsets.clear();
  pageOffsets.reserve(numPages);

  uint32_t previousOffset = 0;
  for (uint32_t i = 0; i < numPages; i++) {
    uint32_t offset = 0;
    if (!readExact(offset) || (i == 0 && offset != 0) || (i > 0 && offset <= previousOffset) || offset >= sourceSize) {
      LOG_ERR("TRS", "Invalid or truncated page index at entry %u", static_cast<unsigned>(i));
      pageOffsets.clear();
      return false;
    }
    pageOffsets.push_back(offset);
    previousOffset = offset;
  }

  totalPages = static_cast<int>(pageOffsets.size());
  indexComplete = complete != 0;
  indexDirty = false;
  LOG_DBG("TRS", "Loaded incremental page index: %d pages, complete=%d", totalPages, indexComplete);
  return true;
}

void TxtReaderActivity::savePageIndexCache() const {
  const std::string cachePath = txt->getCachePath() + "/index.bin";
  const std::string tempPath = cachePath + ".tmp";
  bool writeOk = true;
  {
    HalFile f;
    if (!Storage.openFileForWrite("TRS", tempPath, f)) {
      LOG_ERR("TRS", "Failed to open temporary page index cache");
      return;
    }

    const auto writeExact = [&f, &writeOk](const auto& value) {
      if (writeOk) {
        writeOk = f.write(reinterpret_cast<const uint8_t*>(&value), sizeof(value)) == sizeof(value);
      }
    };
    const uint32_t fileSize = static_cast<uint32_t>(txt->getFileSize());
    const int32_t width = viewportWidth;
    const int32_t lines = linesPerPage;
    const int32_t fontId = cachedFontId;
    const int32_t margin = cachedScreenMargin;
    const uint8_t complete = indexComplete ? 1 : 0;
    const uint32_t count = static_cast<uint32_t>(pageOffsets.size());

    writeExact(CACHE_MAGIC);
    writeExact(CACHE_VERSION);
    writeExact(fileSize);
    writeExact(width);
    writeExact(lines);
    writeExact(fontId);
    writeExact(margin);
    writeExact(cachedParagraphAlignment);
    writeExact(complete);
    writeExact(count);
    for (size_t offset : pageOffsets) {
      const uint32_t storedOffset = static_cast<uint32_t>(offset);
      writeExact(storedOffset);
    }
    if (writeOk) f.flush();
  }

  if (!writeOk) {
    LOG_ERR("TRS", "Short write saving page index cache");
    return;
  }
  Storage.remove(cachePath.c_str());
  if (!Storage.rename(tempPath.c_str(), cachePath.c_str())) {
    LOG_ERR("TRS", "Failed to promote temporary page index cache");
    return;
  }
  LOG_DBG("TRS", "Saved incremental page index: %d pages", totalPages);
}

ScreenshotInfo TxtReaderActivity::getScreenshotInfo() const {
  ScreenshotInfo info;
  info.readerType = ScreenshotInfo::ReaderType::Txt;
  if (txt) {
    const std::string t = txt->getTitle();
    snprintf(info.title, sizeof(info.title), "%s", t.c_str());
  }
  info.currentPage = currentPage + 1;
  info.totalPages = totalPages;
  info.progressPercent = totalPages > 0 ? static_cast<int>((currentPage + 1) * 100.0f / totalPages + 0.5f) : 0;
  if (info.progressPercent > 100) info.progressPercent = 100;
  return info;
}
