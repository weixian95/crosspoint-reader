#pragma once

#include <cstdint>

namespace LoadingCat {

constexpr int LOGICAL_SIZE = 16;
constexpr int PIXEL_SCALE = 2;
constexpr int DISPLAY_SIZE = LOGICAL_SIZE * PIXEL_SCALE;

// Two deliberately coarse 1-bit poses. Bit 15 is the left-most pixel.
// The body and tail stay registered; the eyes and raised paw move so an
// A-B-A cycle reads as the cat licking its paw rather than waving.
static constexpr uint16_t FRAME_A[LOGICAL_SIZE] = {
    0x1830,  // ...##.....##....
    0x3838,  // ..###.....###...
    0x3ffc,  // ..############..
    0x7ffe,  // .##############.
    0x67e6,  // .##..######..##.
    0x6ff6,  // .##.########.##.
    0x7dbe,  // .#####.##.#####.
    0x3efc,  // ..#####.######..
    0x1e78,  // ...####..####...
    0x1cf8,  // ...###..#####...
    0x39f8,  // ..###..######...
    0x33f8,  // ..##..#######...
    0x3ffe,  // ..#############.
    0x3fc6,  // ..########...##.
    0x3b86,  // ..###.###....##.
    0x3b8e,  // ..###.###...###.
};

static constexpr uint16_t FRAME_B[LOGICAL_SIZE] = {
    0x1830,  // ...##.....##....
    0x3838,  // ..###.....###...
    0x3ffc,  // ..############..
    0x7ffe,  // .##############.
    0x7ffe,  // .##############.
    0x73ce,  // .###..####..###.
    0x7c3e,  // .#####....#####.
    0x3cfc,  // ..####..######..
    0x1cf0,  // ...###..####....
    0x19f0,  // ...##..#####....
    0x33f8,  // ..##..#######...
    0x33f8,  // ..##..#######...
    0x3ffe,  // ..#############.
    0x3fc6,  // ..########...##.
    0x3b86,  // ..###.###....##.
    0x3b8e,  // ..###.###...###.
};

static_assert(sizeof(FRAME_A) == 32 && sizeof(FRAME_B) == 32, "loading poses must stay compact");

}  // namespace LoadingCat
