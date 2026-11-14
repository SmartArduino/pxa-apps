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
static Page open(std::string body,bool clocked=false,unsigned fragment_nodes=96) {
    ++case_number;Page p;Limits l;l.heap_bytes=512*1024;l.turn_us=200000;
    l.fragment_nodes=fragment_nodes;
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
    std::cerr<<"Fragment case "<<case_number<<": "<<p.error()<<"; peak="<<p.stats().peak_bytes<<'\n';
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
static void measure_load(bool rich,bool links=false,bool unmatched=false,int escaping=0) {
    std::string body;
    for(unsigned i=0;i<48;++i)body+=links?"<a href=/item/"+std::to_string(i)+">Line</a>":rich?
        "<p ID=p"+std::to_string(i)+" CLASS=Case TITLE=ChApTeR DATA-state=ReAdY LANG=en DIR=ltr ROLE=paragraph ARIA-label=Sample STYLE=\"color:red;font-size:16px\">Line</p>":"<p>Line</p>";
    if(unmatched) {
        body.clear();for(unsigned i=0;i<28;++i)body+="<DiV>";
        body+="<p id=leaf>Line</p>";
        for(unsigned i=0;i<28;++i)body+="</unmatched>";
        for(unsigned i=0;i<28;++i)body+="</dIv>";
    }
    body+="<script>const nodes=document.querySelectorAll('"+std::string(unmatched?"div":links?"a":"p")+"');if(nodes.length!=="+std::string(unmatched?"28":"48")+")throw Error('node count');";
    if(unmatched)body+="let at=document.getElementById('leaf');for(let i=0;i<28;++i){at=at.parentNode;if(at.nodeName!=='DIV')throw Error('unmatched topology')}if(at.parentNode!==document.body)throw Error('unmatched roots');";
    if(rich)body+="for(const n of nodes)if(n.className!=='Case' || n.getAttribute('title')!=='ChApTeR' || n.style.fontSize!=='16px')throw Error('parsed attribute');";
    body+="location.href='/parsed';</script>";
    if(escaping) {
        const auto value=escaping==2?"'a'.repeat(256)+' & < > \" '+ 'b'.repeat(256)":"'&<>\"'.repeat(128)";
        body="<p id=sample></p><script>const n=document.getElementById('sample');n.title=n.textContent="+std::string(value)+
            ";const raw=n.outerHTML;if(!raw.includes('&amp;') || !raw.includes('&quot;'))throw Error('escaped result');location.href='/parsed';</script>";
    }
    std::size_t peak=0;unsigned batches=0;const auto begin=std::clock();
    for(unsigned i=0;i<8;++i){auto p=open(body);settle(p);const auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/parsed");peak=std::max(peak,p.stats().peak_bytes);batches+=p.stats().markup_turns;}
    std::cout<<"{\"pages\":8,\"nodes_per_page\":"<<(escaping?1:unmatched?29:48)<<",\"cpu_ms\":"<<1000.0*double(std::clock()-begin)/CLOCKS_PER_SEC
        <<",\"vm_peak_bytes\":"<<peak<<",\"markup_batches\":"<<batches<<"}\n";
}
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);
    std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});
    assert(!bootstrap.runtime.empty());if(argc==3){const std::string_view mode=argv[2];
        if(mode=="--measure-load" || mode=="--measure-plain" || mode=="--measure-links" || mode=="--measure-unmatched" || mode=="--measure-escaping" || mode=="--measure-escape-sparse")
            measure_load(mode=="--measure-load",mode=="--measure-links",mode=="--measure-unmatched",mode=="--measure-escaping"?1:mode=="--measure-escape-sparse"?2:0);
        else {assert(mode=="--measure" || mode=="--measure-attributes");measure(mode=="--measure-attributes");}return 0;}
    {
        // Keep the legacy parser-step boundary, including comments that end
        // input and raw bodies whose apparent tags are only text. Run this
        // corpus against the original builder as well as the preflight path.
        struct Input { const char* expression; unsigned steps; };
        const Input inputs[]={
            {"'<p>x</p>'",3}, {"'<p title=\">\">x</p>'",3},
            {"'<!-- a -->'",1}, {"'<!-- a --><!-- b -->'",1},
            {"'<p><!-- a -->x<!-- b --></p>'",3}, {"'<p>x</p><!-- tail -->'",4},
            {"'<!doctype html><p>x</p>'",4}, {"'<?ignored?><p>x</p>'",4},
            {"'<div><span>x</span></div>'",5}, {"'</unmatched><p>x</p>'",4},
            {"'<img><br/><hr>'",3}, {"'<p/>text'",2},
            {"'<p'",1}, {"'<p><unfinished'",2},
            {"'<textarea>a<b>&amp;</textarea>'",1},
            {"'<textareax><p>x</p></textareax><p>end</p>'",8},
            {"'<TITLEfoo><b>x</b></TITLEfoo>'",5},
            {"'<plaintext-foo>text</plaintext-foo><p>x</p>'",6},
            {"'<textarea=value><b>x</b></textarea>'",1},
            {"'< /textarea><p>x</p>'",4},
            {"'<TeXtArEa>a</textareax><b>text</b></TEXTAREA><p>x</p>'",4},
            {"'<title>a<b>&amp;</title><p>x</p>'",4},
            {"'<xmp>a<b>&amp;</xmp><p>x</p>'",4},
            {"'<iframe><b>text</b></iframe><p>x</p>'",4},
            {"'<noembed><b>text</b></noembed><p>x</p>'",4},
            {"'<noframes><b>text</b></noframes><p>x</p>'",4},
            {"'<plaintext>a<b>text</b><p>x</p>'",1},
            {"'<style>.a{color:red}</style><p>x</p>'",4},
            {"'<script><b>text</b></scr'+'ipt><p>x</p>'",4},
        };
        for(const auto& input:inputs)for(const auto budget:{input.steps-1,input.steps,input.steps+1}) {
            const auto allowed=budget>=input.steps;
            const std::string body="<div id=h><p id=old>Original</p></div><script>"
                "const h=document.getElementById('h'),old=h.firstChild,saved=h.childNodes;let caught=false;"
                "try{h.innerHTML="+std::string(input.expression)+"}catch(e){if(e.name!=='RangeError')throw e;caught=true}"
                "if(caught!=="+(allowed?"false":"true")+")throw Error('parser-step boundary');"
                "if(caught ? h.childNodes!==saved || h.firstChild!==old || old.parentNode!==h : old.parentNode!==null)throw Error('replacement identity');"
                "location.href='/budget';</script>";
            auto p=open(body,true,budget);settle(p);const auto nav=p.take_navigation();
            assert(nav && nav->url=="https://example.test/budget");
        }
        auto p=open(R"HTML(<script>let caught=false;
        try{document.write('<p id=partial>Kept</p><p id=late>Late</p>')}catch(e){caught=e.name==='RangeError'}
        if(!caught || document.getElementById('partial').textContent!=='Kept' || document.getElementById('late'))throw Error('partial write order');
        location.href='/partial';</script>)HTML",true,3);
        settle(p);const auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/partial");
    }
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
        auto p=open(R"HTML(<STYLE ID=Sheet Media=ScReEn>.Case{color:green}</STYLE>
        <P ID=First iD=Ignored CLASS=Case TITLE=VaLuE data-State=ReAdY StYlE="CoLoR:red;font-size:16px">Text</P>
        <p id=Empty></p><p ID=Unicode K=KePt></p><script>
        const n=document.getElementById('First');
        if(document.getElementById('Ignored') || n.className!=='Case' || n.title!=='VaLuE' || n.getAttribute('data-state')!=='ReAdY' || n.style.fontSize!=='16px')throw Error('mixed attribute values');
        if(document.getElementById('Sheet').getAttribute('media')!=='ScReEn')throw Error('style attributes');
        if(document.getElementById('Unicode').getAttribute('k')!=='KePt')throw Error('Unicode name fallback');
        const h=document.getElementById('Empty');h.innerHTML='<STYLE ID=DynamicSheet media=ScReEn>.Case{color:blue}</STYLE><P ID=Next CLASS=Case TITLE=NeXt>Next</P><b></b>';
        if(!document.getElementById('DynamicSheet') || document.getElementById('Next').title!=='NeXt' || h.lastChild.nodeName!=='B')throw Error('dynamic attributes and empty list');
        h.innerHTML='<p K=Keep ID=Fallback class=Case>Fallback</p>';
        if(document.getElementById('Fallback').getAttribute('k')!=='Keep')throw Error('fragment Unicode fallback');
        h.innerHTML='<p '+ '\u212a'.repeat(64)+'=LongCase ID=LongName>Long</p>';
        if(document.getElementById('LongName').getAttribute('k'.repeat(64))!=='LongCase')throw Error('long UTF-8 name fallback');
        const before=h.firstChild;let caught=false;try{h.innerHTML='<p '+ 'X'.repeat(65)+'=bad>Bad</p>'}catch(e){caught=e.name==='RangeError'}
        if(!caught || h.firstChild!==before)throw Error('overlong name rollback');
        </script>)HTML");settle(p);
    }
    {
        auto p=open(R"HTML(<div id=host></div><script>
        const h=document.getElementById('host');
        h.innerHTML='<DiV>'.repeat(28)+'<p id=leaf>Line</p>'+'</unmatched>'.repeat(28)+'</dIv>'.repeat(28)+'<b id=tail>Tail</b>';
        let at=document.getElementById('leaf');
        for(let i=0;i<28;++i){at=at.parentNode;if(at.nodeName!=='DIV')throw Error('unmatched close topology')}
        if(at.parentNode!==h || document.getElementById('tail').parentNode!==h)throw Error('closing sibling');
        const name='custom-'+'a'.repeat(64);
        h.innerHTML='<'+name+'><p>Long</p></'+name+'><p id=after>After</p>';
        if(h.firstChild.tagName!==name.toUpperCase() || document.getElementById('after').parentNode!==h)throw Error('long closing name');
        h.innerHTML='<div><p id=kept>Keep</p></'+'x'.repeat(6000)+'></div><p id=after>After</p>';
        if(document.getElementById('kept').parentNode!==h.firstChild || document.getElementById('after').parentNode!==h)throw Error('large unmatched closing name');
        </script>)HTML");settle(p);
    }
    {
        auto p=open(R"HTML(<div id=host></div><button onclick="inspect()">Read HTML</button><script>
        const h=document.getElementById('host'),text='A&<>"\0';h.title=text;h.textContent=text;
        const encoded='A&amp;&lt;&gt;&quot;&#xfffd;';
        function inspect(){
          if(h.innerHTML!==encoded || h.outerHTML!=='<div id="host" title="'+encoded+'">'+encoded+'</div>')throw Error('HTML escaping');
          location.href='/escaped';
        }
        </script>)HTML");
        const auto html=settle(p);const auto key=html.find("data-pxa-click=\"");assert(key!=html.npos);
        assert(p.click(unsigned(std::stoul(html.substr(key+16)))));settle(p);
        const auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/escaped");
    }
    {
        auto p=open(R"HTML(<script>
        const n=document.createElement('div');n.textContent='&'.repeat(2048);
        if(n.outerHTML!=='<div>'+'&amp;'.repeat(2048)+'</div>')throw Error('dense escaping');
        </script>)HTML");settle(p);
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
