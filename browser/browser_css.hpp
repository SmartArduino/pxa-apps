#pragma once
// A bounded reading-oriented CSS subset; no DOM, external sheets or selectors
// that require ancestor traversal. Unknown rules never become page text.
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace browser::css {
inline constexpr std::size_t max_bytes = 8192, max_rules = 64, max_selector = 64;
inline constexpr std::size_t cache_slots = 16, max_cache_key = 256;
inline bool ws(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f'; }
inline char lower(char c) { return c >= 'A' && c <= 'Z' ? char(c + 32) : c; }
inline std::string_view trim(std::string_view s) {
    while (!s.empty() && ws(s.front())) s.remove_prefix(1);
    while (!s.empty() && ws(s.back())) s.remove_suffix(1);
    return s;
}
inline bool equal(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
        [](char x, char y) { return lower(x) == lower(y); });
}
inline bool active_style(std::string_view media, std::string_view type) {
    media = trim(media); type = trim(type);
    return (media.empty() || equal(media,"all") || equal(media,"screen")) &&
           (type.empty() || equal(type,"text/css"));
}
inline std::uint32_t readable_color(std::uint32_t rgba) {
    // Page backgrounds are outside this reader subset. Keep light foregrounds
    // visible on its fixed light paper without retaining background surfaces.
    unsigned r = rgba >> 24, g = (rgba >> 16) & 255u, b = (rgba >> 8) & 255u;
    const unsigned brightness = (299*r + 587*g + 114*b) / 1000;
    if (brightness > 150) { r = r*120/brightness; g = g*120/brightness; b = b*120/brightness; }
    return (r << 24) | (g << 16) | (b << 8) | 255u;
}
enum class WhiteSpace : std::uint8_t { normal, preserve, pre_line };
enum class Align : std::uint8_t { left, center, right };
struct Text {
    std::uint32_t color = 0; // zero means the reader's default; colors are opaque
    std::uint16_t scale = 0; // zero preserves the legacy semantic font default
    WhiteSpace whitespace = WhiteSpace::normal;
    Align align = Align::left;
    bool operator==(const Text &) const = default;
};
enum Property : unsigned { color, size, whitespace, align, display, property_count };
struct Declarations {
    std::array<std::int32_t, property_count> values{};
    std::uint8_t set = 0, important = 0;
    void put(Property p, std::int32_t value, bool priority) {
        const auto bit = std::uint8_t(1u << p);
        if ((important & bit) && !priority) return;
        values[p] = value; set |= bit;
        if (priority) important |= bit; else important &= ~bit;
    }
};
inline std::optional<int> integer(std::string_view value) {
    int n = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), n);
    if (error != std::errc{} || end != value.data() + value.size()) return {};
    return n;
}
inline std::optional<std::uint32_t> parse_color(std::string_view v) {
    if (v.starts_with('#') && (v.size() == 4 || v.size() == 7)) {
        std::uint32_t rgb = 0;
        for (char c : v.substr(1)) {
            const auto digit = c >= '0' && c <= '9' ? c - '0'
                             : lower(c) >= 'a' && lower(c) <= 'f' ? lower(c) - 'a' + 10 : -1;
            if (digit < 0) return {};
            rgb = v.size() == 4 ? (rgb << 8) | unsigned(digit * 17) : (rgb << 4) | unsigned(digit);
        }
        return (rgb << 8) | 255u;
    }
    constexpr std::array<std::pair<std::string_view, std::uint32_t>, 16> names{{
        {"black",0x000000ff}, {"white",0xffffffff}, {"red",0xff0000ff}, {"green",0x008000ff},
        {"blue",0x0000ffff}, {"gray",0x808080ff}, {"grey",0x808080ff}, {"silver",0xc0c0c0ff},
        {"maroon",0x800000ff}, {"purple",0x800080ff}, {"fuchsia",0xff00ffff}, {"lime",0x00ff00ff},
        {"olive",0x808000ff}, {"yellow",0xffff00ff}, {"navy",0x000080ff}, {"teal",0x008080ff}}};
    for (auto [name, rgb] : names) if (equal(v, name)) return rgb;
    return {};
}
inline Declarations declarations(std::string_view source) {
    Declarations out;
    // Inline values have the same bounded comment handling as style blocks.
    std::string cleaned;
    if (source.find("/*") != source.npos) {
        cleaned.reserve(std::min(source.size(), max_bytes));
        char quote = 0;
        for (std::size_t i = 0; i < source.size() && cleaned.size() < max_bytes; ++i) {
            const char c = source[i];
            if (quote) {
                cleaned += c;
                if (c == '\\' && i + 1 < source.size()) cleaned += source[++i];
                else if (c == quote) quote = 0;
            } else if (source.substr(i,2) == "/*") {
                auto end = source.find("*/", i + 2);
                if (end == source.npos) break;
                cleaned += ' '; i = end + 1;
            } else { cleaned += c; if (c == '\'' || c == '"') quote = c; }
        }
        source = cleaned;
    }
    while (!source.empty()) {
        std::size_t end = 0; char quote = 0; unsigned parens = 0;
        for (; end < source.size(); ++end) {
            const char c = source[end];
            if (quote) { if (c == '\\') ++end; else if (c == quote) quote = 0; }
            else if (c == '\'' || c == '"') quote = c;
            else if (c == '(') ++parens;
            else if (c == ')' && parens) --parens;
            else if (c == ';' && !parens) break;
        }
        const auto rule = source.substr(0,end);
        source.remove_prefix(std::min(end + 1, source.size()));
        const auto colon = rule.find(':');
        if (colon == rule.npos) continue;
        const auto name = trim(rule.substr(0,colon));
        auto value = trim(rule.substr(colon + 1));
        bool important = false;
        if (const auto bang = value.rfind('!'); bang != value.npos && equal(trim(value.substr(bang+1)),"important")) {
            important = true; value = trim(value.substr(0,bang));
        }
        if (equal(name,"color")) {
            if (auto c = parse_color(value)) out.put(color, std::int32_t(*c), important);
        } else if (equal(name,"font-size")) {
            int scale = 0;
            if (equal(value,"small")) scale = 85;
            else if (equal(value,"medium")) scale = 100;
            else if (equal(value,"large")) scale = 125;
            else if (value.ends_with('%')) {
                if (auto n = integer(value.substr(0,value.size()-1)); n && *n > 0 && *n <= 1000)
                    scale = -*n; // relative to parent, resolved only once on entry
            } else if (value.size() > 2 && equal(value.substr(value.size()-2),"px")) {
                if (auto n = integer(value.substr(0,value.size()-2)); n && *n > 0 && *n <= 512)
                    scale = (*n * 100 + 14) / 15; // avoid losing a pixel at the default reading size
            }
            if (scale) out.put(size, scale, important);
        } else if (equal(name,"white-space")) {
            if (equal(value,"normal")) out.put(whitespace,0,important);
            else if (equal(value,"pre-wrap") || equal(value,"pre")) out.put(whitespace,1,important);
            else if (equal(value,"pre-line")) out.put(whitespace,2,important);
        } else if (equal(name,"text-align")) {
            if (equal(value,"left") || equal(value,"start")) out.put(align,0,important);
            else if (equal(value,"center")) out.put(align,1,important);
            else if (equal(value,"right") || equal(value,"end")) out.put(align,2,important);
        } else if (equal(name,"display")) {
            if (equal(value,"none")) out.put(display,1,important);
            else if (equal(value,"block")) out.put(display,2,important);
            else if (equal(value,"inline")) out.put(display,3,important);
        }
    }
    return out;
}
inline bool ident(char c) {
    return (lower(c) >= 'a' && lower(c) <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
}
inline std::optional<unsigned> specificity(std::string_view selector) {
    if (selector.empty() || selector.size() > max_selector) return {};
    unsigned score = 0; std::size_t at = 0;
    if (selector[0] == '*') ++at;
    else if (selector[0] != '.' && selector[0] != '#') {
        while (at < selector.size() && ident(selector[at])) ++at;
        if (!at) return {};
        score = 1;
    }
    while (at < selector.size()) {
        const char kind = selector[at++];
        if (kind != '.' && kind != '#') return {};
        const auto start = at;
        while (at < selector.size() && ident(selector[at])) ++at;
        if (at == start) return {};
        score += kind == '#' ? 100 : 10;
    }
    return score;
}
inline bool matches(std::string_view selector, std::string_view tag, std::string_view id, std::string_view classes) {
    std::size_t at = 0;
    if (selector.front() == '*') ++at;
    else if (selector.front() != '.' && selector.front() != '#') {
        while (at < selector.size() && ident(selector[at])) ++at;
        if (!equal(selector.substr(0,at),tag)) return false;
    }
    while (at < selector.size()) {
        const char kind = selector[at++]; const auto start = at;
        while (at < selector.size() && ident(selector[at])) ++at;
        const auto name = selector.substr(start,at-start);
        if (kind == '#') { if (id != name) return false; }
        else {
            bool found = false;
            for (auto rest = trim(classes); !rest.empty(); rest = trim(rest)) {
                std::size_t end = 0; while (end < rest.size() && !ws(rest[end])) ++end;
                found |= rest.substr(0,end) == name; rest.remove_prefix(end);
            }
            if (!found) return false;
        }
    }
    return true;
}
class Sheet {
    struct Rule { std::string selector; Declarations declarations; unsigned specificity; };
    std::vector<Rule> rules_;
    std::size_t bytes_ = 0;
    struct CacheEntry {
        std::string key;
        std::array<std::uint16_t,4> lengths{};
        Declarations result;
        std::uint32_t hash = 0;
        bool valid = false;
    };
    mutable std::vector<CacheEntry> cache_;
  public:
    std::size_t rule_count() const { return rules_.size(); }
    bool empty() const { return rules_.empty(); }
    void parse(std::string_view source) {
        source = source.substr(0,max_bytes-bytes_); bytes_ += source.size();
        if (source.empty() || rules_.size() == max_rules) return;
        cache_.clear(); // A later style block can override a previously cached match.
        std::string cleaned;
        char quote = 0;
        if (source.find("/*") != source.npos) {
            cleaned.reserve(source.size());
            for (std::size_t i = 0; i < source.size(); ++i) {
                char c = source[i];
                if (quote) {
                    cleaned += c;
                    if (c == '\\' && i + 1 < source.size()) cleaned += source[++i];
                    else if (c == quote) quote = 0;
                } else if (source.substr(i,2) == "/*") {
                    auto end = source.find("*/",i+2); if (end == source.npos) break;
                    cleaned += ' '; i = end + 1;
                } else { cleaned += c; if (c == '\'' || c == '"') quote = c; }
            }
            source = cleaned;
        }
        while (!source.empty() && rules_.size() < max_rules) {
            const auto open = source.find('{'); if (open == source.npos) break;
            auto selectors = trim(source.substr(0,open));
            // @import and other statement rules do not poison the next rule.
            if (auto semi = selectors.rfind(';'); semi != selectors.npos) selectors = trim(selectors.substr(semi+1));
            std::size_t end = open+1; unsigned depth = 1; quote = 0;
            for (; end < source.size() && depth; ++end) {
                char c = source[end];
                if (quote) { if (c == '\\') ++end; else if (c == quote) quote = 0; }
                else if (c == '\'' || c == '"') quote = c;
                else if (c == '{') ++depth;
                else if (c == '}') --depth;
            }
            if (depth || end > source.size()) break; // never apply a truncated rule
            const auto body = source.substr(open+1,end-open-2);
            if (!selectors.starts_with('@') && body.find('{') == body.npos) {
                const auto props = declarations(body);
                while (!selectors.empty() && rules_.size() < max_rules) {
                    const auto comma = selectors.find(','); const auto s = trim(selectors.substr(0,comma));
                    if (auto score = specificity(s); score && props.set)
                        rules_.push_back({std::string(s),props,*score});
                    if (comma == selectors.npos) break;
                    selectors.remove_prefix(comma+1);
                }
            }
            source.remove_prefix(end);
        }
    }
    Declarations cascade(std::string_view tag, std::string_view id, std::string_view classes,
                         std::string_view inline_style) const {
        if (rules_.empty() && inline_style.empty()) return {};
        const std::array<std::string_view,4> parts{tag,id,classes,inline_style};
        const auto key_size = tag.size()+id.size()+classes.size()+inline_style.size();
        CacheEntry *slot = nullptr;
        std::uint32_t hash = 2166136261u;
        if (key_size <= max_cache_key) {
            for (auto part : parts) {
                hash = (hash ^ std::uint32_t(part.size())) * 16777619u;
                for (unsigned char c : part) hash = (hash ^ c) * 16777619u;
            }
            if (cache_.empty()) cache_.resize(cache_slots);
            slot = &cache_[hash % cache_slots];
            if (slot->valid && slot->hash == hash && slot->key.size() == key_size) {
                bool same = true; std::size_t offset = 0;
                for (std::size_t i=0; i<parts.size(); ++i) {
                    same &= slot->lengths[i] == parts[i].size() &&
                        std::string_view(slot->key).substr(offset,slot->lengths[i]) == parts[i];
                    offset += slot->lengths[i];
                }
                if (same) return slot->result;
            }
        }
        Declarations result;
        std::array<unsigned,property_count> ranks{};
        auto apply = [&](const Declarations &props, unsigned score) {
            for (unsigned p = 0; p < property_count; ++p) {
                const auto bit = std::uint8_t(1u << p);
                if (!(props.set & bit)) continue;
                const auto rank = score + ((props.important & bit) ? 100000u : 0u);
                if (!(result.set & bit) || rank >= ranks[p]) {
                    ranks[p] = rank; result.set |= bit; result.values[p] = props.values[p];
                }
            }
        };
        for (const auto &rule : rules_)
            if (matches(rule.selector,tag,id,classes)) apply(rule.declarations,rule.specificity);
        if (!inline_style.empty()) apply(declarations(inline_style),10000);
        if (slot) {
            // Own exact key bytes; never retain views into the tokenizer's tag
            // buffer. Oversized attributes bypass the cache without truncation.
            std::string key; key.reserve(key_size);
            for (std::size_t i=0; i<parts.size(); ++i) {
                key += parts[i]; slot->lengths[i] = std::uint16_t(parts[i].size());
            }
            slot->key = std::move(key); slot->hash = hash;
            slot->result = result; slot->valid = true;
        }
        return result;
    }
};
inline void inherit(Text &text, const Declarations &props) {
    if (props.set & (1u << color)) text.color = std::uint32_t(props.values[color]);
    if (props.set & (1u << size)) {
        const int value = props.values[size];
        text.scale = std::uint16_t(std::clamp(value < 0 ? -value * int(text.scale) / 100 : value,50,200));
    }
    if (props.set & (1u << whitespace)) text.whitespace = WhiteSpace(props.values[whitespace]);
    if (props.set & (1u << align)) text.align = Align(props.values[align]);
}
} // namespace browser::css
