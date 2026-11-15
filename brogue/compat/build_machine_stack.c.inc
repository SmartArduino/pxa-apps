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

/* A frame contains every original local and argument, with distinct fields for
 * shadowed declarations. Only the two original direct recursive calls yield a
 * child request; the original body resumes immediately after that same call.
 * Scratch ownership is independent of game pointers and normal rollback. */
struct pxa_machine_args {
    enum machineTypes bp;
    short originX;
    short originY;
    unsigned long requiredMachineFlags;
    item * adoptiveItem;
    item ** parentSpawnedItems;
    creature ** parentSpawnedMonsters;
};
struct pxa_machine_frame {
    struct pxa_machine_frame *previous;
    unsigned resume;
    boolean result, child_result;
    struct pxa_machine_args request;
    machineData *owned_data;
    short **owned_grid;
    enum machineTypes a_bp;
    short a_originX;
    short a_originY;
    unsigned long a_requiredMachineFlags;
    item * a_adoptiveItem;
    item ** a_parentSpawnedItems;
    creature ** a_parentSpawnedMonsters;
    short l00_totalFreq;
    short l01_instance;
    short l02_instanceCount;
    short l03_itemCount;
    short l04_monsterCount;
    short l05_qualifyingTileCount;
    short ** l06_distanceMap;
    short l07_personalSpace;
    short l08_locationFailsafe;
    short l09_machineNumber;
    unsigned long l10_alternativeFlags[2];
    char l11_DFSucceeded;
    char l12_terrainSucceeded;
    char l13_generateEverywhere;
    char l14_tryAgain;
    char l15_success;
    char l16_skipFeature[20];
    creature * l17_monst;
    creature * l18_torchBearer;
    creature * l19_leader;
    item * l20_theItem;
    item * l21_torch;
    const machineFeature * l22_feature;
    machineData * l23_p;
    char l24_chooseBP;
    char l25_chooseLocation;
    int l26_failsafe;
    int l27_i;
    int l28_randIndex;
    int l29_i;
    int l30_i;
    int l31_j;
    int l32_randIndex;
    pos l33_originLoc;
    int l34_k;
    int l35_i;
    int l36_j;
    int l37_i;
    int l38_j;
    int l39_layer;
    int l40_i;
    int l41_i;
    int l42_j;
    int l43_distance25;
    int l44_distance75;
    int l45_i;
    int l46_i;
    int l47_i;
    int l48_j;
    int l49_i;
    int l50_randIndex;
    int l51_i;
    int l52_feat;
    short l53_distanceBound[2];
    int l54_i;
    int l55_j;
    int l56_featX;
    int l57_featY;
    int l58_randIndex;
    int l59_i;
    int l60_j;
    int l61_i;
    int l62_j;
    int l63_i;
    int l64_j;
    int l65_j;
    creatureIterator l66_it;
    creature * l67_monst;
    int l68_i;
    int l69_j;
    int l70_i;
    int l71_i;
};
enum pxa_machine_step {PXA_MACHINE_CHILD, PXA_MACHINE_COMPLETE};

static _Noreturn void pxa_machine_context_oom(struct pxa_machine_frame *frame) {
    while (frame) {
        struct pxa_machine_frame *previous = frame->previous;
        if (frame->owned_grid) freeGrid(frame->owned_grid);
        if (frame->owned_data) free(frame->owned_data);
        free(frame);
        frame = previous;
    }
    /* Item/monster references in machineData are borrowed or transferred.
     * Fatal component teardown owns the game world; never roll it back here. */
    brogueAssert(false);
    abort();
}

static machineData *pxa_machine_allocate_data(struct pxa_machine_frame *frame) {
    machineData *data = malloc(sizeof(machineData));
    if (!data) pxa_machine_context_oom(frame);
    frame->owned_data = data;
    return data;
}
static short **pxa_machine_allocate_grid(struct pxa_machine_frame *frame) {
    short i;
    short **grid = malloc(DCOLS * sizeof(short *));
    if (!grid) pxa_machine_context_oom(frame);

    grid[0] = malloc(DROWS * DCOLS * sizeof(short));
    if (!grid[0]) {
        free(grid);
        pxa_machine_context_oom(frame);
    }
    for(i = 1; i < DCOLS; i++) {
        grid[i] = grid[0] + i * DROWS;
    }
    frame->owned_grid = grid;
    return grid;
}
static void pxa_machine_free_data(struct pxa_machine_frame *frame, machineData *data) {
    free(data);
    if (frame->owned_data == data) frame->owned_data = NULL;
}
static void pxa_machine_free_grid(struct pxa_machine_frame *frame, short **grid) {
    freeGrid(grid);
    if (frame->owned_grid == grid) frame->owned_grid = NULL;
}
static struct pxa_machine_frame *pxa_machine_allocate_frame(
    struct pxa_machine_frame *previous, struct pxa_machine_args args) {
    struct pxa_machine_frame *frame = malloc(sizeof(*frame));
    if (!frame) pxa_machine_context_oom(previous);
    frame->previous = previous;
    frame->resume = 0;
    frame->owned_data = NULL;
    frame->owned_grid = NULL;
    frame->a_bp = args.bp;
    frame->a_originX = args.originX;
    frame->a_originY = args.originY;
    frame->a_requiredMachineFlags = args.requiredMachineFlags;
    frame->a_adoptiveItem = args.adoptiveItem;
    frame->a_parentSpawnedItems = args.parentSpawnedItems;
    frame->a_parentSpawnedMonsters = args.parentSpawnedMonsters;
    frame->l23_p = NULL;
    frame->l06_distanceMap = NULL;
    return frame;
}

static enum pxa_machine_step pxa_machine_step(struct pxa_machine_frame *frame) {
    switch (frame->resume) {
        case 0: break;
        case 1: goto pxa_machine_resume_1;
        case 2: goto pxa_machine_resume_2;
        default: brogueAssert(false); abort();
    }


    frame->l02_instanceCount = 0; frame->l06_distanceMap = NULL;

    memcpy(frame->l10_alternativeFlags, (unsigned long[2]){MF_ALTERNATIVE, MF_ALTERNATIVE_2}, sizeof(frame->l10_alternativeFlags));

    frame->l15_success = false;

    frame->l17_monst = NULL; frame->l18_torchBearer = NULL; frame->l19_leader = NULL;

    frame->l20_theItem = NULL; frame->l21_torch = NULL;

    ;

    frame->l23_p = pxa_machine_allocate_data(frame);

    memset(frame->l23_p, 0, sizeof(machineData));

