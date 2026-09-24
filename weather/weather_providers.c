#include "weather_providers.h"

static const char *const origins[WEATHER_PROVIDER_COUNT] = {
    "https://ipwho.is", "https://ipapi.co",
    "https://api.open-meteo.com", "https://wttr.in",
    "https://air-quality-api.open-meteo.com",
    "https://geocoding-api.open-meteo.com",
    "https://archive-api.open-meteo.com"
};

const char *weather_provider_origin(weather_provider_t provider) {
    return provider < WEATHER_PROVIDER_COUNT ? origins[provider] : NULL;
}

weather_provider_t weather_provider_backup(weather_provider_t provider) {
    if (provider == WEATHER_IPWHO) return WEATHER_IPAPI;
    if (provider == WEATHER_OPEN_METEO) return WEATHER_WTTR;
    return WEATHER_PROVIDER_COUNT;
}

static size_t text_size(const char *text) {
    size_t size = 0;
    while (text[size] != '\0') ++size;
    return size;
}

static int same_bytes(const uint8_t *bytes, const char *text, size_t size) {
    for (size_t index = 0; index < size; ++index)
        if (bytes[index] != (uint8_t)text[index]) return 0;
    return 1;
}

static int same_text(const char *left, const char *right) {
    size_t length = text_size(left);
    return length == text_size(right) &&
           same_bytes((const uint8_t *)left, right, length);
}

static int append(char *output, size_t capacity, size_t *offset,
                  const char *text) {
    while (*text != '\0') {
        if (*offset + 1u >= capacity) return 0;
        output[(*offset)++] = *text++;
    }
    output[*offset] = '\0';
    return 1;
}

static int coordinate(char *output, size_t capacity, size_t *offset,
                      int32_t value) {
    uint32_t magnitude = value < 0 ? (uint32_t)(-(value + 1)) + 1u :
                                     (uint32_t)value;
    uint32_t degrees = magnitude / 10000u;
    uint32_t fraction = magnitude % 10000u;
    char digits[4];
    size_t count = 0;
    if (value < 0 && !append(output, capacity, offset, "-")) return 0;
    do { digits[count++] = (char)('0' + degrees % 10u); degrees /= 10u; }
    while (degrees != 0);
    while (count != 0) {
        if (*offset + 1u >= capacity) return 0;
        output[(*offset)++] = digits[--count];
    }
    if (!append(output, capacity, offset, ".")) return 0;
    for (uint32_t divisor = 1000u; divisor != 0; divisor /= 10u) {
        if (*offset + 1u >= capacity) return 0;
        output[(*offset)++] = (char)('0' + (fraction / divisor) % 10u);
    }
    output[*offset] = '\0';
    return 1;
}

int weather_provider_url(weather_provider_t provider,
                         const weather_location_t *location,
                         char *url, size_t capacity) {
    size_t offset = 0;
    if (url == NULL || capacity == 0 || provider >= WEATHER_PROVIDER_COUNT)
        return 0;
    url[0] = '\0';
    if (provider == WEATHER_IPWHO || provider == WEATHER_IPAPI)
        return append(url, capacity, &offset, provider == WEATHER_IPWHO ?
                      "https://ipwho.is/" : "https://ipapi.co/json/");
    if (provider == WEATHER_GEOCODING || provider == WEATHER_HISTORY ||
            location == NULL ||
            location->latitude_e4 < -900000 ||
            location->latitude_e4 > 900000 ||
            location->longitude_e4 < -1800000 ||
            location->longitude_e4 > 1800000) return 0;
    if (!append(url, capacity, &offset,
            provider == WEATHER_OPEN_METEO ?
                "https://api.open-meteo.com/v1/forecast?latitude=" :
            provider == WEATHER_AIR_QUALITY ?
                "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" :
                "https://wttr.in/")) return 0;
    if (!coordinate(url, capacity, &offset, location->latitude_e4) ||
        !append(url, capacity, &offset, provider != WEATHER_WTTR ?
                "&longitude=" : ",") ||
        !coordinate(url, capacity, &offset, location->longitude_e4)) return 0;
    return append(url, capacity, &offset, provider == WEATHER_AIR_QUALITY ?
        "&current=pm2_5,european_aqi" : provider == WEATHER_OPEN_METEO ?
        "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
        "wind_speed_10m,weather_code,is_day"
        "&hourly=temperature_2m,precipitation_probability,weather_code"
        "&forecast_hours=8"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
        "precipitation_probability_max,uv_index_max,sunrise,sunset"
        "&forecast_days=7&timezone=auto" :
        "?m&format=%25t%7C%25C%7C%25h%7C%25w%7C%25f");
}

