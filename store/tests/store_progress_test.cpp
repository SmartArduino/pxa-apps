#include "store_progress.hpp"
#include <cassert>
#include <initializer_list>

int main() {
    using enum store::DownloadPhase;
    assert(store::download_phase(false, 0, 100, 100) == waiting);
    assert(store::download_phase(true, 0, 1000, 100) == program);
    assert(store::download_phase(true, 99, 1000, 100) == program);
    assert(store::download_phase(true, 100, 1000, 100) == resources);
    assert(store::download_phase(true, 999, 1000, 100) == resources);
    assert(store::download_phase(true, 1000, 1000, 100) == installing);
    assert(store::download_phase(true, 1010, 1000, 100) == installing);
    assert(store::download_phase(true, 99, 100, 100) == program); // Resources reused.
    assert(store::download_phase(true, 100, 100, 100) == installing);
    assert(store::download_phase(true, 100, 0, 100) == program); // Unknown length.
    assert(store::download_percent(100, 0) == 0);
    assert(store::download_percent(101, 100) == 100);
    for (std::uint64_t total = 1; total < 512; ++total)
        for (std::uint64_t received = 0; received <= total; ++received)
            assert(store::download_percent(received, total) == received * 100 / total);
    constexpr auto max = std::numeric_limits<std::uint64_t>::max();
    for (const auto total : {max, max - 1, max / 2, max / 100 + 1})
        for (unsigned step = 0; step <= 1000; ++step) {
            const auto received = static_cast<std::uint64_t>(
                static_cast<unsigned __int128>(total) * step / 1000);
            assert(store::download_percent(received, total) ==
                static_cast<unsigned __int128>(received) * 100 / total);
        }
}
