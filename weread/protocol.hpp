#pragma once
#include "json.hpp"
#include "json_writer.hpp"
#include <ctime>

namespace weread {
inline constexpr std::string_view service_path = "/wrwebsimplenjlogic";
#ifdef WEREAD_FIXTURE_PORT
static_assert(WEREAD_FIXTURE_PORT>=1024 && WEREAD_FIXTURE_PORT<=65535);
#define WR_STRING_(x) #x
#define WR_STRING(x) WR_STRING_(x)
inline constexpr std::string_view origin = "http://127.0.0.1:" WR_STRING(WEREAD_FIXTURE_PORT);
#undef WR_STRING
#undef WR_STRING_
#else
inline constexpr std::string_view origin = "https://weread.qq.com";
#endif
inline constexpr std::string_view user_agent =
    "Mozilla/5.0 (X11; U; Linux armv7l like Android; en-us) AppleWebKit/531.2+ (KHTML, like Gecko) "
    "Version/5.0 Safari/533.2+ Kindle/3.0+";
inline std::string quote(std::string_view s) {
    std::string out="\"";
    constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:s) {
        if(c=='"' || c=='\\') {out+='\\';out+=char(c);}
        else if(c<32) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
        else out+=char(c);
    }
    out+='"';return out;
}
inline std::string number(std::int64_t n) {
    std::array<char,24> b;auto p=std::to_chars(b.data(),b.data()+b.size(),n).ptr;return {b.data(),p};
}
inline std::string scalar(Json v, std::size_t limit=1024) {
    if(auto s=v.string(limit))return *s;
    if(auto n=v.number())return number(*n);
    return {};
}
inline bool clean(std::string_view s) {
    return std::ranges::none_of(s,[](unsigned char c){return c<32 || c==127;});
}
inline std::string_view trim(std::string_view s) {
    while(!s.empty() && (s.front()==' ' || s.front()=='\t'))s.remove_prefix(1);
    while(!s.empty() && (s.back()==' ' || s.back()=='\t'))s.remove_suffix(1);
    return s;
}
inline bool login_expired(Json response){
    // The official service uses both a top-level legacy flag and a nested
    // signed error code. A failed shelf request is not generally an expiry.
    return response.get("sessionTimeout").number()==1||
        response.get("data").get("errcode").raw()=="-2012";
}
// This app has exactly one trusted HTTPS service origin. No web navigation or
// ambient cross-domain cookie jar is involved. The wire cookie header is kept
// private, has a strict size bound and is never passed to image CDNs.
struct Session {
    struct Cookie {std::string name,value;};
    std::vector<Cookie> cookies;
    std::string vid;
    bool remember=true;
    void receive(std::string_view header) {
        if(header.size()>2048 || !clean(header))return;
        auto pair=trim(header.substr(0,header.find(';')));auto eq=pair.find('=');
        if(eq==pair.npos)return;
        auto name=pair.substr(0,eq),value=pair.substr(eq+1);
        if(!name.starts_with("wr_") || name.size()>64 ||
           name.find_first_of(" ()<>@,;:\\\"/[]?={}\t")!=name.npos || value.size()>1536)return;
        auto old=std::ranges::find(cookies,name,&Cookie::name);
        if(value.empty()) {if(old!=cookies.end())cookies.erase(old);return;}
        std::size_t bytes=name.size()+value.size();
        for(auto& c:cookies)if(c.name!=name)bytes+=c.name.size()+c.value.size();
        if(bytes>6144)return;
        if(old!=cookies.end())old->value=value;
        else if(cookies.size()<16)cookies.push_back({std::string(name),std::string(value)});
    }
    std::string header() const {
        std::size_t bytes=0;for(const auto& c:cookies)bytes+=c.name.size()+1+c.value.size()+2;
        std::string out;out.reserve(bytes?bytes-2:0);
        for(const auto& c:cookies){if(!out.empty())out+="; ";out+=c.name;out+='=';out+=c.value;}
        return out;
    }
    void clear(){cookies.clear();vid.clear();}
    std::string encode() const {
        std::size_t estimate=64+vid.size();for(const auto& c:cookies)estimate+=c.name.size()+c.value.size()+4;
        std::string out;out.reserve(estimate);JsonObjectWriter writer(out);
        writer.number<"version">(1);writer.text<"vid">(vid);writer.key<"cookies">();out+='[';
        bool first=true;for(auto& c:cookies){if(!first)out+=',';first=false;out+=quote(c.name+'='+c.value);}
        out+=']';writer.close();return out;
    }
    bool decode(std::string_view raw) {
        Json j(raw);if(raw.size()>8192 || !j.valid() || j.get("version").number()!=1)return false;
        Session candidate;candidate.vid=scalar(j.get("vid"),64);
        bool valid=true;
        if(!j.get("cookies").each([&](Json v){
            auto s=v.string(2048);if(!s || s->find(';')!=s->npos){valid=false;return false;}
            const auto before=candidate.cookies.size();candidate.receive(*s);
            if(candidate.cookies.size()!=before+1)valid=false;return valid;
        },16) || !valid || candidate.vid.empty() || !clean(candidate.vid) || candidate.cookies.empty())return false;
        *this=std::move(candidate);return true;
    }
};
struct Book {std::string id,title,author,cover,param;};
struct Shelf {
    std::vector<Book> books;
    std::uint32_t total=0;
    int kk=-1;
    bool parse(std::string_view raw) {
        Json j(raw);if(!j.valid())return false;
        std::vector<Book> next;bool valid=true;
        if(!j.get("books").each([&](Json v){
            Book b{scalar(v.get("bookId"),128),scalar(v.get("title"),384),scalar(v.get("author"),256),scalar(v.get("cover"),1024),{}};
            if(b.id.empty() || b.title.empty() || !clean(b.id) ||
               std::ranges::find(next,b.id,&Book::id)!=next.end()){valid=false;return false;}
            next.push_back(std::move(b));return true;
        },100)||!valid)return false;
        auto urls=j.get("bookReaderUrls");
        if(!urls.raw().empty()&&!urls.each([&](Json v){
            const auto id=scalar(v.get("bId"),128);
            if(auto i=std::ranges::find(next,id,&Book::id);i!=next.end())i->param=scalar(v.get("param"),1024);
            return true;
        },100))return false;
        books=std::move(next);total=j.get("totalCount").number().value_or(books.size());
        const auto key=j.get("kkIdx").raw();int parsed=-1;
        const auto [end,ec]=std::from_chars(key.data(),key.data()+key.size(),parsed);
        if(ec==std::errc{} && end==key.data()+key.size())kk=parsed;
        return true;
    }
};
enum class LoginPhase { idle, uid, qr, polling, commit, complete, expired, failed };
struct Login {
    LoginPhase phase=LoginPhase::idle;
    std::string uid,key;
    std::uint64_t deadline=0;
    void clear(){uid.clear();key.clear();phase=LoginPhase::idle;deadline=0;}
    std::optional<std::string> confirmation(Json j) const {
        auto vid=scalar(j.get("vid"),64), skey=scalar(j.get("skey"),1024), code=scalar(j.get("code"),1024);
        if(vid.empty() || skey.empty() || code.empty())return {};
        // Preserve the service's JSON type. Its login page forwards vid without
        // coercion; sending a quoted number is not the same protocol request.
        const auto wire_vid=j.get("vid").raw();
        return "{\"vid\":"+std::string(wire_vid)+",\"skey\":"+quote(skey)+",\"code\":"+quote(code)+
            ",\"isAutoLogout\":"+number(j.get("isAutoLogout").number().value_or(0))+",\"pf\":2,\"cgiKey\":"+quote(key)+",\"fp\":\"\"}";
    }
};
} // namespace weread