    frame->l24_chooseBP = (((signed short) frame->a_bp) <= 0 ? true : false);

    frame->l25_chooseLocation = (frame->a_originX <= 0 || frame->a_originY <= 0 ? true : false);

    frame->l26_failsafe = 10;
    do {
        frame->l14_tryAgain = false;
        if (--frame->l26_failsafe <= 0) {
            if (frame->l06_distanceMap) {
                pxa_machine_free_grid(frame, frame->l06_distanceMap);
            }
            if (D_MESSAGE_MACHINE_GENERATION) {
                if (frame->l24_chooseBP || frame->l25_chooseLocation) {
                    printf("\nDepth %i: Failed to build a machine; gave up after 10 unsuccessful attempts to find a suitable blueprint and/or location.",
                           rogue.depthLevel);
                } else {
                    printf("\nDepth %i: Failed to build a machine; requested blueprint %i:%s and location did not work.",
                           rogue.depthLevel, frame->a_bp, blueprintCatalog[frame->a_bp].name);
                }
            }
            pxa_machine_free_data(frame, frame->l23_p);
            { frame->result = (false); return PXA_MACHINE_COMPLETE; }
        }

        if (frame->l24_chooseBP) { // If no blueprint is given, then pick one:

            // First, choose the blueprint. We choose from among blueprints
            // that have the required blueprint flags and that satisfy the depth requirements.
            frame->l00_totalFreq = 0;
            for (frame->l27_i = 1; frame->l27_i<gameConst->numberBlueprints; frame->l27_i++) {
                if (blueprintQualifies(frame->l27_i, frame->a_requiredMachineFlags)) {
                    frame->l00_totalFreq += blueprintCatalog[frame->l27_i].frequency;
                }
            }

            if (!frame->l00_totalFreq) { // If no suitable blueprints are in the library, fail.
                if (frame->l06_distanceMap) {
                    pxa_machine_free_grid(frame, frame->l06_distanceMap);
                }
                if (D_MESSAGE_MACHINE_GENERATION) printf("\nDepth %i: Failed to build a machine because no suitable blueprints were available.",
                             rogue.depthLevel);
                pxa_machine_free_data(frame, frame->l23_p);
                { frame->result = (false); return PXA_MACHINE_COMPLETE; }
            }

            // Pick from among the suitable blueprints.
            frame->l28_randIndex = rand_range(1, frame->l00_totalFreq);
            for (frame->l29_i = 1; frame->l29_i<gameConst->numberBlueprints; frame->l29_i++) {
                if (blueprintQualifies(frame->l29_i, frame->a_requiredMachineFlags)) {
                    if (frame->l28_randIndex <= blueprintCatalog[frame->l29_i].frequency) {
                        frame->a_bp = frame->l29_i;
                        break;
                    } else {
                        frame->l28_randIndex -= blueprintCatalog[frame->l29_i].frequency;
                    }
                }
            }

            // If we don't have a blueprint yet, something went wrong.
            brogueAssert(bp>0);
        }

        // Find a location and map out the machine interior.
        if (blueprintCatalog[frame->a_bp].flags & BP_ROOM) {
            // If it's a room machine, count up the gates of appropriate
            // choke size and remember where they are. The origin of the room will be the gate location.
            zeroOutGrid(frame->l23_p->interior);

            if (frame->l25_chooseLocation) {
                analyzeMap(true); // Make sure the chokeMap is up to date.
                frame->l00_totalFreq = 0;
                for(frame->l30_i = 0; frame->l30_i<DCOLS; frame->l30_i++) {
                    for(frame->l31_j = 0; frame->l31_j<DROWS && frame->l00_totalFreq < 50; frame->l31_j++) {
                        if ((pmap[frame->l30_i][frame->l31_j].flags & IS_GATE_SITE)
                            && !(pmap[frame->l30_i][frame->l31_j].flags & IS_IN_MACHINE)
                            && chokeMap[frame->l30_i][frame->l31_j] >= blueprintCatalog[frame->a_bp].roomSize[0]
                            && chokeMap[frame->l30_i][frame->l31_j] <= blueprintCatalog[frame->a_bp].roomSize[1]) {

                            //DEBUG printf("\nDepth %i: Gate site qualified with interior size of %i.", rogue.depthLevel, chokeMap[i][j]);
                            frame->l23_p->gateCandidates[frame->l00_totalFreq] = (pos){ .x = frame->l30_i, .y = frame->l31_j };
                            frame->l00_totalFreq++;
                        }
                    }
                }

                if (frame->l00_totalFreq) {
                    // Choose the gate.
                    frame->l32_randIndex = rand_range(0, frame->l00_totalFreq - 1);
                    frame->a_originX = frame->l23_p->gateCandidates[frame->l32_randIndex].x;
                    frame->a_originY = frame->l23_p->gateCandidates[frame->l32_randIndex].y;
                } else {
                    // If no suitable sites, abort.
                    if (frame->l06_distanceMap) {
                        pxa_machine_free_grid(frame, frame->l06_distanceMap);
                    }
                    if (D_MESSAGE_MACHINE_GENERATION) printf("\nDepth %i: Failed to build a machine; there was no eligible door candidate for the chosen room machine from blueprint %i:%s.",
                                 rogue.depthLevel,
                                 frame->a_bp,
                                 blueprintCatalog[frame->a_bp].name);
                    pxa_machine_free_data(frame, frame->l23_p);
                    { frame->result = (false); return PXA_MACHINE_COMPLETE; }
                }
            }

            // Now map out the interior into interior[][].
            // Start at the gate location and do a depth-first floodfill to grab all adjoining tiles with the
            // same or lower choke value, ignoring any tiles that are already part of a machine.
            // If we get false from this, try again. If we've tried too many times already, abort.
            frame->l14_tryAgain = !addTileToMachineInteriorAndIterate(frame->l23_p->interior, frame->a_originX, frame->a_originY);
        } else if (blueprintCatalog[frame->a_bp].flags & BP_VESTIBULE) {
            if (frame->l25_chooseLocation) {
                // Door machines must have locations passed in. We can't pick one ourselves.
                if (frame->l06_distanceMap) {
                    pxa_machine_free_grid(frame, frame->l06_distanceMap);
                }
                if (D_MESSAGE_MACHINE_GENERATION) printf("\nDepth %i: ERROR: Attempted to build a door machine from blueprint %i:%s without a location being provided.",
                             rogue.depthLevel,
                             frame->a_bp,
                             blueprintCatalog[frame->a_bp].name);
                pxa_machine_free_data(frame, frame->l23_p);
                { frame->result = (false); return PXA_MACHINE_COMPLETE; }
            }
            if (!fillInteriorForVestibuleMachine(frame->l23_p->interior, frame->a_bp, frame->a_originX, frame->a_originY)) {
                if (frame->l06_distanceMap) {
                    pxa_machine_free_grid(frame, frame->l06_distanceMap);
                }
                if (D_MESSAGE_MACHINE_GENERATION) printf("\nDepth %i: Failed to build a door machine from blueprint %i:%s; not enough room.",
                             rogue.depthLevel,
                             frame->a_bp,
                             blueprintCatalog[frame->a_bp].name);
                pxa_machine_free_data(frame, frame->l23_p);
                { frame->result = (false); return PXA_MACHINE_COMPLETE; }
            }
        } else {
            // Find a location and map out the interior for a non-room machine.
            // The strategy here is simply to pick a random location on the map,
            // expand it along a pathing map by one space in all directions until the size reaches
            // the chosen size, and then make sure the resulting space qualifies.
            // If not, try again. If we've tried too many times already, abort.

            frame->l08_locationFailsafe = 10;
            do {
                zeroOutGrid(frame->l23_p->interior);
                frame->l14_tryAgain = false;

                if (frame->l25_chooseLocation) {
                    // Pick a random origin location.
                    frame->l33_originLoc = (pos){ frame->a_originX, frame->a_originY };
                    randomMatchingLocation(&frame->l33_originLoc, FLOOR, NOTHING, -1);
                    frame->a_originX = frame->l33_originLoc.x;
                    frame->a_originY = frame->l33_originLoc.y;
                }

                if (!frame->l06_distanceMap) {
                    frame->l06_distanceMap = pxa_machine_allocate_grid(frame);
                }
                fillGrid(frame->l06_distanceMap, 0);
                calculateDistances(frame->l06_distanceMap, frame->a_originX, frame->a_originY, T_PATHING_BLOCKER, NULL, true, false);
                frame->l05_qualifyingTileCount = 0; // Keeps track of how many interior cells we've added.
                frame->l00_totalFreq = rand_range(blueprintCatalog[frame->a_bp].roomSize[0], blueprintCatalog[frame->a_bp].roomSize[1]); // Keeps track of the goal size.

                fillSequentialList(frame->l23_p->sCols, DCOLS);
                shuffleList(frame->l23_p->sCols, DCOLS);
                fillSequentialList(frame->l23_p->sRows, DROWS);
                shuffleList(frame->l23_p->sRows, DROWS);

                for (frame->l34_k = 0; frame->l34_k<1000 && frame->l05_qualifyingTileCount < frame->l00_totalFreq; frame->l34_k++) {
                    for(frame->l35_i = 0; frame->l35_i<DCOLS && frame->l05_qualifyingTileCount < frame->l00_totalFreq; frame->l35_i++) {
                        for(frame->l36_j = 0; frame->l36_j<DROWS && frame->l05_qualifyingTileCount < frame->l00_totalFreq; frame->l36_j++) {
                            if (frame->l06_distanceMap[frame->l23_p->sCols[frame->l35_i]][frame->l23_p->sRows[frame->l36_j]] == frame->l34_k) {
                                frame->l23_p->interior[frame->l23_p->sCols[frame->l35_i]][frame->l23_p->sRows[frame->l36_j]] = true;
                                frame->l05_qualifyingTileCount++;

                                if (pmap[frame->l23_p->sCols[frame->l35_i]][frame->l23_p->sRows[frame->l36_j]].flags & (HAS_ITEM | HAS_MONSTER | IS_IN_MACHINE)) {
                                    // Abort if we've entered another machine or engulfed another machine's item or monster.
                                    frame->l14_tryAgain = true;
                                    frame->l05_qualifyingTileCount = frame->l00_totalFreq; // This is a hack to drop out of these three for-loops.
                                }
                            }
                        }
                    }
                }

                // Now make sure the interior map satisfies the machine's qualifications.
                if ((blueprintCatalog[frame->a_bp].flags & BP_TREAT_AS_BLOCKING)
                    && levelIsDisconnectedWithBlockingMap(frame->l23_p->interior, false)) {
                    frame->l14_tryAgain = true;
                } else if ((blueprintCatalog[frame->a_bp].flags & BP_REQUIRE_BLOCKING)
                           && levelIsDisconnectedWithBlockingMap(frame->l23_p->interior, true) < 100) {
                    frame->l14_tryAgain = true; // BP_REQUIRE_BLOCKING needs some work to make sure the disconnect is interesting.
                }
                // If locationFailsafe runs out, tryAgain will still be true, and we'll try a different machine.
                // If we're not choosing the blueprint, then don't bother with the locationFailsafe; just use the higher-level failsafe.
            } while (frame->l24_chooseBP && frame->l14_tryAgain && --frame->l08_locationFailsafe);
        }

        // If something went wrong, but we haven't been charged with choosing blueprint OR location,
        // then there is nothing to try again, so just fail.
        if (frame->l14_tryAgain && !frame->l24_chooseBP && !frame->l25_chooseLocation) {
            if (frame->l06_distanceMap) {
                pxa_machine_free_grid(frame, frame->l06_distanceMap);
            }
            pxa_machine_free_data(frame, frame->l23_p);
            { frame->result = (false); return PXA_MACHINE_COMPLETE; }
        }

        // Now loop if necessary.
    } while (frame->l14_tryAgain);

