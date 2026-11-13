#pragma once
#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>

namespace browser::srcset {
inline constexpr unsigned max_candidates = 32;
inline constexpr std::size_t max_bytes = 4096;
inline bool space(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='\f'; }
inline char lower(char c) { return c>='A' && c<='Z' ? char(c+32) : c; }
inline bool unsupported(std::string_view url) {
    const auto path=url.substr(0,url.find_first_of("?#"));
    for (auto suffix : {std::string_view(".svg"),std::string_view(".webp"),std::string_view(".avif")}) {
        if (path.size()<suffix.size()) continue;
        const auto tail=path.substr(path.size()-suffix.size()); bool match=true;
        for(std::size_t i=0;i<tail.size();++i)match &= lower(tail[i])==suffix[i];
        if(match)return true;
    }
    return false;
}
inline bool descriptor(std::string_view value) {
    if(value.empty())return true; // implicit 1x
    if(value.back()=='w') {
        value.remove_suffix(1); unsigned n=0;
        const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),n);
        return error==std::errc{} && end==value.data()+value.size() && n>0;
    }
    if(value.back()!='x')return false;
    value.remove_suffix(1);
    // Positive decimal densities, without a floating-point runtime dependency.
    bool digit=false, dot=false, fraction=false, positive=false;
    for(char c:value) {
        if(c=='.' && !dot) { dot=true;continue; }
        if(c<'0' || c>'9')return false;
        digit=true;positive |= c!='0';if(dot)fraction=true;
    }
    return digit && positive && (!dot || fraction);
}
// Select the first usable candidate in author order. No candidate vector,
// sizes/media-query engine or DPR selection is retained. URL commas are not
// separators until the descriptor state (notably data: and CDN URLs).
template<class Accept>
std::optional<std::string_view> first(std::string_view list, Accept accept) {
    if(list.size()>max_bytes)return {};
    std::size_t at=0;
    for(unsigned count=0;count<max_candidates && at<list.size();++count) {
        while(at<list.size() && (space(list[at]) || list[at]==','))++at;
        const auto start=at;
        while(at<list.size() && !space(list[at]))++at;
        auto url=list.substr(start,at-start);
        bool trailing=false;
        while(!url.empty() && url.back()==',') { url.remove_suffix(1);trailing=true; }
        std::string_view spec;
        if(!trailing) {
            const auto begin=at; unsigned parens=0;
            while(at<list.size()) {
                const auto c=list[at];
                if(c==',' && !parens)break;
                if(c=='(')++parens;else if(c==')' && parens)--parens;
                ++at;
            }
            spec=list.substr(begin,at-begin);
            while(!spec.empty() && space(spec.front()))spec.remove_prefix(1);
            while(!spec.empty() && space(spec.back()))spec.remove_suffix(1);
            if(at<list.size())++at;
        }
        if(!url.empty() && descriptor(spec) && accept(url))return url;
    }
    return {};
}
} // namespace browser::srcset
