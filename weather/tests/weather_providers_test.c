#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../weather_providers.h"

static void location_test(void) {
    static const char who[] =
        "{\"success\":true,\"city\":\"Beijing\",\"latitude\":39.9042,"
        "\"longitude\":116.4074}";
    static const char api[] =
        "{\"city\":\"London\",\"latitude\":51.5072,\"longitude\":-0.1276}";
    static const char bad[] =
        "{\"city\":\"Wrong\",\"latitude\":91,\"longitude\":100}";
    weather_location_t location;
    assert(weather_provider_location(WEATHER_IPWHO, (const uint8_t *)who,
                                     sizeof(who) - 1u, &location));
    assert(strcmp(location.city, "Beijing") == 0);
    assert(location.latitude_e4 == 399042);
    assert(location.longitude_e4 == 1164074);
    assert(weather_provider_location(WEATHER_IPAPI, (const uint8_t *)api,
                                     sizeof(api) - 1u, &location));
    assert(location.longitude_e4 == -1276);
    assert(!weather_provider_location(WEATHER_IPWHO, (const uint8_t *)bad,
                                      sizeof(bad) - 1u, &location));
}

static void conditions_test(void) {
    static const char meteo[] =
        "{\"current_units\":{\"time\":\"iso8601\","
        "\"temperature_2m\":\"°C\"},\"current\":{"
        "\"time\":\"2026-09-24T17:00\",\"temperature_2m\":21.1,"
        "\"relative_humidity_2m\":71,\"apparent_temperature\":22.3,"
        "\"wind_speed_10m\":4.7,\"weather_code\":2,\"is_day\":1},"
        "\"daily\":{\"temperature_2m_max\":[26.5],"
        "\"temperature_2m_min\":[19.7]}}";
    static const char wttr[] = "+22°C|Light rain |66%|↑10km/h|+21°C";
    static const char invalid[] = "{\"current\":{\"time\":\"x\"}}";
    static const char air[] =
        "{\"current_units\":{\"pm2_5\":\"µg/m³\"},"
        "\"current\":{\"pm2_5\":48.8,\"european_aqi\":60}}";
    weather_conditions_t conditions;
    weather_air_t quality;
    assert(weather_provider_conditions(WEATHER_OPEN_METEO,
        (const uint8_t *)meteo, sizeof(meteo) - 1u, &conditions));
    assert(conditions.temperature == 21 && conditions.apparent_temperature == 22);
    assert(conditions.humidity == 71 && conditions.wind_speed == 5);
    assert(conditions.high_temperature == 27 && conditions.low_temperature == 20);
    assert(strcmp(conditions.updated, "2026-09-24T17:00") == 0);
    assert(!weather_provider_conditions(WEATHER_OPEN_METEO,
        (const uint8_t *)invalid, sizeof(invalid) - 1u, &conditions));
    assert(weather_provider_conditions(WEATHER_WTTR, (const uint8_t *)wttr,
                                       sizeof(wttr) - 1u, &conditions));
    assert(conditions.temperature == 22 && conditions.code == 61);
    assert(conditions.humidity == 66 && conditions.wind_speed == 10);
    assert(!conditions.has_range);
    assert(weather_provider_air((const uint8_t *)air, sizeof(air) - 1u,
                                &quality));
    assert(quality.pm25_tenths == 488 && quality.european_aqi == 60);
}

static void urls_test(void) {
    weather_location_t location = {.latitude_e4 = 399042,
                                    .longitude_e4 = 1164074};
    char url[512];
    char short_url[16];
    assert(weather_provider_url(WEATHER_IPWHO, NULL, url, sizeof(url)));
    assert(strcmp(url, "https://ipwho.is/") == 0);
    assert(weather_provider_url(WEATHER_OPEN_METEO, &location,
                                url, sizeof(url)));
    assert(strstr(url, "latitude=39.9042&longitude=116.4074") != NULL);
    assert(weather_provider_url(WEATHER_WTTR, &location, url, sizeof(url)));
    assert(strstr(url, "39.9042,116.4074?m&format=%25t%7C") != NULL);
    assert(weather_provider_url(WEATHER_AIR_QUALITY, &location,
                                url, sizeof(url)));
    assert(strstr(url, "&current=pm2_5,european_aqi") != NULL);
    assert(!weather_provider_url(WEATHER_OPEN_METEO, &location,
                                 short_url, sizeof(short_url)));
    assert(weather_provider_backup(WEATHER_IPWHO) == WEATHER_IPAPI);
    assert(weather_provider_backup(WEATHER_OPEN_METEO) == WEATHER_WTTR);
    assert(weather_provider_backup(WEATHER_WTTR) == WEATHER_PROVIDER_COUNT);
}

