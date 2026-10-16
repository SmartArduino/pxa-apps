#include "pxa_control_capture.h"
#include <pxa/ime.hpp>
#include "session.hpp"
#include "google_engine.hpp"
#include "spellingtrie.h"
#include <cassert>
#include <cstring>
#include <cstdio>

static std::array<std::byte, 4096> packet;
extern "C" std::int32_t pxa_test_control(const std::uint8_t* bytes, std::uint32_t size) {
    assert(size >= 52 && size <= packet.size()); std::memcpy(packet.data(), bytes, size); return 0;
}
extern "C" std::int32_t pxa_io(std::uint64_t, std::uint32_t, std::uint8_t*, std::uint32_t) { return -3; }
static std::string_view preedit() {
    return {reinterpret_cast<const char*>(packet.data()+52), pxa::wire::get16(packet.data()+40)};
}
static std::string_view commit() {
    return {reinterpret_cast<const char*>(packet.data()+52)+preedit().size(), pxa::wire::get16(packet.data()+42)};
}

int main(int argc, char** argv) {
    assert(argc == 2);
#ifdef PINYINIME_SYSTEM_DICT_ONLY
    // A system-only build must reject an unsupported persistence request.
    assert(!ime_engine_open(argv[1], "user.dict"));
#endif
    assert(ime_engine_open(argv[1], "")); ime_engine_set_max_lens(64, 64); ime_t9_init();
    PinyinSession<GooglePinyinEngine> session;
    pxa::Transport transport; transport.phase(pxa::Phase::event);
    pxa::ime::Input input{}; input.session = 42;
    auto send = [&](pxa::ime::Action action, std::string_view text = {}, std::uint32_t value = 0) {
        input.action = action; input.text = text; input.value = value; ++input.sequence;
        assert(session.process(input, transport));
    };
    using enum pxa::ime::Action;
    // The decoder's 39-byte window is shorter than the session's 64-byte limit.
    // All 16 syllables must survive successive sentence/partial-word choices.
    std::string spelling;
    for (unsigned i = 0; i < 8; ++i) spelling += "ni'hao'";
    send(reset); send(text, spelling);
    std::string committed; std::size_t previous = spelling.size();
    for (unsigned i = 0; i < 20 && !preedit().empty(); ++i) {
        send(confirm);
        assert(preedit().size() < previous);
        assert(preedit() == std::string_view(spelling).substr(spelling.size()-preedit().size()));
        if (!i) assert(preedit().size() >= spelling.size()-39);
        committed += commit(); previous = preedit().size();
    }
    assert(preedit().empty() && committed.size() == 16*3);
    send(text, std::string(64, 'n')); send(text, "。", 1);
    assert(preedit().empty() && !commit().empty());
    auto count = ime_engine_search("ni", 2);
    std::array<char, 193> candidate{}; candidate[0] = 'X';
    assert(!ime_engine_candidate(count, candidate.data(), candidate.size()) && candidate[0] == 'X');
    assert(!ime_engine_candidate(SIZE_MAX, candidate.data(), candidate.size()));
    std::uint32_t random = 42;
    for (unsigned trial = 0; trial < 1000; ++trial) {
        auto next = [&] { random = random*1664525+1013904223; return random; };
        std::string value; auto length = next()%65;
        for (unsigned i = 0; i < length; ++i) value += "abcdefghijklmnopqrstuvwxyz'"[next()%27];
        ime_engine_reset(); count = ime_engine_search(value.data(), value.size());
        if (count && ime_engine_candidate(0, candidate.data(), candidate.size())) {
            ime_engine_choose(0); assert(ime_engine_consumed_pinyin() <= value.size());
        }
        assert(!ime_engine_candidate(count, candidate.data(), candidate.size()));
    }
    ime_engine_close();
    assert(ime_engine_open(argv[1], "")); assert(ime_engine_search("hao", 3)); ime_engine_close();
    delete ime_pinyin::SpellingTrie::get_cpinstance();
    assert(ime_engine_open(argv[1], "")); assert(ime_engine_search("ni", 2)); ime_engine_close();
    delete ime_pinyin::SpellingTrie::get_cpinstance();
    ime_t9_deinit(); puts("Full provider: long tail, pool exhaustion, bounds, 1000 stress searches and reopen passed");
}
