#pragma once

#include "game.hpp"
#include <pxa/app.hpp>

#include <algorithm>

namespace tide {

struct VoyageSave {
    static constexpr std::size_t HeadBytes=960;
    static constexpr const char* HeadKey="voyage-v2";
    static constexpr const char* TailKeys[]{"voyage-v2-tail-a","voyage-v2-tail-b"};
    static_assert(SaveBytes+1>HeadBytes && SaveBytes+1-HeadBytes<=HeadBytes);

    std::array<std::byte,SaveBytes+1> bytes{};
    std::array<std::byte,HeadBytes+72> packet{};
    unsigned slot=1;
    bool refresh_slot=false;

    pxa::Task<void> load(pxa::StorageService storage, State& state) {
        auto all=std::span{bytes};
        auto data=all.subspan(1);
        auto head=co_await storage.get(HeadKey,all.first(HeadBytes));
        if (!head && head.error()==pxa::Error::not_found) {
            auto legacy=co_await storage.get("voyage-v1",data);
            if (!legacy) co_return std::unexpected(legacy.error());
            data=data.first(*legacy);
        } else {
            if (!head) co_return std::unexpected(head.error());
            const auto current=std::to_integer<unsigned>(bytes.front());
            if (*head!=HeadBytes || current>1) co_return std::unexpected(pxa::Error::protocol_error);
            slot=current;
            auto tail=co_await storage.get(TailKeys[slot],all.subspan(HeadBytes));
            if (!tail) co_return std::unexpected(pxa::Error::protocol_error);
            data=data.first(HeadBytes-1+*tail);
        }
        if (!decode(state,data)) co_return std::unexpected(pxa::Error::protocol_error);
        co_return pxa::Result<void>{};
    }

    void snapshot(const State& state) {
        const auto encoded=encode(state);
        std::copy(encoded.begin(),encoded.end(),bytes.begin()+1);
    }

    pxa::Task<void> write(pxa::StorageService storage, const State& state) {
        auto all=std::span{bytes};
        if (refresh_slot) {
            const auto head=co_await storage.get(HeadKey,all.first(HeadBytes));
            if (!head && head.error()!=pxa::Error::not_found)
                co_return std::unexpected(head.error());
            const auto current=std::to_integer<unsigned>(bytes.front());
            if (head && *head==HeadBytes && current<2) slot=current;
            refresh_slot=false;
        }
        snapshot(state);
        const unsigned next=1-slot;
        bytes.front()=std::byte(next);
        // Keep the active tail untouched until the KV backend commits the new head.
        auto tail=co_await storage.set(TailKeys[next],all.subspan(HeadBytes),packet);
        if (!tail) co_return tail;
        auto head=co_await storage.set(HeadKey,all.first(HeadBytes),packet);
        if (head) slot=next;
        else refresh_slot=true;
        co_return head;
    }
};

} // namespace tide
