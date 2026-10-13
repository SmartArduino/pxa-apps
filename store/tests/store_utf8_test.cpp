#include "store_json.h"
#include <cassert>
#include <cstring>
#include <string>

static bool parse(const std::string &value, store_app_t &app) {
    const std::string json = "{\"payload\":{\"item\":{\"app_id\":\"demo\",\"summary\":\"" + value + "\"}}}";
    return store_json_parse_app_detail(reinterpret_cast<const uint8_t *>(json.data()), json.size(), &app);
}

int main() {
    store_app_t app{};
    std::string raw, escaped;
    for (int i = 0; i < 40; ++i) { raw += "中"; escaped += "\\u4e2d"; }
    assert(parse(raw + "Z", app));
    assert(std::strlen(app.summary) == 117);
    assert(raw.substr(0, 117) == app.summary);
    assert(parse(escaped + "Z", app));
    assert(raw.substr(0, 117) == app.summary);
    assert(parse(std::string(117, 'a') + "😀Z", app));
    assert(std::string(117, 'a') == app.summary);
    assert(parse("\\ud83d\\ude00", app));
    assert(std::strcmp(app.summary, "😀") == 0);
    for (const auto *invalid : {"\\udc00", "\\u0000", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe4\xb8"})
        assert(!parse(invalid, app));
}
