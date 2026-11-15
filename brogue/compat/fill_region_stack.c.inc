/*
 *  Grid
 *  Brogue
 *
 *  Created by Brian Walker on 12/7/12.
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

/* PXA compatibility adapter: identical DFS order, cell count and boundary
 * break behavior. Only recursion state moves into bounded linear C memory.
 */
static short fillContiguousRegion(short **grid, short x, short y, short fillValue) {
    extern void pxa_brogue_cooperate(void);
    enum { BLOB_FILL_FRAME_CAPACITY = DCOLS * DROWS };
    struct blob_fill_frame {
        short x, y, number_of_cells;
        unsigned char next_direction;
    } frames[BLOB_FILL_FRAME_CAPACITY];
    unsigned frame_count = 1;

    /* The sole upstream caller uses blob labels starting at 2. A label of 1
     * cannot mark visited cells and would also recurse indefinitely upstream. */
    if (fillValue == 1) {
        brogueAssert(false);
        abort();
    }
    grid[x][y] = fillValue;
    frames[0] = (struct blob_fill_frame) {x, y, 1, 0};
    while (frame_count) {
        pxa_brogue_cooperate();
        struct blob_fill_frame *frame = &frames[frame_count - 1];
        enum directions dir;
        short newX, newY;
        if (frame->next_direction == 4) {
            short numberOfCells = frame->number_of_cells;
            --frame_count;
            if (!frame_count) {
                return numberOfCells;
            }
            frames[frame_count - 1].number_of_cells += numberOfCells;
            continue;
        }
        dir = (enum directions) frame->next_direction++;
        newX = frame->x + nbDirs[dir][0];
        newY = frame->y + nbDirs[dir][1];
        if (!coordinatesAreInMap(newX, newY)) {
            /* Preserve the upstream break, including skipping later neighbors. */
            frame->next_direction = 4;
            continue;
        }
        if (grid[newX][newY] == 1) {
            if (frame_count == BLOB_FILL_FRAME_CAPACITY) {
                brogueAssert(false);
                abort();
            }
            grid[newX][newY] = fillValue;
            frames[frame_count] = (struct blob_fill_frame) {newX, newY, 1, 0};
            ++frame_count;
        }
    }
    return 0;
}
