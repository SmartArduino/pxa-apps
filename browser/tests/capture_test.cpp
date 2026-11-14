#include "../browser_script.hpp"
#include "../browser_script_markup.hpp"
#include <cassert>
#include <iostream>
#include <string_view>
using namespace browser::script;
using namespace std::literals;
int main() {
    // Compare the marker search against the former byte-by-byte scanner,
    // including false closing prefixes, case folding, NUL and short tails.
    auto reference=[](std::string_view text,std::string_view name,std::size_t from) {
        for(auto at=from;at+name.size()<=text.size();++at) {
            if(!detail::equal_ascii(text.substr(at,name.size()),name))continue;
            if(at+name.size()<text.size() && detail::tag_boundary(text[at+name.size()]))return at;
            at+=name.size()-1;
        }
        return text.npos;
    };
    std::uint32_t random=1234567;
    for(auto ending:{"</script"sv,"</style"sv,"</title"sv,"</textarea"sv,
                     "</xmp"sv,"</iframe"sv,"</noembed"sv,"</noframes"sv}) {
        for(unsigned trial=0;trial<64;++trial) {
            std::string text;
            for(unsigned i=0;i<trial;++i) {
                random=random*1664525u+1013904223u;
                text+="a</sTiY\0\r\n"[random%10];
            }
            text+=ending;text+="x>"; // an incomplete name must not close
            const auto boundary=text.size();
            for(char c:ending)text+=c>='a' && c<='z'?char(c-'a'+'A'):c;
            text+=" >/\t\r\n\f"[trial%7];text+=">After";
            for(auto size:{boundary-1,boundary,text.size()}) {
                const auto source=std::string_view(text).substr(0,size);
                for(auto from:{std::size_t(0),std::size_t(1),boundary,size,size+1})
                    assert(detail::raw_end(source,ending,from)==reference(source,ending,from));
            }
        }
    }
    struct Case { std::string_view html; bool needed; };
    constexpr Case cases[] = {
        {"<p>Static</p>",false},
        {"<style><button onclick='ghost()'>",false},
        {"<script type=application/json><button onclick='ghost()'>",false},
        {"<script>work()",true},
        {"<button onclick='work()'></button><style><meta http-equiv=Content-Security-Policy content=\"script-src 'none'\">",true},
        {"<button onclick='work()'></button><script type=application/json><meta http-equiv=Content-Security-Policy content=\"script-src 'none'\">",true},
        {"<button disabled onclick='bad )'>Disabled</button>",false},
        {"<INPUT DISABLED='false' OnClick='bad )'>",false},
        {"<button disabled onclick='bad )'></button><input disabled onclick='bad )'>",false},
        {"<button onclick='work()'>Enabled</button>",true},
        {"<input onclick='work()'>",true},
        {"<button disabled onclick='bad )'></button><button onclick='work()'>Enabled</button>",true},
        {"<button disabled onload='work()'>Disabled</button>",true},
        {"<button disabled onsubmit='work()'>Disabled</button>",true},
        {"<button disabled onclick='bad )'></button><body onload='work()'>",true},
        {"<div disabled onclick='work()'>Not a disabled control</div>",true},
        {"<a disabled onclick='work()'>Link</a>",true},
        {"<textarea disabled onclick='work()'>Disabled multiline control</textarea>",false},
        {"<button disabled onclick='bad )'></button><script>work()</script>",true},
        {"<button disabled onclick='bad )'></button><script src=/enable.js></script>",true},
        {"<button disabled onclick='bad )'></button><script type=application/json>{}</script>",false},
        {"<textarea><button onclick='work()'></textarea><button disabled onclick='bad )'>",false},
        {"<button disabled onclick='bad )'></button><!--<button onclick='work()'>-->",false},
        {"<button onclick='work()'></button><meta http-equiv=Content-Security-Policy content=\"script-src 'none'\">",false},
    };
    for (const auto &item:cases) for (std::size_t chunk:{1,2,7,4096}) {
        Capture capture;
        for(std::size_t at=0;at<item.html.size();at+=chunk)capture.append(item.html.substr(at,chunk));
        assert(!capture.limited() && capture.has_script()==item.needed);
        assert(capture.take()==item.html);
    }
    std::cout<<"script capture PASS: disabled clicks, enabled controls, other events, scripts, raw text, policy and streaming\n";
}
