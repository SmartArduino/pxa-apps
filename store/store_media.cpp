#include "store_media.hpp"
#include "store_client.h"
#include "store_model.h"
#include <pxa/assets.hpp>
#include <pxa/http.hpp>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace store {
namespace {
enum class State { empty, waiting, loading, ready, failed };
struct Entry {
    std::array<char, STORE_MAX_MEDIA_URL> path{};
    std::optional<pxa::Asset> asset;
    State state = State::empty;
    bool wanted = false, screenshot = false;
};
struct Loader {
    pxa::Transport transport;
    pxa::RequestTable requests;
    pxa::TaskScope tasks;
    std::array<Entry, media_capacity> entries;
    int active = -1;
    std::uint64_t permission = 0;
    bool changed = false, paused = false;

    pxa::Task<void> load(unsigned index) {
        auto result = co_await fetch(entries[index]);
        entries[index].state = result ? State::ready : State::failed;
        active = -1;
        changed = true;
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> fetch(Entry &entry) {
        char url[320];
        if (!media_url(entry.path.data(), url, sizeof(url)))
            co_return std::unexpected(pxa::Error::denied);
        // Permission ownership stays in the catalog client. open() captures the
        // handle synchronously; releasing this temporary avoids a second owner.
        pxa::Permission borrowed(transport, permission);
        static constexpr std::array<pxa::NetHeaderView, 2> headers{{
            {"Accept", "image/png, image/jpeg"}, {"Accept-Encoding", "identity"}}};
        static constexpr std::array<std::string_view, 2> wanted{
            "content-type", "content-encoding"};
        auto pending = pxa::HttpClient(transport, requests).open(
            {.url = url, .headers = headers, .wanted_headers = wanted,
             .options = {.buffer_bytes = 2048, .header_bytes = 256,
                         .max_body_bytes = 131072, .timeout_ms = 12000}}, borrowed);
        borrowed.release();
        auto exchange = co_await std::move(pending);
        if (!exchange) co_return std::unexpected(exchange.error());
        auto response = co_await std::move(*exchange).response();
        if (!response) co_return std::unexpected(response.error());
        if (response->status_code != 200)
            co_return std::unexpected(pxa::Error::not_found);
        auto encoding = response->headers.first("content-encoding");
        if (encoding && !pxa::http_header_equal(*encoding, "identity"))
            co_return std::unexpected(pxa::Error::unsupported);
        auto image = co_await pxa::AssetService(transport, requests).decode_image(
            std::move(response->body),
            {.max_width = std::uint16_t(entry.screenshot ? 320 : 64),
             .max_height = std::uint16_t(entry.screenshot ? 320 : 64),
             .max_pixels = entry.screenshot ? 32768u : 4096u,
             .max_encoded_bytes = 131072});
        if (!image) co_return std::unexpected(image.error());
        entry.asset.emplace(std::move(*image));
        co_return pxa::Result<void>{};
    }
    void pump() {
        if (paused || active >= 0 || !permission) return;
        for (unsigned i = 0; i < entries.size(); ++i) {
            auto &entry = entries[i];
            if (!entry.wanted || entry.state != State::waiting) continue;
            active = int(i);
            entry.state = State::loading;
            if (!tasks.start(load(i))) {
                entry.state = State::failed;
                active = -1;
                changed = true;
            }
            return;
        }
    }
    void cancel() {
        tasks.cancel();
        if (active >= 0) entries[unsigned(active)].state = State::waiting;
        active = -1;
    }
} loader;
} // namespace
void media_start() {
    loader.transport.phase(pxa::Phase::event);
    // Manual catalog request IDs occupy 1..15. Keep SDK task tokens disjoint.
    for (unsigned i = 0; i < 64; ++i) (void)loader.transport.next_token();
}
void media_update(std::span<const MediaRequest> visible, std::uint64_t permission) {
    loader.permission = permission;
    for (auto &e : loader.entries) {
        e.wanted = std::ranges::any_of(visible, [&](auto r) {
            return r.path == e.path.data() && r.screenshot == e.screenshot;
        });
    }
    if (loader.active >= 0 && !loader.entries[unsigned(loader.active)].wanted)
        loader.cancel();
    for (auto &e : loader.entries) if (!e.wanted && e.state != State::empty) {
        e = {};
        loader.changed = true;
    }
    for (auto r : visible) {
        if (r.path.empty() || r.path.size() >= STORE_MAX_MEDIA_URL) continue;
        if (std::ranges::any_of(loader.entries, [&](auto &e) {
            return e.wanted && r.path == e.path.data() && r.screenshot == e.screenshot;
        })) continue;
        auto slot = std::ranges::find_if(loader.entries, [](auto &e) {
            return e.state == State::empty;
        });
        if (slot == loader.entries.end()) break;
        std::memcpy(slot->path.data(), r.path.data(), r.path.size());
        slot->path[r.path.size()] = 0;
        slot->wanted = true;
        slot->screenshot = r.screenshot;
        slot->state = State::waiting;
    }
    loader.pump();
}
std::uint64_t media_handle(std::string_view path, bool screenshot) {
    for (auto &e : loader.entries)
        if (e.screenshot == screenshot && e.state == State::ready && e.asset && path == e.path.data()) return e.asset->handle();
    return 0;
}
bool media_failed(std::string_view path, bool screenshot) {
    for (auto &e : loader.entries)
        if (e.screenshot == screenshot && e.state == State::failed && path == e.path.data()) return true;
    return false;
}
bool media_event(std::span<const std::byte> bytes) {
    auto event = pxa::parse_event(bytes);
    if (!event || event->token <= 64 || event->token > UINT32_MAX) return false;
    auto handled = loader.requests.dispatch(*event);
    loader.tasks.reap();
    loader.pump();
    return handled;
}
bool media_changed() { return std::exchange(loader.changed, false); }
void media_pause(bool paused) {
    loader.paused = paused;
    if (paused) loader.cancel(); else loader.pump();
}
void media_retry() {
    for (auto &e : loader.entries) if (e.state == State::failed) e.state = State::waiting;
    loader.pump();
}
void media_stop() {
    loader.cancel();
    for (auto &e : loader.entries) e = {};
    loader.transport.phase(pxa::Phase::stopped);
}
} // namespace store