    // This is the point of no return. Back up the level so it can be restored if we have to abort this machine after this point.
    copyMap(pmap, frame->l23_p->levelBackup);

    // Perform any transformations to the interior indicated by the blueprint flags, including expanding the interior if requested.
    prepareInteriorWithMachineFlags(frame->l23_p->interior, frame->a_originX, frame->a_originY, blueprintCatalog[frame->a_bp].flags, blueprintCatalog[frame->a_bp].dungeonProfileType);

    // If necessary, label the interior as IS_IN_AREA_MACHINE or IS_IN_ROOM_MACHINE and mark down the number.
    frame->l09_machineNumber = ++rogue.machineNumber; // Reserve this machine number, starting with 1.
    for(frame->l37_i = 0; frame->l37_i<DCOLS; frame->l37_i++) {
        for(frame->l38_j = 0; frame->l38_j<DROWS; frame->l38_j++) {
            if (frame->l23_p->interior[frame->l37_i][frame->l38_j]) {
                pmap[frame->l37_i][frame->l38_j].flags |= ((blueprintCatalog[frame->a_bp].flags & BP_ROOM) ? IS_IN_ROOM_MACHINE : IS_IN_AREA_MACHINE);
                pmap[frame->l37_i][frame->l38_j].machineNumber = frame->l09_machineNumber;
                // also clear any secret doors, since they screw up distance mapping and aren't fun inside machines
                if (pmap[frame->l37_i][frame->l38_j].layers[DUNGEON] == SECRET_DOOR) {
                    pmap[frame->l37_i][frame->l38_j].layers[DUNGEON] = DOOR;
                }
                // Clear wired tiles in case we stole them from another machine.
                for (frame->l39_layer = 0; frame->l39_layer < NUMBER_TERRAIN_LAYERS; frame->l39_layer++) {
                    if (tileCatalog[pmap[frame->l37_i][frame->l38_j].layers[frame->l39_layer]].mechFlags & (TM_IS_WIRED | TM_IS_CIRCUIT_BREAKER)) {
                        pmap[frame->l37_i][frame->l38_j].layers[frame->l39_layer] = (frame->l39_layer == DUNGEON ? FLOOR : NOTHING);
                    }
                }
            }
        }
    }

//  DEBUG printf("\n\nWorking on blueprint %i, with origin at (%i, %i). Here's the initial interior map:", bp, originX, originY);
//  DEBUG logBuffer(interior);

