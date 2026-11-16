#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace manga {
// Borrowed, bounded JSON views; no document tree and no third-party runtime.
class Json {
    std::string_view s;
    static void ws(std::string_view s, size_t& p) { while(p<s.size() && (s[p]==' '||s[p]=='\n'||s[p]=='\r'||s[p]=='\t'))++p; }
    static bool str(std::string_view s,size_t& p) {
        if(p>=s.size()||s[p++]!='"')return false;
        while(p<s.size()) { unsigned char c=s[p++]; if(c=='"')return true;
            if(c<32)return false;
            if(c=='\\') { if(p>=s.size())return false; c=s[p++];
                if(c=='u') { for(int i=0;i<4;++i) {if(p>=s.size()||hex(s[p++])<0)return false;} }
                else if(std::string_view("\"\\/bfnrt").find(c)==std::string_view::npos)return false;
            }
        }return false;
    }
    static int hex(char c) {return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
    static bool skip(std::string_view s,size_t& p,unsigned depth=0) {
        ws(s,p);if(p>=s.size()||depth>24)return false;
        if(s[p]=='"')return str(s,p);
        if(s[p]=='{'||s[p]=='[') {
            const bool obj=s[p++]=='{';char end=obj?'}':']'; ws(s,p);
            if(p<s.size()&&s[p]==end){++p;return true;}
            while(p<s.size()) {
                ws(s,p);if(obj){if(!str(s,p))return false;ws(s,p);if(p>=s.size()||s[p++]!=':')return false;}
                if(!skip(s,p,depth+1))return false;ws(s,p);if(p>=s.size())return false;
                if(s[p]==end){++p;return true;}if(s[p++]!=',')return false;
            }return false;
        }
        for(auto word:{std::string_view("true"),std::string_view("false"),std::string_view("null")})
            if(s.substr(p).starts_with(word)){p+=word.size();return true;}
        if(s[p]=='-')++p;if(p>=s.size())return false;
        if(s[p]=='0')++p;else {if(s[p]<'1'||s[p]>'9')return false;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;}
        if(p<s.size()&&s[p]=='.'){size_t b=++p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(p==b)return false;}
        if(p<s.size()&&(s[p]=='e'||s[p]=='E')) {++p;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))++p;size_t b=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(p==b)return false;}
        return true;
    }
    static void utf8(std::string& out,unsigned c) {
        if(c<128)out+=char(c);else if(c<2048){out+=char(0xc0|(c>>6));out+=char(0x80|(c&63));}
        else if(c<65536){out+=char(0xe0|(c>>12));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
        else {out+=char(0xf0|(c>>18));out+=char(0x80|((c>>12)&63));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
    }
public:
    Json()=default;explicit Json(std::string_view v):s(v){}
    [[gnu::noinline]] bool valid()const{size_t p=0;if(!skip(s,p))return false;ws(s,p);return p==s.size();}
    [[gnu::noinline]] Json get(std::string_view key)const {
        size_t p=0;ws(s,p);if(p>=s.size()||s[p++]!='{')return {};
        while(p<s.size()) {ws(s,p);size_t b=p;if(!str(s,p))return {};auto k=Json(s.substr(b,p-b)).string();
            ws(s,p);if(p>=s.size()||s[p++]!=':')return {};ws(s,p);b=p;if(!skip(s,p))return {};
            if(k&&*k==key)return Json(s.substr(b,p-b));ws(s,p);if(p>=s.size()||s[p++]!=',')return {};
        }return {};
    }
    template<class F> bool each(F f,size_t limit=1000)const {
        size_t p=0,n=0;ws(s,p);if(p>=s.size()||s[p++]!='[')return false;ws(s,p);
        if(p<s.size()&&s[p]==']')return true;
        while(p<s.size()){ws(s,p);size_t b=p;if(!skip(s,p)||++n>limit)return false;
            if(!f(Json(s.substr(b,p-b))))return true;ws(s,p);if(p>=s.size())return false;
            if(s[p++]==']')return true;if(s[p-1]!=',')return false;
        }return false;
    }
    template<class F> bool members(F f,size_t limit=32)const {
        size_t p=0,n=0;ws(s,p);if(p>=s.size()||s[p++]!='{')return false;ws(s,p);
        if(p<s.size()&&s[p]=='}')return true;
        while(p<s.size()){ws(s,p);size_t b=p;if(!str(s,p)||++n>limit)return false;auto key=Json(s.substr(b,p-b)).string(192);
            ws(s,p);if(!key||p>=s.size()||s[p++]!=':')return false;ws(s,p);b=p;if(!skip(s,p))return false;
            if(!f(*key,Json(s.substr(b,p-b))))return true;ws(s,p);if(p>=s.size())return false;if(s[p++]=='}')return true;if(s[p-1]!=',')return false;
        }return false;
    }
    [[gnu::noinline]] std::optional<std::string> string(size_t max=1024)const {
        size_t p=0;ws(s,p);if(p>=s.size()||s[p++]!='"')return {};
        std::string o;
        auto code=[&]()->unsigned {unsigned v=0;for(int i=0;i<4;++i){if(p>=s.size())return 0xffffffff;int h=hex(s[p++]);if(h<0)return 0xffffffff;v=(v<<4)|unsigned(h);}return v;};
        while(p<s.size()) {unsigned char c=s[p++];if(c=='"')return o;if(c<32)return {};
            if(c=='\\') {if(p>=s.size())return {};c=s[p++];
                if(c=='u'){unsigned v=code();if(v==0xffffffff)return {};
                    if(v>=0xd800&&v<=0xdbff){if(s.substr(p,2)!="\\u")return {};p+=2;auto low=code();if(low<0xdc00||low>0xdfff)return {};v=0x10000+((v-0xd800)<<10)+low-0xdc00;}
                    else if(v>=0xdc00&&v<=0xdfff)return {};utf8(o,v);
                }else {auto at=std::string_view("\"\\/bfnrt").find(c);if(at==std::string_view::npos)return {};o+=std::string_view("\"\\/\b\f\n\r\t")[at];}
            }else o+=char(c);if(o.size()>max)return {};
        }return {};
    }
    unsigned number(unsigned fallback=0)const {if(s.empty())return fallback;unsigned n=0;auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),n);return e==std::errc{}&&p==s.data()+s.size()?n:fallback;}
};
inline std::string quote(std::string_view s) {std::string o="\"";for(unsigned char c:s){if(c=='"'||c=='\\')o+='\\';if(c>=32)o+=char(c);else if(c=='\n')o+="\\n";}return o+'"';}
inline std::string encoded(std::string_view s){std::string o;constexpr char h[]="0123456789ABCDEF";for(unsigned char c:s){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')o+=char(c);else{o+='%';o+=h[c>>4];o+=h[c&15];}}return o;}
inline bool web(std::string_view s){auto p=s.find("://");return (s.starts_with("https://")||s.starts_with("http://"))&&p!=s.npos&&s.size()>p+3&&s.size()<=1024&&s.find_first_of("\r\n\t ") == s.npos;}
inline std::string resolve(std::string_view base,std::string_view ref){if(web(ref))return std::string(ref);if(ref.empty()||ref.find(":")!=ref.npos||ref.starts_with("//"))return {};auto p=base.find("://");if(p==base.npos)return {};if(ref.front()=='/'){auto end=base.find('/',p+3);return std::string(base.substr(0,end))+std::string(ref);}return std::string(base.substr(0,base.rfind('/')+1))+std::string(ref);}
inline bool uuid(std::string_view s){if(s.size()!=36)return false;for(size_t i=0;i<s.size();++i)if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;return true;}
struct Chapter {std::string id,title;};
enum class SourceKind {mangadex,copy,katana,pill,town,guazi,woi,unknown};
struct Source {std::string_view id,title,detail,base;SourceKind kind;unsigned batch;};
inline constexpr std::array<Source,7> sources{{
    {"copymanga","拷贝漫画","中文 · 分组目录","https://www.mangacopy.com",SourceKind::copy,12},
    {"guazimanhua","瓜子漫画","简体中文 · 搜索与章节目录","https://www.guazimanhua.com",SourceKind::guazi,36},
    {"woaimanhua","我爱漫画","简体中文 · 搜索与章节目录","https://www.woimh.com",SourceKind::woi,35},
    {"","MangaDex","简中 · 繁中 · 英文","https://mangadex.org",SourceKind::mangadex,12},
    {"mangakatana","MangaKatana","英文 · 搜索与章节目录","https://mangakatana.com",SourceKind::katana,20},
    {"mangapill","MangaPill","英文 · 搜索与章节目录","https://mangapill.com",SourceKind::pill,50},
    {"mangatown","MangaTown","英文 · 按页加载","https://www.mangatown.com",SourceKind::town,30},
}};
inline const Source* source_info(std::string_view id){for(auto& source:sources)if(id==source.id)return &source;return nullptr;}
inline SourceKind source_kind(std::string_view s){auto p=source_info(s);return p?p->kind:SourceKind::unknown;}
inline std::string_view source_name(std::string_view s){auto p=source_info(s);return p?p->title:"已移除的书源";}
inline bool valid_source(std::string_view s){return source_kind(s)!=SourceKind::unknown;}
inline bool retired_source(std::string_view s){return s=="xkcd"||s=="peppercarrot"||s=="commitstrip"||web(s);}
inline bool katana_book_url(std::string_view s){constexpr std::string_view base="https://mangakatana.com/manga/";return s.starts_with(base)&&s.size()>base.size()&&s.substr(base.size()).find_first_of("/?#\\ ") == s.npos&&web(s);}
inline bool slug(std::string_view s){return !s.empty()&&s.size()<=192&&s.find_first_of("/\\%?#& \r\n\t")==s.npos;}
inline unsigned comic_number(std::string_view s){auto n=Json(s).number();return n<=1000000?n:0;}
inline std::string copy_web(std::string_view id){return "https://www.mangacopy.com/comic/"+encoded(id);}
inline bool site_path(std::string_view s,std::string_view prefix){return s.starts_with(prefix)&&s.size()>prefix.size()&&s.size()<=1024&&s.find_first_of("?#%\\ \r\n\t")==s.npos&&s.find("..") == s.npos&&web(s);}
inline bool digits(std::string_view s){return !s.empty()&&s.find_first_not_of("0123456789")==s.npos;}
inline bool pill_book_url(std::string_view s){constexpr std::string_view prefix="https://mangapill.com/manga/";if(!site_path(s,prefix))return false;s.remove_prefix(prefix.size());auto slash=s.find('/');return slash!=s.npos&&digits(s.substr(0,slash))&&slug(s.substr(slash+1));}
inline bool town_book_url(std::string_view s){constexpr std::string_view prefix="https://www.mangatown.com/manga/";return site_path(s,prefix)&&s.ends_with('/')&&slug(s.substr(prefix.size(),s.size()-prefix.size()-1));}
inline bool pill_chapter_url(std::string_view s,std::string_view book){if(!pill_book_url(book))return false;auto id=book.substr(std::string_view("https://mangapill.com/manga/").size());id=id.substr(0,id.find('/'));return site_path(s,"https://mangapill.com/chapters/"+std::string(id)+"-");}
inline bool town_chapter_url(std::string_view s,std::string_view book){
    if(!town_book_url(book)||!site_path(s,book)||!s.ends_with('/'))return false;
    auto part=s.substr(book.size(),s.size()-book.size()-1);
    if(part.starts_with('v')){auto slash=part.find('/');if(slash==part.npos||!digits(part.substr(1,slash-1)))return false;part.remove_prefix(slash+1);}
    return part.starts_with('c')&&part.size()>1&&part.find_first_not_of("0123456789._",1)==part.npos;
}
inline bool numeric_url(std::string_view s,std::string_view prefix){return s.starts_with(prefix)&&s.size()<=prefix.size()+12&&digits(s.substr(prefix.size()));}
inline bool guazi_book_url(std::string_view s){return numeric_url(s,"https://www.guazimanhua.com/comic.php?id=");}
inline bool guazi_chapter_url(std::string_view s){return numeric_url(s,"https://www.guazimanhua.com/chapter.php?id=");}
inline bool woi_book_url(std::string_view s){return numeric_url(s,"https://www.woimh.com/book/mh");}
inline bool woi_chapter_url(std::string_view s){constexpr std::string_view prefix="https://www.woimh.com/chapter/";if(!s.starts_with(prefix)||s.size()>prefix.size()+30)return false;auto tail=s.substr(prefix.size());auto dash=tail.find('-');return digits(tail.substr(0,dash))&&(dash==tail.npos||digits(tail.substr(dash+1)));}
struct Book {std::string id,title,source,chapter,chapter_title;unsigned page=0,language=0,pages=0;};
inline bool valid_book(const Book& b){if(!valid_source(b.source))return false;switch(source_kind(b.source)){
    case SourceKind::mangadex:return uuid(b.id)&&(b.chapter.empty()||uuid(b.chapter));
    case SourceKind::copy:return slug(b.id)&&(b.chapter.empty()||(b.chapter.starts_with(copy_web(b.id)+"/chapter/")&&uuid(std::string_view(b.chapter).substr(b.chapter.rfind('/')+1))));
    case SourceKind::guazi:return guazi_book_url(b.id)&&(b.chapter.empty()||guazi_chapter_url(b.chapter));
    case SourceKind::woi:return woi_book_url(b.id)&&(b.chapter.empty()||woi_chapter_url(b.chapter));
    case SourceKind::pill:return pill_book_url(b.id)&&(b.chapter.empty()||pill_chapter_url(b.chapter,b.id));
    case SourceKind::town:return town_book_url(b.id)&&(b.chapter.empty()||town_chapter_url(b.chapter,b.id));
    case SourceKind::katana:return katana_book_url(b.id)&&(b.chapter.empty()||(b.chapter.starts_with(b.id+"/c")&&web(b.chapter)));
    default:return false;}}
// Estimate glyph widths, then cut at a complete UTF-8 boundary.
inline std::string ellipsize(std::string_view text,int width,int font=14){int used=0;size_t at=0;
    while(at<text.size()){unsigned char c=text[at];size_t n=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;n=std::min(n,text.size()-at);
        int advance=c<128?(c==' '||c=='i'||c=='l'?font/3:font*3/5):font;if(used+advance>width){if(at>3){auto p=at;while(p&&used+font>width){--p;while(p&&(static_cast<unsigned char>(text[p])&0xc0)==0x80)--p;used-=font;}at=p;}return std::string(text.substr(0,at))+"…";}used+=advance;at+=n;}
    return std::string(text);}
inline bool same(const Book& a,const Book& b){return a.id==b.id&&a.source==b.source;}
inline std::string title(Json attributes){auto t=attributes.get("title");for(auto k:{"zh","zh-hk","en","ja-ro","ja","ko-ro"})if(auto s=t.get(k).string(192);s&&!s->empty())return *s;
    std::string out;attributes.get("altTitles").each([&](Json a){for(auto k:{"zh","zh-hk","en"})if(auto v=a.get(k).string(192)){out=*v;return false;}return true;});return out.empty()?"未命名漫画":out;}
[[gnu::noinline]] inline bool books(Json j,std::vector<Book>& out,std::string_view source) {
    std::vector<Book> next;bool md=source.empty(),ok=true;
    auto valid=(md?j.get("data"):j.get("books")).each([&](Json row){
        auto id=row.get("id").string(1024);auto t=md?title(row.get("attributes")):row.get("title").string(192).value_or("");
        if(!md)id=resolve(source,row.get("chapters").string(1024).value_or(""));
        if(!id||t.empty()||(md?!uuid(*id):!web(*id))){ok=false;return false;}
        next.push_back({*id,t,std::string(source),{},{},0,0});return true;
    },64);if(!valid||!ok)return false;out=std::move(next);return true;
}
[[gnu::noinline]] inline bool chapters(Json j,std::vector<Chapter>& out,bool md,std::string_view base) {
    std::vector<Chapter> next;bool ok=true;
    bool valid=(md?j.get("data"):j.get("chapters")).each([&](Json row){
        std::string id,t;
        if(md){auto a=row.get("attributes");if(a.get("pages").number()==0)return true;id=row.get("id").string(64).value_or("");auto n=a.get("chapter").string(32).value_or("");t=n.empty()?"特别篇":"第 "+n+" 话";auto x=a.get("title").string(192);if(x&&!x->empty())t+=" · "+*x;}
        else{id=resolve(base,row.get("pages").string().value_or(""));t=row.get("title").string(192).value_or("");}
        if((md?!uuid(id):!web(id))||t.empty()){ok=false;return false;}next.push_back({id,t});return true;
    },128);if(!valid||!ok)return false;out=std::move(next);return true;
}
[[gnu::noinline]] inline bool pages(Json j,std::vector<std::string>& out,bool md,std::string_view base,bool original=false) {
    std::vector<std::string> next;bool ok=true;
    std::string prefix;
    if(md){auto b=j.get("baseUrl").string(),h=j.get("chapter").get("hash").string(128);if(!b||!h||h->empty()||!web(*b)||h->find_first_not_of("0123456789abcdef")!=h->npos)return false;prefix=*b+(original?"/data/":"/data-saver/")+*h+"/";}
    auto valid=(md?j.get("chapter").get(original?"data":"dataSaver"):j.get("pages")).each([&](Json row){auto s=row.string();if(!s){ok=false;return false;}auto u=md?prefix+encoded(*s):resolve(base,*s);if(!web(u)){ok=false;return false;}next.push_back(std::move(u));return true;},512);
    if(!valid||!ok||next.empty())return false;out=std::move(next);return true;
}
inline std::string save(const std::vector<Book>& shelf,unsigned lang,std::string_view source) {
    std::string o="{\"version\":3,\"language\":"+std::to_string(lang)+",\"source\":"+quote(source)+",\"shelf\":[";bool comma=false;
    for(auto& b:shelf){if(comma)o+=',';comma=true;o+="{\"id\":"+quote(b.id)+",\"title\":"+quote(b.title)+",\"source\":"+quote(b.source)+",\"chapter\":"+quote(b.chapter)+",\"chapterTitle\":"+quote(b.chapter_title)+",\"page\":"+std::to_string(b.page)+",\"language\":"+std::to_string(b.language)+",\"pages\":"+std::to_string(b.pages)+"}";}return o+"]}";
}
[[gnu::noinline]] inline bool restore(std::string_view text,std::vector<Book>& shelf,unsigned& lang,std::string& source){Json j(text);auto version=j.get("version").number();if(text.size()>32768||!j.valid()||version<1||version>3)return false;
    std::vector<Book> next;auto s=j.get("source").string(1024);auto l=j.get("language").number(99);bool ok=true;
    if(!s||l>2)return false;if(version<3&&retired_source(*s))*s="copymanga";if(!valid_source(*s))return false;
    auto valid=j.get("shelf").each([&](Json r){Book b{r.get("id").string().value_or(""),r.get("title").string(192).value_or(""),r.get("source").string().value_or(""),r.get("chapter").string().value_or(""),r.get("chapterTitle").string(256).value_or(""),r.get("page").number(9999),r.get("language").number(99)};
        if(version<3&&retired_source(b.source))return true;
        b.pages=r.get("pages").number();if(b.title.empty()||b.page>511||b.pages>512||b.language>2||!valid_book(b)){ok=false;return false;}next.push_back(std::move(b));return true;},24);
    if(!valid||!ok)return false;shelf=std::move(next);lang=l;source=*s;return true;
}
}
