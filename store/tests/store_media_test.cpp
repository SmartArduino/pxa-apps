#include "store_json.h"
#include "store_media.hpp"
#include <cassert>
#include <cstring>
#include <string>

int main() {
    char url[320];
    assert(store::media_url("/media/apps/5/53?width=64&format=png", url, sizeof(url)));
    assert(std::string(url) == "https://app.doit.am/media/apps/5/53?width=64&format=png");
    assert(store::media_url(url, url, sizeof(url)));
    for (auto path : {"https://evil.test/media/apps/5/53", "//evil.test/media/apps/a",
         "/api/v2/artifacts/1/download", "/media/apps/../private", "/media/apps/%2e%2e/private",
         "/media/apps/a\\b", "/media/apps/a\nb", "/media/apps/a#b"})
        assert(!store::media_url(path, url, sizeof(url)));
    assert(!store::media_url("/media/apps/1/2", url, 8));
    store_app_t app{};
    store_gallery_t gallery{};
    auto parse = [&](std::string_view item) {
        const std::string body = "{\"payload\":{\"item\":" + std::string(item) + "}}";
        return store_json_parse_app_detail(reinterpret_cast<const uint8_t *>(body.data()), body.size(), &app, &gallery);
    };
    assert(parse(R"({"app_id":"demo","icon_url":null,"screenshots":[]})"));
    assert(!app.icon_url[0] && !gallery.count);
    assert(parse(R"({"app_id":"demo","icon_url":"/media/apps/1/2?width=64","screenshots":[{"url":"/media/apps/1/3","width":320,"height":240,"future":{"key":true}}]})"));
    assert(gallery.count == 1 && gallery.screenshots[0].width == 320 && gallery.screenshots[0].height == 240);
    auto before = app;
    auto gallery_before = gallery;
    assert(!parse(R"({"app_id":"bad","screenshots":[{"url":"broken"})"));
    assert(std::memcmp(&before, &app, sizeof(app)) == 0);
    assert(std::memcmp(&gallery_before, &gallery, sizeof(gallery)) == 0);
    assert(!parse(R"({"app_id":"bad","screenshots":[{"url":"\udc00"}]})"));
    std::string many = R"({"app_id":"many","screenshots":[)";
    for (unsigned i = 0; i < 12; ++i) {
        if (i) many += ',';
        many += R"({"url":"/media/apps/1/3","width":320,"height":240})";
    }
    assert(parse(many + "]}"));
    assert(gallery.count == STORE_MAX_SCREENSHOTS);
    assert(parse("{\"icon_url\":\"" + std::string(300, 'a') + "\",\"screenshots\":[]}"));
    assert(!app.icon_url[0]);
    // A page can overwrite several entries; preserve the exact displaced count.
    store_catalog_t catalog{};
    for (unsigned i = 0; i < STORE_MAX_APPS + 3; ++i) {
        auto *entry = store_catalog_slot(&catalog);
        entry->release_sequence = i;
    }
    assert(catalog.count == STORE_MAX_APPS && catalog.dropped == 3);
    assert(store_catalog_at_const(&catalog, 0)->release_sequence == 3);
    auto catalog_parse = [&](std::string_view json) {
        return store_json_parse_catalog(reinterpret_cast<const uint8_t *>(json.data()), json.size(), &catalog, nullptr);
    };
    catalog = {};
    assert(!catalog_parse(R"({"payload":{"items":[{"app_id":"valid"},{"app_id":"broken","name":"\udc00"}]}})"));
    assert(catalog.count == 1); // A malformed item never becomes a clickable card.
    assert(std::strcmp(store_catalog_at_const(&catalog, 0)->app_id, "valid") == 0);
    assert(!catalog_parse(R"({"payload":{"items":[]}} trailing)"));
    assert(catalog_parse("{\"payload\":{\"items\":[]}} \n\t"));
}
