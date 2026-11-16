#pragma once
#include "browser_core.hpp"
#include <cstdint>
#include <limits>

namespace browser::gmanhua {
inline constexpr std::size_t packet_limit = 32768, dictionary_limit = 256;
inline constexpr unsigned batch_size = 10;

inline bool word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}
struct Cursor {
    std::string_view text;
    std::size_t at = 0;
    void space() { while (at < text.size() && (text[at] == ' ' || text[at] == '\t' ||
                          text[at] == '\r' || text[at] == '\n')) ++at; }
    bool take(std::string_view value) {
        space();
        if (!text.substr(at).starts_with(value)) return false;
        at += value.size(); return true;
    }
    std::optional<std::uint32_t> number() {
        space(); const auto start = at; std::uint32_t value = 0;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
            const unsigned digit = unsigned(text[at++] - '0');
            if (value > (std::numeric_limits<std::uint32_t>::max()-digit)/10) return {};
            value = value*10+digit;
        }
        if (at == start || (at < text.size() && word(text[at]))) return {};
        return value;
    }
    std::optional<std::string> string() {
        space();
        if (at == text.size() || (text[at] != '\'' && text[at] != '"')) return {};
        const char quote = text[at++]; std::string result;
        while (at < text.size()) {
            unsigned char c = text[at++];
            if (c == quote) return result;
            if (c < 32) return {};
            if (c == '\\') {
                if (at == text.size()) return {};
                c = text[at++];
                if (c == 'n') c = '\n';
                else if (c == 'r') c = '\r';
                else if (c == 't') c = '\t';
                else if (c == 'b') c = '\b';
                else if (c == 'f') c = '\f';
                else if (c == 'v') c = '\v';
                else if (c != '\\' && c != '\'' && c != '"' && c != '/') return {};
            }
            if (result.size() == packet_limit) return {};
            result += char(c);
        }
        return {};
    }
};
inline std::optional<Cursor> assignment(std::string_view text, std::string_view name) {
    for (std::size_t at = 0; (at = text.find(name, at)) != text.npos; at += name.size()) {
        if ((at && word(text[at-1])) ||
            (at+name.size() < text.size() && word(text[at+name.size()]))) continue;
        Cursor cursor{text, at+name.size()};
        if (cursor.take("=") && !cursor.text.substr(cursor.at).starts_with('=')) return cursor;
    }
    return {};
}
inline std::optional<std::string_view> origin(std::string_view url) {
    const auto scheme = url.find("://");
    if (scheme == url.npos || (url.substr(0,scheme) != "http" && url.substr(0,scheme) != "https")) return {};
    const auto end = url.find_first_of("/?#", scheme+3);
    auto host = lowercase(url.substr(scheme+3, end == url.npos ? end : end-scheme-3));
    if ((scheme == 5 && host.ends_with(":443")) || (scheme == 4 && host.ends_with(":80")))
        host.resize(host.rfind(':'));
    if (host != "gmanhua.com" && host != "www.gmanhua.com") return {};
    return url.substr(0,end);
}
inline bool site(std::string_view url) { return bool(origin(url)); }
struct Chapter { std::uint32_t cid = 0, page = 1, per_page = 1; };
inline std::optional<Chapter> chapter(std::string_view url) {
    const auto root = origin(url);
    if (!root) return {};
    const auto path = url.substr(root->size(), url.find_first_of("?#", root->size())-root->size());
    Cursor cursor{path}; Chapter result;
    if (!cursor.take("/ch") || !cursor.number() || !cursor.take("-")) return {};
    const auto cid = cursor.number();
    if (!cid || !*cid) return {};
    result.cid = *cid;
    if (!cursor.text.substr(cursor.at).starts_with("-pl") && cursor.take("-p")) {
        auto page = cursor.number(); if (!page || !*page) return {};
        result.page = *page;
    }
    if (cursor.take("-pl")) {
        auto count = cursor.number(); if (!count || !*count) return {};
        result.per_page = *count;
    }
    if (!cursor.take("/") || cursor.at != path.size()) return {};
    return result;
}
// Decode only the public packed string table. No JavaScript, bytecode, regex
// evaluation, function calls or property access is executed by this adapter.
inline std::optional<std::string> unpack(std::string_view packet) {
    if (packet.size() > packet_limit) return {};
    const auto marker = packet.find("return p;}(");
    if (marker == packet.npos) return {};
    Cursor cursor{packet, marker+11};
    auto payload = cursor.string();
    if (!payload || !cursor.take(",")) return {};
    auto radix = cursor.number();
    if (!radix || *radix < 2 || *radix > 62 || !cursor.take(",")) return {};
    auto count = cursor.number();
    if (!count || !*count || *count > dictionary_limit || !cursor.take(",")) return {};
    auto table = cursor.string();
    if (!table || !cursor.take(".split(") || cursor.string() != std::optional<std::string>{"|"} ||
        !cursor.take(")")) return {};
    std::vector<std::string_view> words;
    std::string_view remaining = *table;
    for (;;) {
        if (words.size() == dictionary_limit) return {};
        const auto pipe = remaining.find('|'); words.push_back(remaining.substr(0,pipe));
        if (pipe == remaining.npos) break;
        remaining.remove_prefix(pipe+1);
    }
    if (words.size() < *count) return {};
    std::string result;
    for (std::size_t at = 0; at < payload->size();) {
        const auto start = at;
        if (!word((*payload)[at])) {
            if (result.size() == packet_limit) return {};
            result += (*payload)[at++]; continue;
        }
        unsigned index = 0; bool valid = true;
        while (at < payload->size() && word((*payload)[at])) {
            const char c = (*payload)[at++];
            const unsigned digit = c >= '0' && c <= '9' ? unsigned(c-'0') :
                c >= 'a' && c <= 'z' ? unsigned(c-'a'+10) :
                c >= 'A' && c <= 'Z' ? unsigned(c-'A'+36) : 62u;
            if (digit >= *radix || index > dictionary_limit / *radix) valid = false;
            if (valid) { index = index * *radix + digit; if (index >= *count) valid = false; }
        }
        const auto token = valid && !words[index].empty() ? words[index] :
            std::string_view(*payload).substr(start,at-start);
        if (token.size() > packet_limit-result.size()) return {};
        result += token;
    }
    return result;
}
inline bool hex_key(std::string_view value) {
    if (value.empty() || value.size() > 64) return false;
    for (char c : value) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                             (c >= 'A' && c <= 'F'))) return false;
    return true;
}
struct Metadata { Chapter chapter; std::uint32_t images = 0; std::string key; };
inline std::optional<Metadata> metadata(std::string_view url, std::string_view html) {
    auto path = chapter(url);
    if (!path || html.size() > packet_limit) return {};
    auto cid = assignment(html,"DM5_CID"), count = assignment(html,"DM5_IMAGE_COUNT");
    if (!cid || !count) return {};
    auto id = cid->number(), total = count->number();
    if (!id || *id != path->cid || !total || !*total) return {};
    for (unsigned attempt = 0; attempt < 4;) {
        const auto marker = html.find("return p;}(");
        if (marker == html.npos) break;
        auto source = unpack(html.substr(marker));
        if (source) if (auto value = assignment(*source,"guidkey")) {
            std::string key;
            for (;;) {
                auto part = value->string(); if (!part || part->size() > 64-key.size()) return {};
                key += *part;
                if (value->take(";")) break;
                if (!value->take("+")) return {};
            }
            if (!hex_key(key)) return {};
            return Metadata{*path,*total,std::move(key)};
        }
        html.remove_prefix(marker+11); ++attempt;
    }
    return {};
}
inline bool image_host(std::string_view url) {
    const auto scheme = url.find("://");
    if (scheme == url.npos || (url.substr(0,scheme) != "http" && url.substr(0,scheme) != "https")) return false;
    const auto end = url.find_first_of("/?#",scheme+3);
    const auto host = lowercase(url.substr(scheme+3, end == url.npos ? end : end-scheme-3));
    return host == "cdndm5.com" || host.ends_with(".cdndm5.com");
}
inline std::optional<std::vector<std::string>> images(std::string_view packet, std::uint32_t cid) {
    auto script = unpack(packet); if (!script) return {};
    auto base_value = assignment(*script,"pix"), array = assignment(*script,"pvalue");
    if (!base_value || !array) return {};
    auto base = base_value->string();
    if (!base || !image_host(*base) || base->find_first_of("?#") != base->npos || !array->take("[")) return {};
    std::vector<std::string> names;
    if (!array->take("]")) for (;;) {
        auto value = array->string();
        if (!value || value->empty() || value->front() != '/' || value->starts_with("//") ||
            value->find_first_of("?#") != value->npos || names.size() == max_images) return {};
        names.push_back(std::move(*value));
        if (array->take("]")) break;
        if (!array->take(",")) return {};
    }
    const auto prefix = "?cid="+std::to_string(cid)+"&key=";
    std::string suffix;
    Cursor strings{*script};
    while (strings.at < script->size()) {
        if ((*script)[strings.at] != '\'' && (*script)[strings.at] != '"') { ++strings.at; continue; }
        auto value = strings.string(); if (!value) return {};
        if (value->starts_with(prefix) && hex_key(std::string_view(*value).substr(prefix.size()))) {
            suffix = std::move(*value); break;
        }
    }
    if (suffix.empty()) return {};
    std::vector<std::string> result;
    for (auto& name : names) {
        auto url = resolve({},*base+name+suffix);
        if (!url || !image_host(*url)) return {};
        result.push_back(std::move(*url));
    }
    return result;
}
}
