#pragma once

#include <GfxRenderer.h>

#include <cstdint>

namespace SleepingCat {

enum class BubbleContent : uint8_t {
  Loading,
  Sleeping,
};

constexpr int LOGICAL_WIDTH = 48;
constexpr int LOGICAL_HEIGHT = 48;
constexpr int PIXEL_SCALE = 3;
constexpr int DISPLAY_WIDTH = LOGICAL_WIDTH * PIXEL_SCALE;
constexpr int DISPLAY_HEIGHT = LOGICAL_HEIGHT * PIXEL_SCALE;

constexpr int CAT_Y = 19;
constexpr int CAT_HEIGHT = 23;
constexpr int CAT_BYTES_PER_ROW = LOGICAL_WIDTH / 8;

// Solid curled-cat silhouette shared by loading and sleeping screens. Cropping
// away its blank rows keeps this exact 1-bit asset to 138 bytes.
static constexpr uint8_t CAT_BITMAP[CAT_HEIGHT][CAT_BYTES_PER_ROW] = {
    {0x00, 0x00, 0x1f, 0xfc, 0x00, 0x00}, {0x01, 0x80, 0x3f, 0xff, 0x00, 0x00}, {0x00, 0xf3, 0xff, 0xff, 0xc0, 0x00},
    {0x00, 0xff, 0xff, 0xcf, 0xe0, 0x00}, {0x00, 0xff, 0xff, 0x6f, 0xf8, 0x00}, {0x00, 0xff, 0xff, 0xef, 0xfc, 0x00},
    {0x00, 0xff, 0xff, 0xff, 0xfc, 0x00}, {0x01, 0xff, 0xff, 0xff, 0xfe, 0x00}, {0x01, 0xff, 0xff, 0xf7, 0xff, 0x00},
    {0x01, 0xff, 0xff, 0xfb, 0xff, 0x00}, {0x03, 0xff, 0xff, 0xfb, 0xff, 0x80}, {0x03, 0xff, 0xff, 0xfb, 0xff, 0x80},
    {0x03, 0xff, 0xff, 0xfb, 0xff, 0x80}, {0x03, 0xff, 0xff, 0xfb, 0xff, 0x80}, {0x01, 0xff, 0xff, 0xf7, 0xfb, 0x80},
    {0x01, 0xe0, 0x60, 0x77, 0xf7, 0x80}, {0x01, 0xff, 0xff, 0xff, 0xef, 0x00}, {0x00, 0xff, 0x9f, 0xef, 0xdf, 0x00},
    {0x00, 0x41, 0xe1, 0xc0, 0xfe, 0x00}, {0x00, 0x3e, 0xde, 0x9f, 0xfc, 0x00}, {0x00, 0x7f, 0x7f, 0x7f, 0xf0, 0x00},
    {0x00, 0x7f, 0x7f, 0x7f, 0xc0, 0x00}, {0x00, 0x3e, 0x3e, 0x3e, 0x00, 0x00},
};

constexpr int BUBBLE_X = 24;
constexpr int BUBBLE_Y = 2;
constexpr int BUBBLE_HEIGHT = 17;
constexpr int BUBBLE_BYTES_PER_ROW = (LOGICAL_WIDTH - BUBBLE_X) / 8;

// A hand-normalized one-logical-pixel outline. Corner steps change direction
// without changing stroke thickness. The final four rows form the thought
// connector shared by both variants.
static constexpr uint8_t BUBBLE_BITMAP[BUBBLE_HEIGHT][BUBBLE_BYTES_PER_ROW] = {
    {0x00, 0x3f, 0xe0}, {0x00, 0xc0, 0x18}, {0x01, 0x00, 0x04}, {0x02, 0x00, 0x02}, {0x02, 0x00, 0x02},
    {0x02, 0x00, 0x02}, {0x02, 0x00, 0x02}, {0x02, 0x00, 0x02}, {0x02, 0x00, 0x02}, {0x01, 0x00, 0x04},
    {0x00, 0xc0, 0x18}, {0x00, 0x3f, 0xe0}, {0x01, 0x80, 0x00}, {0x01, 0x80, 0x00}, {0x00, 0x00, 0x00},
    {0x0c, 0x00, 0x00}, {0x00, 0x00, 0x00},
};

constexpr int LOADING_MARKS_Y = 7;
static constexpr uint8_t LOADING_MARKS[2][BUBBLE_BYTES_PER_ROW] = {
    {0x00, 0x63, 0x18},
    {0x00, 0x63, 0x18},
};

constexpr int SLEEPING_MARKS_Y = 5;
static constexpr uint8_t SLEEPING_MARKS[5][BUBBLE_BYTES_PER_ROW] = {
    {0x00, 0xee, 0xe0}, {0x00, 0x22, 0x20}, {0x00, 0x44, 0x40}, {0x00, 0x88, 0x80}, {0x00, 0xee, 0xe0},
};

inline bool packedPixelSet(const uint8_t* row, const int column) {
  return (row[column / 8] & (0x80U >> (column % 8))) != 0;
}

inline bool pixelSet(const BubbleContent content, const int row, const int column) {
  if (row >= CAT_Y && row < CAT_Y + CAT_HEIGHT && packedPixelSet(CAT_BITMAP[row - CAT_Y], column)) return true;

  if (row < BUBBLE_Y || row >= BUBBLE_Y + BUBBLE_HEIGHT || column < BUBBLE_X) return false;
  const int bubbleRow = row - BUBBLE_Y;
  const int bubbleColumn = column - BUBBLE_X;
  if (packedPixelSet(BUBBLE_BITMAP[bubbleRow], bubbleColumn)) return true;

  if (content == BubbleContent::Loading && row >= LOADING_MARKS_Y && row < LOADING_MARKS_Y + 2) {
    return packedPixelSet(LOADING_MARKS[row - LOADING_MARKS_Y], bubbleColumn);
  }
  if (content == BubbleContent::Sleeping && row >= SLEEPING_MARKS_Y && row < SLEEPING_MARKS_Y + 5) {
    return packedPixelSet(SLEEPING_MARKS[row - SLEEPING_MARKS_Y], bubbleColumn);
  }
  return false;
}

inline void draw(const GfxRenderer& renderer, const int x, const int y, const BubbleContent content) {
  // Draw contiguous runs rather than issuing one fill per logical pixel. Each
  // source pixel becomes a crisp 3x3 block directly in the framebuffer.
  for (int row = 0; row < LOGICAL_HEIGHT; row++) {
    int column = 0;
    while (column < LOGICAL_WIDTH) {
      while (column < LOGICAL_WIDTH && !pixelSet(content, row, column)) column++;
      const int runStart = column;
      while (column < LOGICAL_WIDTH && pixelSet(content, row, column)) column++;
      if (runStart < column) {
        renderer.fillRect(x + runStart * PIXEL_SCALE, y + row * PIXEL_SCALE, (column - runStart) * PIXEL_SCALE,
                          PIXEL_SCALE, true);
      }
    }
  }
}

static_assert(sizeof(CAT_BITMAP) + sizeof(BUBBLE_BITMAP) + sizeof(LOADING_MARKS) + sizeof(SLEEPING_MARKS) == 210,
              "shared sleeping-cat artwork must stay compact");

}  // namespace SleepingCat