int weather_provider_search_url(const char *query, char *url, size_t capacity) {
    static const char hexadecimal[] = "0123456789ABCDEF";
    size_t offset = 0;
    size_t length;
    if (query == NULL || url == NULL || capacity == 0) return 0;
    length = text_size(query);
    if (length < 2u || length > 64u) return 0;
    url[0] = '\0';
    if (!append(url, capacity, &offset,
                "https://geocoding-api.open-meteo.com/v1/search?name="))
        return 0;
    for (size_t index = 0; index < length; ++index) {
        uint8_t byte = (uint8_t)query[index];
        if ((byte >= 'a' && byte <= 'z') ||
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') || byte == '-' || byte == '_') {
            if (offset + 1u >= capacity) return 0;
            url[offset++] = (char)byte;
        } else {
            if (offset + 3u >= capacity) return 0;
            url[offset++] = '%';
            url[offset++] = hexadecimal[byte >> 4];
            url[offset++] = hexadecimal[byte & 15u];
        }
    }
    url[offset] = '\0';
    return append(url, capacity, &offset,
                  "&count=4&language=zh&format=json");
}

static int json_field(const uint8_t *body, size_t size, const char *key,
                      size_t *value) {
    size_t key_size = text_size(key);
    for (size_t offset = 0; offset + key_size + 2u < size; ++offset) {
        size_t cursor;
        if (body[offset] != '"' || body[offset + key_size + 1u] != '"' ||
            !same_bytes(body + offset + 1u, key, key_size)) continue;
        cursor = offset + key_size + 2u;
        while (cursor < size && (body[cursor] == ' ' || body[cursor] == '\n'))
            ++cursor;
        if (cursor >= size || body[cursor++] != ':') continue;
        while (cursor < size && (body[cursor] == ' ' || body[cursor] == '\n'))
            ++cursor;
        *value = cursor;
        return 1;
    }
    return 0;
}

static int json_object(const uint8_t *body, size_t size, const char *key,
                       const uint8_t **object, size_t *object_size) {
    size_t value;
    if (!json_field(body, size, key, &value) || value >= size ||
        body[value] != '{') return 0;
    *object = body + value + 1u;
    for (size_t index = value + 1u; index < size; ++index) {
        if (body[index] == '}') {
            *object_size = index - value - 1u;
            return 1;
        }
    }
    return 0;
}

static int number(const uint8_t *body, size_t size, size_t offset,
                  int32_t scale, int32_t *output) {
    int sign = 1;
    int32_t whole = 0;
    int32_t fraction = 0;
    int32_t divisor = 1;
    int discarded = 0;
    if (offset < size && body[offset] == '-') { sign = -1; ++offset; }
    else if (offset < size && body[offset] == '+') ++offset;
    if (offset >= size || body[offset] < '0' || body[offset] > '9') return 0;
    while (offset < size && body[offset] >= '0' && body[offset] <= '9') {
        if (whole > 100000) return 0;
        whole = whole * 10 + (int32_t)(body[offset++] - '0');
    }
    if (offset < size && body[offset] == '.') {
        ++offset;
        if (offset >= size || body[offset] < '0' || body[offset] > '9') return 0;
        while (offset < size && body[offset] >= '0' && body[offset] <= '9') {
            if (divisor < 10000) {
                fraction = fraction * 10 + (int32_t)(body[offset] - '0');
                divisor *= 10;
            } else if (!discarded) {
                if (body[offset] >= '5') ++fraction;
                discarded = 1;
            }
            ++offset;
        }
    }
    if (offset < size && body[offset] != ',' && body[offset] != '}' &&
        body[offset] != ']' && body[offset] != '|' && body[offset] != ' ' &&
        body[offset] != '\n' && body[offset] != '\r' &&
        body[offset] != '%' && body[offset] != 0xc2u &&
        body[offset] != 'k') return 0;
    if (whole > 2000000 / scale) return 0;
    *output = sign * (whole * scale + (fraction * scale + divisor / 2) / divisor);
    return 1;
}

