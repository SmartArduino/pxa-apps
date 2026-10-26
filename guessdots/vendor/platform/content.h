#pragma once

#include <Arduboy2.h>

#include "point.h"

namespace lvl {

enum class Difficulty : uint8_t {
    NONE = 99,
    EASY = 0,
    MEDIUM = 1,
    HARD = 2,
    COUNT = 3, // NONE is a technical option, does not count
};

constexpr uint8_t DIFFICULTIES_CNT = static_cast<uint8_t>(lvl::Difficulty::COUNT);

constexpr uint32_t NULL_LEVEL = 0;

constexpr uint8_t tile_size_easy = 20;
constexpr uint8_t tile_size_medium = 18;
constexpr uint8_t tile_size_hard = 18;

constexpr uint8_t dots_capacity = 25;
constexpr uint8_t numbers_capacity = 16;

struct LevelMetadata {
  bool use_offsets;
  uint8_t tile_size;
  uint8_t dots_cnt;
  uint8_t numbers_cnt;

  uint32_t lvl_limit;
  uint8_t score_value;

  PointS8 dots[dots_capacity];
  PointS8 numbers[numbers_capacity];
  uint32_t cnt_masks[numbers_capacity];
};

/*
pre-calculated dots and numbers grid coords using following python3 function:

def f(xx: int, yy: int, t: int, n: int):
    x, y = xx, yy
    for i in range(n * n):
        print(f"{{.x = {x}, .y = {y}}},")
        if (i + 1) % n == 0:
            y += t
            x = xx
        else:
            x += t


pre-calculated max level limits using following python3 function

def max_num(bits):
    n = 0
    for i in range(bits):
        n |= 1 << i
    return n
*/

// predefined level metadatas (grid setup, bit masks for count nearest dots)
LevelMetadata lm[DIFFICULTIES_CNT] = {
    {
        .use_offsets = false,
        .tile_size = 3,
        .dots_cnt = 9,
        .numbers_cnt = 4,

        .lvl_limit = 511,
        .score_value = 1,

        .dots = {
            {.x = 44, .y = 12},
            {.x = 64, .y = 12},
            {.x = 84, .y = 12},
            {.x = 44, .y = 32},
            {.x = 64, .y = 32},
            {.x = 84, .y = 32},
            {.x = 44, .y = 52},
            {.x = 64, .y = 52},
            {.x = 84, .y = 52},
        },
        .numbers = {
            {.x = 54, .y = 22},
            {.x = 74, .y = 22},
            {.x = 54, .y = 42},
            {.x = 74, .y = 42},
        },
        .cnt_masks = {
            (u32)1 << 0 | (u32)1 << 1 | (u32)1 << 3 | (u32)1 << 4,
            (u32)1 << 1 | (u32)1 << 2 | (u32)1 << 4 | (u32)1 << 5,
            (u32)1 << 3 | (u32)1 << 4 | (u32)1 << 6 | (u32)1 << 7,
            (u32)1 << 4 | (u32)1 << 5 | (u32)1 << 7 | (u32)1 << 8,
        }
    },
    {
        .use_offsets = false,
        .tile_size = 4, 
        .dots_cnt = 16,
        .numbers_cnt = 9, 
        
        .lvl_limit = 65535,
        .score_value = 5,

        .dots = {
            {.x = 37, .y = 5},
            {.x = 55, .y = 5},
            {.x = 73, .y = 5},
            {.x = 91, .y = 5},
            {.x = 37, .y = 23},
            {.x = 55, .y = 23},
            {.x = 73, .y = 23},
            {.x = 91, .y = 23},
            {.x = 37, .y = 41},
            {.x = 55, .y = 41},
            {.x = 73, .y = 41},
            {.x = 91, .y = 41},
            {.x = 37, .y = 59},
            {.x = 55, .y = 59},
            {.x = 73, .y = 59},
            {.x = 91, .y = 59},
        },
        .numbers = {
            {.x = 46, .y = 14},
            {.x = 64, .y = 14},
            {.x = 82, .y = 14},
            {.x = 46, .y = 32},
            {.x = 64, .y = 32},
            {.x = 82, .y = 32},
            {.x = 46, .y = 50},
            {.x = 64, .y = 50},
            {.x = 82, .y = 50},
        },
        .cnt_masks = {
            (u32)1 << 0 | (u32)1 << 1 | (u32)1 << 4 | (u32)1 << 5,
            (u32)1 << 1 | (u32)1 << 2 | (u32)1 << 5 | (u32)1 << 6,
            (u32)1 << 2 | (u32)1 << 3 | (u32)1 << 6 | (u32)1 << 7,
            (u32)1 << 4 | (u32)1 << 5 | (u32)1 << 8 | (u32)1 << 9,
            (u32)1 << 5 | (u32)1 << 6 | (u32)1 << 9 | (u32)1 << 10,
            (u32)1 << 6 | (u32)1 << 7 | (u32)1 << 10 | (u32)1 << 11,
            (u32)1 << 8 | (u32)1 << 9 | (u32)1 << 12 | (u32)1 << 13,
            (u32)1 << 9 | (u32)1 << 10 | (u32)1 << 13 | (u32)1 << 14,
            (u32)1 << 10 | (u32)1 << 11 | (u32)1 << 14 | (u32)1 << 15,
        }
    },
    {
        .use_offsets = true,
        .tile_size = 5,
        .dots_cnt = 25,
        .numbers_cnt = 16,

        .lvl_limit = 33554431,
        .score_value = 10,

        .dots = {
            {.x = 28, .y = -4},
            {.x = 46, .y = -4},
            {.x = 64, .y = -4},
            {.x = 82, .y = -4},
            {.x = 100, .y = -4},
            {.x = 28, .y = 14},
            {.x = 46, .y = 14},
            {.x = 64, .y = 14},
            {.x = 82, .y = 14},
            {.x = 100, .y = 14},
            {.x = 28, .y = 32},
            {.x = 46, .y = 32},
            {.x = 64, .y = 32},
            {.x = 82, .y = 32},
            {.x = 100, .y = 32},
            {.x = 28, .y = 50},
            {.x = 46, .y = 50},
            {.x = 64, .y = 50},
            {.x = 82, .y = 50},
            {.x = 100, .y = 50},
            {.x = 28, .y = 68},
            {.x = 46, .y = 68},
            {.x = 64, .y = 68},
            {.x = 82, .y = 68},
            {.x = 100, .y = 68},
        },
        .numbers = {
            {.x = 37, .y = 5},
            {.x = 55, .y = 5},
            {.x = 73, .y = 5},
            {.x = 91, .y = 5},
            {.x = 37, .y = 23},
            {.x = 55, .y = 23},
            {.x = 73, .y = 23},
            {.x = 91, .y = 23},
            {.x = 37, .y = 41},
            {.x = 55, .y = 41},
            {.x = 73, .y = 41},
            {.x = 91, .y = 41},
            {.x = 37, .y = 59},
            {.x = 55, .y = 59},
            {.x = 73, .y = 59},
            {.x = 91, .y = 59},
        },
        .cnt_masks = {
            (u32)1 << 0 | (u32)1 << 1 | (u32)1 << 5 | (u32)1 << 6,
            (u32)1 << 1 | (u32)1 << 2 | (u32)1 << 6 | (u32)1 << 7,
            (u32)1 << 2 | (u32)1 << 3 | (u32)1 << 7 | (u32)1 << 8,
            (u32)1 << 3 | (u32)1 << 4 | (u32)1 << 8 | (u32)1 << 9,
            (u32)1 << 5 | (u32)1 << 6 | (u32)1 << 10 | (u32)1 << 11,
            (u32)1 << 6 | (u32)1 << 7 | (u32)1 << 11 | (u32)1 << 12,
            (u32)1 << 7 | (u32)1 << 8 | (u32)1 << 12 | (u32)1 << 13,
            (u32)1 << 8 | (u32)1 << 9 | (u32)1 << 13 | (u32)1 << 14,
            (u32)1 << 10 | (u32)1 << 11 | (u32)1 << 15 | (u32)1 << 16,
            (u32)1 << 11 | (u32)1 << 12 | (u32)1 << 16 | (u32)1 << 17,
            (u32)1 << 12 | (u32)1 << 13 | (u32)1 << 17 | (u32)1 << 18,
            (u32)1 << 13 | (u32)1 << 14 | (u32)1 << 18 | (u32)1 << 19,
            (u32)1 << 15 | (u32)1 << 16 | (u32)1 << 20 | (u32)1 << 21,
            (u32)1 << 16 | (u32)1 << 17 | (u32)1 << 21 | (u32)1 << 22,
            (u32)1 << 17 | (u32)1 << 18 | (u32)1 << 22 | (u32)1 << 23,
            (u32)1 << 18 | (u32)1 << 19 | (u32)1 << 23 | (u32)1 << 24,
        }
    }
};

}  // namespace dots_content