    // Calculate the distance map (so that features that want to be close to or far from the origin can be placed accordingly)
    // and figure out the 33rd and 67th percentiles for features that want to be near or far from the origin.
    if (!frame->l06_distanceMap) {
        frame->l06_distanceMap = pxa_machine_allocate_grid(frame);
    }
    fillGrid(frame->l06_distanceMap, 0);
    calculateDistances(frame->l06_distanceMap, frame->a_originX, frame->a_originY, T_PATHING_BLOCKER, NULL, true, true);
    frame->l05_qualifyingTileCount = 0;
    for (frame->l40_i = 0; frame->l40_i<100; frame->l40_i++) {
        frame->l23_p->distances[frame->l40_i] = 0;
    }
    for(frame->l41_i = 0; frame->l41_i<DCOLS; frame->l41_i++) {
        for(frame->l42_j = 0; frame->l42_j<DROWS; frame->l42_j++) {
            if (frame->l23_p->interior[frame->l41_i][frame->l42_j]
                && frame->l06_distanceMap[frame->l41_i][frame->l42_j] < 100) {
                frame->l23_p->distances[frame->l06_distanceMap[frame->l41_i][frame->l42_j]]++; // create a histogram of distances -- poor man's sort function
                frame->l05_qualifyingTileCount++;
            }
        }
    }
    frame->l43_distance25 = (int) (frame->l05_qualifyingTileCount / 4);
    frame->l44_distance75 = (int) (3 * frame->l05_qualifyingTileCount / 4);
    for (frame->l45_i = 0; frame->l45_i<100; frame->l45_i++) {
        if (frame->l43_distance25 <= frame->l23_p->distances[frame->l45_i]) {
            frame->l43_distance25 = frame->l45_i;
            break;
        } else {
            frame->l43_distance25 -= frame->l23_p->distances[frame->l45_i];
        }
    }
    for (frame->l46_i = 0; frame->l46_i<100; frame->l46_i++) {
        if (frame->l44_distance75 <= frame->l23_p->distances[frame->l46_i]) {
            frame->l44_distance75 = frame->l46_i;
            break;
        } else {
            frame->l44_distance75 -= frame->l23_p->distances[frame->l46_i];
        }
    }
    //DEBUG printf("\nDistances calculated: 33rd percentile of distance is %i, and 67th is %i.", distance25, distance75);

    // Now decide which features will be skipped -- of the features marked MF_ALTERNATIVE, skip all but one, chosen randomly.
    // Then repeat and do the same with respect to MF_ALTERNATIVE_2, to provide up to two independent sets of alternative features per machine.

    for (frame->l47_i = 0; frame->l47_i<blueprintCatalog[frame->a_bp].featureCount; frame->l47_i++) {
        frame->l16_skipFeature[frame->l47_i] = false;
    }
    for (frame->l48_j = 0; frame->l48_j <= 1; frame->l48_j++) {
        frame->l00_totalFreq = 0;
        for (frame->l49_i = 0; frame->l49_i<blueprintCatalog[frame->a_bp].featureCount; frame->l49_i++) {
            if (blueprintCatalog[frame->a_bp].feature[frame->l49_i].flags & frame->l10_alternativeFlags[frame->l48_j]) {
                frame->l16_skipFeature[frame->l49_i] = true;
                frame->l00_totalFreq++;
            }
        }
        if (frame->l00_totalFreq > 0) {
            frame->l50_randIndex = rand_range(1, frame->l00_totalFreq);
            for (frame->l51_i = 0; frame->l51_i<blueprintCatalog[frame->a_bp].featureCount; frame->l51_i++) {
                if (blueprintCatalog[frame->a_bp].feature[frame->l51_i].flags & frame->l10_alternativeFlags[frame->l48_j]) {
                    if (frame->l50_randIndex == 1) {
                        frame->l16_skipFeature[frame->l51_i] = false; // This is the alternative that gets built. The rest do not.
                        break;
                    } else {
                        frame->l50_randIndex--;
                    }
                }
            }
        }
    }

    // Keep track of all monsters and items that we spawn -- if we abort, we have to go back and delete them all.
    frame->l03_itemCount = frame->l04_monsterCount = 0;

    // Zero out occupied[][], and use it to keep track of the personal space around each feature that gets placed.
    zeroOutGrid(frame->l23_p->occupied);