static int json_number(const uint8_t *body, size_t size, const char *key,
                       int array, int32_t scale, int32_t *output) {
    size_t value;
    if (!json_field(body, size, key, &value)) return 0;
    if (array) {
        if (value >= size || body[value++] != '[') return 0;
    }
    return number(body, size, value, scale, output);
}

static int json_string(const uint8_t *body, size_t size, const char *key,
                       char *output, size_t capacity) {
    size_t value;
    size_t length = 0;
    if (!json_field(body, size, key, &value) || value >= size ||
        body[value++] != '"' || capacity == 0) return 0;
    while (value < size && body[value] != '"') {
        uint8_t byte = body[value++];
        if (byte < 0x20u || length + 1u >= capacity) return 0;
        if (byte == '\\') {
            if (value >= size) return 0;
            byte = body[value++];
            if (byte != '"' && byte != '/' && byte != '\\') return 0;
        }
        output[length++] = (char)byte;
    }
    if (value >= size) return 0;
    output[length] = '\0';
    return length != 0;
}

int weather_provider_location(weather_provider_t provider, const uint8_t *body,
                              size_t size, weather_location_t *location) {
    weather_location_t parsed = {0};
    if (body == NULL || location == NULL ||
        (provider != WEATHER_IPWHO && provider != WEATHER_IPAPI)) return 0;
    if (!json_number(body, size, "latitude", 0, 10000, &parsed.latitude_e4) ||
        !json_number(body, size, "longitude", 0, 10000, &parsed.longitude_e4) ||
        !json_string(body, size, "city", parsed.city, sizeof(parsed.city)) ||
        parsed.latitude_e4 < -900000 || parsed.latitude_e4 > 900000 ||
        parsed.longitude_e4 < -1800000 || parsed.longitude_e4 > 1800000)
        return 0;
    *location = parsed;
    return 1;
}

static int contains_word(const char *text, const char *word) {
    size_t word_size = text_size(word);
    for (size_t offset = 0; text[offset] != '\0'; ++offset) {
        size_t index = 0;
        while (index < word_size && text[offset + index] != '\0') {
            char letter = text[offset + index];
            if (letter >= 'A' && letter <= 'Z') letter += 'a' - 'A';
            if (letter != word[index]) break;
            ++index;
        }
        if (index == word_size) return 1;
    }
    return 0;
}

int weather_provider_conditions(weather_provider_t provider, const uint8_t *body,
                                size_t size, weather_conditions_t *conditions) {
    weather_conditions_t parsed = {.humidity = -1, .wind_speed = -1, .is_day = 1};
    const uint8_t *current;
    const uint8_t *daily;
    size_t current_size;
    size_t daily_size;
    int32_t value;
    if (body == NULL || conditions == NULL) return 0;
    if (provider == WEATHER_OPEN_METEO) {
        if (!json_object(body, size, "current", &current, &current_size) ||
            !json_number(current, current_size, "temperature_2m", 0, 1,
                         &parsed.temperature) ||
            !json_number(current, current_size, "weather_code", 0, 1,
                         &parsed.code) ||
            parsed.code < 0 || parsed.code > 99 ||
            !json_number(current, current_size, "is_day", 0, 1, &value) ||
            !json_string(current, current_size, "time", parsed.updated,
                         sizeof(parsed.updated))) return 0;
        parsed.is_day = (uint8_t)(value != 0);
        parsed.has_apparent = (uint8_t)json_number(current, current_size,
            "apparent_temperature", 0, 1, &parsed.apparent_temperature);
        if (json_number(current, current_size, "relative_humidity_2m", 0, 1,
                        &value) &&
            value >= 0 && value <= 100) parsed.humidity = value;
        if (json_number(current, current_size, "wind_speed_10m", 0, 1,
                        &value) && value >= 0)
            parsed.wind_speed = value;
        if (json_object(body, size, "daily", &daily, &daily_size))
            parsed.has_range = (uint8_t)(json_number(daily, daily_size,
            "temperature_2m_max", 1, 1, &parsed.high_temperature) &&
            json_number(daily, daily_size, "temperature_2m_min", 1, 1,
                        &parsed.low_temperature));
    } else if (provider == WEATHER_WTTR) {
        char fields[5][80] = {{0}};
        size_t field = 0;
        size_t length = 0;
        if (size == 0 || size > 300) return 0;
        for (size_t index = 0; index < size; ++index) {
            if (body[index] == '|') {
                if (++field >= 5u) return 0;
                length = 0;
            } else if (body[index] != '\r' && body[index] != '\n') {
                if (length + 1u >= sizeof(fields[0])) return 0;
                fields[field][length++] = (char)body[index];
            }
        }
        if (field != 4u || !number((const uint8_t *)fields[0],
                                   text_size(fields[0]), 0, 1,
                                   &parsed.temperature)) return 0;
        parsed.has_apparent = (uint8_t)number((const uint8_t *)fields[4],
                                               text_size(fields[4]), 0, 1,
                                               &parsed.apparent_temperature);
        if (number((const uint8_t *)fields[2], text_size(fields[2]), 0, 1,
                   &value) && value >= 0 && value <= 100) parsed.humidity = value;
        for (size_t index = 0; fields[3][index] != '\0'; ++index) {
            if (fields[3][index] >= '0' && fields[3][index] <= '9') {
                if (number((const uint8_t *)fields[3], text_size(fields[3]),
                           index, 1, &value)) parsed.wind_speed = value;
                break;
            }
        }
        if (fields[1][0] == '\0') return 0;
        parsed.code = contains_word(fields[1], "thunder") ? 95 :
                      contains_word(fields[1], "snow") ? 71 :
                      contains_word(fields[1], "rain") ||
                      contains_word(fields[1], "drizzle") ? 61 :
                      contains_word(fields[1], "fog") ||
                      contains_word(fields[1], "mist") ? 45 :
                      contains_word(fields[1], "cloud") ||
                      contains_word(fields[1], "overcast") ? 3 :
                      contains_word(fields[1], "clear") ||
                      contains_word(fields[1], "sunny") ? 0 : -1;
        if (parsed.code < 0) return 0;
    } else {
        return 0;
    }
    *conditions = parsed;
    return 1;
}

