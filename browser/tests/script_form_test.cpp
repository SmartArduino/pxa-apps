#include "../browser_script.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static unsigned case_number=0;
static Page open(std::string body) {
    ++case_number;
    Page p;Limits l;l.heap_bytes=512*1024;l.turn_us=200000;
    assert(p.open("<html><body>"+body+"</body></html>","https://example.test/start?old=1#old",bootstrap,{},{},l));return p;
}
static std::string settle(Page& p) {
    std::string html;
    for(unsigned i=0;i<4096 && p.active();++i) {
        p.step();if(auto out=p.take_markup())html=std::move(*out);
        if(p.ready() && p.next_wake_us()!=0) {
            if(p.stats().errors)std::cerr<<"Form case "<<case_number<<": "<<p.error()<<" errors="<<p.stats().errors<<'\n';
            assert(p.stats().errors==0);return html;
        }
    }
    assert(false && "form page did not settle");return {};
}
static unsigned hit(const std::string& html,std::string_view id) {
    const auto at=html.find(" id=\""+std::string(id)+"\"");assert(at!=html.npos);
    const auto start=html.rfind('<',at), key=html.find("data-pxa-click=\"",start);
    return key<at ? unsigned(std::stoul(html.substr(key+16))) : 0;
}
static std::string click(Page& p,unsigned id) {assert(id && p.click(id));p.step();return settle(p);}
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);
    bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});assert(!bootstrap.runtime.empty());
    {
        auto p=open(R"HTML(<form action="/echo?stale=1#result" method="GeT">
        <div><input type=hidden name=word value="A B+*!~'()中"></div>
        <input type=hidden name=word value=second>
        <input type=SUBMIT id=go name=go value=send><input type=submit name=other value=ignored>
        </form><script>void 0;</script>)HTML");
        const auto html=settle(p);click(p,hit(html,"go"));const auto nav=p.take_navigation();
        assert(nav && !nav->post && nav->url=="https://example.test/echo?word=A+B%2B*%21%7E%27%28%29%E4%B8%AD&word=second&go=send#result");
    }
    {
        auto p=open(R"HTML(<form id=f><input type=hidden name=q value="A B">
        <button id=go name=go value="*~">Visible label</button><button name=other value=ignored>Other</button></form>
        <script>let f=document.getElementById('f');f.action='/post?kept=1#target';f.method='POST';
        f.enctype='APPLICATION/X-WWW-FORM-URLENCODED';
        if(f.method!=='post' || f.enctype!=='application/x-www-form-urlencoded')throw Error('normalized form attributes');
        f.onsubmit=e=>{if(e.submitter!==document.getElementById('go'))throw Error('submitter')};</script>)HTML");
        const auto html=settle(p);click(p,hit(html,"go"));const auto nav=p.take_navigation();
        assert(nav && nav->post && nav->url=="https://example.test/post?kept=1#target" && nav->body=="q=A+B&go=*%7E");
    }
    {
        auto p=open(R"HTML(<form action=/wrong><button id=b type=BUTTON onclick="this.textContent='Button worked'">Ordinary</button>
        <button id=r type=RESET onclick="this.textContent='Reset handler worked'">Reset</button>
        <button id=d disabled onclick="invalid syntax )">Disabled</button></form><script>void 0;</script>)HTML");
        const auto html=settle(p);assert(!hit(html,"d"));
        assert(click(p,hit(html,"b")).find("Button worked")!=std::string::npos && !p.take_navigation());
        assert(click(p,hit(html,"r")).find("Reset handler worked")!=std::string::npos && !p.take_navigation());
    }
    {
        auto p=open(R"HTML(<form action=/wrong><button id=control type=button onclick="let s=document.getElementById('send');s.disabled=true;s.setAttribute('onclick','invalid syntax )')">Disable</button>
        <button id=send name=go value=yes>Send</button></form><script>void 0;</script>)HTML");
        const auto html=settle(p);assert(p.click(hit(html,"control")));p.step();
        // A queued click may still reference the last visible hit table.
        assert(p.click(hit(html,"send")));p.step();settle(p);assert(!p.take_navigation() && !p.stats().errors);
    }
    {
        auto p=open(R"HTML(<form id=f method=post action=/direct onsubmit="throw Error('submit event must be bypassed')">
        <input type=hidden name=q value=ok><button name=go value=ignored>Send</button></form>
        <script>document.getElementById('f').submit();</script>)HTML");
        settle(p);const auto nav=p.take_navigation();assert(nav && nav->post && nav->body=="q=ok");
    }
    {
        auto p=open(R"HTML(<form id=f action=/request method=post><input type=hidden name=q value=ok>
        <button id=ask type=button onclick="document.getElementById('f').requestSubmit(document.getElementById('send'))">Ask</button>
        <button id=send name=go value=yes>Send</button></form><script>
        let f=document.getElementById('f'),send=document.getElementById('send'),n=0;
        f.onsubmit=e=>{if(e.submitter!==send)throw Error('submitter');++n;f.requestSubmit(send);if(n===1)e.preventDefault()};
        </script>)HTML");
        const auto html=settle(p);click(p,hit(html,"ask"));assert(!p.take_navigation());
        click(p,hit(html,"ask"));const auto nav=p.take_navigation();assert(nav && nav->post && nav->body=="q=ok&go=yes");
    }
    {
        auto p=open(R"HTML(<form method=post action=/recursive onsubmit="this.submit();return false">
        <input type=hidden name=q value=ok><button id=go name=go value=excluded>Send</button></form>
        <script>void 0;</script>)HTML");
        const auto html=settle(p);click(p,hit(html,"go"));const auto nav=p.take_navigation();
        assert(nav && nav->post && nav->body=="q=ok");
    }
    {
        auto p=open(R"HTML(<form id=f action=/outer method=post><div><input type=hidden name=a value=first></div>
        <input type=hidden name=a value=last><input type=checkbox name=c checked value="">
        <input type=radio name=r value=no><input type=radio name=r value=yes checked>
        <input disabled name=ignored value=bad><input type=button name=ignored value=bad><input type=reset name=ignored value=bad></form>
        <form><button id=foreign>Foreign</button></form><script>
        const f=document.getElementById('f');let caught=false;
        try{f.requestSubmit(document.getElementById('foreign'))}catch(e){caught=e instanceof TypeError}
        if(!caught)throw Error('foreign submitter');
        const inner=document.createElement('form'),field=document.createElement('input');field.name='bad';field.value='nested';
        inner.appendChild(field);f.appendChild(inner);
        f.onsubmit=e=>{if(e.submitter!==null)throw Error('null submitter')};f.requestSubmit();
        </script>)HTML");
        settle(p);const auto nav=p.take_navigation();assert(nav && nav->body=="a=first&a=last&c=&r=yes");
    }
    {
        auto p=open(R"HTML(<form id=f action=/budget method=post><input id=v type=hidden name=q value=ok></form><script>
        let f=document.getElementById('f'),v=document.getElementById('v');
        function rejected(fn){let caught=false;try{fn()}catch(e){caught=e instanceof RangeError || e instanceof TypeError}if(!caught)throw Error('form budget')}
        v.value='!'.repeat(3000);rejected(()=>f.submit());
        v.value='x'.repeat(1024);f.method='GET';rejected(()=>f.submit());
        f.method='POST';f.enctype='multipart/form-data';rejected(()=>f.submit());
        f.enctype='application/x-www-form-urlencoded';f.method='POST';v.value='recovered';f.submit();
        </script>)HTML");
        settle(p);const auto nav=p.take_navigation();assert(nav && nav->post && nav->body=="q=recovered");
    }
    {
        auto p=open(R"HTML(<form id=f method=dialog action=/wrong><input name=q value=ok></form><script>
        document.getElementById('f').requestSubmit();</script>)HTML");
        settle(p);assert(!p.take_navigation());
        auto fallback=open(R"HTML(<form id=f method=invalid enctype=invalid action=/fallback><input name=q value=ok></form><script>
        document.getElementById('f').submit();</script>)HTML");
        settle(fallback);const auto nav=fallback.take_navigation();assert(nav && !nav->post && nav->url=="https://example.test/fallback?q=ok");
    }
    std::cout<<"script forms PASS: types, submitter, disabled controls, tree order, events, UTF-8, query replacement and budgets\n";
}
