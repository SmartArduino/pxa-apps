#pragma once
#include <pxa/ime.hpp>
#include "lvgl_pinyin_ime/ime_engine.h"
#include "core/ime_t9.h"
#include <array>
#include <string>

// A Guest instance has one decoder and one active editor. Host generations
// invalidate old editors; the provider never sees their surrounding text.
class Session {
public:
    void reset() { pinyin_.clear(); page_ = count_ = 0; ime_engine_reset(); ime_t9_reset(); }
    pxa::Result<void> process(const pxa::ime::Input& input, pxa::Transport& transport) {
        using enum pxa::ime::Action;
        if (session_ != input.session || input.action == reset) {
            reset_state(input);
        } else if (layout_ != input.layout || language_ != input.language) {
            this->reset(); layout_ = input.layout; language_ = input.language;
        }
        std::string commit;
        auto edit = pxa::ime::Edit::none;
        switch (input.action) {
        case reset: break;
        case text:
            if (input.language == pxa::ime::Language::english || input.sensitive) {
                commit.assign(input.text); edit = pxa::ime::Edit::insert;
            } else if (!input.value && input.layout == pxa::ime::Layout::nine &&
                       input.text.size() == 1 && input.text[0] >= '2' && input.text[0] <= '9') {
                if (ime_t9_push_digit(input.text[0])) { pinyin_ = ime_t9_selected_pinyin(); search(); }
            } else if (!input.value && input.layout == pxa::ime::Layout::nine && input.text == "'") {
                if (ime_t9_split()) { pinyin_ = ime_t9_selected_pinyin(); search(); }
            } else {
                bool letters = !input.value && !input.text.empty();
                for (char c : input.text) letters &= (c >= 'a' && c <= 'z') || c == '\'';
                if (letters) {
                    if (pinyin_.size() + input.text.size() <= IME_ENGINE_MAX_PINYIN_LEN) {
                        pinyin_.append(input.text); search();
                    }
                } else {
                    // Punctuation commits the live composition before itself.
                    while (!pinyin_.empty()) {
                        const auto previous = pinyin_.size();
                        commit += this->choose(0);
                        if (pinyin_.size() >= previous) { commit += pinyin_; this->reset(); }
                    }
                    commit.append(input.text);
                    if (!commit.empty()) edit = pxa::ime::Edit::insert;
                }
            }
            break;
        case backspace:
            if (ime_t9_len()) {
                ime_t9_backspace(); pinyin_ = ime_t9_selected_pinyin(); search();
            } else if (!pinyin_.empty()) { pinyin_.pop_back(); search(); }
            else edit = pxa::ime::Edit::backspace;
            break;
        case confirm:
            if (!pinyin_.empty()) { commit = this->choose(0); edit = pxa::ime::Edit::insert; }
            else if (input.value == 0) { commit = " "; edit = pxa::ime::Edit::insert; }
            else edit = pxa::ime::Edit::submit;
            break;
        case choose:
            if (input.value < count_) { commit = this->choose(input.value); edit = pxa::ime::Edit::insert; }
            break;
        case page: page_ = std::min<std::size_t>(input.value, count_ ? count_ - 1 : 0); break;
        case choose_pinyin:
            if (ime_t9_select(input.value)) { pinyin_ = ime_t9_selected_pinyin(); search(); }
            break;
        }
        if (edit == pxa::ime::Edit::insert && commit.empty()) edit = pxa::ime::Edit::none;
        pxa::ime::Frame frame(wire_, input);
        auto result = frame.composition(pinyin_, edit, commit);
        if (!result) return result;
        std::array<char, IME_ENGINE_MAX_PINYIN_LEN * 3 + 1> candidate;
        std::size_t shown = 0;
        // Bound the response, not the dictionary. The Host packs this window by
        // measured glyph widths and requests the next actual visible index.
        for (std::size_t i = page_; i < count_ && shown < 12; ++i) {
            auto n = ime_engine_candidate(i, candidate.data(), candidate.size());
            if (!n || !frame.candidate({candidate.data(), n})) break;
            ++shown;
        }
        for (std::size_t i = 0; i < ime_t9_count(); ++i) {
            if (!frame.spelling(ime_t9_pinyin(i))) break;
        }
        frame.page(page_, page_ + shown < count_);
        if (input.action == confirm && input.value) frame.submit();
        return frame.present(transport);
    }
private:
    void reset_state(const pxa::ime::Input& input) {
        reset(); session_ = input.session; layout_ = input.layout; language_ = input.language;
    }
    void search() {
        page_ = 0;
        count_ = pinyin_.empty() ? 0 : ime_engine_search(pinyin_.data(), pinyin_.size());
    }
    std::string choose(std::size_t index) {
        if (pinyin_.empty()) return {};
        std::array<char, IME_ENGINE_MAX_PINYIN_LEN * 3 + 1> buffer;
        auto n = count_ ? ime_engine_candidate(index, buffer.data(), buffer.size()) : 0;
        if (!n) { auto raw = pinyin_; reset(); return raw; }
        std::string chosen(buffer.data(), n);
        ime_engine_choose(index);
        std::size_t remaining = 0;
        const char* tail = ime_engine_remaining_pinyin(&remaining);
        std::string next(tail ? tail : "", remaining);
        reset();
        pinyin_ = std::move(next);
        if (layout_ == pxa::ime::Layout::nine && !pinyin_.empty()) (void)ime_t9_restore(pinyin_.c_str());
        search();
        return chosen;
    }
    std::uint64_t session_ = 0;
    pxa::ime::Layout layout_ = pxa::ime::Layout::full;
    pxa::ime::Language language_ = pxa::ime::Language::chinese;
    std::string pinyin_;
    std::size_t page_ = 0, count_ = 0;
    std::array<std::byte, 2048> wire_;
};
