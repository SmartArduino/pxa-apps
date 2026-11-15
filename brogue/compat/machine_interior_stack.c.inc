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

/* Mark-before-descent bounds live frames by map cells. A failed child stops
 * every suspended ancestor without scanning any remaining neighbors. */
static boolean addTileToMachineInteriorAndIterate(char interior[DCOLS][DROWS], short startX, short startY) {
    extern void pxa_brogue_cooperate(void);
    enum { MACHINE_INTERIOR_CAPACITY = DCOLS * DROWS };
    struct machine_interior_frame {short x, y; unsigned char next_direction; boolean good;}
        frames[MACHINE_INTERIOR_CAPACITY];
    unsigned frame_count = 1;
    interior[startX][startY] = true;
    frames[0] = (struct machine_interior_frame) {startX, startY, 0, true};
    while (frame_count) {
        pxa_brogue_cooperate();
        struct machine_interior_frame *frame = &frames[frame_count - 1];
        short newX, newY;
        unsigned char dir;
        if (!frame->good || frame->next_direction == 4) {
            boolean good = frame->good;
            --frame_count;
            if (!frame_count) {return good;}
            frames[frame_count - 1].good = good;
            continue;
        }
        dir = frame->next_direction++;
        newX = frame->x + nbDirs[dir][0];
        newY = frame->y + nbDirs[dir][1];
        if (coordinatesAreInMap(newX, newY)) {
            if ((pmap[newX][newY].flags & HAS_ITEM)
                || ((pmap[newX][newY].flags & IS_IN_MACHINE) && !(pmap[newX][newY].flags & IS_GATE_SITE))) {
                frame->good = false;
                continue;
            }
            if (!interior[newX][newY]
                && chokeMap[newX][newY] <= chokeMap[frame->x][frame->y]
                && !(pmap[newX][newY].flags & IS_IN_MACHINE)) {
                if (frame_count == MACHINE_INTERIOR_CAPACITY) {brogueAssert(false);abort();}
                interior[newX][newY] = true;
                frames[frame_count] = (struct machine_interior_frame) {newX, newY, 0, true};
                ++frame_count;
            }
        }
    }
    return true;
}