int weather_provider_air(const uint8_t *body, size_t size, weather_air_t *air) {
    const uint8_t *current;
    size_t current_size;
    weather_air_t parsed;
    if (body == NULL || air == NULL ||
        !json_object(body, size, "current", &current, &current_size) ||
        !json_number(current, current_size, "european_aqi", 0, 1,
                     &parsed.european_aqi) ||
        !json_number(current, current_size, "pm2_5", 0, 10,
                     &parsed.pm25_tenths) ||
        parsed.european_aqi < 0 || parsed.pm25_tenths < 0) return 0;
    *air = parsed;
    return 1;
}

static size_t json_number_array(const uint8_t *body, size_t size,
                                const char *key, int32_t *values,
                                size_t capacity, int32_t scale) {
    size_t offset;
    size_t count = 0;
    if (!json_field(body, size, key, &offset) || offset >= size ||
        body[offset++] != '[') return 0;
    while (offset < size && count < capacity) {
        while (offset < size && (body[offset] == ' ' || body[offset] == '\n'))
            ++offset;
        if (offset >= size || body[offset] == ']') break;
        if (!number(body, size, offset, scale, &values[count])) return 0;
        ++count;
        while (offset < size && body[offset] != ',' && body[offset] != ']')
            ++offset;
        if (offset < size && body[offset] == ',') ++offset;
    }
    return count;
}

static size_t json_time_array(const uint8_t *body, size_t size,
                              const char *key, char values[][17],
                              size_t capacity) {
    size_t offset;
    size_t count = 0;
    if (!json_field(body, size, key, &offset) || offset >= size ||
        body[offset++] != '[') return 0;
    while (offset < size && count < capacity) {
        size_t length = 0;
        while (offset < size && (body[offset] == ' ' || body[offset] == '\n'))
            ++offset;
        if (offset >= size || body[offset] == ']') break;
        if (body[offset] != '"') return 0;
        ++offset;
        while (offset < size && body[offset] != '"' && length < 16u)
            values[count][length++] = (char)body[offset++];
        if (offset >= size || body[offset++] != '"' || length < 10u) return 0;
        values[count++][length] = '\0';
        if (offset < size && body[offset] == ',') ++offset;
    }
    return count;
}

