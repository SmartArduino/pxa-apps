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

/* Original neighbor-string decision preserved verbatim; no RNG or rules change. */
static boolean pxa_remove_loopiness(short x, short y) {
    short newX, newY, dir, sdir;
    short numStrings, maxStringLength, currentStringLength;

    if (!(pmap[x][y].flags & IN_LOOP)) {
        return false;
    }

    // find an unloopy neighbor to start on
    for (sdir = 0; sdir < DIRECTION_COUNT; sdir++) {
        newX = x + cDirs[sdir][0];
        newY = y + cDirs[sdir][1];
        if (!coordinatesAreInMap(newX, newY)
            || !(pmap[newX][newY].flags & IN_LOOP)) {
            break;
        }
    }
    if (sdir == 8) { // no unloopy neighbors
        return false; // leave cell loopy
    }

    // starting on this unloopy neighbor, work clockwise and count up (a) the number of strings
    // of loopy neighbors, and (b) the length of the longest such string.
    numStrings = maxStringLength = currentStringLength = 0;
    boolean inString = false;
    for (dir = sdir; dir < sdir + 8; dir++) {
        newX = x + cDirs[dir % 8][0];
        newY = y + cDirs[dir % 8][1];
        if (coordinatesAreInMap(newX, newY) && (pmap[newX][newY].flags & IN_LOOP)) {
            currentStringLength++;
            if (!inString) {
                if (numStrings > 0) {
                    return false; // more than one string here; leave loopy
                }
                numStrings++;
                inString = true;
            }
        } else if (inString) {
            if (currentStringLength > maxStringLength) {
                maxStringLength = currentStringLength;
            }
            currentStringLength = 0;
            inString = false;
        }
    }
    if (inString && currentStringLength > maxStringLength) {
        maxStringLength = currentStringLength;
    }
    if (numStrings == 1 && maxStringLength <= 4) {
        pmap[x][y].flags &= ~IN_LOOP;

        return true;
    } else {
        return false;
    }
}

/* Preserve the original local decision before visiting every valid neighbor.
 * Only cells whose IN_LOOP bit was cleared need a persistent DFS frame. */
static boolean checkLoopiness(short x, short y) {
    extern void pxa_brogue_cooperate(void);
    enum { LOOPINESS_FRAME_CAPACITY = DCOLS * DROWS };
    struct loopiness_frame {
        short x, y;
        unsigned char next_direction;
    } frames[LOOPINESS_FRAME_CAPACITY];
    unsigned frame_count = 1;

    if (!pxa_remove_loopiness(x, y)) {
        return false;
    }
    frames[0] = (struct loopiness_frame) {x, y, 0};
    while (frame_count) {
        pxa_brogue_cooperate();
        struct loopiness_frame *frame = &frames[frame_count - 1];
        short newX, newY;
        unsigned char dir;
        if (frame->next_direction == DIRECTION_COUNT) {
            --frame_count;
            continue;
        }
        dir = frame->next_direction++;
        newX = frame->x + cDirs[dir][0];
        newY = frame->y + cDirs[dir][1];
        if (coordinatesAreInMap(newX, newY) && pxa_remove_loopiness(newX, newY)) {
            if (frame_count == LOOPINESS_FRAME_CAPACITY) {
                brogueAssert(false);
                abort();
            }
            frames[frame_count] = (struct loopiness_frame) {newX, newY, 0};
            ++frame_count;
        }
    }
    return true;
}
