// SPDX-License-Identifier: MIT
#pragma once
#include "../common/media/json.hpp"
#include <cstdio>
#include <cstring>

namespace fish::dsh {
struct Config {
    char url[257]{};
    char pet[65] = "main";
    char token[129]{};
};
inline bool valid(const Config &config) {
    const char *authority = nullptr;
    if (!std::strncmp(config.url, "http://", 7)) authority = config.url + 7;
    if (!std::strncmp(config.url, "https://", 8)) authority = config.url + 8;
    if (!authority || !*authority || *authority == '/' || !config.pet[0]) return false;
    const char *path = std::strchr(authority, '/');
    for (const char *p = config.url; *p; ++p)
        if ((unsigned char)*p <= 32 || (unsigned char)*p >= 127 ||
            *p == '?' || *p == '#' || *p == '\\' || *p == '"' ||
            (*p == '@' && (!path || p < path))) return false;
    for (const unsigned char *p = (const unsigned char *)config.pet; *p; ++p)
        if (*p < 32 || *p == 127) return false;
    for (const unsigned char *p = (const unsigned char *)config.token; *p; ++p)
        if (*p <= 32 || *p >= 127) return false;
    return true;
}
inline bool document(const uint8_t *data, size_t size) {
    if (!data || !size) return false;
    media::json::Reader reader{data, data + size};
    if (!reader.skip()) return false;
    reader.ws();
    return reader.p == reader.end;
}
inline bool parse_config(const uint8_t *data, size_t size, Config &out) {
    if (!document(data, size)) return false;
    media::json::Value value{data, size};
    Config next;
    bool truncated = false;
    if (!value.get("url").string(next.url, sizeof(next.url), &truncated) || truncated) return false;
    if (value.get("pet") && (!value.get("pet").string(next.pet, sizeof(next.pet), &truncated) || truncated)) return false;
    if (value.get("token") && (!value.get("token").string(next.token, sizeof(next.token), &truncated) || truncated)) return false;
    size_t n = std::strlen(next.url);
    while (n && next.url[n - 1] == '/') next.url[--n] = 0;
    if (!valid(next)) return false;
    out = next;
    return true;
}
inline bool quote(const char *text, char *out, size_t capacity) {
    constexpr char hex[] = "0123456789abcdef";
    size_t n = 0;
    if (capacity < 3) return false;
    out[n++] = '"';
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        size_t bytes = *p < 32 ? 6 : (*p == '"' || *p == '\\') ? 2 : 1;
        if (n + bytes + 2 > capacity) return false;
        if (*p < 32) {
            std::memcpy(out + n, "\\u00", 4); n += 4;
            out[n++] = hex[*p >> 4]; out[n++] = hex[*p & 15];
        } else {
            if (bytes == 2) out[n++] = '\\';
            out[n++] = char(*p);
        }
    }
    out[n++] = '"'; out[n] = 0;
    return true;
}
inline size_t encode_config(const Config &config, char *out, size_t capacity) {
    char url[520], pet[392], token[264];
    if (!valid(config) || !quote(config.url, url, sizeof(url)) ||
        !quote(config.pet, pet, sizeof(pet)) || !quote(config.token, token, sizeof(token))) return 0;
    int n = std::snprintf(out, capacity, "{\"url\":%s,\"pet\":%s,\"token\":%s}", url, pet, token);
    return n > 0 && size_t(n) < capacity ? size_t(n) : 0;
}
inline bool address(const Config &config, const char *endpoint, char *out, size_t capacity) {
    char pet[193];
    if (!valid(config) || !media::json::encode(config.pet, pet, sizeof(pet))) return false;
    char value[513];
    int n = std::snprintf(value, sizeof(value), "%s/%s?pet=%s&surface=any", config.url, endpoint, pet);
    if (n <= 0 || size_t(n) >= sizeof(value) || size_t(n) >= capacity) return false;
    std::memcpy(out, value, size_t(n) + 1);
    return true;
}
struct Reply {
    bool ok = false, truncated = false;
    char text[2049]{}, reason[80]{};
};
inline bool parse_reply(const uint8_t *data, size_t size, Reply &out) {
    if (!document(data, size)) return false;
    media::json::Value value{data, size};
    const auto ok = value.get("ok");
    if (!ok || (!ok.boolean() && (ok.n != 5 || std::memcmp(ok.p, "false", 5)))) return false;
    Reply reply;
    reply.ok = ok.boolean();
    if (reply.ok) {
        if (!value.get("text").string(reply.text, sizeof(reply.text), &reply.truncated) || !reply.text[0]) return false;
    } else {
        value.get("reason").string(reply.reason, sizeof(reply.reason));
        value.get("message").string(reply.text, sizeof(reply.text), &reply.truncated);
    }
    out = reply;
    return true;
}
inline uint64_t animation(const uint8_t *data, size_t size, const char *pet,
                          char *name, size_t capacity) {
    if (!document(data, size)) return 0;
    const auto leaf = media::json::Value{data, size}.get("pets").get(pet).get("anim");
    bool truncated = false;
    if (!leaf.get("data").get("name").string(name, capacity, &truncated) || truncated) return 0;
    return leaf.get("counter").number();
}
inline media::json::Value bundled_state(const uint8_t *data, size_t size, const char *pet) {
    if (!document(data, size)) return {};
    const auto state = media::json::Value{data, size}.get("pxa_state");
    const auto selected = state.get("pets").get(pet);
    return selected && selected.p[0] == '{' ? state : media::json::Value{};
}
} // namespace fish::dsh