int weather_provider_forecast(const uint8_t *body, size_t size,
                              weather_forecast_t *forecast) {
    const uint8_t *hourly;
    const uint8_t *daily;
    size_t hourly_size;
    size_t daily_size;
    char hour_times[WEATHER_HOURLY_COUNT][17] = {{0}};
    char day_times[WEATHER_DAILY_COUNT][17] = {{0}};
    char sunrise[1][17] = {{0}};
    char sunset[1][17] = {{0}};
    int32_t hour_temp[WEATHER_HOURLY_COUNT];
    int32_t hour_rain[WEATHER_HOURLY_COUNT];
    int32_t hour_code[WEATHER_HOURLY_COUNT];
    int32_t day_high[WEATHER_DAILY_COUNT];
    int32_t day_low[WEATHER_DAILY_COUNT];
    int32_t day_code[WEATHER_DAILY_COUNT];
    int32_t day_rain[WEATHER_DAILY_COUNT];
    weather_forecast_t parsed = {0};
    size_t hours;
    size_t days;
    if (body == NULL || forecast == NULL ||
        !json_object(body, size, "hourly", &hourly, &hourly_size) ||
        !json_object(body, size, "daily", &daily, &daily_size)) return 0;
    hours = json_time_array(hourly, hourly_size, "time", hour_times,
                            WEATHER_HOURLY_COUNT);
    days = json_time_array(daily, daily_size, "time", day_times,
                           WEATHER_DAILY_COUNT);
    if (hours == 0 || days == 0 ||
        json_number_array(hourly, hourly_size, "temperature_2m", hour_temp,
                          hours, 1) != hours ||
        json_number_array(hourly, hourly_size, "precipitation_probability",
                          hour_rain, hours, 1) != hours ||
        json_number_array(hourly, hourly_size, "weather_code", hour_code,
                          hours, 1) != hours ||
        json_number_array(daily, daily_size, "temperature_2m_max", day_high,
                          days, 1) != days ||
        json_number_array(daily, daily_size, "temperature_2m_min", day_low,
                          days, 1) != days ||
        json_number_array(daily, daily_size, "weather_code", day_code,
                          days, 1) != days ||
        json_number_array(daily, daily_size, "precipitation_probability_max",
                          day_rain, days, 1) != days) return 0;
    for (size_t index = 0; index < hours; ++index) {
        if (hour_rain[index] < 0 || hour_rain[index] > 100 ||
            text_size(hour_times[index]) != 16u ||
            hour_times[index][10] != 'T') return 0;
        for (size_t character = 0; character < 17u; ++character)
            parsed.hours[index].time[character] = hour_times[index][character];
        parsed.hours[index].temperature = hour_temp[index];
        parsed.hours[index].rain_chance = hour_rain[index];
        parsed.hours[index].code = hour_code[index];
    }
    for (size_t index = 0; index < days; ++index) {
        if (day_rain[index] < 0 || day_rain[index] > 100 ||
            text_size(day_times[index]) != 10u) return 0;
        for (size_t character = 0; character < 10u; ++character)
            parsed.days[index].date[character] = day_times[index][character];
        parsed.days[index].high = day_high[index];
        parsed.days[index].low = day_low[index];
        parsed.days[index].code = day_code[index];
        parsed.days[index].precipitation = day_rain[index];
    }
    parsed.hour_count = hours;
    parsed.day_count = days;
    parsed.uv_max = -1;
    (void)json_number(daily, daily_size, "uv_index_max", 1, 1,
                      &parsed.uv_max);
    if (json_time_array(daily, daily_size, "sunrise", sunrise, 1u) == 1u &&
        json_time_array(daily, daily_size, "sunset", sunset, 1u) == 1u) {
        for (size_t character = 0; character < 17u; ++character) {
            parsed.sunrise[character] = sunrise[0][character];
            parsed.sunset[character] = sunset[0][character];
        }
    }
    *forecast = parsed;
    return 1;
}

static int month_days(int year, int month) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    return days[month - 1] + (month == 2 &&
        (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)));
}

static int previous_day(int *year, int *month, int *day) {
    if (--*day != 0) return 1;
    if (--*month == 0) {
        *month = 12;
        if (--*year < 2000) return 0;
    }
    *day = month_days(*year, *month);
    return 1;
}

static void write_date(char output[11], int year, int month, int day) {
    output[0] = (char)('0' + year / 1000);
    output[1] = (char)('0' + year / 100 % 10);
    output[2] = (char)('0' + year / 10 % 10);
    output[3] = (char)('0' + year % 10);
    output[4] = '-';
    output[5] = (char)('0' + month / 10);
    output[6] = (char)('0' + month % 10);
    output[7] = '-';
    output[8] = (char)('0' + day / 10);
    output[9] = (char)('0' + day % 10);
    output[10] = '\0';
}

