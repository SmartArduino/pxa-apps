#include "game.hpp"
#include "vector.hpp"

#define SCORE_MAX 3

// helper functions

static void
draw();

static void
resetBall();

// game state functions

static void
menuTitle();

static void
gameSetup();

static void
gameMain();

static void
gamePause();

static void
menuWin1();

static void
menuWin2();

typedef void (*tick_func_t)();

static tick_func_t gameTick{ &menuTitle };
static bool twoPlayers{ false };

Ball ball;
Player1 player1;
Player2 player2;

void tick()
{
	gameTick();
}

bool twoPlayerGame()
{
	return twoPlayers;
}

void
draw()
{
	// cool border around the screen
	arduboy.drawRect(0, 0, WIDTH, HEIGHT, WHITE);
	// dotted line in the middle
	for (uint8_t i{ 2 }; i < HEIGHT; i += 8)
	{
		arduboy.drawFastVLine(WIDTH / 2, i, 4, WHITE);
	}
	// scores
	arduboy.setCursor(WIDTH/2 - 12, 2);
	arduboy.print(player1.getScore());
	arduboy.setCursor(WIDTH/2 + 3, 2);
	arduboy.print(player2.getScore());
	// objects
	ball.draw();
	player1.draw();
	player2.draw();
}

void
resetBall()
{
	ball.setPosition(Vector{ WIDTH / 2, HEIGHT / 2 });
	ball.setVelocity(Vector{ 1, 1 });
}

void
menuTitle()
{
	arduboy.setCursor(0, 0);
	arduboy.print(F("players:"));
	arduboy.setCursor(96, 0);
	arduboy.print(twoPlayers ? 2 : 1);
	if (arduboy.justPressed(LEFT_BUTTON) ||
		arduboy.justPressed(RIGHT_BUTTON))
	{
		twoPlayers = !twoPlayers;
	}
	if (arduboy.justPressed(A_BUTTON))
	{
		gameTick = &gameSetup;
	}
}

void
gameSetup()
{
	arduboy.initRandomSeed();
	resetBall();
	// set both paddles in the center of the vertical space
	player1.setPosition(Vector{ 9, 24 });
	player1.resetScore();
	player2.setPosition(Vector{ WIDTH - PADDLE_WIDTH - 9, 24 });
	player2.resetScore();
	draw();
	arduboy.display();
	delay(1000);
	gameTick = &gameMain;
}

void
gameMain()
{
	draw();
	// pause the game if needed
	if (arduboy.justPressed(LEFT_BUTTON))
	{
		gameTick = &gamePause;
		return;
	}
	ball.move();
	player1.move();
	player2.move();
	// check if player 1 scored
	if (ball.getPosition().getX() >= WIDTH - BALL_SIZE)
	{
		player1.updateScore();
		if (player1.getScore() >= SCORE_MAX)
		{
			gameTick = &menuWin1;
		}
		else
		{
			resetBall();
		}
		sound.tone(POINT_FREQ, POINT_DUR);
	}
	// check if player 2 scored
	else if (ball.getPosition().getX() < 1)
	{
		player2.updateScore();
		if (player2.getScore() >= SCORE_MAX)
		{
			gameTick = &menuWin2;
		}
		else
		{
			resetBall();
		}
		sound.tone(POINT_FREQ, POINT_DUR);
	}
}

void
gamePause()
{
	draw();
	// resume the game if needed
	if (arduboy.justPressed(LEFT_BUTTON))
	{
		gameTick = &gameMain;
	}
}

void
menuWin1()
{
	arduboy.setCursor(0, 0);
	arduboy.print(F("Player 1\nwins!\nPress A to\nrestart"));
	if (arduboy.justPressed(A_BUTTON))
	{
		gameTick = &gameSetup;
	}
}

void
menuWin2()
{
	arduboy.setCursor(0, 0);
	arduboy.print(F("Player 2\nwins!\nPress A to\nrestart"));
	if (arduboy.justPressed(A_BUTTON))
	{
		gameTick = &gameSetup;
	}
}

// PXA reset boundary, preserving the upstream state and game rules.
void pong_reset() { gameTick=&menuTitle; twoPlayers=false; ball=Ball{}; player1=Player1(); player2=Player2(); }
int pong_phase() { return gameTick==&menuTitle?0:gameTick==&gameSetup?1:gameTick==&gameMain?2:gameTick==&gamePause?3:gameTick==&menuWin1?4:5; }
