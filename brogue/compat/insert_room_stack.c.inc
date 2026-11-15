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

/* PXA compatibility adapter, same four-neighbor DFS and visit order.
 * Only recursion bookkeeping moves to bounded linear-memory frames.
 * No random numbers, IO, suspension points or game rules change.
 */
static void insertRoomAt(short **dungeonMap, short **roomMap, const short roomToDungeonX, const short roomToDungeonY, const short xRoom, const short yRoom) {
    extern void pxa_brogue_cooperate(void);
    enum { ROOM_INSERT_FRAME_CAPACITY = DCOLS * DROWS };
    struct room_insert_frame {
        short x, y;
        unsigned char next_direction;
    } frames[ROOM_INSERT_FRAME_CAPACITY];
    unsigned frame_count = 1;

    brogueAssert(coordinatesAreInMap(xRoom + roomToDungeonX, yRoom + roomToDungeonY));
    dungeonMap[xRoom + roomToDungeonX][yRoom + roomToDungeonY] = 1;
    frames[0] = (struct room_insert_frame) {xRoom, yRoom, 0};
    while (frame_count) {
        pxa_brogue_cooperate();
        struct room_insert_frame *frame = &frames[frame_count - 1];
        short newX, newY;
        enum directions dir;
        if (frame->next_direction == 4) {
            --frame_count;
            continue;
        }
        dir = (enum directions) frame->next_direction++;
        newX = frame->x + nbDirs[dir][0];
        newY = frame->y + nbDirs[dir][1];
        if (coordinatesAreInMap(newX, newY)
            && roomMap[newX][newY]
            && coordinatesAreInMap(newX + roomToDungeonX, newY + roomToDungeonY)
            && dungeonMap[newX + roomToDungeonX][newY + roomToDungeonY] == 0) {
            /* Every pushed destination is marked first and cannot be pushed
             * again. The map therefore bounds depth by DCOLS * DROWS. */
            if (frame_count == ROOM_INSERT_FRAME_CAPACITY) {
                brogueAssert(false);
                abort();
            }
            dungeonMap[newX + roomToDungeonX][newY + roomToDungeonY] = 1;
            frames[frame_count] = (struct room_insert_frame) {newX, newY, 0};
            ++frame_count;
        }
    }
}
