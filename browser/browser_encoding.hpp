#pragma once
#include "browser_core.hpp"

namespace browser {
enum class TextEncoding { utf8, gb18030, unsupported };
inline TextEncoding text_encoding(std::string_view label) {
    const auto s = lowercase(trim(label));
    if (s == "utf-8" || s == "utf8" || s == "us-ascii") return TextEncoding::utf8;
    if (s == "gbk" || s == "gb18030" || s == "gb2312" || s == "gb_2312" ||
        s == "gb_2312-80" || s == "chinese" || s == "csgb2312" || s == "x-gbk" ||
        s == "csiso58gb231280" || s == "iso-ir-58") return TextEncoding::gb18030;
    return TextEncoding::unsupported;
}
inline std::optional<std::string> content_charset(std::string_view content) {
    auto s = lowercase(content);
    for (std::string_view rest = s; !rest.empty();) {
        auto end = rest.find(';'), equals = rest.find('=');
        if (equals < end && trim(rest.substr(0, equals)) == "charset") {
            auto value = trim(rest.substr(equals + 1, end == rest.npos ? end : end - equals - 1));
            if (value.size() >= 2 && (value.front() == '\'' || value.front() == '"') &&
                value.back() == value.front()) value = value.substr(1, value.size() - 2);
            return std::string(value);
        }
        if (end == rest.npos) break;
        rest.remove_prefix(end + 1);
    }
    return {};
}
// Bounded prescan of the first 1024 bytes, independent of HTTP read boundaries.
// BOM > explicit HTTP declaration > meta > UTF-8 default. Ignore comments and
// script/style text rather than mistaking a quoted <meta> for a declaration.
inline TextEncoding sniff_encoding(std::string_view type, std::string_view prefix, bool html) {
    prefix = prefix.substr(0, 1024);
    if (prefix.starts_with("\xef\xbb\xbf")) return TextEncoding::utf8;
    if (prefix.starts_with("\xff\xfe") || prefix.starts_with("\xfe\xff")) return TextEncoding::unsupported;
    if (auto label = content_charset(type)) return text_encoding(*label);
    if (!html) return TextEncoding::utf8;
    const auto lower_prefix = lowercase(prefix);
    for (std::size_t pos = 0; pos < prefix.size();) {
        pos = prefix.find('<', pos);
        if (pos == prefix.npos) break;
        if (prefix.substr(pos).starts_with("<!--")) {
            const auto end = prefix.find("-->", pos + 4);
            if (end == prefix.npos) break;
            pos = end + 3; continue;
        }
        auto end = pos + 1; char quote = 0;
        for (; end < prefix.size(); ++end) {
            const char c = prefix[end];
            if (quote) { if (c == quote) quote = 0; }
            else if (c == '\'' || c == '"') quote = c;
            else if (c == '>') break;
        }
        if (end == prefix.size()) break;
        const auto t = tag(prefix.substr(pos + 1, end - pos - 1));
        pos = end + 1;
        if (!t.closing && (t.name == "script" || t.name == "style")) {
            pos = lower_prefix.find("</" + t.name, pos);
            if (pos == prefix.npos) break;
        }
        if (t.closing || t.name != "meta") continue;
        if (t.has("charset")) return text_encoding(t.get("charset"));
        if (lowercase(t.get("http-equiv")) == "content-type")
            if (auto label = content_charset(t.get("content"))) return text_encoding(*label);
    }
    return TextEncoding::utf8;
}

inline constexpr std::size_t gb_pairs = 23940, gb_ranges = 207,
                             gb_map_bytes = gb_pairs * 2 + gb_ranges * 8;
// The caller owns the optional table. Three bytes of carry survive arbitrary
// network boundaries; malformed sequences restore ASCII delimiters as required
// by WHATWG. No per-codepoint allocation, lookup cache or persistent decoder.
class Gb18030Decoder {
    std::span<const std::byte> map_;
    unsigned first_ = 0, second_ = 0, third_ = 0;
    std::uint32_t number(std::size_t at, unsigned count) const {
        std::uint32_t n = 0;
        for (unsigned i = 0; i < count; ++i) n |= std::to_integer<unsigned>(map_[at + i]) << (i * 8);
        return n;
    }
    std::uint32_t range(unsigned pointer) const {
        if ((pointer > 39419 && pointer < 189000) || pointer > 1237575) return 0xfffd;
        if (pointer == 7457) return 0xe7c7;
        std::size_t lo = 0, hi = gb_ranges;
        while (lo + 1 < hi) {
            const auto mid = (lo + hi) / 2;
            if (number(gb_pairs * 2 + mid * 8, 4) <= pointer) lo = mid;
            else hi = mid;
        }
        const auto at = gb_pairs * 2 + lo * 8;
        return number(at + 4, 4) + pointer - number(at, 4);
    }
    void byte(unsigned c, std::string &out) {
        if (third_) {
            auto second = second_, third = third_;
            const auto pointer = (first_ - 0x81) * 12600 + (second - 0x30) * 1260 +
                                 (third - 0x81) * 10 + c - 0x30;
            first_ = second_ = third_ = 0;
            if (c >= 0x30 && c <= 0x39) utf8(out, range(pointer));
            else { utf8(out, 0xfffd); byte(second, out); byte(third, out); byte(c, out); }
        } else if (second_) {
            if (c >= 0x81 && c <= 0xfe) third_ = c;
            else {
                auto second = second_; first_ = second_ = 0;
                utf8(out, 0xfffd); byte(second, out); byte(c, out);
            }
        } else if (first_) {
            if (c >= 0x30 && c <= 0x39) { second_ = c; return; }
            const auto first = first_; first_ = 0;
            if ((c >= 0x40 && c <= 0x7e) || (c >= 0x80 && c <= 0xfe))
                utf8(out, number(((first - 0x81) * 190 + c - (c < 0x7f ? 0x40 : 0x41)) * 2, 2));
            else { utf8(out, 0xfffd); if (c < 128) byte(c, out); }
        } else if (c < 128) out += char(c);
        else if (c == 0x80) utf8(out, 0x20ac);
        else if (c >= 0x81 && c <= 0xfe) first_ = c;
        else utf8(out, 0xfffd);
    }
public:
    explicit Gb18030Decoder(std::span<const std::byte> map) : map_(map) {}
    void feed(std::string_view input, std::string &output) {
        if (map_.size() != gb_map_bytes) return;
        for (unsigned char c : input) byte(c, output);
    }
    void finish(std::string &output) {
        if (first_) utf8(output, 0xfffd);
        first_ = second_ = third_ = 0;
    }
};
} // namespace browser
