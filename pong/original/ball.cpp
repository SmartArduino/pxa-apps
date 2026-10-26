#include "ball.hpp"
#include "game.hpp"
#include "paddle.hpp"

#define LEFT false
#define RIGHT true

const Vector& Ball::getPosition() const noexcept
{
	return position;
}

const Vector& Ball::getVelocity() const noexcept
{
	return velocity;
}

void Ball::setPosition(const Vector& v) noexcept
{
	position = v;
}

void Ball::setVelocity(const Vector& v) noexcept
{
	bool changed{ velocity.getX() != v.getX() ||
		velocity.getY() != v.getY() };
	velocity = v;
	// reset the incremental error value
	if (changed)
	{
		if (isVelocitySteep())
		{
			err = velocity.getY() / 2;
		}
		else
		{
			err = velocity.getX() / 2;
		}
	}
}

void Ball::move()
{
	for (int8_t i{ 0 }; i < 2; ++i)
	{
		// bounce off the walls
		checkWalls();
		// bounce off the two paddles
		bounceOff(player1, LEFT);
		bounceOff(player2, RIGHT);
		// actually move the ball
		animate();
	}
}

void Ball::draw() const
{
	arduboy.fillRect(position.getX(), position.getY(), BALL_SIZE,
		BALL_SIZE, WHITE);
}

void Ball::animate()
{
	// move the ball using an adaptation of Arduboy2Base::drawLine()
	bool steep{ isVelocitySteep() };
	// increment x (slope <= 1) or y (slope > 1)
	// the other coordinate isn't incremented just yet, so that contributes
	//  to the incremental error until it becomes big enough to deal with
	if (steep)
	{
		position.setY(position.getY() + (velocity.getY() > 0 ? 1 : -1));
		err -= abs(velocity.getX());
	}
	else
	{
		position.setX(position.getX() + (velocity.getX() > 0 ? 1 : -1));
		err -= abs(velocity.getY());
	}
	// time to correct the incremental error
	if (err < 0)
	{
		int16_t change; // the delta x or y needed to correct the error
		if (steep)
		{
			// correct the x coordinate
			if (velocity.getX() > 0)
			{
				change = 1;
			}
			else
			{
				change = -1;
			}
			position.setX(position.getX() + change);
			err += abs(velocity.getY());
		}
		else
		{
			// correct the y coordinate
			if (velocity.getY() > 0)
			{
				change = 1;
			}
			else
			{
				change = -1;
			}
			position.setY(position.getY() + change);
			err += abs(velocity.getX());
		}
	}
}

void Ball::checkWalls()
{
	// bounce off the top wall
	if (position.getY() <= 1)
	{
		setVelocity(Vector{ velocity.getX(), static_cast<int16_t>(abs(velocity.getY())) });
		sound.tone(WALL_FREQ, WALL_DUR);
	}
	// bounce off the bottom wall
	else if (position.getY() >= HEIGHT - BALL_SIZE - 1)
	{
		setVelocity(Vector{ velocity.getX(), static_cast<int16_t>(-abs(velocity.getY())) });
		sound.tone(WALL_FREQ, WALL_DUR);
	}
}

void Ball::bounceOff(const PaddleBase& paddle, bool side)
{
	// get the minkowski difference between the paddle and the ball
	// a-b=c, where a=paddle, b=ball, c=diff
	int16_t cx{ static_cast<int16_t>(paddle.getPosition().getX() - position.getX() - BALL_SIZE) };
	int16_t cy{ static_cast<int16_t>(paddle.getPosition().getY() - position.getY() - BALL_SIZE) };
	constexpr int16_t cw{ PADDLE_WIDTH + BALL_SIZE };
	constexpr int16_t ch{ PADDLE_HEIGHT + BALL_SIZE };
	// if the difference contains the origin, then a collision occured
	if (cx <= 0 && cx + cw >= 0 && cy <= 0 && cy + ch >= 0)
	{
		// calculate the direction relative to the center of the paddle
		Vector paddleCenter{ static_cast<int16_t>(paddle.getPosition().getX() + (PADDLE_WIDTH / 2)),
			static_cast<int16_t>(paddle.getPosition().getY() + (PADDLE_HEIGHT / 2)) };
		setVelocity(Vector{ position - paddleCenter });
		sound.tone(PADDLE_FREQ, PADDLE_DUR);
	}
}

void Ball::bounceOff(bool side)
{
	if (!side) // left
	{
		setVelocity(Vector{ static_cast<int16_t>(abs(velocity.getX())), velocity.getY() });
	}
	else // right
	{
		setVelocity(Vector{ static_cast<int16_t>(-abs(velocity.getX())), velocity.getY() });
	}
}

bool Ball::isVelocitySteep() const
{
	return abs(velocity.getY()) > abs(velocity.getX());
}
