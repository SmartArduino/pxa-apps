#pragma once
#include "generated/characters.hpp"
#include <algorithm>
#include <array>
#include <span>
#include <string_view>

namespace pinyin_lite {

// Immutable data is included in the measured Guest linear memory. There is no
// heap dictionary or cache: lookup state is a spelling and a syllable range.
class CharacterEngine {
public:
    static constexpr std::size_t max_pinyin = 64;
    void reset() { size_ = consumed_ = count_ = offset_ = 0; }

    std::size_t search(std::string_view text) {
        reset();
        if (text.empty() || text.size() > max_pinyin) return 0;
        for (char c : text) if ((c < 'a' || c > 'z') && c != '\'') return 0;
        std::copy(text.begin(), text.end(), spelling_.begin());
        size_ = text.size();
        std::size_t start = text.find_first_not_of('\'');
        if (start == text.npos) return 0;
        auto segment = text.substr(start, text.find('\'', start) == text.npos ? text.size() - start :
                                             text.find('\'', start) - start);
        auto normalized = normalize(segment);
        auto exact = lookup(normalized);
        if (exact != std::end(table::syllables) && std::string_view(exact->spelling) == normalized) {
            select(exact, start + segment.size());
        } else {
            // A partially typed syllable offers completions. Once another
            // syllable follows, consume the longest complete prefix. Explicit
            // separators disambiguate xi'an from xian; no word model is used.
            auto prefix = exact;
            if (prefix != std::end(table::syllables) && std::string_view(prefix->spelling).starts_with(normalized)) {
                const auto* end = std::partition_point(prefix, std::end(table::syllables),
                    [normalized](const auto& syllable) { return std::string_view(syllable.spelling).starts_with(normalized); });
                offset_ = prefix->offset;
                count_ = end[-1].offset + end[-1].count - offset_;
                consumed_ = start + segment.size();
            } else {
                for (std::size_t n = std::min<std::size_t>(6, segment.size()); n > 0; --n) {
                    auto head = normalize(segment.substr(0, n));
                    auto found = lookup(head);
                    if (found != std::end(table::syllables) && std::string_view(found->spelling) == head) {
                        select(found, start + n); break;
                    }
                }
            }
        }
        return count_;
    }

    std::size_t candidate(std::size_t index, std::span<char> output) const {
        if (index >= count_ || output.size() < 4) return 0;
        // Prefix matches occupy one contiguous range in the immutable table.
        // Direct indexing also makes far candidate pages constant-time.
        const auto cp = table::characters[offset_ + index];
        output[0] = static_cast<char>(0xe0 | (cp >> 12));
        output[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        output[2] = static_cast<char>(0x80 | (cp & 0x3f));
        output[3] = '\0';
        return 3;
    }

    void choose(std::size_t index) {
        if (index >= count_) return;
        while (consumed_ < size_ && spelling_[consumed_] == '\'') ++consumed_;
    }
    std::string_view remaining() const {
        return {spelling_.data() + consumed_, static_cast<std::size_t>(size_ - consumed_)};
    }
    std::size_t consumed() const { return consumed_; }

private:
    static std::string_view normalize(std::string_view text) {
        if (text == "lve") return "lue";
        if (text == "nve") return "nue";
        return text;
    }
    static const table::Syllable* lookup(std::string_view spelling) {
        return std::lower_bound(std::begin(table::syllables), std::end(table::syllables), spelling,
            [](const auto& syllable, auto value) { return std::string_view(syllable.spelling) < value; });
    }
    void select(const table::Syllable* syllable, std::size_t consumed) {
        offset_ = syllable->offset;
        count_ = syllable->count; consumed_ = consumed;
    }
    std::array<char, max_pinyin> spelling_{};
    std::uint16_t size_ = 0, consumed_ = 0, count_ = 0, offset_ = 0;
};
static_assert(sizeof(CharacterEngine) <= 80);
}
