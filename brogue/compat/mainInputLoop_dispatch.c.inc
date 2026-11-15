/*
 *  IO.c
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

/* Keep the original UI locals in the existing Wasm linear-memory C stack.
 * The worker returns only at the original child-call sites, so its large
 * interpreter frame is absent while the dispatcher executes that same call.
 * Every initializer, branch, call, return, and RNG operation stays in order. */
struct pxa_ui_mainInputLoop_frame {
    unsigned resume;
    pos l00_oldTargetLoc;
    short l01_steps;
    short l02_oldRNG;
    short l03_dir;
    pos l04_path[1000];
    creature * l05_monst;
    item * l06_theItem;
    SavedDisplayBuffer l07_rbuf;
    char l08_canceled;
    char l09_targetConfirmed;
    char l10_tabKey;
    char l11_focusedOnMonster;
    char l12_focusedOnItem;
    char l13_focusedOnTerrain;
    char l14_playingBack;
    char l15_doEvent;
    char l16_textDisplayed;
    rogueEvent l17_theEvent;
    short ** l18_costMap;
    short ** l19_playerPathingMap;
    short ** l20_cursorSnapMap;
    brogueButton l21_buttons[5];
    buttonState l22_state;
    short l23_buttonInput;
    short l24_backupCost;
    pos l25_originLoc;
    pos l26_pathDestination;
    pos l27_newLoc;
};
static __attribute__((noinline)) unsigned pxa_ui_mainInputLoop_step(struct pxa_ui_mainInputLoop_frame *frame) {
    switch (frame->resume) {
        case 0: break;
        case 1: goto pxa_ui_mainInputLoop_resume_1;
        case 2: goto pxa_ui_mainInputLoop_resume_2;
        case 3: goto pxa_ui_mainInputLoop_resume_3;
        case 4: goto pxa_ui_mainInputLoop_resume_4;
        case 5: goto pxa_ui_mainInputLoop_resume_5;
        case 6: goto pxa_ui_mainInputLoop_resume_6;
        default: brogueAssert(false); abort();
    }

    frame->l00_oldTargetLoc = (pos){ 0, 0 };
    ;
    ;
    ;
    ;
    ;

    ;

    ;
    ;
    memcpy(frame->l21_buttons, (brogueButton[5]){{{0}}}, sizeof(frame->l21_buttons));
    ;
    ;
    ;

    frame->l08_canceled = false;
    rogue.cursorMode = false; // Controls whether the keyboard moves the cursor or the character.
    frame->l01_steps = 0;

    rogue.cursorPathIntensity = (rogue.cursorMode ? 50 : 20);

    // Initialize buttons.
    initializeMenuButtons(&frame->l22_state, frame->l21_buttons);

    frame->l14_playingBack = rogue.playbackMode;
    rogue.playbackMode = false;
    frame->l18_costMap = allocGrid();
    frame->l19_playerPathingMap = allocGrid();
    frame->l20_cursorSnapMap = allocGrid();

    rogue.cursorLoc = INVALID_POS;

    while (!rogue.gameHasEnded && (!frame->l14_playingBack || !frame->l08_canceled)) { // repeats until the game ends

        frame->l02_oldRNG = rogue.RNG;
        rogue.RNG = RNG_COSMETIC;

        frame->l11_focusedOnMonster = frame->l12_focusedOnItem = frame->l13_focusedOnTerrain = false;
        frame->l01_steps = 0;
        clearCursorPath();

        frame->l25_originLoc = player.loc;

        if (frame->l14_playingBack && rogue.cursorMode) {
            temporaryMessage("Examine what? (<hjklyubn>, mouse, or <tab>)", 0);
        }

        if (!frame->l14_playingBack
            && posEq(player.loc, rogue.cursorLoc)
            && posEq(frame->l00_oldTargetLoc, rogue.cursorLoc)) {

            // Path hides when you reach your destination.
            rogue.cursorMode = false;
            rogue.cursorPathIntensity = (rogue.cursorMode ? 50 : 20);
            rogue.cursorLoc = INVALID_POS;
        }

        frame->l00_oldTargetLoc = rogue.cursorLoc;

        populateCreatureCostMap(frame->l18_costMap, &player);

        fillGrid(frame->l19_playerPathingMap, 30000);
        frame->l19_playerPathingMap[player.loc.x][player.loc.y] = 0;
        dijkstraScan(frame->l19_playerPathingMap, frame->l18_costMap, true);
        processSnapMap(frame->l20_cursorSnapMap);

        do {
            frame->l16_textDisplayed = false;

            // Draw the cursor and path
            if (isPosInMap(frame->l00_oldTargetLoc)) {
                refreshDungeonCell(frame->l00_oldTargetLoc);               // Remove old cursor.
            }
            if (!frame->l14_playingBack) {
                if (isPosInMap(frame->l00_oldTargetLoc)) {
                    hilitePath(frame->l04_path, frame->l01_steps, true);                                  // Unhilite old path.
                }
                if (isPosInMap(rogue.cursorLoc)) {
                    ;
                    if (frame->l20_cursorSnapMap[rogue.cursorLoc.x][rogue.cursorLoc.y] >= 0
                        && frame->l20_cursorSnapMap[rogue.cursorLoc.x][rogue.cursorLoc.y] < 30000) {
                        frame->l26_pathDestination = rogue.cursorLoc;
                    } else {
                        // If the cursor is aimed at an inaccessible area, find the nearest accessible area to path toward.
                        frame->l26_pathDestination = getClosestValidLocationOnMap(frame->l20_cursorSnapMap, rogue.cursorLoc.x, rogue.cursorLoc.y);
                    }

                    fillGrid(frame->l19_playerPathingMap, 30000);
                    frame->l19_playerPathingMap[frame->l26_pathDestination.x][frame->l26_pathDestination.y] = 0;
                    frame->l24_backupCost = frame->l18_costMap[frame->l26_pathDestination.x][frame->l26_pathDestination.y];
                    frame->l18_costMap[frame->l26_pathDestination.x][frame->l26_pathDestination.y] = 1;
                    dijkstraScan(frame->l19_playerPathingMap, frame->l18_costMap, true);
                    frame->l18_costMap[frame->l26_pathDestination.x][frame->l26_pathDestination.y] = frame->l24_backupCost;
                    frame->l01_steps = getPlayerPathOnMap(frame->l04_path, frame->l19_playerPathingMap, player.loc);

//                  steps = getPlayerPathOnMap(path, playerPathingMap, pathDestination[0], pathDestination[1]) - 1; // Get new path.
//                  reversePath(path, steps);   // Flip it around, back-to-front.

                    if (frame->l01_steps >= 0) {
                        frame->l04_path[frame->l01_steps] = frame->l26_pathDestination;
                    }
                    frame->l01_steps++;
//                  if (playerPathingMap[cursor[0]][cursor[1]] != 1
                    if (frame->l19_playerPathingMap[player.loc.x][player.loc.y] != 1
                        || !posEq(frame->l26_pathDestination, rogue.cursorLoc)) {

                        hilitePath(frame->l04_path, frame->l01_steps, false);     // Hilite new path.
                    }
                }
            }

            if (isPosInMap(rogue.cursorLoc)) {
                hiliteCell(rogue.cursorLoc.x,
                           rogue.cursorLoc.y,
                           &white,
                           (frame->l01_steps <= 0
                            || posEq(frame->l04_path[frame->l01_steps-1], rogue.cursorLoc)
                            || (!frame->l14_playingBack && distanceBetween(player.loc, rogue.cursorLoc) <= 1) ? 100 : 25),
                           true);

                frame->l00_oldTargetLoc = rogue.cursorLoc;

                frame->l05_monst = monsterAtLoc(rogue.cursorLoc);
                frame->l06_theItem = itemAtLoc(rogue.cursorLoc);
                if (frame->l05_monst != NULL && (canSeeMonster(frame->l05_monst) || rogue.playbackOmniscience)) {
                    rogue.playbackMode = frame->l14_playingBack;
                    refreshSideBar(rogue.cursorLoc.x, rogue.cursorLoc.y, false);
                    rogue.playbackMode = false;

                    frame->l11_focusedOnMonster = true;
                    if (frame->l05_monst != &player && (!player.status[STATUS_HALLUCINATING] || rogue.playbackOmniscience || player.status[STATUS_TELEPATHIC])) {
                        frame->l07_rbuf = saveDisplayBuffer();
                        printMonsterDetails(frame->l05_monst);
                        frame->l16_textDisplayed = true;
                    }
                } else if (frame->l06_theItem != NULL && playerCanSeeOrSense(rogue.cursorLoc.x, rogue.cursorLoc.y)) {
                    rogue.playbackMode = frame->l14_playingBack;
                    refreshSideBar(rogue.cursorLoc.x, rogue.cursorLoc.y, false);
                    rogue.playbackMode = false;

                    frame->l12_focusedOnItem = true;
                    if (!player.status[STATUS_HALLUCINATING] || rogue.playbackOmniscience) {
                        frame->l07_rbuf = saveDisplayBuffer();
                        printFloorItemDetails(frame->l06_theItem);
                        frame->l16_textDisplayed = true;
                    }
                } else if (cellHasTMFlag(rogue.cursorLoc, TM_LIST_IN_SIDEBAR) && playerCanSeeOrSense(rogue.cursorLoc.x, rogue.cursorLoc.y)) {
                    rogue.playbackMode = frame->l14_playingBack;
                    refreshSideBar(rogue.cursorLoc.x, rogue.cursorLoc.y, false);
                    rogue.playbackMode = false;
                    frame->l13_focusedOnTerrain = true;
                }

                printLocationDescription(rogue.cursorLoc.x, rogue.cursorLoc.y);
            }

            // Get the input!
            rogue.playbackMode = frame->l14_playingBack;
            frame->l15_doEvent = moveCursor(&frame->l09_targetConfirmed, &frame->l08_canceled, &frame->l10_tabKey, &rogue.cursorLoc, &frame->l17_theEvent, &frame->l22_state, !frame->l16_textDisplayed, rogue.cursorMode, true);
            rogue.playbackMode = false;

            if (frame->l22_state.buttonChosen == 3) { // Actions menu button.
                frame->l23_buttonInput = actionMenu(frame->l21_buttons[3].x - 4, frame->l14_playingBack); // Returns the corresponding keystroke.
                if (frame->l23_buttonInput == -1) { // Canceled.
                    frame->l15_doEvent = false;
                } else {
                    frame->l17_theEvent.eventType = KEYSTROKE;
                    frame->l17_theEvent.param1 = frame->l23_buttonInput;
                    frame->l17_theEvent.param2 = 0;
                    frame->l17_theEvent.shiftKey = frame->l17_theEvent.controlKey = false;
                    frame->l15_doEvent = true;
                }
            } else if (frame->l22_state.buttonChosen > -1) {
                frame->l17_theEvent.eventType = KEYSTROKE;
                frame->l17_theEvent.param1 = frame->l21_buttons[frame->l22_state.buttonChosen].hotkey[0];
                frame->l17_theEvent.param2 = 0;
            }
            frame->l22_state.buttonChosen = -1;

            if (frame->l14_playingBack) {
                if (frame->l08_canceled) {
                    rogue.cursorMode = false;
                    rogue.cursorPathIntensity = (rogue.cursorMode ? 50 : 20);
                }

                if (frame->l17_theEvent.eventType == KEYSTROKE
                    && frame->l17_theEvent.param1 == ACKNOWLEDGE_KEY) { // To unpause by button during playback.
                    frame->l08_canceled = true;
                } else {
                    frame->l08_canceled = false;
                }
            }

            if (frame->l11_focusedOnMonster || frame->l12_focusedOnItem || frame->l13_focusedOnTerrain) {
                frame->l11_focusedOnMonster = false;
                frame->l12_focusedOnItem = false;
                frame->l13_focusedOnTerrain = false;
                if (frame->l16_textDisplayed) {
                    restoreDisplayBuffer(&frame->l07_rbuf); // Erase the monster info window.
                }
                rogue.playbackMode = frame->l14_playingBack;
                refreshSideBar(-1, -1, false);
                rogue.playbackMode = false;
            }

            if (frame->l10_tabKey && !frame->l14_playingBack) { // The tab key cycles the cursor through monsters, items and terrain features.
                ;
                if (nextTargetAfter(NULL, &frame->l27_newLoc, rogue.cursorLoc, AUTOTARGET_MODE_EXPLORE, frame->l17_theEvent.shiftKey)) {
                    rogue.cursorLoc = frame->l27_newLoc;
                }
            }

            if (frame->l17_theEvent.eventType == KEYSTROKE
                && (frame->l17_theEvent.param1 == ASCEND_KEY && rogue.cursorLoc.x == rogue.upLoc.x && rogue.cursorLoc.y == rogue.upLoc.y
                    || frame->l17_theEvent.param1 == DESCEND_KEY && rogue.cursorLoc.x == rogue.downLoc.x && rogue.cursorLoc.y == rogue.downLoc.y)) {

                    frame->l09_targetConfirmed = true;
                    frame->l15_doEvent = false;
                }
        } while (!frame->l09_targetConfirmed && !frame->l08_canceled && !frame->l15_doEvent && !rogue.gameHasEnded);

        if (isPosInMap(frame->l00_oldTargetLoc)) {
            refreshDungeonCell(frame->l00_oldTargetLoc);                       // Remove old rogue.cursorLoc.
        }

        rogue.RNG = frame->l02_oldRNG;;

        if (frame->l08_canceled && !frame->l14_playingBack) {
            hideCursor();
            confirmMessages();
        } else if (frame->l09_targetConfirmed && !frame->l14_playingBack && isPosInMap(rogue.cursorLoc)) {
            if (frame->l17_theEvent.eventType == MOUSE_UP
                && frame->l17_theEvent.controlKey
                && frame->l01_steps > 1) {
                // Control-clicking moves the player one step along the path.
                for (frame->l03_dir=0;
                     frame->l03_dir < DIRECTION_COUNT && !posEq(posNeighborInDirection(player.loc, frame->l03_dir) , frame->l04_path[0]);
                     frame->l03_dir++);
                frame->resume = 1; return 1;
        pxa_ui_mainInputLoop_resume_1: frame->resume = 0;
            } else if (D_WORMHOLING) {
                frame->resume = 2; return 2;
        pxa_ui_mainInputLoop_resume_2: frame->resume = 0;
            } else {
                confirmMessages();
                if (posEq(frame->l25_originLoc, rogue.cursorLoc)) {
                    confirmMessages();
                } else if (abs(player.loc.x - rogue.cursorLoc.x) + abs(player.loc.y - rogue.cursorLoc.y) == 1 // horizontal or vertical
                           || (distanceBetween(player.loc, rogue.cursorLoc) == 1 // includes diagonals
                               && (!diagonalBlocked(player.loc.x, player.loc.y, rogue.cursorLoc.x, rogue.cursorLoc.y, !rogue.playbackOmniscience)
                                   || ((pmapAt(rogue.cursorLoc)->flags & HAS_MONSTER) && (monsterAtLoc(rogue.cursorLoc)->info.flags & MONST_ATTACKABLE_THRU_WALLS)) // there's a turret there
                                   || ((terrainFlags(rogue.cursorLoc) & T_OBSTRUCTS_PASSABILITY) && (terrainMechFlags(rogue.cursorLoc) & TM_PROMOTES_ON_PLAYER_ENTRY))))) { // there's a lever there
                                                                                                                                                                                      // Clicking one space away will cause the player to try to move there directly irrespective of path.
                                   for (frame->l03_dir=0;
                                        frame->l03_dir < DIRECTION_COUNT && (player.loc.x + nbDirs[frame->l03_dir][0] != rogue.cursorLoc.x || player.loc.y + nbDirs[frame->l03_dir][1] != rogue.cursorLoc.y);
                                        frame->l03_dir++);
                                   frame->resume = 3; return 3;
        pxa_ui_mainInputLoop_resume_3: frame->resume = 0;
                               } else if (frame->l01_steps) {
                                   frame->resume = 4; return 4;
        pxa_ui_mainInputLoop_resume_4: frame->resume = 0;
                               }
            }
        } else if (frame->l15_doEvent) {
            // If the player entered input during moveCursor() that wasn't a cursor movement command.
            // Mainly, we want to filter out directional keystrokes when we're in cursor mode, since
            // those should move the cursor but not the player.
            brogueAssert(rogue.RNG == RNG_SUBSTANTIVE);
            if (frame->l14_playingBack) {
                rogue.playbackMode = true;
                frame->resume = 5; return 5;
        pxa_ui_mainInputLoop_resume_5: frame->resume = 0;
#ifdef ENABLE_PLAYBACK_SWITCH
                if (!rogue.playbackMode) {
                    // Playback mode is off, user must have taken control
                    // Redraw buttons to reflect that
                    initializeMenuButtons(&frame->l22_state, frame->l21_buttons);
                }
#endif
                frame->l14_playingBack = rogue.playbackMode;
                rogue.playbackMode = false;
            } else {
                frame->resume = 6; return 6;
        pxa_ui_mainInputLoop_resume_6: frame->resume = 0;
                if (rogue.playbackMode) {
                    frame->l14_playingBack = true;
                    rogue.playbackMode = false;
                    confirmMessages();
                    break;
                }
            }
        }
    }

    rogue.playbackMode = frame->l14_playingBack;
    refreshSideBar(-1, -1, false);
    freeGrid(frame->l18_costMap);
    freeGrid(frame->l19_playerPathingMap);
    freeGrid(frame->l20_cursorSnapMap);

    return 0;
}
void mainInputLoop() {
    struct pxa_ui_mainInputLoop_frame context;
    struct pxa_ui_mainInputLoop_frame *frame = &context;
    frame->resume = 0;
    for (;;) {
        switch (pxa_ui_mainInputLoop_step(frame)) {
            case 0: return;
            case 1: playerMoves(frame->l03_dir); break;
            case 2: travel(rogue.cursorLoc, true); break;
            case 3: playerMoves(frame->l03_dir); break;
            case 4: travelRoute(frame->l04_path, frame->l01_steps); break;
            case 5: executePlaybackInput(&frame->l17_theEvent); break;
            case 6: executeEvent(&frame->l17_theEvent); break;
            default: brogueAssert(false); abort();
        }
    }
}
