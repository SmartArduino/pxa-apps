// Fragment failure recovery, identity, radio ownership and real VM hot path.
#include "../browser_script.hpp"
#include <cassert>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static unsigned case_number=0;
static Page open(std::string body,bool clocked=false) {
    ++case_number;Page p;Limits l;l.heap_bytes=512*1024;l.turn_us=200000;
    Hooks h;if(clocked)h.now_us=[](void*)->std::uint64_t{return 0;};
    assert(p.open("<html><head><style>.proof{color:red}</style></head><body>"+body+"</body></html>","https://example.test/",bootstrap,h,{},l));return p;
}
static std::string settle(Page& p) {
    std::string html;
    for(unsigned i=0;i<4096 && p.active();++i) {
        p.step();if(auto v=p.take_markup())html=std::move(*v);
        if(p.ready() && p.next_wake_us()!=0) {
            if(p.stats().errors)std::cerr<<"Fragment case "<<case_number<<": "<<p.error()<<'\n';
            assert(!p.stats().errors);return html;
        }
    }
    assert(false && "fragment did not settle");return {};
}
static unsigned hit(const std::string& html,std::string_view id) {
    const auto at=html.find(" id=\""+std::string(id)+"\"");assert(at!=html.npos);
    const auto start=html.rfind('<',at),key=html.find("data-pxa-click=\"",start);
    assert(key<at);return unsigned(std::stoul(html.substr(key+16)));
}
static void measure(bool attributes) {
    std::string body=R"HTML(<button id=go onclick="update()">Update</button><div id=host></div><script>
    let n=0;function update(){const h=document.getElementById('host');
      h.innerHTML='<p id=sample class=sample style="color:red;font-size:16px">Updated '+(++n)+'</p>';
      if(h.firstChild.parentNode!==h || h.textContent!=='Updated '+n)throw Error('fragment result');
      location.href='/count/'+n;
    }</script>)HTML";
    if(attributes) {
        const auto at=body.find("class=sample style=");assert(at!=body.npos);
        body.replace(at,19,"class=sample title=chapter data-page=2 data-state=ready lang=en dir=ltr role=paragraph aria-label=sample style=");
    }
    auto p=open(std::move(body));
    const auto html=settle(p);const auto id=hit(html,"go");assert(p.click(id));settle(p);assert(p.take_navigation());
    const auto before=p.stats().markup_turns;const auto begin=std::clock();
    for(unsigned i=0;i<64;++i) {
        assert(p.click(id));settle(p);const auto nav=p.take_navigation();
        assert(nav && nav->url=="https://example.test/count/"+std::to_string(i+2));
    }
    std::cout<<"{\"updates\":64,\"cpu_ms\":"<<1000.0*double(std::clock()-begin)/CLOCKS_PER_SEC
        <<",\"vm_peak_bytes\":"<<p.stats().peak_bytes<<",\"markup_batches\":"<<p.stats().markup_turns-before<<"}\n";
}
int main(int argc,char** argv) {
    assert(argc==2 || (argc==3 && (std::string_view(argv[2])=="--measure" || std::string_view(argv[2])=="--measure-attributes")));
    std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});
    assert(!bootstrap.runtime.empty());if(argc==3){measure(std::string_view(argv[2])=="--measure-attributes");return 0;}
    {
        auto p=open(R"HTML(<form><input id=peer type=radio name=mode value=old checked><div id=host>
        <button id=old type=button onclick="location.href='/old-click'">Old</button><input id=edit value=kept></div></form><script>
        const h=document.getElementById('host'),old=document.getElementById('old'),edit=document.getElementById('edit'),
          peer=document.getElementById('peer'),saved=h.childNodes,root=document.documentElement,body=document.body,head=document.head;
        function check(ok,label){if(!ok)throw Error(label)}
        function rejected(source){let caught=false;try{h.innerHTML=source}catch(e){caught=e.name==='RangeError'}
          check(caught,'expected recoverable quota');
          check(h.childNodes===saved && old.parentNode===h && edit.parentNode===h && edit.value==='kept','identity retained');
          check(peer.checked && document.getElementById('old')===old,'radio and lookup retained');
          check(document.documentElement===root && document.head===head && document.body===body,'document roots retained');
        }
        rejected('<p>'+ 'x'.repeat(8200)+'</p>');
        rejected('<p>'+ '中'.repeat(2800)+'</p>');
        rejected('<input type=radio name=mode checked><p>'.repeat(60));
        rejected('<body><head><html>'+ '<p>n</p>'.repeat(40));
        rejected('<input type=radio name=mode checked><p title="'+ 'x'.repeat(4097)+'">bad</p>');
        rejected('<p style="color:rgb('+ '('.repeat(17)+'1'+ ')'.repeat(17)+')">bad</p>');
        for(let i=0;i<12;++i)rejected('<style>'+'.unused{color:red}'.repeat(50)+'</style><p title="'+'x'.repeat(4097)+'">bad</p>');
        </script>)HTML",true);
        const auto html=settle(p);assert(p.click(hit(html,"old")));settle(p);const auto nav=p.take_navigation();
        assert(nav && nav->url=="https://example.test/old-click");
    }
    {
        auto p=open(R"HTML(<button id=go onclick="fail()">Reject</button><div id=host><p id=old>Keep</p></div><script>
        const h=document.getElementById('host'),old=h.firstChild;let n=0;
        function fail(){let caught=false;try{h.innerHTML='<input id=new><p title="'+'x'.repeat(4097)+'">Bad</p>'}
          catch(e){caught=e.name==='RangeError'}
          if(!caught || h.firstChild!==old)throw Error('recoverable failure');location.href='/rejected/'+(++n);
        }</script>)HTML");
        const auto html=settle(p);const auto id=hit(html,"go");const auto batches=p.stats().markup_turns;
        for(unsigned i=0;i<12;++i) {
            assert(p.click(id));assert(settle(p).empty());const auto nav=p.take_navigation();
            assert(nav && nav->url=="https://example.test/rejected/"+std::to_string(i+1));
        }
        assert(p.stats().markup_turns==batches);
    }
    {
        auto p=open(R"HTML(<form id=f><input id=peer type=radio name=mode value=old checked><div id=host><p id=old>Old</p></div></form>
        <p class=proof>Proof</p><script>
        const h=document.getElementById('host'),old=h.firstChild;
        for(let i=0;i<12;++i){try{h.innerHTML='<style>'+'.unused{color:red}'.repeat(50)+'</style><p title="'+'x'.repeat(4097)+'">Bad</p>'}catch(e){}}
        h.innerHTML='<style id=sheet>.proof{color:green}</style><div><input id=first type=radio name=mode value=a checked><input id=last type=radio name=mode value=b checked></div><p id=dup ID=ignored>New</p>';
        if(old.parentNode!==null || h.firstChild.parentNode!==h)throw Error('commit parents');
        if(document.getElementById('peer').checked || document.getElementById('first').checked || !document.getElementById('last').checked)throw Error('committed radio group');
        if(!document.getElementById('sheet').textContent.includes('color:green'))throw Error('style budget rollback');
        if(!document.getElementById('dup') || document.getElementById('ignored'))throw Error('first duplicate attribute');
        document.getElementById('f').submit();
        </script>)HTML",true);
        settle(p);const auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/?mode=b");
    }
    {
        auto p=open(R"HTML(<textarea id=area>Original</textarea><script>
        const n=document.getElementById('area'),old=n.firstChild;let caught=false;
        try{n.innerHTML='\0'.repeat(3000)}catch(e){caught=e.name==='RangeError'}
        if(!caught || n.firstChild!==old || old.parentNode!==n)throw Error('text context rollback');
        n.innerHTML='&lt;b&gt;New&lt;/b&gt;';if(n.textContent!=='<b>New</b>')throw Error('text context recovery');
        </script>)HTML");settle(p);
    }
    {
        auto p=open(R"HTML(<script>
        const outer=document.createElement('div'),h=document.createElement('div');
        for(let i=0;i<6;++i){const box=document.createElement('div');
          for(let j=0;j<90;++j)box.appendChild(document.createElement('b'));outer.appendChild(box)}
        outer.appendChild(h);h.innerHTML='<p>Old</p>';const old=h.firstChild;
        for(let i=0;i<12;++i){let caught=false;
          try{h.innerHTML='<style>'+'.unused{color:red}'.repeat(50)+'</style><input type=radio name=mode checked>'}
          catch(e){caught=e.name==='RangeError'}
          if(!caught || h.firstChild!==old || old.parentNode!==h)throw Error('radio traversal before commit');
        }
        h.innerHTML='<style>.proof{color:green}</style>';
        if(!h.firstChild.textContent.includes('color:green'))throw Error('radio rejection style rollback');
        </script>)HTML",true);settle(p);
    }
    {
        // document.write appends directly: failed partial writes retain their
        // style charge, so they cannot bypass the cumulative stylesheet cap.
        auto p=open(R"HTML(<script>
        for(let i=0;i<12;++i){try{document.write('<style>'+'.unused{color:red}'.repeat(50)+'</style><p title="'+'x'.repeat(4097)+'">Bad</p>')}catch(e){}}
        const h=document.createElement('div');h.innerHTML='<style>.proof{color:green}</style>';
        if(h.firstChild.textContent!=='')throw Error('partial writes bypassed style budget');
        </script>)HTML",true);settle(p);
    }
    {
        // Catching a fragment error must not turn a hard VM allocation failure
        // into a recoverable quota failure. Retain old trees to force real OOM.
        auto p=open(R"HTML(<div id=host></div><script>
        const h=document.getElementById('host'),keep=[];
        try{for(let i=0;i<128;++i){h.innerHTML='<p>'+'x'.repeat(6000)+'</p>';keep.push(h.firstChild)}}catch(e){}
        </script>)HTML",true);
        for(unsigned i=0;i<4096 && p.active();++i)p.step();
        assert(!p.active() && p.stats().errors && p.error()=="脚本内存达到上限");
    }
    {
        // A catch/retry loop still polls the normal VM execution deadline.
        auto p=open(R"HTML(<div id=host></div><script>
        const h=document.getElementById('host');
        for(;;){try{h.innerHTML='<p title="'+'x'.repeat(4097)+'">Bad</p>'}catch(e){}}
        </script>)HTML");
        for(unsigned i=0;i<4096 && p.active();++i)p.step();
        assert(!p.active() && p.stats().timeouts==1 && p.error()=="脚本执行超时");
    }
    std::cout<<"fragment updates PASS: failure identity/handlers/roots, no empty snapshots, radio commit, CSS rollback and recovery\n";
}
