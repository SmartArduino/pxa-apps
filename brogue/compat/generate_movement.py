#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Generate the pinned Brogue field-of-view traversal compatibility unit."""
from pathlib import Path
import hashlib
import sys
EXPECTED_SOURCE_SHA256 = '69ea5b07feb70792046b513b0c44a40d5018bcc3ee7bbbdbf20bd865cb4f650b'
ORIGINAL_FUNCTION = "void scanOctantFOV(char grid[DCOLS][DROWS], short xLoc, short yLoc, short octant, fixpt maxRadius,\n                   short columnsRightFromOrigin, long startSlope, long endSlope, unsigned long forbiddenTerrain,\n                   unsigned long forbiddenFlags, boolean cautiousOnWalls) {\n    const pos loc = { xLoc, yLoc };\n\n    if (columnsRightFromOrigin * FP_FACTOR >= maxRadius) return;\n\n    short i, a, b, iStart, iEnd, x, y, x2, y2; // x and y are temporary variables on which we do the octant transform\n    long newStartSlope, newEndSlope;\n    boolean cellObstructed;\n\n    newStartSlope = startSlope;\n\n    a = ((LOS_SLOPE_GRANULARITY / -2 + 1) + startSlope * columnsRightFromOrigin) / LOS_SLOPE_GRANULARITY;\n    b = ((LOS_SLOPE_GRANULARITY / -2 + 1) + endSlope * columnsRightFromOrigin) / LOS_SLOPE_GRANULARITY;\n\n    iStart = min(a, b);\n    iEnd = max(a, b);\n\n    // restrict vision to a circle of radius maxRadius\n    if ((columnsRightFromOrigin*columnsRightFromOrigin + iEnd*iEnd) >= maxRadius*maxRadius / FP_FACTOR / FP_FACTOR) {\n        return;\n    }\n    if ((columnsRightFromOrigin*columnsRightFromOrigin + iStart*iStart) >= maxRadius*maxRadius / FP_FACTOR / FP_FACTOR) {\n        iStart = (int) (-1 * fp_sqrt((maxRadius*maxRadius / FP_FACTOR) - (columnsRightFromOrigin*columnsRightFromOrigin * FP_FACTOR)) / FP_FACTOR);\n    }\n\n    x = loc.x + columnsRightFromOrigin;\n    y = loc.y + iStart;\n    betweenOctant1andN(&x, &y, loc.x, loc.y, octant);\n    boolean currentlyLit = coordinatesAreInMap(x, y) && !(cellHasTerrainFlag((pos){ x, y }, forbiddenTerrain) ||\n                                                          (pmap[x][y].flags & forbiddenFlags));\n    for (i = iStart; i <= iEnd; i++) {\n        x = loc.x + columnsRightFromOrigin;\n        y = loc.y + i;\n        betweenOctant1andN(&x, &y, loc.x, loc.y, octant);\n        if (!coordinatesAreInMap(x, y)) {\n            // We're off the map -- here there be memory corruption.\n            continue;\n        }\n        cellObstructed = (cellHasTerrainFlag((pos){ x, y }, forbiddenTerrain) || (pmap[x][y].flags & forbiddenFlags));\n        // if we're cautious on walls and this is a wall:\n        if (cautiousOnWalls && cellObstructed) {\n            // (x2, y2) is the tile one space closer to the origin from the tile we're on:\n            x2 = loc.x + columnsRightFromOrigin - 1;\n            y2 = loc.y + i;\n            if (i < 0) {\n                y2++;\n            } else if (i > 0) {\n                y2--;\n            }\n            betweenOctant1andN(&x2, &y2, loc.x, loc.y, octant);\n\n            if (pmap[x2][y2].flags & IN_FIELD_OF_VIEW) {\n                // previous tile is visible, so illuminate\n                grid[x][y] = 1;\n            }\n        } else {\n            // illuminate\n            grid[x][y] = 1;\n        }\n        if (!cellObstructed && !currentlyLit) { // next column slope starts here\n            newStartSlope = (long int) ((LOS_SLOPE_GRANULARITY * (i) - LOS_SLOPE_GRANULARITY / 2) / (columnsRightFromOrigin * 2 + 1) * 2);\n            currentlyLit = true;\n        } else if (cellObstructed && currentlyLit) { // next column slope ends here\n            newEndSlope = (long int) ((LOS_SLOPE_GRANULARITY * (i) - LOS_SLOPE_GRANULARITY / 2)\n                            / (columnsRightFromOrigin * 2 - 1) * 2);\n            if (newStartSlope <= newEndSlope) {\n                // run next column\n                scanOctantFOV(grid, loc.x, loc.y, octant, maxRadius, columnsRightFromOrigin + 1, newStartSlope, newEndSlope,\n                              forbiddenTerrain, forbiddenFlags, cautiousOnWalls);\n            }\n            currentlyLit = false;\n        }\n    }\n    if (currentlyLit) { // got to the bottom of the scan while lit\n        newEndSlope = endSlope;\n        if (newStartSlope <= newEndSlope) {\n            // run next column\n            scanOctantFOV(grid, loc.x, loc.y, octant, maxRadius, columnsRightFromOrigin + 1, newStartSlope, newEndSlope,\n                          forbiddenTerrain, forbiddenFlags, cautiousOnWalls);\n        }\n    }\n}\n"

def generate(source: Path, output: Path):
    raw = source.read_bytes()
    if hashlib.sha256(raw).hexdigest() != EXPECTED_SOURCE_SHA256:
        raise SystemExit("Brogue Movement.c differs from the reviewed upstream source")
    text = raw.decode('utf-8')
    if text.count(ORIGINAL_FUNCTION) != 1:
        raise SystemExit("Expected exactly one pinned scanOctantFOV")
    replacement = Path(__file__).with_name('fov_stack.c.inc').read_text(encoding='utf-8')
    if replacement.count('void scanOctantFOV(') != 1:
        raise SystemExit("Expected exactly one FOV adapter")
    generated = text.replace(ORIGINAL_FUNCTION, replacement, 1)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(generated, encoding='utf-8', newline='\n')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: generate_items.py UPSTREAM_MOVEMENT OUTPUT')
    generate(Path(sys.argv[1]), Path(sys.argv[2]))
