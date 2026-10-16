#include "../browser_script.hpp"
#include "../browser_cookies.hpp"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
using namespace std::string_literals;
static Bootstrap bootstrap;
static Limits policy() { Limits l; l.heap_bytes=512*1024; l.turn_us=200000; return l; }
static std::string run(Page& p, unsigned turns=1024) {
    std::string result;
    for (unsigned i=0;i<turns && p.active();++i) {
        p.step(); if (auto html=p.take_markup()) result=std::move(*html);
        if (p.ready() && !result.empty()) break;
    }
    return result;
}
static std::optional<std::string> snapshot(Page& p) {
    for(unsigned i=0;i<1024 && p.active();++i) {
        if(auto html=p.take_markup())return html;
        p.step();
    }
    return {};
}
static Page open(std::string body, Limits limits=policy()) {
    Page page; assert(page.open("<html><body>"+body+"</body></html>","https://example.test/start",bootstrap,{}, {},limits)); return page;
}
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});assert(!bootstrap.runtime.empty());
    assert(same_origin("https://EXAMPLE.test:443/a","https://example.test/b"));
    assert(!same_origin("https://example.test","https://example.test.evil"));
    assert(!script_url_allowed("https://example.test","http://example.test/x",true));
    assert(!script_url_allowed("https://example.test","https://other.test/x",false));
    assert(script_url_allowed("https://example.test","https://other.test/x",true));
    Capture c;c.append("<!--<script>--><p>static</p><script type='application/json'>{}</script>");assert(!c.has_script());
    c.append("<script src='/code.js'></script>");assert(c.has_script());c.append(std::string(32768,'x'));assert(c.limited());
    {
        const auto before="<!--<style>ignored</style>--><style id='theme' media='a>b'>"s;
        const auto after="</STyLe><script>var literal='<style>keep</style></scripture>';</script>"s;
        const auto source=before+std::string(100000,'x')+"</stylex>not a close"+after;
        for(unsigned chunk:{1u,2u,3u,7u,17u,1024u}) {
            Capture capture;
            for(std::size_t at=0;at<source.size();at+=chunk)capture.append(std::string_view(source).substr(at,chunk));
            assert(!capture.limited() && capture.has_script());
            assert(capture.take()==before+"</style>"+after.substr(8));
        }
        Capture policy;policy.append(source+"<meta http-equiv='Content-Security-Policy' content=\"script-src 'none'\">");
        assert(!policy.limited() && !policy.has_script());
        auto p=open("<p id=a></p><script>var x='</scripture><style>literal</style>';document.getElementById('a').textContent='Raw text OK';</script>");
        assert(run(p).find("Raw text OK")!=std::string::npos && p.error().empty());
    }
    {
        auto p=open(R"HTML(<p id="a">Before</p><script>
        var box=document.createElement('div'),s=document.createElement('script');
        s.src='https://cdn.example.test/dynamic.js';
        s.onload=function(){document.getElementById('a').textContent='Dynamic '+payload;s.remove();document.body.appendChild(s)};
        box.appendChild(s);document.body.appendChild(box);
        </script>)HTML");
        std::optional<Request> q;
        for(unsigned i=0;i<256&&!q;++i){p.step();(void)p.take_markup();q=p.take_request();}
        assert(q && q->script && q->url=="https://cdn.example.test/dynamic.js");
        p.complete({q->id,200,"var payload=42;"});
        std::string html;
        for(unsigned i=0;i<256;++i){p.step();if(auto next=p.take_markup())html=*next;}
        assert(html.find("Dynamic 42")!=html.npos && !p.take_request() && p.stats().errors==0);
    }
    {
        auto p=open(R"HTML(<p id="a">Before</p><script>
        var s=document.createElement('script');document.body.appendChild(s);
        s.onerror=function(){document.getElementById('a').textContent='Dynamic failure handled'};
        s.src='/missing.js';
        </script>)HTML");
        std::optional<Request> q;
        for(unsigned i=0;i<256&&!q;++i){p.step();(void)p.take_markup();q=p.take_request();}
        assert(q && q->script);p.complete({q->id,404,"not a script",true});
        std::string html;
        for(unsigned i=0;i<256;++i){p.step();if(auto next=p.take_markup())html=*next;}
        assert(html.find("Dynamic failure handled")!=html.npos && p.stats().errors==0);
    }
    {
        auto p=open("<p id=a>Before</p><script>document.getElementById('a').textContent='Initialized';</script>");
        assert(p.active() && !p.initialized() && p.stats().current_bytes==0);
        p.pause(true);p.step();assert(p.stats().current_bytes==0);p.pause(false);
        for(int i=0;i<16 && !p.initialized();++i)p.step();
        assert(p.initialized() && !p.ready());assert(run(p).find("Initialized")!=std::string::npos);
        auto before=p.stats().current_bytes;
        assert(p.set_heap_limit(4*1024*1024));assert(p.stats().current_bytes==before);
        assert(!p.set_heap_limit(0));assert(!p.set_heap_limit(8*1024*1024));
        assert(!p.set_heap_limit(128*1024));assert(p.active());
        assert(p.set_heap_limit(256*1024));
    }
    {
        // Ordinary nested factories retain lexical scope and member receivers.
        auto p=open(R"HTML(<p id=a></p><script>
        var v=(function(){var n=10;return (function(){var x=7;return (function(){return n+x})()})()})();
        if(v!==17)throw Error('factory');
        class C{#x=3;m(){return this.#x}static ok(o){return #x in o}}
        var c=new C;if(c.m()!==3||!C.ok(c)||C.ok({}))throw Error('class');
        var o={v:4,m(){return this.v}};
        if(o?.m?.()!==4||null?.m!==undefined||new (function(){this.x=7})().x!==7)throw Error('member');
        var n=1;if(n++!==1||++n!==3||n**2!==9||(-n)**2!==9)throw Error('unary');
        document.getElementById('a').textContent='Factories OK';
        </script>)HTML");
        assert(run(p).find("Factories OK")!=std::string::npos && p.stats().errors==0);
    }
    {
        // Binary-precedence parser keeps upstream associativity/private-in rules.
        auto p=open(R"HTML(<p id=a></p><script>
        function eq(a,b){if(a!==b)throw Error('precedence: '+a+' != '+b)}
        eq(2+3*4,14);eq(20/2/5,2);eq(9-3-1,5);eq(1+2<<2,12);
        eq(4>>1+1,1);eq(8>>>1,4);eq(2**3**2,512);eq((-2)**2,4);
        eq(1<2==true,true);eq(3===3&6,0);eq(5&3^2|8,11);
        eq('x' in {x:1},true);eq([] instanceof Array,true);
        let n=0;eq(false&&++n,false);eq(true||++n,true);eq(n,0);
        eq(null??3,3);eq(0??3,0);let x,y;x=y=4;eq(x+y,8);
        eq(true?1:false?2:3,1);let keys='';for(let k in {a:1,b:2})keys+=k;eq(keys,'ab');
        class C{#x;has(o){return #x in o}equal(o){return #x in o===true}}
        let c=new C;eq(c.has(c),true);eq(c.equal(c),true);eq(c.has({}),false);
        for(let code of ['-2**2','true??false||true','class X{#p;m(o){return #p in o<1}}']){
            let failed=false;try{new Function(code)}catch(e){failed=e instanceof SyntaxError}eq(failed,true);
        }
        document.getElementById('a').textContent='Precedence OK';
        </script>)HTML");
        assert(run(p).find("Precedence OK")!=std::string::npos);assert(p.error().empty());
    }
    {
        auto p=open("<div id=a></div><script>window.onload=()=>{document.getElementById('a').innerHTML='<b>Onload fragment</b>';};</script>");
        assert(run(p).find("<b>Onload fragment</b>")!=std::string::npos);assert(p.error().empty());
    }
    {
        auto p=open("<div id=a></div><script>for(let i=0;i<120;++i)document.getElementById('a').innerHTML='<b>Repeated fragment</b>';</script>");
        assert(run(p).find("Repeated fragment")!=std::string::npos);assert(p.error().empty());
    }
    // Closing or navigating at any initialization phase releases partial realms.
    for (unsigned phase=0;phase<=6;++phase) {
        auto p=open("<p>Cancelled initialization</p><script>0;</script>");
        for(unsigned step=0;step<phase;++step)p.step();
    }
    {
        auto p=open("<div id=a></div><script>let n=document.getElementById('a');for(let i=0;i<100;++i){let c=document.createElement('div');n.appendChild(c);n=c;}n.textContent='Deep & safe';if(document.getElementById('a').textContent!=='Deep & safe')throw Error('text traversal');</script>");
        assert(run(p).find("Deep &amp; safe")!=std::string::npos);assert(p.error().empty());
    }
    {
        auto p=open("<p id=a>Safe</p><script>document.body.childNodes.push(document.body);</script>");
        run(p);assert(!p.active() && !p.error().empty());
    }
    {
        Capture policy;policy.append("<script>1</script><meta http-equiv='Content-Security-Policy' content=\"script-src 'none'\">");assert(!policy.has_script());
    }
    {
        auto p=open("<p id='a'>Before</p><button onclick=\"document.getElementById('a').textContent='Clicked';\">Tap</button><script>window.onload=function(){document.getElementById('a').textContent='Hello JS 中文';};</script>");
        auto html=run(p);assert(html.find("Hello JS 中文")!=html.npos);assert(p.error().empty());
        const auto turns=p.stats().turns;for(int i=0;i<100;++i){p.step();assert(!p.take_markup());}assert(p.stats().turns==turns);
        assert(p.click(1));p.step();html=*snapshot(p);assert(html.find("Clicked")!=html.npos);
        browser::Document doc;browser::Parser parser(doc,true);parser.feed(html);parser.finish();assert(doc.links.size()==1 && doc.links[0].script_node==1);
        browser::Document untrusted;browser::Parser regular(untrusted);regular.feed(html);regular.finish();assert(untrusted.links.empty());
    }
    {
        auto p=open("<p id='a'>Waiting</p><script src='/a.js'></script><script>document.getElementById('a').textContent += '-ordered';</script>");
        for(int i=0;i<12;++i)p.step();auto request=p.take_request();assert(request && request->script && request->url=="https://example.test/a.js");
        p.complete({request->id,200,"document.getElementById('a').textContent='External';"});auto html=run(p);assert(html.find("External-ordered")!=html.npos);
    }
    {
        auto p=open("<p id='a'>Waiting</p><script>fetch('/data').then(r=>r.json()).then(r=>document.getElementById('a').innerHTML='<b>'+r.text+'</b>');</script>");
        run(p);auto request=p.take_request();assert(request && !request->script);
        p.complete({request->id,200,"{\"text\":\"Network OK\"}"});std::string html;
        for(int i=0;i<128 && html.find("<b>Network OK</b>")==html.npos;++i){p.step();if(auto out=p.take_markup())html=*out;}
        assert(html.find("<b>Network OK</b>")!=html.npos);
    }
    {
        auto p=open("<p id='a'>Waiting</p><script>let r=new XMLHttpRequest();r.open('GET','/cancel');r.send();r.abort();try{fetch('https://other.test/secret')}catch(e){};document.getElementById('a').textContent='OK';</script>");
        auto html=run(p);assert(!p.take_request());assert(html.find("OK")!=html.npos);
    }
    {
        auto p=open("<p id='a'>Waiting</p><script>setTimeout(()=>{document.getElementById('a').style.display='none'},0)</script>");run(p);p.step();auto html=snapshot(p);assert(html && html->find("display:none")!=html->npos);
    }
    {
        auto p=open("<a href='/next'>Next</a><script>window.onload=()=>{};</script>");run(p);assert(p.click(1));p.step();auto next=p.take_navigation();assert(next && next->url=="https://example.test/next");
    }
    {
        auto p=open("<input id='q' value='old'><script>window.onload=()=>{};</script>");auto html=run(p);browser::Document doc;browser::Parser parser(doc,true);parser.feed(html);parser.finish();assert(doc.fields.size()==1 && doc.fields[0].script_node);
        p.change_field(doc.fields[0].script_node,"changed",false);p.step();assert(snapshot(p)->find("changed")!=std::string::npos);
    }
    {
        auto p=open("<p id='a'></p><script>throw Error('ordinary error');</script><script>document.getElementById('a').textContent='Recovered';</script>");assert(run(p).find("Recovered")!=std::string::npos);assert(p.stats().errors==1);
    }
    {
        auto l=policy();l.turn_us=2000;auto p=open("<p>Safe</p><script>while(true){}</script>",l);run(p);assert(!p.active() && p.stats().timeouts==1);
    }
    {
        // Native RegExp execution must poll the same deadline as JS bytecode.
        auto l=policy();l.turn_us=2000;
        auto p=open("<p>Safe</p><script>/(a+)+$/.test('a'.repeat(100)+'!');</script>",l);
        run(p);assert(!p.active() && p.stats().timeouts==1);
    }
    {
        auto p=open("<p id=a></p><script>function recurse(){return recurse()};recurse();</script><script>document.getElementById('a').textContent='Stack recovered';</script>");
        assert(run(p).find("Stack recovered")!=std::string::npos);assert(p.stats().errors==1);
    }
    {
        // An endless microtask chain advances one bounded job per browser turn.
        auto p=open("<p>Promises</p><script>function again(){Promise.resolve().then(again)};again();</script>");
        run(p);const auto before=p.stats().turns;
        for(int i=0;i<20;++i)p.step();
        assert(p.active() && p.stats().turns==before+20);
        p.pause(true);for(int i=0;i<20;++i)p.step();assert(p.stats().turns==before+20);
    }
    {
        auto p=open("<p>Safe</p><script>var a=[];while(true)a.push('x'.repeat(1024));</script>");run(p);assert(!p.active() && p.stats().peak_bytes<=policy().heap_bytes);
    }
    {
        Storage s;const std::string empty,one="one";assert(s.set("https://a.test","key",&empty,false));assert(s.get("https://a.test/x","key",false)==empty);assert(!s.get("https://b.test","key",false));assert(!s.get("https://a.test","key",true));assert(s.set("https://a.test","key",&one,true));assert(s.set("https://a.test","key",nullptr,false));assert(!s.get("https://a.test","key",false));assert(s.get("https://a.test","key",true)==one);
        browser::Cookies jar;jar.receive("https://a.test/","secret=server; HttpOnly; Secure; Path=/",0,0);assert(jar.request("https://a.test/",0,true).empty());jar.receive("https://a.test/","secret=overwrite; Path=/",0,0,true);assert(jar.request("https://a.test/",0)=="secret=server");
    }
    {
        auto p=open("<script>document.write('<p id=a>Written</p>');</script>");assert(run(p).find("Written")!=std::string::npos);
    }
    {
        auto p=open("<p id='a'>Before</p><script>setTimeout(()=>document.getElementById('a').textContent='After',0);</script>");
        run(p);p.pause(true);const auto turns=p.stats().turns;for(int i=0;i<10;++i)p.step();assert(p.stats().turns==turns);p.pause(false);p.step();assert(snapshot(p)->find("After")!=std::string::npos);
    }
    {
        auto p=open("<p id='a'>Before</p><script>fetch('/data').then(r=>r.text()).then(t=>document.getElementById('a').textContent=t);</script>");
        run(p);auto request=p.take_request();assert(request);const auto bytes=p.stats().current_bytes;
        p.complete({request->id,200,"Queued"});assert(p.stats().current_bytes==bytes);
        for(int i=0;i<12;++i)p.step();assert(snapshot(p)->find("Queued")!=std::string::npos);
    }
    {
        auto l=policy();l.nodes=3;auto p=open("<p>one</p><p>two</p><p>three</p><script>0;</script>",l);run(p);assert(!p.active());
    }
    {
        Storage store;Hooks hooks;hooks.owner=&store;
        hooks.storage_get=[](void* p,std::string_view u,std::string_view k,bool s){return static_cast<Storage*>(p)->get(u,k,s);};
        hooks.storage_set=[](void* p,std::string_view u,std::string_view k,const std::string* v,bool s,bool clear){return static_cast<Storage*>(p)->set(u,k,v,s,clear);};
        Page p;assert(p.open("<p id=a></p><script>localStorage.setItem('','');localStorage.setItem('keep','1');localStorage.removeItem('');sessionStorage.setItem('keep','2');document.getElementById('a').textContent=[localStorage.getItem(''),localStorage.getItem('keep'),sessionStorage.getItem('keep')].join(':');</script>","https://example.test/",bootstrap,hooks,{},policy()));
        assert(run(p).find(":1:2")!=std::string::npos);assert(!store.get("https://example.test","",false));
    }
    {
        auto p=open("<input id='a'><input id='b'><script>window.onload=()=>{};</script>");run(p);
        p.change_field(1,"FIRST",false);p.change_field(2,"SECOND",false);p.step();p.step();auto html=snapshot(p);assert(html && html->find("FIRST")!=html->npos && html->find("SECOND")!=html->npos);
    }
    {
        std::string body="<input id=q value=old><p id=a>Before</p>";
        for(int i=0;i<40;++i)body+="<p>snapshot chunk</p>";
        body+="<button onclick=\"document.getElementById('a').textContent='Clicked '+(++n)\">Tap</button><script>let n=0;</script>";
        auto p=open(body);assert(!run(p).empty());
        p.click(2);p.step();assert(!p.take_markup());
        p.pause(true);assert(!p.take_markup());p.pause(false);
        // Keep old visible hit IDs until commit; edits cancel partial snapshots.
        p.click(2);p.change_field(1,"DURING",false);p.step();
        auto html=snapshot(p);assert(html && html->find("Clicked 2")!=html->npos && html->find("DURING")!=html->npos);
        assert(p.error().empty());
    }
    {
        auto p=open(R"(<button onclick="this.setAttribute('onclick', 'this.textContent=\'Second\';this.removeAttribute(\'onclick\')');this.textContent='First'">Tap</button><script>Function=function(){throw Error('replaced global compiler')};</script>)");
        run(p);p.click(1);p.step();auto first=snapshot(p);assert(first && first->find("First")!=first->npos);
        p.click(1);p.step();auto second=snapshot(p);assert(second && second->find("Second")!=second->npos);
        p.click(1);p.step();auto third=snapshot(p);assert(!third && p.active() && p.stats().errors==0);
    }
    {
        auto p=open(R"HTML(<div onclick="invalid syntax )"><button onclick="event.stopPropagation();this.textContent='Child'">Tap</button></div>)HTML");
        run(p);p.click(2);p.step();auto html=snapshot(p);
        assert(html && html->find("Child")!=html->npos && p.stats().errors==0);
        // The cached child handler must keep stopping the parent on reentry.
        p.click(2);p.step();assert(!snapshot(p) && p.stats().errors==0);
    }
    if (const auto* model=std::getenv("PXA_SCRIPT_TEST_WASI_MODEL"); model && std::string_view(model)=="1") {
        for (auto source : {std::string(1000,'!')+"true", std::string(1000,'(')+"1"+std::string(1000,')'),
                            std::string(1000,'{')+"0;"+std::string(1000,'}')}) {
            auto p=open("<p id=a>Safe</p><script>"+source+"</script><script>document.getElementById('a').textContent='Parser recovered';</script>");
            assert(run(p).find("Parser recovered")!=std::string::npos);assert(p.stats().errors==1);
        }
        std::string nested="1";
        for(unsigned i=0;i<24;++i)nested="1+2*3<4&5^6|7&&("+nested+")";
        auto p=open("<p id=a>Safe</p><script>"+nested+";</script><script>document.getElementById('a').textContent='Mixed parser recovered';</script>");
        assert(run(p).find("Mixed parser recovered")!=std::string::npos);assert(p.stats().errors==1);
        const auto json="JSON.parse('"+std::string(100,'[')+"0"+std::string(100,']')+"')";
        for (const auto& source : {json, "var v=0;for(var i=0;i<100;i++)v=[v];JSON.stringify(v)"s}) {
            auto page=open("<p id=a>Safe</p><script>"+source+";</script><script>document.getElementById('a').textContent='JSON recovered';</script>");
            assert(run(page).find("JSON recovered")!=std::string::npos && page.stats().errors==1);
        }
        // Unwinding constructs fresh errors, preserving the prototype and
        // catch semantics. A cached mutable singleton would fail this case.
        auto errors=open("<p id=a></p><script>var a,b;try{"+json+"}catch(e){a=e}try{"+json+"}catch(e){b=e}document.getElementById('a').textContent=(a instanceof InternalError && b instanceof InternalError && a!==b && String(a).indexOf('stack overflow')>=0)?'Fresh errors':'Wrong errors';</script>");
        assert(run(errors).find("Fresh errors")!=std::string::npos && errors.stats().errors==0);
        auto reentry=open("<p id=a></p><script>var caught=false;var v={get x(){try{"+json+"}catch(e){caught=e instanceof InternalError}return 1}};var text=JSON.stringify(v);document.getElementById('a').textContent=(caught && text==='{\"x\":1}')?'Nested error caught':'Wrong nested error';</script>");
        assert(run(reentry).find("Nested error caught")!=std::string::npos && reentry.stats().errors==0);
    }
    for(auto bytes : {1024u,16384u,65536u,131072u}) {
        auto l=policy();l.heap_bytes=bytes;Page p;(void)p.open("<p>Low memory</p>","https://example.test/",bootstrap,{}, {},l);
        for(int i=0;i<16 && p.active();++i)p.step();
        assert(!p.active());assert(p.stats().peak_bytes<=bytes);
    }
    for(int i=0;i<16;++i){auto p=open("<p>Restart</p><script>window.onload=()=>{};</script>");assert(!run(p).empty());}
    std::cout<<"script regression passed: DOM, order, events, XHR/fetch, policies, limits, idle, restart\n";
}
