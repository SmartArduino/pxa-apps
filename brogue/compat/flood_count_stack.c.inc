/*
 *  Architect.c
 *  Brogue
 *
 *  Created by Brian Walker on 1/10/09.
 *  Copyright 2012. All rights reserved.
 *
 *  This file is part of Brogue.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Keep original short accumulation and per-return min(), including conversion
 * order; this compatibility adapter does not normalize upstream arithmetic. */
static short pxa_flood_count_initial(short x, short y, char passMap[DCOLS][DROWS]) {
    short count = (passMap[x][y] == 2 ? 5000 : 1);
    if (pmap[x][y].flags & IS_IN_AREA_MACHINE) {
        count = 10000;
    }
    return count;
}

static short floodFillCount(char results[DCOLS][DROWS], char passMap[DCOLS][DROWS], short startX, short startY) {
    extern void pxa_brogue_cooperate(void);
    enum { FLOOD_COUNT_FRAME_CAPACITY = DCOLS * DROWS };
    struct flood_count_frame {short x, y, count; unsigned char next_direction;}
        frames[FLOOD_COUNT_FRAME_CAPACITY];
    unsigned frame_count = 1;
    short initial_count = pxa_flood_count_initial(startX, startY, passMap);
    results[startX][startY] = true;
    frames[0] = (struct flood_count_frame) {startX, startY, initial_count, 0};
    while (frame_count) {
        pxa_brogue_cooperate();
        struct flood_count_frame *frame = &frames[frame_count - 1];
        unsigned char dir;
        short newX, newY;
        if (frame->next_direction == 4) {
            short count = min(frame->count, 10000);
            --frame_count;
            if (!frame_count) {
                return count;
            }
            frames[frame_count - 1].count += count;
            continue;
        }
        dir = frame->next_direction++;
        newX = frame->x + nbDirs[dir][0];
        newY = frame->y + nbDirs[dir][1];
        if (coordinatesAreInMap(newX, newY)
            && passMap[newX][newY]
            && !results[newX][newY]) {
            short count = pxa_flood_count_initial(newX, newY, passMap);
            if (frame_count == FLOOD_COUNT_FRAME_CAPACITY) {
                brogueAssert(false);
                abort();
            }
            results[newX][newY] = true;
            frames[frame_count] = (struct flood_count_frame) {newX, newY, count, 0};
            ++frame_count;
        }
    }
    return 0;
}