    // Now tick through the features and build them.
    for (frame->l52_feat = 0; frame->l52_feat < blueprintCatalog[frame->a_bp].featureCount; frame->l52_feat++) {

        if (frame->l16_skipFeature[frame->l52_feat]) {
            continue; // Skip the alternative features that were not selected for building.
        }

        frame->l22_feature = &(blueprintCatalog[frame->a_bp].feature[frame->l52_feat]);

        // Figure out the distance bounds.
        memcpy(frame->l53_distanceBound, (short[2]){ 0, 10000 }, sizeof(frame->l53_distanceBound));
        if (frame->l22_feature->flags & MF_NEAR_ORIGIN) {
            frame->l53_distanceBound[1] = frame->l43_distance25;
        }
        if (frame->l22_feature->flags & MF_FAR_FROM_ORIGIN) {
            frame->l53_distanceBound[0] = frame->l44_distance75;
        }

        if (frame->l22_feature->flags & (MF_IN_VIEW_OF_ORIGIN | MF_IN_PASSABLE_VIEW_OF_ORIGIN)) {
            zeroOutGrid(frame->l23_p->viewMap);
            if (frame->l22_feature->flags & MF_IN_PASSABLE_VIEW_OF_ORIGIN) {
                getFOVMask(frame->l23_p->viewMap, frame->a_originX, frame->a_originY, max(DCOLS, DROWS) * FP_FACTOR, T_PATHING_BLOCKER, 0, false);
            } else {
                getFOVMask(frame->l23_p->viewMap, frame->a_originX, frame->a_originY, max(DCOLS, DROWS) * FP_FACTOR, (T_OBSTRUCTS_PASSABILITY | T_OBSTRUCTS_VISION), 0, false);
            }
            frame->l23_p->viewMap[frame->a_originX][frame->a_originY] = true;

            if (D_INSPECT_MACHINES) {
                dumpLevelToScreen();
                hiliteCharGrid(frame->l23_p->viewMap, &omniscienceColor, 75);
                temporaryMessage("Showing visibility.", REQUIRE_ACKNOWLEDGMENT);
            }
        }

        do { // If the MF_REPEAT_UNTIL_NO_PROGRESS flag is set, repeat until we fail to build the required number of instances.

            // Make a master map of candidate locations for this feature.
            frame->l05_qualifyingTileCount = 0;
            for(frame->l54_i = 0; frame->l54_i<DCOLS; frame->l54_i++) {
                for(frame->l55_j = 0; frame->l55_j<DROWS; frame->l55_j++) {
                    if (cellIsFeatureCandidate(frame->l54_i, frame->l55_j,
                                               frame->a_originX, frame->a_originY,
                                               frame->l53_distanceBound,
                                               frame->l23_p->interior, frame->l23_p->occupied, frame->l23_p->viewMap, frame->l06_distanceMap,
                                               frame->l09_machineNumber, frame->l22_feature->flags, blueprintCatalog[frame->a_bp].flags)) {
                        frame->l05_qualifyingTileCount++;
                        frame->l23_p->candidates[frame->l54_i][frame->l55_j] = true;
                    } else {
                        frame->l23_p->candidates[frame->l54_i][frame->l55_j] = false;
                    }
                }
            }

            if (D_INSPECT_MACHINES) {
                dumpLevelToScreen();
                hiliteCharGrid(frame->l23_p->occupied, &red, 75);
                hiliteCharGrid(frame->l23_p->candidates, &green, 75);
                hiliteCharGrid(frame->l23_p->interior, &blue, 75);
                temporaryMessage("Indicating: Occupied (red); Candidates (green); Interior (blue).", REQUIRE_ACKNOWLEDGMENT);
            }

            if (frame->l22_feature->flags & MF_EVERYWHERE & ~MF_BUILD_AT_ORIGIN) {
                // Generate everywhere that qualifies -- instead of randomly picking tiles, keep spawning until we run out of eligible tiles.
                frame->l13_generateEverywhere = true;
            } else {
                // build as many instances as required
                frame->l13_generateEverywhere = false;
                frame->l02_instanceCount = rand_range(frame->l22_feature->instanceCountRange[0], frame->l22_feature->instanceCountRange[1]);
            }

            // Cache the personal space constant.
            frame->l07_personalSpace = frame->l22_feature->personalSpace;

            for (frame->l01_instance = 0; (frame->l13_generateEverywhere || frame->l01_instance < frame->l02_instanceCount) && frame->l05_qualifyingTileCount > 0;) {

                // Find a location for the feature.
                ;
                ;
                if (frame->l22_feature->flags & MF_BUILD_AT_ORIGIN) {
                    // Does the feature want to be at the origin? If so, put it there. (Just an optimization.)
                    frame->l56_featX = frame->a_originX;
                    frame->l57_featY = frame->a_originY;
                } else {
                    // Pick our candidate location randomly, and also strike it from
                    // the candidates map so that subsequent instances of this same feature can't choose it.
                    frame->l56_featX = -1;
                    frame->l57_featY = -1;
                    frame->l58_randIndex = rand_range(1, frame->l05_qualifyingTileCount);
                    for(frame->l59_i = 0; frame->l59_i<DCOLS && frame->l56_featX < 0; frame->l59_i++) {
                        for(frame->l60_j = 0; frame->l60_j<DROWS && frame->l56_featX < 0; frame->l60_j++) {
                            if (frame->l23_p->candidates[frame->l59_i][frame->l60_j]) {
                                if (frame->l58_randIndex == 1) {
                                    // This is the place!
                                    frame->l56_featX = frame->l59_i;
                                    frame->l57_featY = frame->l60_j;
                                    frame->l59_i = DCOLS;  // break out of the loops
                                    frame->l60_j = DROWS;
                                } else {
                                    frame->l58_randIndex--;
                                }
                            }
                        }
                    }
                }
                // Don't waste time trying the same place again whether or not this attempt succeeds.
                frame->l23_p->candidates[frame->l56_featX][frame->l57_featY] = false;
                frame->l05_qualifyingTileCount--;

                frame->l11_DFSucceeded = frame->l12_terrainSucceeded = true;

                // Try to build the DF first, if any, since we don't want it to be disrupted by subsequently placed terrain.
                if (frame->l22_feature->featureDF) {
                    frame->l11_DFSucceeded = spawnDungeonFeature(frame->l56_featX, frame->l57_featY, &dungeonFeatureCatalog[frame->l22_feature->featureDF], false,
                                                      !(frame->l22_feature->flags & MF_PERMIT_BLOCKING));
                }

                // Now try to place the terrain tile, if any.
                if (frame->l11_DFSucceeded && frame->l22_feature->terrain) {
                    // Must we check for blocking?
                    if (!(frame->l22_feature->flags & MF_PERMIT_BLOCKING)
                        && ((tileCatalog[frame->l22_feature->terrain].flags & T_PATHING_BLOCKER) || (frame->l22_feature->flags & MF_TREAT_AS_BLOCKING))) {
                        // Yes, check for blocking.

                        zeroOutGrid(frame->l23_p->blockingMap);
                        frame->l23_p->blockingMap[frame->l56_featX][frame->l57_featY] = true;
                        frame->l12_terrainSucceeded = !levelIsDisconnectedWithBlockingMap(frame->l23_p->blockingMap, false);
                    }
                    if (frame->l12_terrainSucceeded) {
                        pmap[frame->l56_featX][frame->l57_featY].layers[frame->l22_feature->layer] = frame->l22_feature->terrain;
                    }
                }

                // OK, if placement was successful, clear some personal space around the feature so subsequent features can't be generated too close.
                // Personal space of 0 means nothing gets cleared, 1 means that only the tile itself gets cleared, and 2 means the 3x3 grid centered on it.

                if (frame->l11_DFSucceeded && frame->l12_terrainSucceeded) {
                    for (frame->l61_i = frame->l56_featX - frame->l07_personalSpace + 1;
                         frame->l61_i <= frame->l56_featX + frame->l07_personalSpace - 1;
                         frame->l61_i++) {
                        for (frame->l62_j = frame->l57_featY - frame->l07_personalSpace + 1;
                             frame->l62_j <= frame->l57_featY + frame->l07_personalSpace - 1;
                             frame->l62_j++) {
                            if (coordinatesAreInMap(frame->l61_i, frame->l62_j)) {
                                if (frame->l23_p->candidates[frame->l61_i][frame->l62_j]) {
                                    brogueAssert(!p->occupied[i][j] || (i == originX && j == originY)); // Candidates[][] should never be true where occupied[][] is true.
                                    frame->l23_p->candidates[frame->l61_i][frame->l62_j] = false;
                                    frame->l05_qualifyingTileCount--;
                                }
                                frame->l23_p->occupied[frame->l61_i][frame->l62_j] = true;
                            }
                        }
                    }
                    frame->l01_instance++; // we've placed an instance
                    //DEBUG printf("\nPlaced instance #%i of feature %i at (%i, %i).", instance, feat, featX, featY);
                }

                if (frame->l11_DFSucceeded && frame->l12_terrainSucceeded) { // Proceed only if the terrain stuff for this instance succeeded.

                    frame->l20_theItem = NULL;

                    // Mark the feature location as part of the machine, in case it is not already inside of it.
                    pmap[frame->l56_featX][frame->l57_featY].flags |= ((blueprintCatalog[frame->a_bp].flags & BP_ROOM) ? IS_IN_ROOM_MACHINE : IS_IN_AREA_MACHINE);
                    pmap[frame->l56_featX][frame->l57_featY].machineNumber = frame->l09_machineNumber;

                    // Mark the feature location as impregnable if requested.
                    if (frame->l22_feature->flags & MF_IMPREGNABLE) {
                        pmap[frame->l56_featX][frame->l57_featY].flags |= IMPREGNABLE;
                    }

                    // Generate an item as necessary.
                    if ((frame->l22_feature->flags & MF_GENERATE_ITEM)
                        || (frame->a_adoptiveItem && (frame->l22_feature->flags & MF_ADOPT_ITEM) && (blueprintCatalog[frame->a_bp].flags & BP_ADOPT_ITEM))) {
                        // Are we adopting an item instead of generating one?
                        if (frame->a_adoptiveItem && (frame->l22_feature->flags & MF_ADOPT_ITEM) && (blueprintCatalog[frame->a_bp].flags & BP_ADOPT_ITEM)) {
                            frame->l20_theItem = frame->a_adoptiveItem;
                            frame->a_adoptiveItem = NULL; // can be adopted only once
                        } else {
                            // Have to create an item ourselves.
                            frame->l20_theItem = generateItem(frame->l22_feature->itemCategory, frame->l22_feature->itemKind);
                            frame->l26_failsafe = 1000;
                            while ((frame->l20_theItem->flags & ITEM_CURSED)
                                   || ((frame->l22_feature->flags & MF_REQUIRE_GOOD_RUNIC) && (!(frame->l20_theItem->flags & ITEM_RUNIC))) // runic if requested
                                   || ((frame->l22_feature->flags & MF_NO_THROWING_WEAPONS) && frame->l20_theItem->category == WEAPON && frame->l20_theItem->quantity > 1) // no throwing weapons if prohibited
                                   || ((frame->l22_feature->flags & MF_REQUIRE_HEAVY_WEAPON) && (!itemIsHeavyWeapon(frame->l20_theItem) || !itemIsPositivelyEnchanted(frame->l20_theItem))) // must be a positively enchanted heavy weapon
                                   || itemIsADuplicate(frame->l20_theItem, frame->l23_p->spawnedItems, frame->l03_itemCount)) { // don't want to duplicates of rings, staffs, etc.
                                deleteItem(frame->l20_theItem);
                                frame->l20_theItem = generateItem(frame->l22_feature->itemCategory, frame->l22_feature->itemKind);
                                if (frame->l26_failsafe <= 0) {
                                    break;
                                }
                                frame->l26_failsafe--;
                            }
                            frame->l23_p->spawnedItems[frame->l03_itemCount] = frame->l20_theItem; // Keep a list of generated items so that we can delete them all if construction fails.
                            frame->l03_itemCount++;
                        }
                        frame->l20_theItem->flags |= frame->l22_feature->itemFlags;

                        addLocationToKey(frame->l20_theItem, frame->l56_featX, frame->l57_featY, (frame->l22_feature->flags & MF_KEY_DISPOSABLE) ? true : false);
                        frame->l20_theItem->originDepth = rogue.depthLevel;
                        if (frame->l22_feature->flags & MF_SKELETON_KEY) {
                            addMachineNumberToKey(frame->l20_theItem, frame->l09_machineNumber, (frame->l22_feature->flags & MF_KEY_DISPOSABLE) ? true : false);
                        }
                        if (!(frame->l22_feature->flags & MF_OUTSOURCE_ITEM_TO_MACHINE)
                            && !(frame->l22_feature->flags & MF_MONSTER_TAKE_ITEM)) {
                            // Place the item at the feature location.
                            placeItemAt(frame->l20_theItem, (pos){ frame->l56_featX, frame->l57_featY });
                        }
                    }

                    if (frame->l22_feature->flags & (MF_OUTSOURCE_ITEM_TO_MACHINE | MF_BUILD_VESTIBULE)) {
                        // Put this item up for adoption, or generate a door guard machine.
                        // Try to create a sub-machine that qualifies.
                        // If we fail 10 times, abort the entire machine (including any sub-machines already built).
                        // Also, if we build a sub-machine, and it succeeds, but this (its parent machine) fails,
                        // we pass the monsters and items that it spawned back to the parent,
                        // so that if the parent fails, they can all be freed.
                        ;
                        for (frame->l63_i=10; frame->l63_i > 0; frame->l63_i--) {
                            // First make sure our adopted item, if any, is not on the floor or in the pack already.
                            // Otherwise, a previous attempt to place it may have put it on the floor in a different
                            // machine, only to have that machine fail and be deleted, leaving the item remaining on
                            // the floor where placed.
                            if ((frame->l22_feature->flags & MF_OUTSOURCE_ITEM_TO_MACHINE) && frame->l20_theItem) {
                                removeItemFromChain(frame->l20_theItem, floorItems);
                                removeItemFromChain(frame->l20_theItem, packItems);
                                frame->l20_theItem->nextItem = NULL;
                                frame->request = (struct pxa_machine_args) {.bp=-1, .originX=-1, .originY=-1, .requiredMachineFlags=BP_ADOPT_ITEM, .adoptiveItem=frame->l20_theItem, .parentSpawnedItems=frame->l23_p->spawnedItemsSub, .parentSpawnedMonsters=frame->l23_p->spawnedMonstersSub};
                                frame->resume = 1;
                                return PXA_MACHINE_CHILD;
                            pxa_machine_resume_1:
                                frame->resume = 0;
                                frame->l15_success = frame->child_result;
                            } else if (frame->l22_feature->flags & MF_BUILD_VESTIBULE) {
                                frame->request = (struct pxa_machine_args) {.bp=-1, .originX=frame->l56_featX, .originY=frame->l57_featY, .requiredMachineFlags=BP_VESTIBULE, .adoptiveItem=NULL, .parentSpawnedItems=frame->l23_p->spawnedItemsSub, .parentSpawnedMonsters=frame->l23_p->spawnedMonstersSub};
                                frame->resume = 2;
                                return PXA_MACHINE_CHILD;
                            pxa_machine_resume_2:
                                frame->resume = 0;
                                frame->l15_success = frame->child_result;
                            }

                            // Now put the item up for adoption.
                            if (frame->l15_success) {
                                // Success! Now we have to add that machine's items and monsters to our own list, so they
                                // all get deleted if this machine or its parent fails.
                                for (frame->l64_j = 0; frame->l64_j<MACHINES_BUFFER_LENGTH && frame->l23_p->spawnedItemsSub[frame->l64_j]; frame->l64_j++) {
                                    frame->l23_p->spawnedItems[frame->l03_itemCount] = frame->l23_p->spawnedItemsSub[frame->l64_j];
                                    frame->l03_itemCount++;
                                    frame->l23_p->spawnedItemsSub[frame->l64_j] = NULL;
                                }
                                for (frame->l65_j = 0; frame->l65_j<MACHINES_BUFFER_LENGTH && frame->l23_p->spawnedMonstersSub[frame->l65_j]; frame->l65_j++) {
                                    frame->l23_p->spawnedMonsters[frame->l04_monsterCount] = frame->l23_p->spawnedMonstersSub[frame->l65_j];
                                    frame->l04_monsterCount++;
                                    frame->l23_p->spawnedMonstersSub[frame->l65_j] = NULL;
                                }
                                break;
                            }
                        }

                        if (!frame->l63_i) {
                            if (D_MESSAGE_MACHINE_GENERATION) printf("\nDepth %i: Failed to place blueprint %i:%s because it requires an adoptive machine and we couldn't place one.", rogue.depthLevel, frame->a_bp, blueprintCatalog[frame->a_bp].name);
                            // failure! abort!
                            copyMap(frame->l23_p->levelBackup, pmap);
                            abortItemsAndMonsters(frame->l23_p->spawnedItems, frame->l23_p->spawnedMonsters);
                            pxa_machine_free_grid(frame, frame->l06_distanceMap);
                            pxa_machine_free_data(frame, frame->l23_p);
                            { frame->result = (false); return PXA_MACHINE_COMPLETE; }
                        }
                        frame->l20_theItem = NULL;
                    }

                    // Generate a horde as necessary.
                    if ((frame->l22_feature->flags & MF_GENERATE_HORDE)
                        || frame->l22_feature->monsterID) {

                        if (frame->l22_feature->flags & MF_GENERATE_HORDE) {
                            frame->l17_monst = spawnHorde(0,
                                               (pos){ frame->l56_featX, frame->l57_featY },
                                               ((HORDE_IS_SUMMONED | HORDE_LEADER_CAPTIVE) & ~(frame->l22_feature->hordeFlags)),
                                               frame->l22_feature->hordeFlags);
                            if (frame->l17_monst) {
                                frame->l17_monst->bookkeepingFlags |= MB_JUST_SUMMONED;
                            }
                        }

                        if (frame->l22_feature->monsterID) {
                            frame->l17_monst = monsterAtLoc((pos){ frame->l56_featX, frame->l57_featY });
                            if (frame->l17_monst) {
                                killCreature(frame->l17_monst, true); // If there's already a monster here, quietly bury the body.
                            }
                            frame->l17_monst = generateMonster(frame->l22_feature->monsterID, true, true);
                            if (frame->l17_monst) {
                                frame->l17_monst->loc = (pos){ .x = frame->l56_featX, .y = frame->l57_featY };
                                pmapAt(frame->l17_monst->loc)->flags |= HAS_MONSTER;
                                frame->l17_monst->bookkeepingFlags |= MB_JUST_SUMMONED;
                            }
                        }

                        if (frame->l17_monst) {
                            if (!frame->l19_leader) {
                                frame->l19_leader = frame->l17_monst;
                            }

                            // Give our item to the monster leader if appropriate.
                            // Actually just remember that we have to give it to this monster; the actual
                            // hand-off happens after we're sure that the machine will succeed.
                            if (frame->l20_theItem && (frame->l22_feature->flags & MF_MONSTER_TAKE_ITEM)) {
                                frame->l18_torchBearer = frame->l17_monst;
                                frame->l21_torch = frame->l20_theItem;
                            }
                        }

                        for (frame->l66_it = iterateCreatures(monsters); hasNextCreature(frame->l66_it);) {
                            frame->l67_monst = nextCreature(&frame->l66_it);
                            if (frame->l67_monst->bookkeepingFlags & MB_JUST_SUMMONED) {

                                // All monsters spawned by a machine are tribemates.
                                // Assign leader/follower roles if they are not yet assigned.
                                if (!(frame->l67_monst->bookkeepingFlags & (MB_LEADER | MB_FOLLOWER))) {
                                    if (frame->l19_leader && frame->l19_leader != frame->l67_monst) {
                                        frame->l67_monst->leader = frame->l19_leader;
                                        frame->l67_monst->bookkeepingFlags &= ~MB_LEADER;
                                        frame->l67_monst->bookkeepingFlags |= MB_FOLLOWER;
                                        frame->l19_leader->bookkeepingFlags |= MB_LEADER;
                                    } else {
                                        frame->l19_leader = frame->l67_monst;
                                    }
                                }

                                frame->l67_monst->bookkeepingFlags &= ~MB_JUST_SUMMONED;
                                frame->l23_p->spawnedMonsters[frame->l04_monsterCount] = frame->l67_monst;
                                frame->l04_monsterCount++;
                                if (frame->l22_feature->flags & MF_MONSTER_SLEEPING) {
                                    frame->l67_monst->creatureState = MONSTER_SLEEPING;
                                }
                                if (frame->l22_feature->flags & MF_MONSTER_FLEEING) {
                                    frame->l67_monst->creatureState = MONSTER_FLEEING;
                                    frame->l67_monst->creatureMode = MODE_PERM_FLEEING;
                                }
                                if (frame->l22_feature->flags & MF_MONSTERS_DORMANT) {
                                    toggleMonsterDormancy(frame->l67_monst);
                                    if (!(frame->l22_feature->flags & MF_MONSTER_SLEEPING) && frame->l67_monst->creatureState != MONSTER_ALLY) {
                                        frame->l67_monst->creatureState = MONSTER_TRACKING_SCENT;
                                    }
                                }
                                frame->l67_monst->machineHome = frame->l09_machineNumber; // Monster remembers the machine that spawned it.
                            }
                        }
                    }
                }
                frame->l20_theItem = NULL;

                // Finished with this instance!
            }
        } while ((frame->l22_feature->flags & MF_REPEAT_UNTIL_NO_PROGRESS) && frame->l01_instance >= frame->l22_feature->minimumInstanceCount);

        //DEBUG printf("\nFinished feature %i. Here's the candidates map:", feat);
        //DEBUG logBuffer(candidates);

        if (frame->l01_instance < frame->l22_feature->minimumInstanceCount && !(frame->l22_feature->flags & MF_REPEAT_UNTIL_NO_PROGRESS)) {
            // failure! abort!

            if (D_MESSAGE_MACHINE_GENERATION) printf("\nDepth %i: Failed to place blueprint %i:%s because of feature %i; needed %i instances but got only %i.",
                         rogue.depthLevel, frame->a_bp, blueprintCatalog[frame->a_bp].name, frame->l52_feat, frame->l22_feature->minimumInstanceCount, frame->l01_instance);

            // Restore the map to how it was before we touched it.
            copyMap(frame->l23_p->levelBackup, pmap);
            abortItemsAndMonsters(frame->l23_p->spawnedItems, frame->l23_p->spawnedMonsters);
            pxa_machine_free_grid(frame, frame->l06_distanceMap);
            pxa_machine_free_data(frame, frame->l23_p);
            { frame->result = (false); return PXA_MACHINE_COMPLETE; }
        }
    }

