#include "paddle.hpp"
#include "game.hpp"

// config for ai
#define AI_REACT_X 95 // cutoff for reacting to the ball
#define AI_RAND 15 // reciprocal of the chance of reacting before the cutoff

const Vector& PaddleBase::getPosition() const noexcept
{
	return position;
}

int8_t PaddleBase::getScore() const noexcept
{
	return score;
}

void PaddleBase::setPosition(const Vector& v) noexcept
{
	position = v;
}

void PaddleBase::updateScore() noexcept
{
	++score;
}

void PaddleBase::resetScore() noexcept
{
	score = 0;
}

void PaddleBase::draw() const
{
	arduboy.fillRect(position.getX(), position.getY(), PADDLE_WIDTH,
		PADDLE_HEIGHT, WHITE);
}

void PaddleBase::move(int16_t distance)
{
	int16_t newY = position.getY() + distance;
	if (newY <= 0)
	{
		position.setY(1);
	}
	else if (newY > HEIGHT - PADDLE_HEIGHT - 1)
	{
		position.setY(HEIGHT - PADDLE_HEIGHT - 1);
	}
	else
	{
		position.setY(newY);
	}
}

void Player1::move_impl()
{
	if (arduboy.pressed(UP_BUTTON))
	{
		PaddleBase::move(-1);
	}
	if (arduboy.pressed(DOWN_BUTTON))
	{
		PaddleBase::move(1);
	}
}

void Player2::move_impl()
{
	if (twoPlayerGame())
	{
		if (arduboy.pressed(B_BUTTON))
		{
			PaddleBase::move(-1);
		}
		if (arduboy.pressed(A_BUTTON))
		{
			PaddleBase::move(1);
		}
	}
	else
	{
		// bother the paddle if the ball is close or a random time
		if (ball.getPosition().getX() > AI_REACT_X || !random(AI_RAND))
		{
			// move up if the ball is higher
			if (ball.getPosition().getY() <= getPosition().getY())
			{
				PaddleBase::move(-1);
			}
			// move down if the ball is lower
			if (ball.getPosition().getY() + BALL_SIZE >=
				getPosition().getY() + PADDLE_HEIGHT)
			{
				PaddleBase::move(1);
			}
		}
	}
}