int weather_provider_history_url(const weather_location_t *location,
                                 const char *local_date, char *url,
                                 size_t capacity) {
    size_t offset = 0;
    int year;
    int month;
    int day;
    char start[11];
    char end[11];
    if (location == NULL || local_date == NULL || url == NULL || capacity == 0 ||
        text_size(local_date) < 10u || local_date[4] != '-' ||
        local_date[7] != '-' || location->latitude_e4 < -900000 ||
        location->latitude_e4 > 900000 ||
        location->longitude_e4 < -1800000 ||
        location->longitude_e4 > 1800000) return 0;
    for (size_t index = 0; index < 10u; ++index) {
        if (index != 4u && index != 7u &&
            (local_date[index] < '0' || local_date[index] > '9')) return 0;
    }
    year = (local_date[0] - '0') * 1000 + (local_date[1] - '0') * 100 +
           (local_date[2] - '0') * 10 + local_date[3] - '0';
    month = (local_date[5] - '0') * 10 + local_date[6] - '0';
    day = (local_date[8] - '0') * 10 + local_date[9] - '0';
    if (year < 2000 || year > 2099 || day < 1 ||
        day > month_days(year, month)) return 0;
    for (size_t index = 0; index < 5u; ++index)
        if (!previous_day(&year, &month, &day)) return 0;
    write_date(end, year, month, day);
    for (size_t index = 0; index < 6u; ++index)
        if (!previous_day(&year, &month, &day)) return 0;
    write_date(start, year, month, day);
    url[0] = '\0';
    return append(url, capacity, &offset,
                  "https://archive-api.open-meteo.com/v1/archive?latitude=") &&
           coordinate(url, capacity, &offset, location->latitude_e4) &&
           append(url, capacity, &offset, "&longitude=") &&
           coordinate(url, capacity, &offset, location->longitude_e4) &&
           append(url, capacity, &offset, "&start_date=") &&
           append(url, capacity, &offset, start) &&
           append(url, capacity, &offset, "&end_date=") &&
           append(url, capacity, &offset, end) &&
           append(url, capacity, &offset,
                  "&daily=weather_code,temperature_2m_max,"
                  "temperature_2m_min,precipitation_sum&timezone=auto");
}

size_t weather_provider_history(const uint8_t *body, size_t size,
                                weather_day_t *days, size_t capacity) {
    const uint8_t *daily;
    size_t daily_size;
    char dates[WEATHER_DAILY_COUNT][17] = {{0}};
    int32_t high[WEATHER_DAILY_COUNT];
    int32_t low[WEATHER_DAILY_COUNT];
    int32_t code[WEATHER_DAILY_COUNT];
    int32_t rain[WEATHER_DAILY_COUNT];
    size_t count;
    if (body == NULL || days == NULL || capacity == 0 ||
        !json_object(body, size, "daily", &daily, &daily_size)) return 0;
    if (capacity > WEATHER_DAILY_COUNT) capacity = WEATHER_DAILY_COUNT;
    count = json_time_array(daily, daily_size, "time", dates, capacity);
    if (count == 0 ||
        json_number_array(daily, daily_size, "temperature_2m_max", high,
                          count, 1) != count ||
        json_number_array(daily, daily_size, "temperature_2m_min", low,
                          count, 1) != count ||
        json_number_array(daily, daily_size, "weather_code", code,
                          count, 1) != count ||
        json_number_array(daily, daily_size, "precipitation_sum", rain,
                          count, 10) != count) return 0;
    for (size_t index = 0; index < count; ++index) {
        if (rain[index] < 0 || text_size(dates[index]) != 10u) return 0;
        for (size_t character = 0; character < 10u; ++character)
            days[index].date[character] = dates[index][character];
        days[index].date[10] = '\0';
        days[index].high = high[index];
        days[index].low = low[index];
        days[index].code = code[index];
        days[index].precipitation = rain[index];
    }
    return count;
}

