#ifndef PADDLE_HPP
#define PADDLE_HPP

#include "vector.hpp"
#include <stdint.h>

#define PADDLE_WIDTH 4
#define PADDLE_HEIGHT 12

// base paddle class
class PaddleBase
{
public:
	const Vector& getPosition() const noexcept;
	int8_t getScore() const noexcept;
	void setPosition(const Vector& v) noexcept;
	void updateScore() noexcept;
	void resetScore() noexcept;

	// draws the paddle on the screen
	void draw() const;

protected:
	PaddleBase() = default;

	// moves the paddle up(-) or down(+)
	void move(int16_t distance);

private:
	Vector position;
	int8_t score;
};

// statically polymorphic paddle
template<typename T>
class Paddle
	: public PaddleBase
{
public:
	void move();

protected:
	Paddle() = default;
};

// player 1's paddle
class Player1 final
	: public Paddle<Player1>
{
	friend class Paddle<Player1>;
private:
	void move_impl();
};

// player 2's paddle
class Player2 final
	: public Paddle<Player2>
{
	friend class Paddle<Player2>;
private:
	void move_impl();
};

template<typename T>
void Paddle<T>::move()
{
	static_cast<T*>(this)->move_impl();
}

#endif // PADDLE_HPP
