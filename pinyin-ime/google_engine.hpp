#pragma once
#include "lvgl_pinyin_ime/ime_engine.h"
#include <span>
#include <string_view>

// The vendored decoder has per-Guest global state; this adapter is empty.
struct GooglePinyinEngine {
    void reset() { ime_engine_reset(); }
    std::size_t search(std::string_view text) { return ime_engine_search(text.data(), text.size()); }
    std::size_t candidate(std::size_t index, std::span<char> output) {
        return ime_engine_candidate(index, output.data(), output.size());
    }
    void choose(std::size_t index) { (void)ime_engine_choose(index); }
    std::string_view remaining() {
        std::size_t length = 0;
        const char* text = ime_engine_remaining_pinyin(&length);
        return text ? std::string_view(text, length) : std::string_view{};
    }
};
