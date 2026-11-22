#pragma once

#include "game.hpp"
#include <pxa/app.hpp>

#include <algorithm>

namespace tide {

struct VoyageSave {
    // The embedded Storage service accepts at most 960 bytes per value.
    static constexpr std::size_t HeadBytes=960;
    static constexpr const char* HeadKey="voyage-v2";
    static constexpr const char* TailKeys[]{"voyage-v2-tail-a","voyage-v2-tail-b"};
    static constexpr const char* EndKeys[]{"voyage-v2-end-a","voyage-v2-end-b"};
    static constexpr std::size_t FirstTailBytes=960;
    static constexpr std::size_t TailBytes=SaveBytes+1-HeadBytes;
    static_assert(TailBytes>FirstTailBytes && TailBytes<=2*FirstTailBytes);

    std::array<std::byte,SaveBytes+1> bytes{};
    std::array<std::byte,std::max(HeadBytes,FirstTailBytes)+72> packet{};
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
            const auto descriptor=std::to_integer<unsigned>(bytes.front());
            if (*head!=HeadBytes || descriptor>3) co_return std::unexpected(pxa::Error::protocol_error);
            slot=descriptor%2;
            const bool split_tail=descriptor>=2;
            auto tail_buffer=all.subspan(HeadBytes);
            auto tail=co_await storage.get(TailKeys[slot],
                split_tail ? tail_buffer.first(FirstTailBytes) : tail_buffer);
            if (!tail) co_return std::unexpected(pxa::Error::protocol_error);
            std::size_t data_size=HeadBytes-1+*tail;
            if (split_tail) {
                if (*tail!=FirstTailBytes) co_return std::unexpected(pxa::Error::protocol_error);
                auto end=co_await storage.get(EndKeys[slot],tail_buffer.subspan(FirstTailBytes));
                if (!end) co_return std::unexpected(pxa::Error::protocol_error);
                data_size+=*end;
            }
            data=data.first(data_size);
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
            const auto descriptor=std::to_integer<unsigned>(bytes.front());
            if (head && *head==HeadBytes && descriptor<=3) slot=descriptor%2;
            refresh_slot=false;
        }
        snapshot(state);
        const unsigned next=1-slot;
        bytes.front()=std::byte(2+next);
        // Keep the active chunks untouched until the KV backend commits the new head.
        const auto tail_buffer=all.subspan(HeadBytes);
        auto tail=co_await storage.set(TailKeys[next],tail_buffer.first(FirstTailBytes),packet);
        if (!tail) co_return tail;
        auto end=co_await storage.set(EndKeys[next],tail_buffer.subspan(FirstTailBytes),packet);
        if (!end) co_return end;
        auto head=co_await storage.set(HeadKey,all.first(HeadBytes),packet);
        if (head) slot=next;
        else refresh_slot=true;
        co_return head;
    }
};

} // namespace tide
