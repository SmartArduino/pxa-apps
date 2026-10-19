#pragma once
#include "protocol.hpp"
#include "html.hpp"

namespace weread {
struct BookImage {
    std::string url,alt;
    std::uint32_t offset=0,aspect_q16=65536;
    std::uint16_t width_em_q8=0;
    bool explicit_aspect=false;
};
inline constexpr std::size_t document_image_budget=65536;
// Positive bounded fixed point, independent of locale/libc floating parsing.
inline std::optional<unsigned> decimal_fixed(std::string_view text,unsigned scale,unsigned maximum){
    if(text.empty()||text.size()>16)return {};
    std::uint64_t whole=0,fraction=0,denominator=1;bool dot=false,any=false;
    for(char c:text){
        if(c=='.'&&!dot){dot=true;continue;}
        if(c<'0'||c>'9')return {};
        any=true;
        if(!dot){whole=whole*10+unsigned(c-'0');if(whole>maximum)return {};}
        else if(denominator<100000000){fraction=fraction*10+unsigned(c-'0');denominator*=10;}
    }
    if(!any)return {};
    const auto value=whole*scale+(fraction*scale+denominator/2)/denominator;
    if(!value||value>std::uint64_t(maximum)*scale)return {};
    return unsigned(value);
}
inline std::string attribute_text(std::string_view text,std::size_t maximum){
    std::string out;
    for(std::size_t at=0;at<text.size();){
        if(text[at]=='&'){
            const auto end=text.find(';',at+1);
            if(end!=text.npos&&end-at<16)if(auto cp=html::entity(text.substr(at+1,end-at-1))){
                append_rune(out,*cp);at=end+1;if(out.size()>maximum)return {};continue;
            }
        }
        const auto cp=rune(text.substr(at));append_rune(out,cp.value);at+=cp.bytes;
        if(out.size()>maximum)return {};
    }
    return out;
}
inline bool trusted_image_url(std::string_view url){
    if(url.empty()||url.size()>2048||!clean(url)||url.find_first_of(" <>\\\"\t")!=url.npos)return false;
    constexpr std::array<std::string_view,3> origins{"https://res.weread.qq.com/","https://cdn.weread.qq.com/",
        "https://wfqqreader-1252317822.image.myqcloud.com/"};
    bool allowed=std::ranges::any_of(origins,[&](auto base){return url.starts_with(base);});
    // Tencent's resource service delegates signed image URLs to this CDN.
    // Validate the whole authority, never a substring in the path or userinfo.
    if(url.starts_with("https://")){
        const auto authority=url.substr(8,url.find_first_of("/?#",8)-8);
        constexpr std::string_view suffix=".gfp.tencent-cloud.com";
        allowed=allowed||(authority.size()>suffix.size()&&authority.ends_with(suffix)&&
            std::ranges::all_of(authority,[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='-';}));
    }
#ifdef WEREAD_FIXTURE_PORT
    allowed=allowed||url.starts_with(std::string(origin)+"/");
#endif
    return allowed;
}
inline std::string cover_image_url(std::string_view raw){
    // JSON URLs are not HTML attributes: do not rewrite signed query bytes.
    std::string url(raw);
    if(url.starts_with("//"))url="https:"+url;
    constexpr std::string_view legacy="http://wfqqreader-1252317822.image.myqcloud.com/";
    if(url.starts_with(legacy))url.insert(4,"s");
    return trusted_image_url(url)?url:std::string{};
}
inline std::optional<std::string> book_image_url(std::string_view raw){
    if(raw.size()>4096)return {};
    auto url=attribute_text(raw,2048);
    if(url.starts_with("//"))url="https:"+url;
    if(!trusted_image_url(url))return {};
    return url;
}
inline std::optional<std::string> image_redirect(std::string_view base,std::string_view location){
    location=trim(location);
    if(location.empty()||location.size()>2048||!trusted_image_url(base))return {};
    std::string url;
    if(location.starts_with("//"))url="https:"+std::string(location);
    else if(location.starts_with("https://"))url=location;
    else {
        // A colon before a path delimiter denotes a different URI scheme.
        const auto colon=location.find(':');
        if(colon!=location.npos&&colon<location.find_first_of("/?#"))return {};
        const auto authority_end=base.find('/',base.find("://")+3);
        const auto origin=base.substr(0,authority_end);
        if(location.front()=='/')url=std::string(origin)+std::string(location);
        else {
            base=base.substr(0,base.find_first_of("?#"));
            if(location.front()=='?')url=std::string(base)+std::string(location);
            else url=std::string(base.substr(0,base.rfind('/')+1))+std::string(location);
        }
    }
    // Location is a raw HTTP header, not HTML: signed query bytes must stay
    // unchanged, including any literal ampersand/entity-like sequence.
    if(!trusted_image_url(url))return {};
    return url;
}
inline BookImage book_image(const html::Tag& tag,std::uint32_t offset){
    BookImage image;image.offset=offset;
    if(auto url=book_image_url(tag.attribute("src").value_or("")))image.url=std::move(*url);
    image.alt=attribute_text(tag.attribute("alt").value_or(""),192);
    if(auto ratio=decimal_fixed(tag.attribute("data-ratio").value_or(""),65536,64)){
        // WeRead stores height/width; layout uses width/height.
        image.aspect_q16=std::uint32_t(std::clamp<std::uint64_t>(
            (std::uint64_t(65536)*65536+*ratio/2)/ *ratio,1024,4194304));
        image.explicit_aspect=true;
    }
    auto style=tag.attribute("style").value_or("");
    while(!style.empty()){
        const auto separator=style.find(';');const auto property=trim(style.substr(0,separator));
        const auto colon=property.find(':');
        if(colon!=property.npos&&html::equal(trim(property.substr(0,colon)),"width")){
            auto value=trim(property.substr(colon+1));
            if(value.ends_with("em"))if(auto width=decimal_fixed(trim(value.substr(0,value.size()-2)),256,128))image.width_em_q8=std::uint16_t(*width);
        }
        if(separator==style.npos)break;style.remove_prefix(separator+1);
    }
    return image;
}
inline std::size_t migrate_text_offset(std::size_t old,std::span<const BookImage> images){
    if(!old)return 0;
    for(const auto& image:images)if(image.offset<=old)old+=3;else break;
    return old;
}
} // namespace weread
