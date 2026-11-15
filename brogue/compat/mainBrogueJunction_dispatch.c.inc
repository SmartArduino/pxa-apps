/*
 *  Buttons.c
 *  Brogue
 *
 *  Created by Brian Walker on 1/14/12.
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
struct pxa_ui_mainBrogueJunction_frame {
    unsigned resume;
    rogueEvent l00_theEvent;
    char l01_path[4096];
    char l02_buf[100];
    char l03_seedDefault[100];
    short l04_i;
    short l05_j;
    short l06_k;
};
static __attribute__((noinline)) unsigned pxa_ui_mainBrogueJunction_step(struct pxa_ui_mainBrogueJunction_frame *frame) {
    switch (frame->resume) {
        case 0: break;
        case 1: goto pxa_ui_mainBrogueJunction_resume_1;
        case 2: goto pxa_ui_mainBrogueJunction_resume_2;
        default: brogueAssert(false); abort();
    }

    ;
    ;
    ;

    // clear screen and display buffer
    for (frame->l04_i=0; frame->l04_i<COLS; frame->l04_i++) {
        for (frame->l05_j=0; frame->l05_j<ROWS; frame->l05_j++) {
            displayBuffer.cells[frame->l04_i][frame->l05_j].character = 0;
            displayBuffer.cells[frame->l04_i][frame->l05_j].opacity = 100;
            for (frame->l06_k=0; frame->l06_k<3; frame->l06_k++) {
                displayBuffer.cells[frame->l04_i][frame->l05_j].foreColorComponents[frame->l06_k] = 0;
                displayBuffer.cells[frame->l04_i][frame->l05_j].backColorComponents[frame->l06_k] = 0;
            }
            plotCharWithColor(' ', (windowpos){ frame->l04_i, frame->l05_j }, &black, &black);
        }
    }

    initializeGameVariant();

    initializeLaunchArguments(&rogue.nextGame, rogue.nextGamePath, &rogue.nextGameSeed);

    do {
        rogue.gameHasEnded = false;
        rogue.playbackFastForward = false;
        rogue.playbackMode = false;
        switch (rogue.nextGame) {
            case NG_NOTHING:
                // Run the main menu to get a decision out of the player.
                titleMenu();
                break;
            case NG_GAME_VARIANT:
                rogue.nextGame = NG_NOTHING;
                initializeGameVariant();
                break;
            case NG_NEW_GAME:
            case NG_NEW_GAME_WITH_SEED:
                rogue.nextGamePath[0] = '\0';
                randomNumbersGenerated = 0;

                rogue.playbackMode = false;
                rogue.playbackFastForward = false;
                rogue.playbackBetweenTurns = false;

                getAvailableFilePath(frame->l01_path, LAST_GAME_NAME, GAME_SUFFIX);
                strcat(frame->l01_path, GAME_SUFFIX);
                strcpy(currentFilePath, frame->l01_path);

                if (rogue.nextGame == NG_NEW_GAME_WITH_SEED) {
                    if (rogue.nextGameSeed == 0) { // Prompt for seed; default is the previous game's seed.
                        if (previousGameSeed == 0) {
                            frame->l03_seedDefault[0] = '\0';
                        } else {
                            sprintf(frame->l03_seedDefault, "%llu", (unsigned long long)previousGameSeed);
                        }
                        if (getInputTextString(frame->l02_buf, "Generate dungeon with seed number:",
                                               20, // length of "18446744073709551615" (2^64 - 1)
                                               frame->l03_seedDefault,
                                               "",
                                               TEXT_INPUT_NUMBERS,
                                               true)
                            && frame->l02_buf[0] != '\0') {
                            if (!tryParseUint64(frame->l02_buf, &rogue.nextGameSeed)) {
                                // seed is too large, default to the largest possible seed
                                rogue.nextGameSeed = 18446744073709551615ULL;
                            }
                        } else {
                            rogue.nextGame = NG_NOTHING;
                            break; // Don't start a new game after all.
                        }
                    }
                } else {
                    rogue.nextGameSeed = 0; // Seed based on clock.
                }

                rogue.nextGame = NG_NOTHING;
                initializeRogue(rogue.nextGameSeed);
                startLevel(rogue.depthLevel, 1); // descending into level 1

                frame->resume = 1; return 1;
        pxa_ui_mainBrogueJunction_resume_1: frame->resume = 0;
                if(serverMode) {
                    rogue.nextGame = NG_QUIT;
                }
                freeEverything();
                break;
            case NG_OPEN_GAME:
                rogue.nextGame = NG_NOTHING;
                frame->l01_path[0] = '\0';
                if (rogue.nextGamePath[0]) {
                    strcpy(frame->l01_path, rogue.nextGamePath);
                    strcpy(rogue.currentGamePath, rogue.nextGamePath);
                    rogue.nextGamePath[0] = '\0';
                } else {
                    dialogChooseFile(frame->l01_path, GAME_SUFFIX, "Open saved game:");
                    //chooseFile(path, "Open saved game: ", "Saved game", GAME_SUFFIX);
                }

                if (openFile(frame->l01_path)) {
                    if (loadSavedGame()) {
                        frame->resume = 2; return 2;
        pxa_ui_mainBrogueJunction_resume_2: frame->resume = 0;
                    }
                    freeEverything();
                } else {
                    //dialogAlert("File not found.");
                }
                rogue.playbackMode = false;
                rogue.playbackOOS = false;

                if(serverMode) {
                    rogue.nextGame = NG_QUIT;
                }
                break;
            case NG_VIEW_RECORDING:
                rogue.nextGame = NG_NOTHING;

                frame->l01_path[0] = '\0';
                if (rogue.nextGamePath[0]) {
                    strcpy(frame->l01_path, rogue.nextGamePath);
                    strcpy(rogue.currentGamePath, rogue.nextGamePath);
                    rogue.nextGamePath[0] = '\0';
                } else {
                    dialogChooseFile(frame->l01_path, RECORDING_SUFFIX, "View recording:");
                    //chooseFile(path, "View recording: ", "Recording", RECORDING_SUFFIX);
                }

                if (openFile(frame->l01_path)) {
                    randomNumbersGenerated = 0;
                    rogue.playbackMode = true;
                    initializeRogue(0); // Seed argument is ignored because we're in playback.
                    if (!rogue.gameHasEnded) {
                        startLevel(rogue.depthLevel, 1);
                        if (nonInteractivePlayback) {
                            rogue.playbackPaused = false;
                        } else {
                            rogue.playbackPaused = true;
                        }
                        displayAnnotation(); // in case there's an annotation for turn 0
                    }

                    while(!rogue.gameHasEnded && rogue.playbackMode) {
                        if (rogue.playbackPaused) {
                            rogue.playbackPaused = false;
                            pausePlayback();
                        }
#ifdef ENABLE_PLAYBACK_SWITCH
                        // We are coming from the end of a recording the user has taken over.
                        // No more event checks, that has already been handled
                        if (rogue.gameHasEnded) {
                            break;
                        }
#endif
                        rogue.RNG = RNG_COSMETIC; // dancing terrain colors can't influence recordings
                        rogue.playbackBetweenTurns = true;
                        nextBrogueEvent(&frame->l00_theEvent, false, true, false);
                        rogue.RNG = RNG_SUBSTANTIVE;

                        executeEvent(&frame->l00_theEvent);
                    }

                    freeEverything();
                } else {
                    // announce file not found
                }
                rogue.playbackMode = false;
                rogue.playbackOOS = false;

                if(serverMode || nonInteractivePlayback) {
                    rogue.nextGame = NG_QUIT;
                }
                break;
            case NG_HIGH_SCORES:
                rogue.nextGame = NG_NOTHING;
                printHighScores(false);
                break;
            case NG_GAME_STATS:
                rogue.nextGame = NG_NOTHING;
                viewGameStats();
                break;
            case NG_QUIT:
                // No need to do anything.
                break;
            default:
                break;
        }
    } while (rogue.nextGame != NG_QUIT);

    return 0;
}
void mainBrogueJunction() {
    struct pxa_ui_mainBrogueJunction_frame context;
    struct pxa_ui_mainBrogueJunction_frame *frame = &context;
    frame->resume = 0;
    for (;;) {
        switch (pxa_ui_mainBrogueJunction_step(frame)) {
            case 0: return;
            case 1: mainInputLoop(); break;
            case 2: mainInputLoop(); break;
            default: brogueAssert(false); abort();
        }
    }
}
