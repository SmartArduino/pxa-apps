#include "vector.hpp"

Vector::Vector(int16_t x, int16_t y)
	: x{ x }, y{ y }
{
}

Vector Vector::operator+(const Vector& v) const
{
	return Vector{ static_cast<int16_t>(x + v.x), static_cast<int16_t>(y + v.y) };
}

Vector Vector::operator-(const Vector& v) const
{
	return Vector{ static_cast<int16_t>(x - v.x), static_cast<int16_t>(y - v.y) };
}

int16_t Vector::getX() const noexcept
{
	return x;
}

int16_t Vector::getY() const noexcept
{
	return y;
}

void Vector::setX(int16_t value) noexcept
{
	x = value;
}

void Vector::setY(int16_t value) noexcept
{
	y = value;
}
