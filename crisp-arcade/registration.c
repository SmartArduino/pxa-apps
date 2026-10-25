// SPDX-License-Identifier: MIT
// Deliberately replaces upstream menuGameList.c: no other games are linked.
void addGameCastN(void);
void addGameGrapplingH(void);
void addGameReflector(void);
void addGameColorRoll(void);
void addGames(void) {
  addGameCastN();
  addGameGrapplingH();
  addGameReflector();
  addGameColorRoll();
}
