#ifndef VECTOR_HPP
#define VECTOR_HPP

#include <stdint.h>

class Vector
{
public:
	Vector() = default;
	Vector(int16_t x, int16_t y);

	Vector operator+(const Vector& v) const;
	Vector operator-(const Vector& v) const;

	int16_t getX() const noexcept;
	int16_t getY() const noexcept;
	void setX(int16_t value) noexcept;
	void setY(int16_t value) noexcept;

private:
	int16_t x;
	int16_t y;
};

#endif // VECTOR_HPP
