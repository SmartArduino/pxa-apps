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

/* PXA compatibility adapter: preserve exact DFS visit and predicate order.
 * Only recursion state moves to bounded linear C-memory frames. */
static void lakeFloodFill(short x, short y, short **floodMap, short **grid, short **lakeMap, short dungeonToGridX, short dungeonToGridY) {
    extern void pxa_brogue_cooperate(void);
    enum { LAKE_FILL_FRAME_CAPACITY = DCOLS * DROWS };
    struct lake_fill_frame {
        short x, y;
        unsigned char next_direction;
    } frames[LAKE_FILL_FRAME_CAPACITY];
    unsigned frame_count = 1;

    floodMap[x][y] = true;
    frames[0] = (struct lake_fill_frame) {x, y, 0};
    while (frame_count) {
        pxa_brogue_cooperate();
        struct lake_fill_frame *frame = &frames[frame_count - 1];
        enum directions dir;
        short newX, newY;
        if (frame->next_direction == 4) {
            --frame_count;
            continue;
        }
        dir = (enum directions) frame->next_direction++;
        newX = frame->x + nbDirs[dir][0];
        newY = frame->y + nbDirs[dir][1];
        if (coordinatesAreInMap(newX, newY)
            && !floodMap[newX][newY]
            && (!cellHasTerrainFlag((pos){ newX, newY }, T_PATHING_BLOCKER) || cellHasTMFlag((pos){ newX, newY }, TM_CONNECTS_LEVEL))
            && !lakeMap[newX][newY]
            && (!coordinatesAreInMap(newX+dungeonToGridX, newY+dungeonToGridY) || !grid[newX+dungeonToGridX][newY+dungeonToGridY])) {
            if (frame_count == LAKE_FILL_FRAME_CAPACITY) {
                brogueAssert(false);
                abort();
            }
            floodMap[newX][newY] = true;
            frames[frame_count] = (struct lake_fill_frame) {newX, newY, 0};
            ++frame_count;
        }
    }
}
