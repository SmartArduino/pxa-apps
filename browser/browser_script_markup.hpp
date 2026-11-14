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
        if(at==text.npos)return at;
        if(at+name.size()<text.size() && tag_boundary(text[at+name.size()]))return at;
        from=at+name.size();
    }
}
inline std::optional<std::string> text_value(std::string_view input,bool references,std::size_t limit) {
    if(input.find_first_of(std::string_view("\r\0",2))==input.npos)
        return references?browser::entities(input):std::string(input);
    std::string normalized;normalized.reserve(std::min(input.size(),limit));
    for(std::size_t i=0;i<input.size();++i) {
        char c=input[i];
        if(c=='\r') {c='\n';if(i+1<input.size() && input[i+1]=='\n')++i;}
        if(normalized.size()+(c?1:3)>limit)return {};
        if(c)normalized+=c;else normalized+="\xef\xbf\xbd";
    }
    return references?browser::entities(normalized):std::move(normalized);
}
struct Token { std::string_view text; bool tag = false; };
// In the interpreted component, an aggregate-returning Wasm call for each
// token is costly. Keep this bounded lexer in its parser/scanner caller.
[[gnu::always_inline]] inline Token token(std::string_view html, std::size_t& at) {
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
// Raw bodies and their closing token are one native DOM-building step.
// An empty ending denotes plaintext, whose body lasts until EOF.
inline std::string_view take_raw_body(std::string_view html, std::string_view ending,
                                      std::size_t& at) {
    const auto closing=ending.empty()?html.npos:raw_end(html,ending,at);
    const auto end=closing==html.npos?html.size():closing;
    const auto body=html.substr(at,end-at);
    at=end;if(closing!=html.npos)(void)token(html,at);
    return body;
}
// Only these short raw names matter to the structural scan. Match the tag
// parser's ASCII name grammar without allocating its attribute array/name.
[[gnu::always_inline]] inline std::optional<std::string_view> fragment_raw_ending(std::string_view text) {
    text=trim(text);if(text.starts_with('/'))return {};
    std::array<char,9> head;std::size_t count=0;
    for(const auto byte:text) {
        const auto c=lower(byte);
        if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='-'))break;
        if(count==head.size())return {};
        head[count++]=c;
    }
    const std::string_view name(head.data(),count);
    if(name=="script")return "</script";
    if(name=="style")return "</style";
    const auto kind=browser::html_text(name);
    if(kind==browser::HtmlText::data)return {};
    return kind==browser::HtmlText::plaintext?std::string_view{}:browser::text_end_tag(name);
}
// Check the existing parser-step allowance without allocating JS nodes.
// This applies to detached replacements; append-only writes must retain any
// partial changes made before a later error. Use the same raw-body consumer
// as the builder so literal tags/comments cannot inflate the step count.
// Keep scanner locals out of the native dispatcher frame retained by WAMR.
[[gnu::noinline]] inline bool fragment_fits(std::string_view html,unsigned limit) {
    if(html.size()<=limit)return true; // every step consumes at least one byte
    if(limit) {
        // Tags and intervening text need at most 2*'<' + 1 steps. Comments,
        // quotes and raw text only reduce that bound; common fragments need
        // no tag/attribute parsing at all. Stop once the bound cannot prove fit.
        std::size_t scan=0;unsigned markers=0;
        while((scan=html.find('<',scan))!=html.npos) {
            if(++markers>(limit-1)/2)break;
            ++scan;
        }
        if(scan==html.npos)return true;
    }
    std::size_t at=0;unsigned steps=0;
    while(at<html.size()) {
        if(steps++==limit)return false;
        const auto item=token(html,at);
        if(!item.tag || item.text.empty() || item.text.starts_with('!') || item.text.starts_with('?'))continue;
        if(const auto ending=fragment_raw_ending(item.text))
            (void)take_raw_body(html,*ending,at);
    }
    return true;
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
