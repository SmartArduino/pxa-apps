// SPDX-License-Identifier: MIT
#pragma once
#include "core.h"
#include <string.h>
namespace advisor2048 {
inline bool move(uint8_t board[4][4], unsigned direction) {
    uint32_t score = 0;
    switch (direction) {
    case 0:
        return moveUp(board, &score);
    case 1:
        return moveRight(board, &score);
    case 2:
        return moveDown(board, &score);
    default:
        return moveLeft(board, &score);
    }
}
inline int value(const uint8_t board[4][4]) {
    int empty = 0, neighbors = 0, smooth = 0, largest = 0, corner = 0;
    for (int x = 0; x < 4; ++x)
        for (int y = 0; y < 4; ++y) {
            int v = board[x][y];
            empty += v == 0;
            if (v > largest)
                largest = v;
            if ((x == 0 || x == 3) && (y == 0 || y == 3) && v > corner)
                corner = v;
            if (x < 3) {
                int d = v - int(board[x + 1][y]);
                smooth += d < 0 ? -d : d;
                neighbors += v && v == board[x + 1][y];
            }
            if (y < 3) {
                int d = v - int(board[x][y + 1]);
                smooth += d < 0 ? -d : d;
                neighbors += v && v == board[x][y + 1];
            }
        }
    return empty * 90 + neighbors * 22 - smooth * 3 + (corner == largest ? largest * 12 : 0);
}
// A small deterministic look-ahead; advice never mutates the board or consumes RNG.
inline int suggest(const uint8_t board[4][4]) {
    int best = -100000, best_direction = -1;
    for (unsigned d = 0; d < 4; ++d) {
        uint8_t next[4][4];
        memcpy(next, board, 16);
        if (!move(next, d))
            continue;
        int score = value(next) * 2, future = -100000;
        for (unsigned e = 0; e < 4; ++e) {
            uint8_t second[4][4];
            memcpy(second, next, 16);
            if (move(second, e)) {
                int n = value(second);
                if (n > future)
                    future = n;
            }
        }
        if (future > -100000)
            score += future;
        if (score > best) {
            best = score;
            best_direction = int(d);
        }
    }
    return best_direction;
}
} // namespace advisor2048