size_t weather_provider_search_results(const uint8_t *body, size_t size,
                                       weather_city_result_t *results,
                                       size_t capacity) {
    size_t start;
    size_t count = 0;
    if (body == NULL || results == NULL || capacity == 0 ||
        !json_field(body, size, "results", &start) || start >= size ||
        body[start] != '[') return 0;
    for (size_t offset = start + 1u; offset < size && count < capacity; ++offset) {
        weather_city_result_t candidate = {0};
        char region[64] = {0};
        char country[64] = {0};
        size_t object_start;
        size_t object_size;
        size_t label_size = 0;
        if (body[offset] == ']') break;
        if (body[offset] != '{') continue;
        object_start = offset + 1u;
        while (offset < size && body[offset] != '}') ++offset;
        if (offset >= size) break;
        object_size = offset - object_start;
        if (!json_string(body + object_start, object_size, "name",
                         candidate.location.city,
                         sizeof(candidate.location.city)) ||
            !json_number(body + object_start, object_size, "latitude", 0,
                         10000, &candidate.location.latitude_e4) ||
            !json_number(body + object_start, object_size, "longitude", 0,
                         10000, &candidate.location.longitude_e4) ||
            candidate.location.latitude_e4 < -900000 ||
            candidate.location.latitude_e4 > 900000 ||
            candidate.location.longitude_e4 < -1800000 ||
            candidate.location.longitude_e4 > 1800000) continue;
        (void)json_string(body + object_start, object_size, "admin1",
                          region, sizeof(region));
        (void)json_string(body + object_start, object_size, "country",
                          country, sizeof(country));
        candidate.label[0] = '\0';
        if (!append(candidate.label, sizeof(candidate.label), &label_size,
                    candidate.location.city) ||
            (region[0] != '\0' &&
             !same_text(region, candidate.location.city) &&
             (!append(candidate.label, sizeof(candidate.label), &label_size,
                      " · ") ||
              !append(candidate.label, sizeof(candidate.label), &label_size,
                      region))) ||
            (country[0] != '\0' &&
             (!append(candidate.label, sizeof(candidate.label), &label_size,
                      " · ") ||
              !append(candidate.label, sizeof(candidate.label), &label_size,
                      country)))) continue;
        if (text_size(candidate.label) < sizeof(candidate.location.city)) {
            for (size_t index = 0; index <= text_size(candidate.label); ++index)
                candidate.location.city[index] = candidate.label[index];
        }
        results[count++] = candidate;
    }
    return count;
}

size_t weather_location_encode(const weather_location_t *location,
                               uint8_t *bytes, size_t capacity) {
    size_t length;
    if (location == NULL || bytes == NULL ||
        location->latitude_e4 < -900000 || location->latitude_e4 > 900000 ||
        location->longitude_e4 < -1800000 || location->longitude_e4 > 1800000)
        return 0;
    length = text_size(location->city);
    if (length == 0 || length >= sizeof(location->city) ||
        capacity < length + 9u) return 0;
    for (size_t index = 0; index < 4u; ++index) {
        bytes[index] = (uint8_t)((uint32_t)location->latitude_e4 >> (8u * index));
        bytes[index + 4u] = (uint8_t)((uint32_t)location->longitude_e4 >>
                                      (8u * index));
    }
    bytes[8] = (uint8_t)length;
    for (size_t index = 0; index < length; ++index)
        bytes[9u + index] = (uint8_t)location->city[index];
    return length + 9u;
}

int weather_location_decode(const uint8_t *bytes, size_t size,
                            weather_location_t *location) {
    weather_location_t parsed = {0};
    if (bytes == NULL || location == NULL || size < 10u ||
        bytes[8] == 0 || bytes[8] >= sizeof(parsed.city) ||
        size != (size_t)bytes[8] + 9u) return 0;
    for (size_t index = 0; index < 4u; ++index) {
        parsed.latitude_e4 = (int32_t)((uint32_t)parsed.latitude_e4 |
                                ((uint32_t)bytes[index] << (8u * index)));
        parsed.longitude_e4 = (int32_t)((uint32_t)parsed.longitude_e4 |
                                ((uint32_t)bytes[index + 4u] << (8u * index)));
    }
    if (parsed.latitude_e4 < -900000 || parsed.latitude_e4 > 900000 ||
        parsed.longitude_e4 < -1800000 || parsed.longitude_e4 > 1800000)
        return 0;
    for (size_t index = 0; index < bytes[8]; ++index) {
        if (bytes[index + 9u] < 0x20u) return 0;
        parsed.city[index] = (char)bytes[index + 9u];
    }
    *location = parsed;
    return 1;
}
