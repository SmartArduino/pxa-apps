#include <Arduboy2.h>
#include "game.h"

Game game(24);

void setup () {
  game.init();
}

void loop () {
  game.run();
}