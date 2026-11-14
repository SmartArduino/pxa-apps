#include "../browser_script.hpp"
#include <cassert>
#include <iostream>
#include <string_view>
using namespace browser::script;
int main() {
    struct Case { std::string_view html; bool needed; };
    constexpr Case cases[] = {
        {"<p>Static</p>",false},
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
