// SPDX-License-Identifier: MIT
#pragma once
#include "life.hpp"
namespace farm {
enum class Weather : uint8_t { Sun, Rain, Storm, Snow, Wind, Count };
Weather weather_on(uint16_t day);
const char *weather_name(Weather w, bool en=false);
bool wet_weather(Weather w);
bool shop_open(const State &s);
bool building_open(const State &s, Region building);
bool crop_in_season(uint16_t day, int crop);
int food_energy(int item);
int item_price(int item);
const char *seasonal_name(int item, bool en=false);
bool spend_energy(State &s, int amount);
int movement_speed(const State &s); // World pixels per second; running is free.
struct Lighting {
  uint16_t color = 0;
  uint8_t amount = 0; // 0..255, mixed against the original RGB565 palette.
  int key() const { return amount ? (int(color) << 8) | amount : 0; }
};
Lighting ambient_light(const State &s);
uint16_t apply_light(uint16_t color, Lighting light);
const char *calendar_event(int day, bool en=false);
}