    // Clear out the interior flag for all non-wired cells, if requested.
    if (blueprintCatalog[frame->a_bp].flags & BP_NO_INTERIOR_FLAG) {
        for(frame->l68_i = 0; frame->l68_i<DCOLS; frame->l68_i++) {
            for(frame->l69_j = 0; frame->l69_j<DROWS; frame->l69_j++) {
                if (pmap[frame->l68_i][frame->l69_j].machineNumber == frame->l09_machineNumber
                    && !cellHasTMFlag((pos){ frame->l68_i, frame->l69_j }, (TM_IS_WIRED | TM_IS_CIRCUIT_BREAKER))) {

                    pmap[frame->l68_i][frame->l69_j].flags &= ~IS_IN_MACHINE;
                    pmap[frame->l68_i][frame->l69_j].machineNumber = 0;
                }
            }
        }
    }

    if (frame->l18_torchBearer && frame->l21_torch) {
        if (frame->l18_torchBearer->carriedItem) {
            deleteItem(frame->l18_torchBearer->carriedItem);
        }
        removeItemFromChain(frame->l21_torch, floorItems);
        frame->l18_torchBearer->carriedItem = frame->l21_torch;
    }

    pxa_machine_free_grid(frame, frame->l06_distanceMap);
    if (D_MESSAGE_MACHINE_GENERATION) printf("\nDepth %i: Built a machine from blueprint %i:%s with an origin at (%i, %i).", rogue.depthLevel, frame->a_bp, blueprintCatalog[frame->a_bp].name, frame->a_originX, frame->a_originY);

