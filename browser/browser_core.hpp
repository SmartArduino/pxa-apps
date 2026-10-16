#pragma once
// Independent of PXA: parsing/navigation can be regression-tested without a VM.
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "browser_inline_image.hpp"

namespace browser {
using namespace std::literals;
enum class Error { invalid_url, unsupported_scheme, limit, unsupported_form };
constexpr std::size_t max_url = 1024, max_text = 49152, max_spans = 768, max_links = 96,
                      max_fields = 24, max_forms = 8, max_pieces = 4096, max_images = 32;
inline bool space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}
inline char lower(char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
inline std::string lowercase(std::string_view s) {
    std::string r(s);
    for (auto &c : r)
        c = lower(c);
    return r;
}
inline std::string_view trim(std::string_view s) {
    while (!s.empty() && space(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && space(s.back()))
        s.remove_suffix(1);
    return s;
}
inline void utf8(std::string &s, std::uint32_t cp) {
    if (cp == 0 || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        cp = 0xfffd;
    if (cp < 128)
        s += char(cp);
    else if (cp < 2048) {
        s += char(0xc0 | (cp >> 6));
        s += char(0x80 | (cp & 63));
    } else if (cp < 65536) {
        s += char(0xe0 | (cp >> 12));
        s += char(0x80 | ((cp >> 6) & 63));
        s += char(0x80 | (cp & 63));
    } else {
        s += char(0xf0 | (cp >> 18));
        s += char(0x80 | ((cp >> 12) & 63));
        s += char(0x80 | ((cp >> 6) & 63));
        s += char(0x80 | (cp & 63));
    }
}
inline std::string entities(std::string_view s) {
    std::string r;
    r.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') {
            r += s[i];
            continue;
        }
        auto end = s.find(';', i + 1);
        if (end == s.npos || end - i > 32) {
            r += '&';
            continue;
        }
        auto name = s.substr(i + 1, end - i - 1);
        std::uint32_t cp = 0;
        if (name == "amp")
            cp = '&';
        else if (name == "lt")
            cp = '<';
        else if (name == "gt")
            cp = '>';
        else if (name == "quot")
            cp = '"';
        else if (name == "apos")
            cp = '\'';
        else if (name == "nbsp")
            cp = ' ';
        else if (name == "copy")
            cp = 0xa9;
        else if (name == "mdash")
            cp = 0x2014;
        else if (name == "ndash")
            cp = 0x2013;
        else if (name == "hellip")
            cp = 0x2026;
        else if (name.starts_with('#')) {
            name.remove_prefix(1);
            int base = 10;
            if (name.starts_with('x') || name.starts_with('X')) {
                name.remove_prefix(1);
                base = 16;
            }
            auto [p, ec] = std::from_chars(name.data(), name.data() + name.size(), cp, base);
            if (ec != std::errc{} || p != name.data() + name.size()) {
                r += '&';
                continue;
            }
            if (!cp)
                cp = 0xfffd;
        }
        if (!cp) {
            r += '&';
            continue;
        }
        utf8(r, cp);
        i = end;
    }
    return r;
}
inline std::string encode(std::string_view s, bool form = false) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string r;
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
            r += char(c);
        else if (c == ' ' && form)
            r += '+';
        else {
            r += '%';
            r += hex[c >> 4];
            r += hex[c & 15];
        }
    }
    return r;
}
// HTTP URL references. Reject userinfo, controls and backslashes; never
// silently reinterpret a script/file URL. Unicode path/query are UTF-8 percent
// encoded.
inline std::expected<std::string, Error> resolve(std::string_view base, std::string_view ref,
                                                 bool typed = false) {
    ref = trim(ref);
    if (ref.size() > max_url)
        return std::unexpected(Error::limit);
    for (unsigned char c : ref)
        if (c < 32 || c == 127 || c == '\\')
            return std::unexpected(Error::invalid_url);
    std::string absolute;
    auto colon = ref.find(':'), slash = ref.find_first_of("/?#");
    bool scheme = colon != ref.npos && (slash == ref.npos || colon < slash);
    if (typed && !scheme && !ref.starts_with('/') && !ref.starts_with('#') && !ref.starts_with('?'))
        absolute = "https://" + std::string(ref);
    else if (scheme)
        absolute = ref;
    else {
        auto origin_end =
            base.find_first_of("/?#", base.find("://") == base.npos ? 0 : base.find("://") + 3);
        if (base.find("://") == base.npos)
            return std::unexpected(Error::invalid_url);
        auto origin = base.substr(0, origin_end);
        if (ref.starts_with("//"))
            absolute = std::string(base.substr(0, base.find(':') + 1)) + std::string(ref);
        else if (ref.starts_with('/'))
            absolute = std::string(origin) + std::string(ref);
        else if (ref.starts_with('#'))
            absolute = std::string(base.substr(0, base.find('#'))) + std::string(ref);
        else if (ref.starts_with('?'))
            absolute = std::string(base.substr(0, base.find_first_of("?#"))) + std::string(ref);
        else if (ref.empty())
            absolute = base;
        else {
            auto clean = base.substr(0, base.find_first_of("?#"));
            auto last = clean.rfind('/');
            absolute = (origin_end == base.npos || last < base.find("://") + 3
                            ? std::string(origin) + "/"
                            : std::string(clean.substr(0, last + 1))) +
                       std::string(ref);
        }
    }
    auto sep = absolute.find("://");
    if (sep == absolute.npos)
        return std::unexpected(Error::unsupported_scheme);
    auto protocol = lowercase(std::string_view(absolute).substr(0, sep));
    if (protocol != "http" && protocol != "https")
        return std::unexpected(Error::unsupported_scheme);
    auto auth_end = absolute.find_first_of("/?#", sep + 3);
    if (auth_end == absolute.npos)
        auth_end = absolute.size();
    auto authority = std::string_view(absolute).substr(sep + 3, auth_end - sep - 3);
    if (authority.empty() || authority.find_first_of("@ %") != authority.npos)
        return std::unexpected(Error::invalid_url);
    for (unsigned char c : authority)
        if (c >= 128)
            return std::unexpected(Error::invalid_url); // IDNA requires a separate module.
    auto path_end = absolute.find_first_of("?#", auth_end);
    if (path_end == absolute.npos)
        path_end = absolute.size();
    auto path = std::string_view(absolute).substr(auth_end, path_end - auth_end);
    // Remove dot segments without collapsing repeated slashes (resource names
    // may distinguish /a//b from /a/b).
    std::string remaining(path.empty() ? "/" : path), normalized;
    while (!remaining.empty()) {
        if (remaining.starts_with("../"))
            remaining.erase(0, 3);
        else if (remaining.starts_with("./"))
            remaining.erase(0, 2);
        else if (remaining.starts_with("/./"))
            remaining.erase(0, 2);
        else if (remaining == "/.")
            remaining = "/";
        else if (remaining.starts_with("/../") || remaining == "/..") {
            remaining.erase(0, 3);
            if (remaining.empty())
                remaining = "/";
            auto last = normalized.rfind('/');
            normalized.resize(last == normalized.npos ? 0 : last);
        } else if (remaining == "." || remaining == "..")
            remaining.clear();
        else {
            auto end = remaining.find('/', remaining.starts_with('/') ? 1 : 0);
            if (end == remaining.npos)
                end = remaining.size();
            normalized.append(remaining, 0, end);
            remaining.erase(0, end);
        }
    }
    std::string result = protocol + "://" + lowercase(authority) + normalized;
    result += std::string_view(absolute).substr(path_end);
    std::string out;
    out.reserve(result.size());
    constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned char c : result) {
        if (c >= 128 || c == ' ' || c == '"' || c == '<' || c == '>') {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        } else
            out += char(c);
    }
    if (out.size() > max_url)
        return std::unexpected(Error::limit);
    return out;
}
inline std::string_view transport_url(std::string_view s) { return s.substr(0, s.find('#')); }
enum class ReferrerPolicy : std::uint8_t {
    no_referrer, no_referrer_when_downgrade, origin, origin_when_cross_origin,
    same_origin, strict_origin, strict_origin_when_cross_origin, unsafe_url
};
inline std::optional<ReferrerPolicy> referrer_policy(std::string_view value) {
    constexpr std::array names{"no-referrer"sv, "no-referrer-when-downgrade"sv, "origin"sv,
        "origin-when-cross-origin"sv, "same-origin"sv, "strict-origin"sv,
        "strict-origin-when-cross-origin"sv, "unsafe-url"sv};
    std::optional<ReferrerPolicy> result;
    // HTTP permits a comma-separated fallback list; the last understood entry wins.
    for (;;) {
        const auto comma = value.find(',');
        const auto item = lowercase(trim(value.substr(0, comma)));
        for (std::size_t i = 0; i < names.size(); ++i)
            if (item == names[i]) result = ReferrerPolicy(i);
        if (comma == value.npos) return result;
        value.remove_prefix(comma + 1);
    }
}
inline std::string request_referrer(std::string_view source, std::string_view target,
    ReferrerPolicy policy = ReferrerPolicy::strict_origin_when_cross_origin) {
    // resolve rejects credentials and control characters and normalizes paths.
    const auto from = resolve({}, source), to = resolve({}, target);
    if (!from || !to) return {};
    auto url_origin = [](std::string_view u) {
        const auto end = u.find_first_of("/?#", u.find("://") + 3);
        auto value = u.substr(0, end);
        if ((u.starts_with("https:") && value.ends_with(":443")) ||
            (u.starts_with("http:") && value.ends_with(":80")))
            value = value.substr(0, value.rfind(':'));
        return value;
    };
    const bool same = url_origin(*from) == url_origin(*to);
    const bool downgrade = from->starts_with("https:") && to->starts_with("http:");
    using enum ReferrerPolicy;
    if (policy == no_referrer || (policy == same_origin && !same) ||
        (downgrade && (policy == no_referrer_when_downgrade || policy == strict_origin ||
                       policy == strict_origin_when_cross_origin))) return {};
    if (policy == origin || policy == strict_origin ||
        (!same && (policy == origin_when_cross_origin || policy == strict_origin_when_cross_origin)))
        return std::string(url_origin(*from)) + "/";
    return std::string(transport_url(*from));
}
// HTML ids contain decoded UTF-8; URL fragments may contain percent escapes.
// Compare without a temporary string. Unlike form data, '+' remains literal.
constexpr bool fragment_matches(std::string_view id, std::string_view fragment) noexcept {
    auto hex = [](char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    std::size_t matched = 0;
    for (std::size_t i = 0; i < fragment.size(); ++i) {
        auto c = fragment[i];
        if (c == '%' && fragment.size() - i >= 3) {
            const auto hi = hex(fragment[i + 1]), lo = hex(fragment[i + 2]);
            if (hi >= 0 && lo >= 0) {
                c = char(hi * 16 + lo);
                i += 2;
            }
        }
        if (matched == id.size() || id[matched++] != c)
            return false;
    }
    return matched == id.size();
}
enum class Style : std::uint8_t { body, heading, code, muted, button };
// Compatibility is opt-in: a mobile identity does not imply CSS/JS support.
inline constexpr std::string_view user_agent(bool mobile) noexcept {
    return mobile ? "Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 "
                    "(KHTML, like Gecko) "
                    "Chrome/143.0.0.0 Mobile Safari/537.36 PXA-Browser/0.1"
                  : "PXA-Browser/0.1 (Mobile; TextMode)";
}
struct Span {
    std::uint32_t offset = 0, size = 0;
    std::int16_t link = -1, field = -1;
    Style style = Style::body;
    std::int16_t image = -1;
};
struct Image {
    std::string source;
    std::uint16_t width = 240, height = 150;
    std::optional<ReferrerPolicy> referrer{};
};
struct Link {
    std::string target;
    std::uint32_t script_node = 0;
};
struct Anchor {
    std::string name;
    std::uint32_t offset = 0;
};
struct Form {
    std::string action;
    bool post = false, supported = true;
};
enum class FieldType : std::uint8_t { text, password, hidden, checkbox, submit };
struct Field {
    std::string name, value, label;
    std::int16_t form = -1;
    FieldType type = FieldType::text;
    bool checked = false;
    std::uint32_t script_node = 0;
};
struct Document {
    std::string text, title, base;
    std::vector<Span> spans;
    std::vector<Link> links;
    std::vector<Anchor> anchors;
    std::vector<Form> forms;
    std::vector<Field> fields;
    std::vector<Image> images;
    bool truncated = false, wml = false, links_limited = false;
    ReferrerPolicy referrer = ReferrerPolicy::strict_origin_when_cross_origin;
    void clear(std::string url) {
        text.clear();
        title.clear();
        base = std::move(url);
        spans.clear();
        links.clear();
        anchors.clear();
        forms.clear();
        fields.clear();
        images.clear();
        truncated = wml = links_limited = false;
        referrer = ReferrerPolicy::strict_origin_when_cross_origin;
    }
    int add_link(std::string target) {
        const auto found = std::ranges::find(links, target, &Link::target);
        if (found != links.end()) return int(found - links.begin());
        if (links.size() == max_links) { links_limited = true; return -1; }
        links.push_back({std::move(target)});
        return int(links.size() - 1);
    }
    void append(std::string_view value, Style style = Style::body, int link = -1, int field = -1,
                int image = -1) {
        if (value.empty() || truncated)
            return;
        if (value.size() > max_text - text.size() || spans.size() >= max_spans) {
            truncated = true;
            return;
        }
        if (!spans.empty() && spans.back().style == style && spans.back().link == link &&
            spans.back().field == field && image < 0 && spans.back().image < 0)
            spans.back().size += value.size();
        else
            spans.push_back({std::uint32_t(text.size()), std::uint32_t(value.size()),
                             std::int16_t(link), std::int16_t(field), style, std::int16_t(image)});
        if (text.size() + value.size() > text.capacity() && text.capacity() > max_text / 2) {
            // Normal geometric growth can almost double the 48 KiB ceiling.
            // Allocate the final capacity only when a large page needs it;
            // a fresh string also avoids reserve() doubling on some libraries.
            // Append before swapping so views into text remain valid.
            std::string grown;
            grown.reserve(max_text);
            const auto size = text.size() + value.size();
            grown.resize_and_overwrite(size, [&](char *out, std::size_t) noexcept {
                std::copy_n(text.data(), text.size(), out);
                std::copy_n(value.data(), value.size(), out + text.size());
                return size;
            });
            text.swap(grown);
        } else
            text += value;
    }
    void newline() {
        if (!text.empty() && text.back() != '\n')
            append("\n");
    }
};
struct Attribute {
    std::string_view name, value;
};
struct Tag {
    std::string name;
    bool closing = false;
    std::array<Attribute, 24> attrs{};
    std::size_t count = 0;
    std::string get(std::string_view key) const {
        for (std::size_t i = 0; i < count; ++i)
            if (lowercase(attrs[i].name) == key)
                return entities(attrs[i].value);
        return {};
    }
    bool has(std::string_view key) const {
        for (std::size_t i = 0; i < count; ++i)
            if (lowercase(attrs[i].name) == key)
                return true;
        return false;
    }
};
inline Tag tag(std::string_view s) {
    Tag t;
    s = trim(s);
    if (s.starts_with('/')) {
        t.closing = true;
        s.remove_prefix(1);
        s = trim(s);
    }
    std::size_t p = 0;
    while (p < s.size() && ((lower(s[p]) >= 'a' && lower(s[p]) <= 'z') ||
                            (s[p] >= '0' && s[p] <= '9') || s[p] == '-'))
        ++p;
    t.name = lowercase(s.substr(0, p));
    s.remove_prefix(p);
    while (!s.empty() && t.count < t.attrs.size()) {
        s = trim(s);
        if (s.empty() || s.front() == '/')
            break;
        p = 0;
        while (p < s.size() && !space(s[p]) && s[p] != '=' && s[p] != '/')
            ++p;
        if (!p)
            break;
        auto name = s.substr(0, p);
        s.remove_prefix(p);
        s = trim(s);
        std::string_view value;
        if (s.starts_with('=')) {
            s.remove_prefix(1);
            s = trim(s);
            if (s.starts_with('"') || s.starts_with('\'')) {
                char q = s.front();
                s.remove_prefix(1);
                p = s.find(q);
                if (p == s.npos)
                    p = s.size();
                value = s.substr(0, p);
                s.remove_prefix(std::min(p + 1, s.size()));
            } else {
                p = 0;
                while (p < s.size() && !space(s[p]))
                    ++p;
                value = s.substr(0, p);
                s.remove_prefix(p);
            }
        }
        t.attrs[t.count++] = {name, value};
    }
    return t;
}
// Some static cards use only a literal location assignment instead of href.
// Recognize that one grammar; never evaluate script, escapes or expressions.
inline std::optional<std::string_view> static_navigation(std::string_view code) {
    code = trim(code);
    if (code.starts_with("window.location.href")) code.remove_prefix(20);
    else if (code.starts_with("location.href")) code.remove_prefix(13);
    else return {};
    code = trim(code);
    if (!code.starts_with('=')) return {};
    code = trim(code.substr(1));
    if (code.size() < 2 || (code.front() != '\'' && code.front() != '"')) return {};
    auto end = code.find(code.front(), 1);
    if (end == code.npos) return {};
    auto url = code.substr(1,end-1), tail = trim(code.substr(end+1));
    if ((!tail.empty() && tail != ";") || url.find('\\') != url.npos) return {};
    return url;
}
// A bounded streaming tokenizer, not a DOM. Partial UTF-8/entities/tags survive
// arbitrary transport boundaries. Script/style raw text never becomes content.
class Parser {
    Document &doc_;
    bool scripted_ = false;
    static std::uint32_t node_id(std::string_view value) {
        std::uint32_t id = 0; auto [end, error] = std::from_chars(value.data(), value.data()+value.size(), id);
        return error == std::errc{} && end == value.data()+value.size() ? id : 0;
    }
    std::string pending_, raw_, entity_, label_, hidden_;
    unsigned hidden_depth_ = 0;
    bool in_label_ = false;
    bool in_tag_ = false, comment_ = false, discard_tag_ = false;
    char quote_ = 0;
    Style style_ = Style::body;
    int link_ = -1, form_ = -1;
    bool head_ = false, title_ = false, pre_ = false, last_space_ = false;
    std::array<char, 4> unicode_{};
    unsigned unicode_size_ = 0, unicode_want_ = 0;
    void glyph(std::string_view s) {
        if (hidden_depth_) return;
        if (title_) {
            if (doc_.title.size() + s.size() <= 256)
                doc_.title += s;
            return;
        }
        if (head_ || !raw_.empty() || hidden_depth_)
            return;
        if (in_label_ && label_.size() + s.size() <= 128) label_ += s;
        doc_.append(s, style_, link_);
        last_space_ = false;
    }
    void character(unsigned char c) {
        if (unicode_want_) {
            if ((c & 0xc0) != 0x80) {
                glyph("\xef\xbf\xbd");
                unicode_want_ = unicode_size_ = 0;
                character(c);
                return;
            }
            unicode_[unicode_size_++] = char(c);
            if (unicode_size_ == unicode_want_) {
                auto u = static_cast<unsigned char>(unicode_[0]);
                auto b = static_cast<unsigned char>(unicode_[1]);
                bool valid = !(u == 0xe0 && b < 0xa0) && !(u == 0xed && b >= 0xa0) &&
                             !(u == 0xf0 && b < 0x90) && !(u == 0xf4 && b >= 0x90);
                glyph(valid ? std::string_view(unicode_.data(), unicode_size_) : "\xef\xbf\xbd"sv);
                unicode_want_ = unicode_size_ = 0;
            }
            return;
        }
        if (c >= 128) {
            unicode_want_ = c >= 0xc2 && c <= 0xdf   ? 2
                            : c >= 0xe0 && c <= 0xef ? 3
                            : c >= 0xf0 && c <= 0xf4 ? 4
                                                     : 0;
            if (unicode_want_) {
                unicode_[0] = char(c);
                unicode_size_ = 1;
            } else
                glyph("\xef\xbf\xbd");
            return;
        }
        if (c < 32 && !space(c))
            return;
        if (space(c) && !pre_) {
            if (!last_space_ &&
                (title_ ? !doc_.title.empty() : (!doc_.text.empty() && doc_.text.back() != '\n')))
                glyph(" ");
            last_space_ = true;
        } else {
            char v = char(c);
            glyph({&v, 1});
        }
    }
    void textchar(char c) {
        if (!entity_.empty()) {
            if (c == ';') {
                entity_ += ';';
                auto decoded = entities(entity_);
                entity_.clear();
                for (unsigned char b : decoded)
                    character(b);
                return;
            }
            if (entity_.size() < 32 && !space(c) && c != '<' && c != '&') {
                entity_ += c;
                return;
            }
            auto literal = std::move(entity_);
            entity_.clear();
            for (unsigned char b : literal)
                character(b);
        }
        if (c == '&')
            entity_ = "&";
        else
            character(static_cast<unsigned char>(c));
    }
    void handle(std::string_view value) {
        auto t = tag(value);
        auto &n = t.name;
        if (n.empty())
            return;
        if (!raw_.empty()) {
            if (t.closing && n == raw_)
                raw_.clear();
            return;
        }
        if (n == "script" || n == "style") {
            if (!t.closing)
                raw_ = n;
            return;
        }
        // Honor explicit hiding without building a CSS tree. Count only the
        // root tag's nesting: unrelated/mismatched child closes cannot reveal
        // a hidden login dialog or accidentally hide the rest of a valid page.
        if (hidden_depth_) {
            if (n == hidden_) {
                if (t.closing) --hidden_depth_;
                else if (!trim(value).ends_with('/')) ++hidden_depth_;
            }
            return;
        }
        if (!t.closing) {
            bool hidden = t.has("hidden");
            auto css = lowercase(t.get("style"));
            for (std::string_view rest = css; !rest.empty();) {
                auto end = rest.find(';');
                auto rule = rest.substr(0, end);
                auto colon = rule.find(':');
                if (colon != rule.npos) {
                    auto name = trim(rule.substr(0, colon));
                    auto v = trim(rule.substr(colon + 1));
                    v = trim(v.substr(0, v.find('!')));
                    hidden |= name == "display" && v == "none";
                }
                if (end == rest.npos) break;
                rest.remove_prefix(end + 1);
            }
            if (hidden) {
                constexpr std::string_view voids = " area base br col embed hr img input link meta param source track wbr ";
                if (!trim(value).ends_with('/') && voids.find(" " + n + " ") == voids.npos) {
                    hidden_ = n; hidden_depth_ = 1;
                }
                return;
            }
        }
        if (n == "head") {
            head_ = !t.closing;
            return;
        }
        if (n == "meta" && !t.closing && lowercase(t.get("name")) == "referrer") {
            if (auto policy = referrer_policy(t.get("content"))) doc_.referrer = *policy;
            return;
        }
        if (n == "title") {
            title_ = !t.closing;
            return;
        }
        if (n == "body")
            head_ = title_ = false;
        if (n == "base" && !t.closing) {
            if (auto url = resolve(doc_.base, t.get("href")); url)
                doc_.base = *url;
            return;
        }
        if (n == "wml" && !t.closing) {
            doc_.wml = true;
            if (doc_.forms.empty()) doc_.forms.push_back({doc_.base});
            form_ = 0;
        }
        if (n == "card" && !t.closing) {
            head_ = title_ = false;
            doc_.newline();
            auto title = t.get("title");
            if (doc_.title.empty()) doc_.title = title;
            doc_.append(title, Style::heading); doc_.newline();
        }
        if (doc_.wml && n == "go" && !t.closing) {
            auto method = lowercase(t.get("method"));
            if (method.empty() || method == "get")
                if (auto target = resolve(doc_.base, t.get("href")); target) {
                    int index = doc_.add_link(std::move(*target));
                    doc_.append(" [打开] ", Style::body, index);
                }
        }
        if (head_)
            return;
        bool block = n == "p" || n == "div" || n == "section" || n == "article" || n == "header" ||
                     n == "footer" || n == "main" || n == "ul" || n == "ol" || n == "li" ||
                     n == "blockquote" || n == "tr" || n == "pre" || n == "h1" || n == "h2" ||
                     n == "h3" || n == "h4" || n == "h5" || n == "h6" || n == "form";
        if (block || n == "br" || n == "hr")
            doc_.newline();
        if (!t.closing && doc_.anchors.size() < 96) {
            auto id = t.get("id");
            if (id.empty() && n == "a")
                id = t.get("name");
            if (!id.empty() && id.size() <= 128)
                doc_.anchors.push_back({std::move(id), std::uint32_t(doc_.text.size())});
        }
        if (!t.closing && t.has("onclick")) {
            const auto handler = t.get("onclick");
            if (auto literal = static_navigation(handler))
                if (auto url = resolve(doc_.base, *literal)) {
                    const int index = doc_.add_link(std::move(*url));
                    doc_.append("打开链接", Style::body, index); doc_.newline();
                }
        }
        if (n == "h1" || n == "h2" || n == "h3" || n == "h4" || n == "h5" || n == "h6")
            style_ = t.closing ? Style::body : Style::heading;
        if (n == "pre") {
            pre_ = !t.closing;
            style_ = pre_ ? Style::code : Style::body;
        }
        if (n == "li" && !t.closing)
            doc_.append("• ");
        if (n == "td" || n == "th")
            doc_.append("  ");
        if (n == "a") {
            if (t.closing)
                link_ = -1;
            else {
                link_ = -1;
                auto href = t.get("href");
                if (const auto id = scripted_ ? node_id(t.get("data-pxa-click")) : 0) {
                    if (doc_.links.size() < max_links) {
                        link_ = int(doc_.links.size());
                        const auto target = resolve(doc_.base, href);
                        doc_.links.push_back({target ? *target : std::string{}, id});
                    }
                } else if (!href.empty())
                    if (auto url = resolve(doc_.base, href); url)
                        link_ = doc_.add_link(std::move(*url));
            }
        }
        if (n == "img" && !t.closing) {
            auto alt = t.get("alt");
            auto src = t.get("src");
            if (src.empty())
                src = t.get("data-src");
            auto source = resolve(doc_.base, src);
            if (inline_image::payload(src)) {
                std::size_t used = 0;
                for (const auto& image : doc_.images)
                    if (image.source.starts_with(inline_image::png_prefix)) used += image.source.size();
                if (src.size() <= inline_image::document_budget - std::min(used, inline_image::document_budget))
                    source = std::string(src);
            }
            auto dimension = [&](std::string_view name, int fallback) {
                auto value = t.get(name);
                int parsed = 0;
                auto [end, error] =
                    std::from_chars(value.data(), value.data() + value.size(), parsed);
                return error == std::errc{} && end == value.data() + value.size() && parsed > 0 &&
                               parsed <= 4096
                           ? parsed
                           : fallback;
            };
            int w = dimension("width", 240), h = dimension("height", 150);
            if (w <= 1 && h <= 1)
                return; // Common tracking pixels have no reading content.
            if (!src.empty() && source && doc_.images.size() < max_images) {
                int index = int(doc_.images.size());
                doc_.newline();
                doc_.images.push_back({std::move(*source), std::uint16_t(w), std::uint16_t(h),
                    referrer_policy(t.get("referrerpolicy"))});
                doc_.append(alt.empty() ? "图片" : alt, Style::muted, link_, -1, index);
                doc_.newline();
            } else
                doc_.append(alt.empty() ? "[图片]" : "[图片: " + alt + "]", Style::muted, link_);
        }
        if (n == "label") {
            in_label_ = !t.closing;
            if (in_label_) label_.clear();
        }
        if (n == "form") {
            if (t.closing)
                form_ = -1;
            else if (doc_.forms.size() < max_forms) {
                auto method = lowercase(t.get("method"));
                auto enctype = lowercase(t.get("enctype"));
                auto url = resolve(doc_.base, t.get("action"));
                form_ = int(doc_.forms.size());
                doc_.forms.push_back(
                    {url ? *url : "", method == "post",
                     bool(url) && (method.empty() || method == "get" || method == "post") &&
                         (enctype.empty() || enctype == "application/x-www-form-urlencoded")});
            }
        }
        if ((n == "input" || n == "button") && !t.closing && form_ >= 0 &&
            doc_.fields.size() >= max_fields)
            doc_.forms[form_].supported = false;
        if ((n == "input" || n == "button") && !t.closing && (form_ >= 0 || scripted_) &&
            doc_.fields.size() < max_fields) {
            auto type = lowercase(t.get("type"));
            if (type == "reset" || type == "button") return;
            if (type == "file" || type == "image" || type == "radio") {
                if (form_ >= 0) doc_.forms[form_].supported = false;
                return;
            }
            if (t.has("disabled"))
                return;
            auto ft = n == "button" || type == "submit" ? FieldType::submit
                      : type == "hidden"                ? FieldType::hidden
                      : type == "password"              ? FieldType::password
                      : type == "checkbox"              ? FieldType::checkbox
                                                        : FieldType::text;
            auto name = t.get("name"), v = t.get("value"), label = t.get("placeholder");
            if (name.size() > 128 || v.size() > 512 || label.size() > 128) {
                if (form_ >= 0) doc_.forms[form_].supported = false;
                return;
            }
            if (label.empty() && ft == FieldType::submit) label = v;
            if (label.empty() && ft != FieldType::hidden && ft != FieldType::submit)
                label = trim(label_);
            if (ft != FieldType::hidden) label_.clear();
            if (label.empty())
                label = name;
            if (label.empty())
                label = ft == FieldType::submit ? "提交" : "输入";
            if (ft == FieldType::checkbox && v.empty())
                v = "on";
            int index = int(doc_.fields.size());
            doc_.fields.push_back({std::move(name), std::move(v), std::move(label),
                                   std::int16_t(form_), ft, t.has("checked"), scripted_ ? node_id(t.get("data-pxa-field")) : 0});
            if (ft != FieldType::hidden) {
                doc_.newline();
                doc_.append(doc_.fields.back().label, Style::button, -1, index);
                doc_.newline();
            }
        }
        if ((n == "select" || n == "textarea") && !t.closing && form_ >= 0)
            doc_.forms[form_].supported = false;
    }

  public:
    explicit Parser(Document &d, bool scripted = false) : doc_(d), scripted_(scripted) {
        pending_.reserve(2048);
        entity_.reserve(32);
    }
    void feed(std::string_view bytes) {
        for (char c : bytes) {
            if (doc_.truncated)
                return;
            if (comment_) {
                pending_ += c;
                if (pending_.ends_with("-->")) {
                    comment_ = false;
                    pending_.clear();
                } else if (pending_.size() > 8)
                    pending_.erase(0, pending_.size() - 3);
                continue;
            }
            if (in_tag_) {
                if (!raw_.empty()) {
                    if (c == '<')
                        pending_.clear();
                    else if (c == '>') {
                        handle(pending_);
                        pending_.clear();
                        in_tag_ = false;
                    } else if (pending_.size() < 32)
                        pending_ += c;
                    else {
                        pending_.clear();
                        in_tag_ = false;
                    }
                    continue;
                }
                if (pending_.empty() && c != '/' && c != '!' && c != '?' &&
                    !(lower(c) >= 'a' && lower(c) <= 'z')) {
                    in_tag_ = false;
                    textchar('<');
                    textchar(c);
                    continue;
                }
                if (discard_tag_) {
                    if (c == '>') {
                        discard_tag_ = in_tag_ = false;
                        pending_.clear();
                    }
                    continue;
                }
                if (quote_) {
                    if (c == quote_)
                        quote_ = 0;
                } else if (c == '"' || c == '\'')
                    quote_ = c;
                else if (c == '>') {
                    handle(pending_);
                    pending_.clear();
                    in_tag_ = false;
                    continue;
                }
                pending_ += c;
                if (pending_ == "!--") {
                    comment_ = true;
                    in_tag_ = false;
                    quote_ = 0;
                    continue;
                }
                const bool image_tag = pending_.size() >= 4 && lower(pending_[0]) == 'i' &&
                    lower(pending_[1]) == 'm' && lower(pending_[2]) == 'g' && space(pending_[3]);
                if (pending_.size() >= (image_tag ? 4608u : 2048u)) {
                    discard_tag_ = true;
                    pending_.clear();
                }
                continue;
            }
            if (c == '<') {
                if (!entity_.empty()) {
                    auto literal = std::move(entity_);
                    entity_.clear();
                    for (unsigned char b : literal)
                        character(b);
                }
                in_tag_ = true;
                pending_.clear();
                quote_ = 0;
            } else if (raw_.empty())
                textchar(c);
        }
    }
    void feed_plain(std::string_view bytes) {
        pre_ = true;
        for (unsigned char c : bytes)
            character(c);
    }
    void finish() {
        if (!entity_.empty()) {
            auto v = std::move(entity_);
            entity_.clear();
            for (unsigned char c : v)
                character(c);
        }
        if (unicode_want_) {
            glyph("\xef\xbf\xbd");
            unicode_want_ = unicode_size_ = 0;
        }
        if (in_tag_ && !discard_tag_ && raw_.empty() && !head_) {
            glyph("<");
            for (unsigned char c : pending_)
                character(c);
        }
        doc_.newline();
        doc_.title = std::string(trim(doc_.title));
    }
};
struct Submission {
    std::string url, body;
    bool post = false;
};
inline std::expected<Submission, Error> submit(const Document &doc, std::size_t form,
                                               int clicked = -1) {
    if (form >= doc.forms.size() || !doc.forms[form].supported)
        return std::unexpected(Error::unsupported_form);
    Submission out{doc.forms[form].action, {}, doc.forms[form].post};
    if (!out.post && std::ranges::any_of(doc.fields, [&](const auto &f) {
            return f.form == int(form) && f.type == FieldType::password;
        })) return std::unexpected(Error::unsupported_form);
    for (std::size_t i = 0; i < doc.fields.size(); ++i) {
        auto &f = doc.fields[i];
        if (f.form != int(form) || f.name.empty() ||
            (f.type == FieldType::submit && int(i) != clicked) ||
            (f.type == FieldType::checkbox && !f.checked))
            continue;
        auto pair = encode(f.name, true) + "=" + encode(f.value, true);
        if (out.body.size() + pair.size() + 1 > 2048)
            return std::unexpected(Error::limit);
        if (!out.body.empty())
            out.body += '&';
        out.body += pair;
    }
    if (!out.post) {
        out.url = std::string(transport_url(out.url));
        auto q = out.url.find('?');
        if (q != out.url.npos)
            out.url.resize(q);
        out.url += '?';
        out.url += out.body;
        out.body.clear();
        if (out.url.size() > max_url)
            return std::unexpected(Error::limit);
    }
    return out;
}
inline std::expected<std::string, Error> wml_target(const Document &doc, std::string_view target) {
    if (!doc.wml) return resolve(doc.base, target);
    std::string result;
    for (std::size_t i = 0; i < target.size();) {
        if (target[i] != '$') { result += target[i++]; continue; }
        ++i;
        if (i < target.size() && target[i] == '$') { result += '$'; ++i; continue; }
        if (i >= target.size() || target[i] != '(') return std::unexpected(Error::unsupported_form);
        auto end = target.find(')', ++i);
        if (end == target.npos) return std::unexpected(Error::unsupported_form);
        auto name = target.substr(i, end - i);
        auto field = std::ranges::find_if(doc.fields, [&](const auto &f) { return f.name == name; });
        if (field == doc.fields.end()) return std::unexpected(Error::unsupported_form);
        result += encode(field->value); i = end + 1;
        if (result.size() > max_url) return std::unexpected(Error::limit);
    }
    return resolve(doc.base, result);
}
struct Piece {
    std::uint32_t offset = 0, y = 0;
    std::uint16_t size = 0, x = 0, width = 0, span = 0;
};
inline int glyph_advance(unsigned char c, int font) noexcept {
    return c == ' ' ? std::max(1, font / 2)
           : c < 128 && std::string_view("ilI.,:;!'|`").find(char(c)) != std::string_view::npos
               ? std::max(1, font / 2)
           : c < 128 && c != 'W' && c != 'M' ? std::max(1, font * 3 / 4)
                                             : font;
}
// A single clipped line must also be bounded before command encoding. Keep
// complete UTF-8 characters and leave long URLs/field values in their owner.
inline std::string_view line_prefix(std::string_view text, int width, int font) noexcept {
    std::size_t end = 0;
    int used = 0;
    for (std::size_t p = 0; p < text.size();) {
        auto c = static_cast<unsigned char>(text[p]);
        auto n = std::min<std::size_t>(c < 128   ? 1
                                       : c < 224 ? 2
                                       : c < 240 ? 3
                                                 : 4,
                                       text.size() - p);
        // Conservative, matching Layout's widths; this is not font shaping.
        int advance = glyph_advance(c, font);
        if (used + advance > width || c == '\n' || c == '\r')
            break;
        used += advance;
        p += n;
        end = p;
    }
    return text.substr(0, end);
}
struct Layout {
    std::vector<Piece> pieces;
    int height = 0;
    bool truncated = false;
    static int font(Style style, int body) { return style == Style::heading ? body * 5 / 4 : body; }
    static std::pair<int, int> image_size(const Image &image, int width, int body) {
        int w = std::clamp(int(image.width) * body / 15, 1, width);
        return {w, std::clamp(int(std::int64_t(w) * image.height / std::max(1, int(image.width))),
                              body * 2, 4096)};
    }
    void build(const Document &d, int width, int body) {
        pieces.clear();
        height = 0;
        truncated = false;
        int x = 0, y = 0,
            line_height = body * 3 / 2; // bounded by document, no frame-time allocation
        for (std::size_t si = 0; si < d.spans.size(); ++si) {
            auto &s = d.spans[si];
            if (s.image >= 0 && std::size_t(s.image) < d.images.size()) {
                if (pieces.size() >= max_pieces) {
                    truncated = true;
                    break;
                }
                if (x)
                    y += line_height;
                auto [w, h] = image_size(d.images[s.image], width, body);
                pieces.push_back(
                    {s.offset, std::uint32_t(y), 0, 0, std::uint16_t(w), std::uint16_t(si)});
                y += h + body / 2;
                x = 0;
                line_height = body * 3 / 2;
                continue;
            }
            int size = font(s.style, body);
            std::size_t start = s.offset, p = start, end = start + s.size;
            int begin_x = x;
            auto emit = [&] {
                if (p > start) {
                    if (pieces.size() >= max_pieces) {
                        truncated = true;
                        return;
                    }
                    pieces.push_back({std::uint32_t(start), std::uint32_t(y),
                                      std::uint16_t(p - start), std::uint16_t(begin_x),
                                      std::uint16_t(std::max(1, x - begin_x)), std::uint16_t(si)});
                }
                start = p;
                begin_x = x;
            };
            while (p < end && !truncated) {
                unsigned char c = d.text[p];
                std::size_t n = c < 128 ? 1 : c < 224 ? 2 : c < 240 ? 3 : 4;
                n = std::min(n, end - p);
                if (c == '\n') {
                    emit();
                    ++p;
                    start = p;
                    y += line_height;
                    x = 0;
                    begin_x = 0;
                    line_height = body * 3 / 2;
                    continue;
                }
                // Full em for unknown glyphs avoids overlaps for CJK/fallback
                // fonts. ASCII uses conservative widths for proportional text.
                int advance = glyph_advance(c, size);
                int word_width = advance;
                // Keep ordinary Latin words together. Scan only at a word's
                // start, once per word, without a second token array. Long
                // URLs still wrap by character; preformatted text is literal.
                if (s.style != Style::code && lower(char(c)) >= 'a' && lower(char(c)) <= 'z' &&
                    (p == s.offset || space(d.text[p - 1]) ||
                     static_cast<unsigned char>(d.text[p - 1]) >= 128)) {
                    for (auto q = p + 1; q < end && word_width <= width; ++q) {
                        const auto next = static_cast<unsigned char>(d.text[q]);
                        if (next >= 128 || space(char(next)))
                            break;
                        word_width += glyph_advance(next, size);
                    }
                    if (word_width > width)
                        word_width = advance;
                }
                if (x + word_width > width && x) {
                    emit();
                    y += line_height;
                    x = 0;
                    begin_x = 0;
                    start = p;
                    line_height = body * 3 / 2;
                }
                x += advance;
                line_height = std::max(line_height, s.field >= 0 ? size * 2 : size * 3 / 2);
                p += n;
            }
            emit();
            if (truncated)
                break;
        }
        height = y + line_height;
    }
};
struct HistoryEntry {
    std::string url;
    int scroll = 0;
};
class History {
    std::vector<HistoryEntry> entries_;
    std::size_t current_ = 0;

  public:
    void visit(std::string url) {
        if (!entries_.empty() && entries_[current_].url == url) {
            entries_[current_].scroll = 0;
            return;
        }
        if (!entries_.empty())
            entries_.resize(current_ + 1);
        if (entries_.size() == 24)
            entries_.erase(entries_.begin());
        entries_.push_back({std::move(url), 0});
        current_ = entries_.size() - 1;
    }
    std::span<const HistoryEntry> entries() const { return entries_; }
    std::size_t current() const { return current_; }
    void clear(std::string url, int scroll) {
        std::vector<HistoryEntry>{}.swap(entries_);
        visit(std::move(url)); position(scroll);
    }
    std::optional<HistoryEntry> select(std::size_t index) {
        if (index >= entries_.size()) return {};
        current_ = index; return entries_[current_];
    }
    bool back() const { return !entries_.empty() && current_ > 0; }
    bool forward() const { return current_ + 1 < entries_.size(); }
    std::optional<HistoryEntry> move(int delta) {
        if ((delta < 0 && !back()) || (delta > 0 && !forward()))
            return {};
        current_ = std::size_t(int(current_) + delta);
        return entries_[current_];
    }
    void replace_current(std::string url) {
        if (!entries_.empty()) entries_[current_].url = std::move(url);
    }
    void position(int y) {
        if (!entries_.empty())
            entries_[current_].scroll = y;
    }
};
} // namespace browser
