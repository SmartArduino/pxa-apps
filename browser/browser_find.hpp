#pragma once
#include "browser_core.hpp"

namespace browser {
// Keep only the query and current result. Do not copy the page or allocate an
// index proportional to its length. Searches run on explicit actions/layout
// changes, never on an unchanged frame. UTF-8 is exact; ASCII ignores case.
class PageFind {
    std::string query_;
    std::size_t offset_ = std::string::npos, count_ = 0, index_ = 0;

    void select(const Document &document, const Layout &layout, std::size_t wanted) {
        count_ = 0;
        offset_ = std::string::npos;
        if (!active() || layout.pieces.empty()) return;
        const auto &last = layout.pieces.back();
        const auto limit = std::min(document.text.size(), std::size_t(last.offset) + last.size);
        const std::string_view text(document.text.data(), limit);
        std::size_t last_match = std::string::npos;
        // Adjacent ordinary spans form one searchable run, so inline links or
        // style changes do not break a phrase. Form labels, hidden/password
        // values and image alternatives are not searchable page text.
        for (std::size_t i = 0; i < document.spans.size();) {
            const auto &span = document.spans[i++];
            if (span.field >= 0 || span.image >= 0 || span.offset >= limit) continue;
            const auto begin = std::size_t(span.offset);
            auto end = std::min(limit, begin + span.size);
            while (i < document.spans.size()) {
                const auto &next = document.spans[i];
                if (next.field >= 0 || next.image >= 0 || next.offset != end || end >= limit) break;
                end = std::min(limit, end + next.size);
                ++i;
            }
            auto at = text.begin() + begin;
            const auto stop = text.begin() + end;
            while (at != stop) {
                at = std::search(at, stop, query_.begin(), query_.end(),
                                 [](char a, char b) { return lower(a) == lower(b); });
                if (at == stop) break;
                last_match = std::size_t(at - text.begin());
                if (count_ == wanted) offset_ = last_match;
                ++count_;
                at += query_.size();
            }
        }
        index_ = count_ ? std::min(wanted, count_ - 1) : 0;
        if (offset_ == std::string::npos) offset_ = last_match;
    }

  public:
    static constexpr std::size_t max_query = 128;
    bool active() const { return !query_.empty(); }
    std::string_view query() const { return query_; }
    std::size_t count() const { return count_; }
    std::size_t ordinal() const { return count_ ? index_ + 1 : 0; }
    std::size_t offset() const { return offset_; }
    bool overlaps(const Piece &piece) const {
        return count_ && piece.offset < offset_ + query_.size() &&
               offset_ < std::size_t(piece.offset) + piece.size;
    }
    void clear() { std::string{}.swap(query_); offset_ = std::string::npos; count_ = index_ = 0; }
    bool start(const Document &document, const Layout &layout, std::string_view value) {
        value = trim(value);
        if (value.empty() || value.size() > max_query) return false;
        for (unsigned char c : value)
            if (c < 32 || c == 127) return false;
        query_ = value;
        select(document, layout, 0);
        return true;
    }
    void refresh(const Document &document, const Layout &layout) {
        select(document, layout, index_);
    }
    void move(const Document &document, const Layout &layout, bool backwards) {
        if (!count_) return;
        select(document, layout, backwards ? (index_ ? index_ - 1 : count_ - 1)
                                           : (index_ + 1) % count_);
    }
};
} // namespace browser
