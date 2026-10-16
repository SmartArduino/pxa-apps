#pragma once
#include "browser_core.hpp"

namespace browser::script::detail {
inline bool equal_ascii(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
        [](char x, char y) { return lower(x) == lower(y); });
}
inline std::size_t find_ascii(std::string_view text, std::string_view needle, std::size_t from = 0) {
    for (auto i = from; i + needle.size() <= text.size(); ++i)
        if (equal_ascii(text.substr(i, needle.size()), needle)) return i;
    return text.npos;
}
inline bool tag_boundary(char c) { return space(c) || c == '>' || c == '/'; }
inline std::size_t raw_end(std::string_view text, std::string_view name, std::size_t from) {
    for (;;) {
        auto at=find_ascii(text,name,from);
        if(at==text.npos || at+name.size()==text.size() || tag_boundary(text[at+name.size()])) return at;
        from=at+name.size();
    }
}
struct Token { std::string_view text; bool tag = false; };
inline Token token(std::string_view html, std::size_t& at) {
    while (at < html.size()) {
        const auto start = at;
        if (html.substr(at).starts_with("<!--")) {
            auto end = html.find("-->", at + 4);
            at = end == html.npos ? html.size() : end + 3;
            continue;
        }
        if (html[at] != '<') {
            at = html.find('<', at);
            if (at == html.npos) at = html.size();
            return {html.substr(start, at - start), false};
        }
        char quote = 0;
        for (++at; at < html.size(); ++at) {
            const char c = html[at];
            if (quote) { if (c == quote) quote = 0; }
            else if (c == '\'' || c == '"') quote = c;
            else if (c == '>') { ++at; return {html.substr(start + 1, at - start - 2), true}; }
        }
        return {};
    }
    return {};
}
inline bool executable(const Tag& t) {
    const auto type = lowercase(trim(t.get("type")));
    return type.empty() || type == "text/javascript" || type == "application/javascript";
}
inline std::string origin(std::string_view url) {
    const auto scheme = url.find("://");
    if (scheme == url.npos) return {};
    auto protocol = lowercase(url.substr(0, scheme));
    if (protocol != "http" && protocol != "https") return {};
    auto authority = url.substr(scheme + 3);
    authority = authority.substr(0, authority.find_first_of("/?#"));
    if (authority.empty() || authority.find('@') != authority.npos) return {};
    auto host = lowercase(authority);
    const auto default_port = protocol == "https" ? ":443"sv : ":80"sv;
    if (host.ends_with(default_port)) host.resize(host.size() - default_port.size());
    return protocol + "://" + host;
}
}
