#include "store_size.hpp"
#include <cassert>
#include <cstring>
#include <limits>
#include <utility>

int main() {
    char output[32];
    for (auto [bytes, expected] : {
        std::pair<std::uint64_t, const char *>{0, "0 B"},
        {1023, "1023 B"}, {1024, "1.00 KiB"},
        {393480, "384.26 KiB"}, {1048575, "1.00 MiB"},
        {1409532, "1.34 MiB"}, {1531566, "1.46 MiB"},
        {std::numeric_limits<std::uint64_t>::max(), "16777216.00 TiB"},
    }) {
        store::format_package_size(output, bytes);
        assert(std::strcmp(output, expected) == 0);
    }
    store::format_package_bytes(output, 1409532);
    assert(std::strcmp(output, "1409532 B") == 0);
    store::format_package_size({}, 1409532);
    char tiny[2] = {'x', 'x'};
    store::format_package_size(tiny, 1409532);
    assert(tiny[1] == '\0');

    store_app_t app{};
    app.size = 3251965;
    std::uint64_t total;
    assert(store::package_download_size(app, total) && total == app.size);
    app.requires_resources = 1;
    assert(!store::package_download_size(app, total));
    app.resource_size_known = 1;
    app.resource_size = 81901993;
    assert(store::package_download_size(app, total) && total == 85153958);
    assert(app.size == 3251965); // Installer transport still authenticates only code.
    store::format_package_size(output, total);
    assert(std::strcmp(output, "81.21 MiB") == 0);
    app.size = std::numeric_limits<std::uint64_t>::max();
    assert(!store::package_download_size(app, total));
}
