#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>
namespace weread {
// Local numeric diagnostics only: no heap buffer or general formatting runtime.
struct Metric {
    std::string_view name;
    std::uint64_t magnitude;
    bool negative;
    template<std::integral T> Metric(std::string_view key,T value):name(key){
        if constexpr(std::is_signed_v<T>){
            negative=value<0;
            magnitude=negative?std::uint64_t(-(value+1))+1:std::uint64_t(value);
        }else {negative=false;magnitude=std::uint64_t(value);}
    }
};
struct DiagnosticLine {
    std::array<char,256> bytes{};
    std::size_t size=0;
    std::string_view view()const{return {bytes.data(),size};}
};
[[gnu::noinline]] inline std::optional<DiagnosticLine> diagnostic(std::string_view prefix,std::initializer_list<Metric> metrics){
    DiagnosticLine out;
    const auto append=[&](std::string_view text){
        if(text.size()>out.bytes.size()-out.size||text.contains('\0'))return false;
        std::copy(text.begin(),text.end(),out.bytes.data()+out.size);out.size+=text.size();return true;
    };
    if(prefix.empty()||!append(prefix))return {};
    for(const auto& field:metrics){
        if(field.name.empty()||!append(" ")||!append(field.name)||!append("=")||
           (field.negative&&!append("-")))return {};
        const auto [end,error]=std::to_chars(out.bytes.data()+out.size,out.bytes.data()+out.bytes.size(),field.magnitude);
        if(error!=std::errc{})return {};
        out.size=std::size_t(end-out.bytes.data());
    }
    return out;
}
} // namespace weread
