#ifndef WEATHER_PROVIDERS_H
#define WEATHER_PROVIDERS_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    WEATHER_IPWHO,
    WEATHER_IPAPI,
    WEATHER_OPEN_METEO,
    WEATHER_WTTR,
    WEATHER_AIR_QUALITY,
    WEATHER_GEOCODING,
    WEATHER_HISTORY,
    WEATHER_PROVIDER_COUNT
} weather_provider_t;

typedef struct {
    int32_t latitude_e4;
    int32_t longitude_e4;
    char city[64];
} weather_location_t;

typedef struct {
    int32_t temperature;
    int32_t apparent_temperature;
    int32_t humidity;
    int32_t wind_speed;
    int32_t high_temperature;
    int32_t low_temperature;
    int32_t code;
    uint8_t has_apparent;
    uint8_t has_range;
    uint8_t is_day;
    char updated[64];
} weather_conditions_t;

typedef struct {
    int32_t european_aqi;
    int32_t pm25_tenths;
} weather_air_t;

#define WEATHER_CITY_RESULTS 4u
#define WEATHER_HOURLY_COUNT 8u
#define WEATHER_DAILY_COUNT 7u

typedef struct {
    weather_location_t location;
    char label[128];
} weather_city_result_t;

typedef struct {
    char time[17];
    int32_t temperature;
    int32_t code;
    int32_t rain_chance;
} weather_hour_t;

typedef struct {
    char date[11];
    int32_t low;
    int32_t high;
    int32_t code;
    int32_t precipitation;
} weather_day_t;

typedef struct {
    weather_hour_t hours[WEATHER_HOURLY_COUNT];
    weather_day_t days[WEATHER_DAILY_COUNT];
    size_t hour_count;
    size_t day_count;
    int32_t uv_max;
    char sunrise[17];
    char sunset[17];
} weather_forecast_t;

const char *weather_provider_origin(weather_provider_t provider);
weather_provider_t weather_provider_backup(weather_provider_t provider);
int weather_provider_url(weather_provider_t provider,
                         const weather_location_t *location,
                         char *url, size_t capacity);
int weather_provider_location(weather_provider_t provider, const uint8_t *body,
                              size_t size, weather_location_t *location);
int weather_provider_conditions(weather_provider_t provider, const uint8_t *body,
                                size_t size, weather_conditions_t *conditions);
int weather_provider_air(const uint8_t *body, size_t size, weather_air_t *air);
int weather_provider_forecast(const uint8_t *body, size_t size,
                              weather_forecast_t *forecast);
int weather_provider_history_url(const weather_location_t *location,
                                 const char *local_date, char *url,
                                 size_t capacity);
size_t weather_provider_history(const uint8_t *body, size_t size,
                                weather_day_t *days, size_t capacity);
int weather_provider_search_url(const char *query, char *url, size_t capacity);
size_t weather_provider_search_results(const uint8_t *body, size_t size,
                                       weather_city_result_t *results,
                                       size_t capacity);
size_t weather_location_encode(const weather_location_t *location,
                               uint8_t *bytes, size_t capacity);
int weather_location_decode(const uint8_t *bytes, size_t size,
                            weather_location_t *location);

#endif
