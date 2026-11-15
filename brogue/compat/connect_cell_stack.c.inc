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

/* Preserve the recursive zone DFS and per-child size accumulation. Each cell
 * is marked before descent, so at most DCOLS * DROWS frames are live. Callers
 * use a nonzero byte-sized zone label (the upstream zoneSizes table has 200
 * entries); a zero stored label would also violate upstream termination. */
static short connectCell(short x, short y, short zoneLabel, char blockingMap[DCOLS][DROWS], char zoneMap[DCOLS][DROWS]) {
    extern void pxa_brogue_cooperate(void);
    enum { CONNECT_FRAME_CAPACITY = DCOLS * DROWS };
    struct connect_frame {short x, y, size; unsigned char next_direction;}
        frames[CONNECT_FRAME_CAPACITY];
    unsigned frame_count = 1;
    if ((char) zoneLabel == 0) {
        brogueAssert(false);
        abort();
    }
    zoneMap[x][y] = zoneLabel;
    frames[0] = (struct connect_frame) {x, y, 1, 0};
    while (frame_count) {
        pxa_brogue_cooperate();
        struct connect_frame *frame = &frames[frame_count - 1];
        unsigned char dir;
        short newX, newY;
        if (frame->next_direction == 4) {
            short size = frame->size;
            --frame_count;
            if (!frame_count) {
                return size;
            }
            frames[frame_count - 1].size += size;
            continue;
        }
        dir = frame->next_direction++;
        newX = frame->x + nbDirs[dir][0];
        newY = frame->y + nbDirs[dir][1];
        if (coordinatesAreInMap(newX, newY)
            && zoneMap[newX][newY] == 0
            && (!blockingMap || !blockingMap[newX][newY])
            && cellIsPassableOrDoor(newX, newY)) {
            if (frame_count == CONNECT_FRAME_CAPACITY) {
                brogueAssert(false);
                abort();
            }
            zoneMap[newX][newY] = zoneLabel;
            frames[frame_count] = (struct connect_frame) {newX, newY, 1, 0};
            ++frame_count;
        }
    }
    return 0;
}