    //Pass created items and monsters to parent where they will be deleted on failure to place parent machine
    if (frame->a_parentSpawnedItems) {
        for (frame->l70_i = 0; frame->l70_i<frame->l03_itemCount; frame->l70_i++) {
            frame->a_parentSpawnedItems[frame->l70_i] = frame->l23_p->spawnedItems[frame->l70_i];
        }
    }
    if (frame->a_parentSpawnedMonsters) {
        for (frame->l71_i = 0; frame->l71_i<frame->l04_monsterCount; frame->l71_i++) {
            frame->a_parentSpawnedMonsters[frame->l71_i] = frame->l23_p->spawnedMonsters[frame->l71_i];
        }
    }

    pxa_machine_free_data(frame, frame->l23_p);
    { frame->result = (true); return PXA_MACHINE_COMPLETE; }

}

boolean buildAMachine(enum machineTypes bp,
                      short originX,
                      short originY,
                      unsigned long requiredMachineFlags,
                      item * adoptiveItem,
                      item ** parentSpawnedItems,
                      creature ** parentSpawnedMonsters) {
    extern void pxa_brogue_cooperate(void);
    struct pxa_machine_frame *frame = pxa_machine_allocate_frame(NULL,
        (struct pxa_machine_args) {.bp=bp, .originX=originX, .originY=originY, .requiredMachineFlags=requiredMachineFlags, .adoptiveItem=adoptiveItem, .parentSpawnedItems=parentSpawnedItems, .parentSpawnedMonsters=parentSpawnedMonsters});
    while (frame) {
        pxa_brogue_cooperate();
        if (pxa_machine_step(frame) == PXA_MACHINE_CHILD) {
            frame = pxa_machine_allocate_frame(frame, frame->request);
        } else {
            struct pxa_machine_frame *previous = frame->previous;
            boolean result = frame->result;
            brogueAssert(!frame->owned_data && !frame->owned_grid);
            free(frame);
            if (!previous) return result;
            previous->child_result = result;
            frame = previous;
        }
    }
    return false;
}
