#pragma once
#include "json.hpp"

namespace weread::detail {
// A resumable validator for the serializer's DATA grammar. It borrows its
// source, uses a fixed nesting stack, and never evaluates or expands a value.
// Position and node count belong to the caller, including across literals.
class LiteralScan {
    enum class Mode : std::uint8_t {
        value, first_value, key, first_key, colon, separator,
        quoted, escaped, unicode, name, number, bang,
        array_open, array_first_number, array_number, array_close, void_zero, void_end,
        complete, invalid
    };
    Mode mode=Mode::value;
    std::array<char,25> closes{};
    unsigned depth=0,hex_left=0;
    std::size_t begin=0;
    char quote=0;
    bool key=false;
    static bool first(char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c=='$';}
    static bool identifier(char c){return first(c)||(c>='0'&&c<='9');}
    static bool numeric(char c){return (c>='0'&&c<='9')||c=='.'||c=='e'||c=='E'||c=='+'||c=='-';}
    static bool space(char c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
    void end_value(){mode=depth?Mode::separator:Mode::complete;}
    void end_string(){if(key)mode=Mode::colon;else end_value();}
public:
    bool done()const{return mode==Mode::complete||mode==Mode::invalid;}
    bool valid()const{return mode==Mode::complete;}
    // Byte slicing also applies inside a single long string, escape, identifier
    // or number. Json validates a completed number without allocating a tree.
    bool step(std::string_view source,std::size_t& at,std::size_t& nodes,
              bool references,std::size_t bytes=4096){
        const auto stop=at+std::min(bytes,source.size()-at);
        while(!done()&&at<stop){
            const char c=source[at];
            if(mode==Mode::quoted){
                ++at;
                if(c==quote)end_string();
                else if(c=='\\')mode=Mode::escaped;
                else if(static_cast<unsigned char>(c)<32)mode=Mode::invalid;
                continue;
            }
            if(mode==Mode::escaped){
                ++at;
                if(c=='u'){hex_left=4;mode=Mode::unicode;}
                else if((c=='\''&&quote=='\'')||c=='"'||c=='\\'||c=='/'||c=='b'||c=='f'||c=='n'||c=='r'||c=='t')mode=Mode::quoted;
                else mode=Mode::invalid;
                continue;
            }
            if(mode==Mode::unicode){
                ++at;
                if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))mode=Mode::invalid;
                else if(!--hex_left)mode=Mode::quoted;
                continue;
            }
            if(mode==Mode::name){
                if(identifier(c)){++at;continue;}
                const auto name=source.substr(begin,at-begin);
                if(key)mode=Mode::colon;
                else if(name=="Array")mode=Mode::array_open;
                else if(name=="void")mode=Mode::void_zero;
                else if(references||name=="true"||name=="false"||name=="null")end_value();
                else mode=Mode::invalid;
                continue;
            }
            if(mode==Mode::number){
                if(numeric(c)){++at;continue;}
                if(Json(source.substr(begin,at-begin)).valid())end_value();
                else mode=Mode::invalid;
                continue;
            }
            if(mode==Mode::bang){
                ++at;if(c=='0'||c=='1')end_value();else mode=Mode::invalid;
                continue;
            }
            if(mode==Mode::void_end){
                if(identifier(c))mode=Mode::invalid;else end_value();
                continue;
            }
            if(mode==Mode::array_number){
                if(c>='0'&&c<='9'){++at;continue;}
                unsigned count=0;
                const auto [end,ec]=std::from_chars(source.data()+begin,source.data()+at,count);
                if(ec!=std::errc{}||end!=source.data()+at||count>8192)mode=Mode::invalid;
                else mode=Mode::array_close;
                continue;
            }
            if(space(c)){++at;continue;}
            if(mode==Mode::array_open){
                ++at;if(c=='(')mode=Mode::array_first_number;else mode=Mode::invalid;
                continue;
            }
            if(mode==Mode::array_first_number){begin=at;mode=Mode::array_number;continue;}
            if(mode==Mode::array_close){++at;if(c==')')end_value();else mode=Mode::invalid;continue;}
            if(mode==Mode::void_zero){++at;if(c=='0')mode=Mode::void_end;else mode=Mode::invalid;continue;}
            if(mode==Mode::separator){
                ++at;
                if(c==closes[depth-1]){--depth;end_value();}
                else if(c==',')mode=closes[depth-1]=='}'?Mode::key:Mode::value;
                else mode=Mode::invalid;
                continue;
            }
            if(mode==Mode::colon){++at;if(c==':')mode=Mode::value;else mode=Mode::invalid;continue;}
            if(mode==Mode::first_key&&c=='}'){
                ++at;--depth;end_value();continue;
            }
            if(mode==Mode::first_value&&c==']'){
                ++at;--depth;end_value();continue;
            }
            key=mode==Mode::key||mode==Mode::first_key;
            if(!key&&(depth>24||++nodes>500000)){mode=Mode::invalid;continue;}
            if(c=='"'||c=='\''){quote=c;++at;mode=Mode::quoted;continue;}
            begin=at;
            if(first(c)){++at;mode=Mode::name;continue;}
            if(key){mode=Mode::invalid;continue;}
            if(c=='-'||(c>='0'&&c<='9')){++at;mode=Mode::number;continue;}
            if(c=='!'){++at;mode=Mode::bang;continue;}
            if(c=='{'||c=='['){
                closes[depth++]=c=='{'?'}':']';++at;
                mode=c=='{'?Mode::first_key:Mode::first_value;continue;
            }
            mode=Mode::invalid;
        }
        // A valid serializer value always has enclosing punctuation afterwards.
        // Truncated tokens/containers must fail even when the final byte used
        // the entire caller's slice.
        if(!done()&&at==source.size())mode=Mode::invalid;
        return mode!=Mode::invalid;
    }
};
}