static void forecast_history_test(void) {
    static const char forecast_body[] =
        "{\"current\":{\"time\":\"2026-09-24T18:30\"},"
        "\"hourly\":{\"time\":[\"2026-09-24T18:00\",\"2026-09-24T19:00\"],"
        "\"temperature_2m\":[20.5,19.4],"
        "\"precipitation_probability\":[44,60],\"weather_code\":[2,61]},"
        "\"daily\":{\"time\":[\"2026-09-24\",\"2026-09-25\"],"
        "\"weather_code\":[2,61],\"temperature_2m_max\":[29.3,26.1],"
        "\"temperature_2m_min\":[17.3,16.5],"
        "\"precipitation_probability_max\":[44,80],"
        "\"uv_index_max\":[5.3,3.1],"
        "\"sunrise\":[\"2026-09-24T06:02\",\"2026-09-25T06:03\"],"
        "\"sunset\":[\"2026-09-24T18:09\",\"2026-09-25T18:08\"]}}";
    static const char history_body[] =
        "{\"daily\":{\"time\":[\"2026-09-13\",\"2026-09-14\"],"
        "\"weather_code\":[3,51],\"temperature_2m_max\":[31.5,28.2],"
        "\"temperature_2m_min\":[19.4,21.2],"
        "\"precipitation_sum\":[0.00,1.10]}}";
    weather_location_t location = {.latitude_e4 = 399042,
                                   .longitude_e4 = 1164074};
    weather_forecast_t forecast;
    weather_day_t days[WEATHER_DAILY_COUNT];
    char url[512];
    assert(weather_provider_forecast((const uint8_t *)forecast_body,
                                     sizeof(forecast_body) - 1u, &forecast));
    assert(forecast.hour_count == 2u && forecast.day_count == 2u);
    assert(forecast.hours[1].rain_chance == 60);
    assert(forecast.days[1].high == 26 && forecast.days[1].precipitation == 80);
    assert(forecast.uv_max == 5);
    assert(strcmp(forecast.sunrise, "2026-09-24T06:02") == 0);
    assert(weather_provider_history_url(&location, "2026-09-24T18:30",
                                         url, sizeof(url)));
    assert(strstr(url, "start_date=2026-09-13&end_date=2026-09-19") != NULL);
    assert(weather_provider_history_url(&location, "2024-03-05", url,
                                         sizeof(url)));
    assert(strstr(url, "start_date=2024-02-23&end_date=2024-02-29") != NULL);
    assert(!weather_provider_history_url(&location, "2026-02-30", url,
                                          sizeof(url)));
    assert(weather_provider_history((const uint8_t *)history_body,
        sizeof(history_body) - 1u, days, WEATHER_DAILY_COUNT) == 2u);
    assert(days[1].precipitation == 11);
    assert(strcmp(days[1].date, "2026-09-14") == 0);
}

static void city_search_test(void) {
    static const char response[] =
        "{\"results\":[{\"name\":\"北京\",\"latitude\":39.9075,"
        "\"longitude\":116.39723,\"country\":\"中国\","
        "\"admin1\":\"北京市\"},{\"name\":\"Beijing\","
        "\"latitude\":35.20917,\"longitude\":110.73278,"
        "\"country\":\"中国\",\"admin1\":\"山西\"}]}";
    weather_city_result_t results[WEATHER_CITY_RESULTS];
    weather_location_t restored;
    uint8_t saved[73];
    char url[384];
    size_t size;
    assert(weather_provider_search_url("北京", url, sizeof(url)));
    assert(strstr(url, "name=%E5%8C%97%E4%BA%AC&count=4") != NULL);
    assert(!weather_provider_search_url("", url, sizeof(url)));
    assert(weather_provider_search_results((const uint8_t *)response,
        sizeof(response) - 1u, results, WEATHER_CITY_RESULTS) == 2u);
    assert(results[0].location.latitude_e4 == 399075);
    assert(results[1].location.latitude_e4 == 352092);
    assert(strstr(results[0].label, "北京市") != NULL);
    assert(strstr(results[1].label, "山西") != NULL);
    size = weather_location_encode(&results[0].location, saved, sizeof(saved));
    assert(size != 0);
    assert(weather_location_decode(saved, size, &restored));
    assert(restored.latitude_e4 == 399075);
    assert(strcmp(restored.city, results[0].location.city) == 0);
    assert(!weather_location_decode(saved, size - 1u, &restored));
}

int main(void) {
    location_test();
    conditions_test();
    urls_test();
    city_search_test();
    forecast_history_test();
    return 0;
}
