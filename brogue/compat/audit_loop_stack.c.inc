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

/* Preserve every eligible/noneligible visit; only marked cells retain frames. */
static boolean pxa_mark_audit_cell(short x, short y, char grid[DCOLS][DROWS]) {
    if (coordinatesAreInMap(x, y)
        && !grid[x][y]
        && !(pmap[x][y].flags & IN_LOOP)) {
        grid[x][y] = true;
        return true;
    }
    return false;
}

static void auditLoop(short x, short y, char grid[DCOLS][DROWS]) {
    extern void pxa_brogue_cooperate(void);
    enum { AUDIT_LOOP_FRAME_CAPACITY = DCOLS * DROWS };
    struct audit_loop_frame {short x, y; unsigned char next_direction;}
        frames[AUDIT_LOOP_FRAME_CAPACITY];
    unsigned frame_count = 1;
    if (!pxa_mark_audit_cell(x, y, grid)) {
        return;
    }
    frames[0] = (struct audit_loop_frame) {x, y, 0};
    while (frame_count) {
        pxa_brogue_cooperate();
        struct audit_loop_frame *frame = &frames[frame_count - 1];
        short newX, newY;
        unsigned char dir;
        if (frame->next_direction == DIRECTION_COUNT) {
            --frame_count;
            continue;
        }
        dir = frame->next_direction++;
        newX = frame->x + nbDirs[dir][0];
        newY = frame->y + nbDirs[dir][1];
        if (coordinatesAreInMap(newX, newY) && pxa_mark_audit_cell(newX, newY, grid)) {
            if (frame_count == AUDIT_LOOP_FRAME_CAPACITY) {
                brogueAssert(false);
                abort();
            }
            frames[frame_count] = (struct audit_loop_frame) {newX, newY, 0};
            ++frame_count;
        }
    }
}
