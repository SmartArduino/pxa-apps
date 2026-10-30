// SPDX-License-Identifier: CC0-1.0
#pragma once
#include "core.hpp"
namespace sokoban {
// Walk to an empty cell without ever planning a push. Return -1 if unreachable.
inline int walk_path(const State &s, unsigned destination, uint8_t *directions, unsigned capacity) {
    if (s.player >= CELLS || destination >= CELLS || s.lower[destination] == WALL || s.upper[destination])
        return -1;
    if (destination == s.player)
        return 0;
    int16_t previous[CELLS];
    uint8_t via[CELLS], queue[CELLS];
    for (auto &v : previous)
        v = -1;
    unsigned head = 0, tail = 0;
    queue[tail++] = s.player;
    previous[s.player] = s.player;
    const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    while (head < tail && previous[destination] < 0) {
        unsigned cell = queue[head++];
        for (unsigned d = 0; d < 4; ++d) {
            int x = int(cell % WIDTH) + dx[d], y = int(cell / WIDTH) + dy[d];
            if (x < 0 || x >= int(WIDTH) || y < 0 || y >= int(HEIGHT))
                continue;
            unsigned n = unsigned(y) * WIDTH + unsigned(x);
            if (previous[n] >= 0 || s.lower[n] == WALL || s.upper[n])
                continue;
            previous[n] = int16_t(cell);
            via[n] = uint8_t(d);
            queue[tail++] = uint8_t(n);
        }
    }
    if (previous[destination] < 0)
        return -1;
    uint8_t reverse[CELLS];
    unsigned count = 0;
    for (unsigned n = destination; n != s.player; n = unsigned(previous[n]))
        reverse[count++] = via[n];
    if (count > capacity || (!directions && count))
        return -1;
    for (unsigned i = 0; i < count; ++i)
        directions[i] = reverse[count - 1 - i];
    return int(count);
}
inline bool cornered_crate(const State &s) {
    for (unsigned i = 0; i < CELLS; ++i)
        if (s.upper[i] && s.lower[i] != GOAL) {
            int x = int(i % WIDTH), y = int(i / WIDTH);
            bool horizontal =
                x == 0 || x == int(WIDTH) - 1 || s.lower[i - 1] == WALL || s.lower[i + 1] == WALL;
            bool vertical = y == 0 || y == int(HEIGHT) - 1 || s.lower[i - WIDTH] == WALL ||
                            s.lower[i + WIDTH] == WALL;
            if (horizontal && vertical)
                return true;
        }
    return false;
}
} // namespace sokoban
