/*
 *  Movement.c
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

/* Each child scans the next outward column. With the original in-map origin,
 * eight octants and narrowed [-1,0] slopes, an entire column is outside the map
 * after max(DCOLS,DROWS) columns. Keep suspended row scans explicitly; no slope,
 * visibility predicate, fixed-point rounding or illumination rule is changed. */
void scanOctantFOV(char grid[DCOLS][DROWS], short xLoc, short yLoc, short octant, fixpt maxRadius,
                   short columnsRightFromOrigin, long startSlope, long endSlope, unsigned long forbiddenTerrain,
                   unsigned long forbiddenFlags, boolean cautiousOnWalls) {
    extern void pxa_brogue_cooperate(void);
    enum { FOV_FRAME_CAPACITY = DCOLS + DROWS + 1 };
    enum { FOV_ENTER, FOV_SCAN, FOV_AFTER_CHILD, FOV_RETURN };
    struct fov_frame {short column, i, i_end; long start_slope, end_slope, new_start_slope;
                      boolean currently_lit; unsigned char stage;} frames[FOV_FRAME_CAPACITY];
    const pos loc = {xLoc, yLoc};
    unsigned frame_count = 1;
    frames[0] = (struct fov_frame) {.column=columnsRightFromOrigin, .start_slope=startSlope,
                                  .end_slope=endSlope, .stage=FOV_ENTER};
    while (frame_count) {
        pxa_brogue_cooperate();
        struct fov_frame *frame = &frames[frame_count - 1];
        short x, y, x2, y2;
        long newEndSlope;
        boolean cellObstructed;
        if (frame->stage == FOV_RETURN) {
            --frame_count;
            continue;
        }
        if (frame->stage == FOV_ENTER) {
            short a, b, iStart;
            if (frame->column * FP_FACTOR >= maxRadius) {
                --frame_count;
                continue;
            }
            frame->new_start_slope = frame->start_slope;
            a = ((LOS_SLOPE_GRANULARITY / -2 + 1) + frame->start_slope * frame->column) / LOS_SLOPE_GRANULARITY;
            b = ((LOS_SLOPE_GRANULARITY / -2 + 1) + frame->end_slope * frame->column) / LOS_SLOPE_GRANULARITY;
            iStart = min(a, b);
            frame->i_end = max(a, b);
            if ((frame->column*frame->column + frame->i_end*frame->i_end) >= maxRadius*maxRadius / FP_FACTOR / FP_FACTOR) {
                --frame_count;
                continue;
            }
            if ((frame->column*frame->column + iStart*iStart) >= maxRadius*maxRadius / FP_FACTOR / FP_FACTOR) {
                iStart = (int) (-1 * fp_sqrt((maxRadius*maxRadius / FP_FACTOR) - (frame->column*frame->column * FP_FACTOR)) / FP_FACTOR);
            }
            x = loc.x + frame->column;
            y = loc.y + iStart;
            betweenOctant1andN(&x, &y, loc.x, loc.y, octant);
            frame->currently_lit = coordinatesAreInMap(x, y) && !(cellHasTerrainFlag((pos){x,y}, forbiddenTerrain) || (pmap[x][y].flags & forbiddenFlags));
            frame->i = iStart;
            frame->stage = FOV_SCAN;
        } else if (frame->stage == FOV_AFTER_CHILD) {
            frame->currently_lit = false;
            ++frame->i;
            frame->stage = FOV_SCAN;
        }
        if (frame->i > frame->i_end) {
            if (frame->currently_lit) {
                newEndSlope = frame->end_slope;
                if (frame->new_start_slope <= newEndSlope) {
                    frame->stage = FOV_RETURN;
                    if (frame_count == FOV_FRAME_CAPACITY) {brogueAssert(false);abort();}
                    frames[frame_count] = (struct fov_frame) {.column=frame->column+1,
                        .start_slope=frame->new_start_slope, .end_slope=newEndSlope, .stage=FOV_ENTER};
                    ++frame_count;
                    continue;
                }
            }
            --frame_count;
            continue;
        }
        x = loc.x + frame->column;
        y = loc.y + frame->i;
        betweenOctant1andN(&x, &y, loc.x, loc.y, octant);
        if (!coordinatesAreInMap(x, y)) {++frame->i;continue;}
        cellObstructed = (cellHasTerrainFlag((pos){x,y}, forbiddenTerrain) || (pmap[x][y].flags & forbiddenFlags));
        if (cautiousOnWalls && cellObstructed) {
            x2 = loc.x + frame->column - 1;
            y2 = loc.y + frame->i;
            if (frame->i < 0) {y2++;} else if (frame->i > 0) {y2--;}
            betweenOctant1andN(&x2, &y2, loc.x, loc.y, octant);
            if (pmap[x2][y2].flags & IN_FIELD_OF_VIEW) {grid[x][y] = 1;}
        } else {grid[x][y] = 1;}
        if (!cellObstructed && !frame->currently_lit) {
            frame->new_start_slope = (long int) ((LOS_SLOPE_GRANULARITY * (frame->i) - LOS_SLOPE_GRANULARITY / 2) / (frame->column * 2 + 1) * 2);
            frame->currently_lit = true;
        } else if (cellObstructed && frame->currently_lit) {
            newEndSlope = (long int) ((LOS_SLOPE_GRANULARITY * (frame->i) - LOS_SLOPE_GRANULARITY / 2) / (frame->column * 2 - 1) * 2);
            if (frame->new_start_slope <= newEndSlope) {
                frame->stage = FOV_AFTER_CHILD;
                if (frame_count == FOV_FRAME_CAPACITY) {brogueAssert(false);abort();}
                frames[frame_count] = (struct fov_frame) {.column=frame->column+1,
                    .start_slope=frame->new_start_slope, .end_slope=newEndSlope, .stage=FOV_ENTER};
                ++frame_count;
                continue;
            }
            frame->currently_lit = false;
        }
        ++frame->i;
    }
}
