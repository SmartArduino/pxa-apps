#include "game.hpp"
#include <Arduboy2.h>
#include <ArduboyTones.h>

Arduboy2 arduboy;
ArduboyTones sound{ arduboy.audio.enabled };

void
setup()
{

	arduboy.begin();
	arduboy.setFrameRate(30);
	arduboy.setTextSize(2);
	arduboy.clear();
}

void
loop()
{
	if (!arduboy.nextFrame())
	{
		return;
	}
	arduboy.pollButtons();
	tick();

	arduboy.display(CLEAR_BUFFER);
}
