// Optional native benchmark. It captures complete provider replies, not UI FPS.
#include "pxa_control_capture.h"
#include <pxa/ime.hpp>
#include "session.hpp"
#ifdef PXA_IME_LITE
#include "engine.hpp"
using Engine = pinyin_lite::CharacterEngine;
#else
#include "google_engine.hpp"
using Engine = GooglePinyinEngine;
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#ifdef IME_BENCHMARK_MEMORY
#include <pthread.h>
extern "C" {
#include "ime_alloc_trace.h"
}
#include <new>
void* operator new(std::size_t size) { auto p = std::malloc(size); if (!p) std::abort(); return p; }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
#endif

static std::uint64_t reply_hash = 14695981039346656037ull;
static std::size_t replies, reply_bytes;
extern "C" std::int32_t pxa_test_control(const std::uint8_t* data, std::uint32_t size) {
    assert(size >= 52);
    for (auto byte : std::span(data, size)) reply_hash = (reply_hash ^ byte) * 1099511628211ull;
    ++replies; reply_bytes += size;
    return 0;
}
extern "C" std::int32_t pxa_io(std::uint64_t, std::uint32_t, std::uint8_t*, std::uint32_t) { return -3; }

static std::uint64_t now() {
    timespec value{}; assert(!clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value));
    return std::uint64_t(value.tv_sec) * 1000000000 + value.tv_nsec;
}

int main(int argc, char** argv) {
    assert(argc == 4);
    unsigned workload = unsigned(std::strtoul(argv[2], nullptr, 10));
    unsigned iterations = unsigned(std::strtoul(argv[3], nullptr, 10));
    assert(workload < 4 && iterations);
    auto init_start = now();
#ifndef PXA_IME_LITE
    assert(ime_engine_open(argv[1], ""));
    ime_engine_set_max_lens(64, 64);
#endif
    ime_t9_init();
    PinyinSession<Engine> session;
    pxa::Transport transport; transport.phase(pxa::Phase::event);
    pxa::ime::Input input{}; input.session = 1;
    auto send = [&](pxa::ime::Action action, std::string_view text = {}, std::uint32_t value = 0) {
        input.action = action; input.text = text; input.value = value; ++input.sequence;
        assert(session.process(input, transport));
    };
    using enum pxa::ime::Action;
    auto cycle = [&] {
        if (workload == 0) {
            input.layout = pxa::ime::Layout::full;
            for (auto spelling : {"nihao", "zhongguo", "shijie", "tianqi", "xi'an", "nihaoshijie"}) {
                send(reset);
                for (auto c : std::string_view(spelling)) send(text, {&c, 1});
                send(choose, {}, 1); send(text, "，", 1);
            }
        } else if (workload == 1) {
            input.layout = pxa::ime::Layout::nine;
            for (auto digits : {"64426", "94664", "743543", "2345464", "777777777777777777777777"}) {
                send(reset);
                for (auto c : std::string_view(digits)) send(text, {&c, 1});
                send(choose_pinyin, {}, 1); send(text, "'");
                send(backspace); send(confirm); send(text, "!", 1);
            }
        } else if (workload == 2) {
            input.layout = pxa::ime::Layout::full;
            send(reset); send(text, "sh");
            for (unsigned page_start : {0u, 12u, 24u, 48u, 96u, 192u, 384u, 10000u}) send(page, {}, page_start);
            send(reset); send(text, "shi"); send(page, {}, 24); send(choose, {}, 24);
        } else {
            input.layout = pxa::ime::Layout::full;
            send(reset); send(text, "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn");
            send(text, "。", 1);
            send(text, "ni'hao'shi'jie"); send(text, "!", 1);
            for (unsigned i = 0; i < 8; ++i) send(backspace);
        }
    };
    cycle();
    auto initialization_ns = now() - init_start;
    input.sequence = 0; replies = reply_bytes = 0; reply_hash = 14695981039346656037ull;
#ifdef IME_BENCHMARK_MEMORY
    auto stable = live_bytes, initialization_peak = peak_bytes;
    peak_bytes = live_bytes; alloc_calls = 0;
#endif
    auto start = now();
    for (unsigned i = 0; i < iterations; ++i) cycle();
    auto elapsed = now() - start;
    std::printf("{\"workload\":%u,\"iterations\":%u,\"replies\":%zu,\"reply_bytes\":%zu,\"reply_hash\":\"%016llx\",\"cpu_us_per_cycle\":%.3f,\"initialization_and_warmup_us\":%.3f,\"session_bytes\":%zu",
        workload, iterations, replies, reply_bytes, (unsigned long long)reply_hash,
        double(elapsed) / iterations / 1000, double(initialization_ns) / 1000, sizeof(session));
#ifdef IME_BENCHMARK_MEMORY
    std::printf(",\"stable\":%zu,\"initialization_peak\":%zu,\"run_peak\":%zu,\"end\":%zu,\"allocation_calls\":%zu",
        stable, initialization_peak, peak_bytes, live_bytes, alloc_calls);
#endif
    std::puts("}");
#ifndef PXA_IME_LITE
    ime_engine_close();
#endif
    ime_t9_deinit();
}
