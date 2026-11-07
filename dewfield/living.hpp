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
int daylight(const State &s); // 0=day, 5=night; bounded palette blend
const char *calendar_event(int day, bool en=false);
}
