/*
 *  Items.c
 *  Brogue
 *
 *  Created by Brian Walker on 1/17/09.
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

/* Heat can wrap as unsigned short at doors, so a cell may be active more
 * than once. Grow explicit frames in the existing linear-memory heap instead
 * of assuming that map cell count bounds recursive depth. Checked allocation
 * failure ends the component; it never clips traversal or changes heat math. */
static unsigned short pxa_item_heat_enter(unsigned short heatMap[DCOLS][DROWS], unsigned short heatLevel, pos loc) {
    if (pmapAt(loc)->layers[DUNGEON] == DOOR) {
        heatLevel += 10;
    } else if (pmapAt(loc)->layers[DUNGEON] == SECRET_DOOR) {
        heatLevel += 3000;
    }
    if (heatMap[loc.x][loc.y] > heatLevel) {
        heatMap[loc.x][loc.y] = heatLevel;
    }
    return heatLevel;
}

static void pxa_item_heat_allocation_failed(void *frames) {
    free(frames);
    brogueAssert(false);
    abort();
}

static void fillItemSpawnHeatMap(unsigned short heatMap[DCOLS][DROWS], unsigned short heatLevel, pos loc) {
    extern void pxa_brogue_cooperate(void);
    struct item_heat_frame {pos loc; unsigned short heat_level; unsigned char next_direction;};
    size_t capacity = DCOLS * DROWS;
    const size_t maximum_capacity = ((size_t) -1) / sizeof(struct item_heat_frame);
    size_t frame_count = 1;
    struct item_heat_frame *frames = malloc(capacity * sizeof(*frames));
    if (!frames) {
        pxa_item_heat_allocation_failed(NULL);
    }
    frames[0] = (struct item_heat_frame) {loc, pxa_item_heat_enter(heatMap, heatLevel, loc), 0};
    while (frame_count) {
        pxa_brogue_cooperate();
        struct item_heat_frame *frame = &frames[frame_count - 1];
        unsigned char dir;
        pos neighbor;
        unsigned short current_heat;
        if (frame->next_direction == 4) {
            --frame_count;
            continue;
        }
        dir = frame->next_direction++;
        neighbor = posNeighborInDirection(frame->loc, dir);
        current_heat = frame->heat_level;
        if (isPosInMap(neighbor)
            && !cellHasTerrainFlag(neighbor, T_IS_DEEP_WATER | T_LAVA_INSTA_DEATH | T_AUTO_DESCENT)
            && isPassableOrSecretDoor(neighbor)
            && current_heat < heatMap[neighbor.x][neighbor.y]) {
            if (frame_count == capacity) {
                size_t next_capacity;
                struct item_heat_frame *grown;
                if (capacity == maximum_capacity) {
                    pxa_item_heat_allocation_failed(frames);
                }
                next_capacity = capacity > maximum_capacity / 2 ? maximum_capacity : capacity * 2;
                grown = realloc(frames, next_capacity * sizeof(*frames));
                if (!grown) {
                    pxa_item_heat_allocation_failed(frames);
                }
                frames = grown;
                capacity = next_capacity;
            }
            frames[frame_count] = (struct item_heat_frame) {neighbor, pxa_item_heat_enter(heatMap, current_heat, neighbor), 0};
            ++frame_count;
        }
    }
    free(frames);
}
