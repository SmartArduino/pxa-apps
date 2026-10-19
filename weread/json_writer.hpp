#pragma once
#include "json.hpp"
namespace weread {
// Append directly to the destination: no per-field quoted/numeric strings.
[[gnu::noinline]] inline void json_quote_to(std::string& out,std::string_view value){
    constexpr char hex[]="0123456789abcdef";out+='"';
    for(unsigned char c:value){
        if(c=='"'||c=='\\'){out+='\\';out+=char(c);}
        else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
        else out+=char(c);
    }
    out+='"';
}
template<std::size_t N> struct JsonKey {
    std::array<char,N+2> wire{};
    bool valid=true;
    consteval JsonKey(const char(&name)[N]){
        wire[0]='"';
        for(std::size_t i=0;i<N-1;++i){
            if(static_cast<unsigned char>(name[i])<32||name[i]=='"'||name[i]=='\\')valid=false;
            wire[i+1]=name[i];
        }
        wire[N]='"';wire[N+1]=':';
    }
};
[[gnu::noinline]] inline void json_number_to(std::string& out,std::uint32_t value){
    std::array<char,10> buffer;const auto end=std::to_chars(buffer.data(),buffer.data()+buffer.size(),value).ptr;
    out.append(buffer.data(),end);
}
class JsonObjectWriter {
    std::string& out_;
    bool first_=true;
public:
    explicit JsonObjectWriter(std::string& out):out_(out){out_+='{';}
    template<JsonKey Name> requires(Name.valid) void key(){if(!first_)out_+=',';first_=false;out_.append(Name.wire.data(),Name.wire.size());}
    template<JsonKey Name> void text(std::string_view value){key<Name>();json_quote_to(out_,value);}
    template<JsonKey Name> void number(std::uint32_t value){key<Name>();json_number_to(out_,value);}
    template<JsonKey Name> void boolean(bool value){key<Name>();out_+=value?"true":"false";}
    void close(){out_+='}';}
};
}
