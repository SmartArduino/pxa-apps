#pragma once
#include "protocol.hpp"

namespace weread::agent {
inline constexpr std::string_view version="1.0.5";
#ifdef WEREAD_FIXTURE_PORT
inline constexpr auto gateway_origin=origin;
#else
inline constexpr std::string_view gateway_origin="https://i.weread.qq.com";
#endif
inline constexpr std::string_view key_path="/api/skills/apikeyGet?only_show=1";
inline constexpr std::string_view gateway_path="/api/agent/gateway";
inline constexpr std::string_view renewal_path="/web/login/renewal";
[[gnu::noinline]] inline bool renewal_confirmed(Json response){
    const auto error=response.get("errcode").raw();
    return response.valid()&&(response.get("succ").raw()=="1"||response.get("succ").raw()=="true")&&
        (error.empty()||error=="0")&&!login_expired(response);
}
enum class Api {book_info,book_progress,highlights,thoughts};
enum class Target {web,gateway};
inline bool route_allowed(Target target,std::string_view path){
    return clean(path)&&(target==Target::gateway?path==gateway_path:
        path.starts_with("/wrwebsimplenjlogic/")||path.starts_with("/api/store/search?")||path==key_path||path==renewal_path);
}
inline std::string payload(Api api,std::string_view book,std::int64_t cursor=0){
    if(cursor<0)return {};
    std::string_view name;
    switch(api){
    case Api::book_info:name="/book/info";break;
    case Api::book_progress:name="/book/getprogress";break;
    case Api::highlights:name="/book/bookmarklist";break;
    case Api::thoughts:name="/review/list/mine";break;
    default:return {};
    }
    std::string out;out.reserve(96+book.size());JsonObjectWriter writer(out);
    writer.text<"api_name">(name);writer.text<"skill_version">(version);
    if(api==Api::thoughts){
        writer.text<"bookid">(book);writer.number<"count">(10);
        writer.key<"synckey">();out+=number(cursor);
    }else writer.text<"bookId">(book);
    writer.close();return out;
}
inline bool valid_key(std::string_view key){
    return key.starts_with("wrk-")&&key.size()>=8&&key.size()<=256&&
        std::ranges::all_of(key,[](unsigned char c){return (c>='a'&&c<='z')||
            (c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_';});
}
enum class Status {ok,authorization,upgrade,unavailable,malformed};
inline Status status(Json data){
    auto raw=data.raw();while(!raw.empty()&&(raw.front()==' '||raw.front()=='\n'||raw.front()=='\r'||raw.front()=='\t'))raw.remove_prefix(1);
    if(!data.valid()||raw.empty()||raw.front()!='{')return Status::malformed;
    if(!data.get("upgrade_info").raw().empty()&&data.get("upgrade_info").raw()!="null")return Status::upgrade;
    auto error=data.get("errcode").raw();
    if(error.empty()||error=="0")return Status::ok;
    if(error=="401"||error=="-2012")return Status::authorization;
    std::int32_t code=0;auto [end,ec]=std::from_chars(error.data(),error.data()+error.size(),code);
    return ec==std::errc{}&&end==error.data()+error.size()?Status::unavailable:Status::malformed;
}
inline std::string_view message(Status status){
    switch(status){
    case Status::authorization:return "官方接口授权暂不可用，请在微信读书中检查 Skill 是否已开通";
    case Status::upgrade:return "官方接口需要更新，请更新应用后重试";
    case Status::malformed:return "图书信息格式暂不支持，请稍后重试";
    default:return "图书信息暂不可用，请稍后重试";
    }
}
inline std::optional<std::string> existing_key(Json data){
    if(status(data)!=Status::ok)return {};
    auto key=data.get("apikey").string(256);
    if(!key||!valid_key(*key))return {};
    return key;
}
// A detail document is created only when opened. Fields borrow the one bounded
// response while parsing; the result keeps just the text needed by the UI.
inline std::optional<std::string> book_document(std::string_view raw,std::string_view expected){
    Json data(raw);
    if(expected.empty()||expected.size()>128||raw.size()>32768||status(data)!=Status::ok)return {};
    const auto [id,title,author,translator,intro,publisher,date,isbn,words,rating,count]=data.select({
        "bookId","title","author","translator","intro","publisher","publishTime","isbn","wordCount","newRating","newRatingCount"});
    if(scalar(id,128)!=expected)return {};
    auto heading=title.string(384);if(!heading||heading->empty()||!valid_utf8(*heading))return {};
    std::string result=std::move(*heading);
    const auto append=[&](std::string_view label,Json field,std::size_t maximum){
        if(field.raw().empty()||field.raw()=="null")return true;
        auto value=field.string(maximum);if(!value||!valid_utf8(*value))return false;
        if(!value->empty()){result+='\n';result+=label;result+=*value;}return true;
    };
    if(!append("作者：",author,256)||!append("译者：",translator,256)||!append("出版社：",publisher,256)||
       !append("出版时间：",date,64)||!append("ISBN：",isbn,32))return {};
    if(auto n=words.number();n&&*n)result+="\n字数："+number(*n);
    if(auto n=rating.number();n&&*n<=100){
        result+="\n评分："+number(*n)+" / 100";
        if(auto voters=count.number();voters&&*voters)result+="（"+number(*voters)+" 人）";
    }
    if(!append("\n",intro,8192))return {};
    return result;
}
inline std::optional<std::string> progress_line(std::string_view raw,std::string_view expected){
    Json data(raw);
    if(expected.empty()||expected.size()>128||raw.size()>8192||status(data)!=Status::ok||scalar(data.get("bookId"),128)!=expected)return {};
    const auto [percent,seconds,finished]=data.get("book").select({"progress","recordReadingTime","finishTime"});
    auto value=percent.number();if(!value||*value>100)return {};
    std::string result="云端进度："+number(*value)+"%";
    if(*value==100&&finished.number().value_or(0)>0)result+=" · 已读完";
    if(auto time=seconds.number();time&&*time){
        const auto minutes=*time/60;
        result+="\n累计阅读：";
        if(!minutes)result+="不到 1 分钟";
        else {if(minutes>=60)result+=number(minutes/60)+" 小时 ";result+=number(minutes%60)+" 分钟";}
    }
    return result;
}
} // namespace weread::agent
