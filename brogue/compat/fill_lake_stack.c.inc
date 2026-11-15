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

/* PXA compatibility adapter: same nested scan order and all three writes.
 * The initial unmarked root plus at most one frame per consumed map cell
 * bounds recursion bookkeeping by DCOLS * DROWS + 1 linear-memory frames. */
static void fillLake(short x, short y, short liquid, short scanWidth, char wreathMap[DCOLS][DROWS], short **unfilledLakeMap) {
    extern void pxa_brogue_cooperate(void);
    enum { FILL_LAKE_FRAME_CAPACITY = DCOLS * DROWS + 1 };
    struct fill_lake_frame {
        short x, y, next_x, next_y;
    } frames[FILL_LAKE_FRAME_CAPACITY];
    unsigned frame_count = 1;

    frames[0] = (struct fill_lake_frame) {x, y, x - scanWidth, y - scanWidth};
    while (frame_count) {
        pxa_brogue_cooperate();
        struct fill_lake_frame *frame = &frames[frame_count - 1];
        short i, j;
        if (frame->next_x > frame->x + scanWidth) {
            --frame_count;
            continue;
        }
        if (frame->next_y > frame->y + scanWidth) {
            ++frame->next_x;
            frame->next_y = frame->y - scanWidth;
            continue;
        }
        i = frame->next_x;
        j = frame->next_y++;
        if (coordinatesAreInMap(i, j) && unfilledLakeMap[i][j]) {
            unfilledLakeMap[i][j] = false;
            pmap[i][j].layers[LIQUID] = liquid;
            wreathMap[i][j] = 1;
            if (frame_count == FILL_LAKE_FRAME_CAPACITY) {
                brogueAssert(false);
                abort();
            }
            frames[frame_count] = (struct fill_lake_frame) {i, j, i - scanWidth, j - scanWidth};
            ++frame_count;
        }
    }
}
