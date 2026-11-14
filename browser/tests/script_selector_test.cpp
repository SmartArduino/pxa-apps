// Real QuickJS queries: tree order, scoping, backtracking, errors and budgets.
#include "../browser_script.hpp"
#include <cassert>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static unsigned case_number=0;
static Page open(std::string body,bool virtual_clock=false) {
    ++case_number;Page p;Limits l;l.heap_bytes=512*1024;l.turn_us=200000;
    Hooks h;if(virtual_clock)h.now_us=[](void*)->std::uint64_t{return 0;};
    assert(p.open("<html><body>"+body+"</body></html>","https://example.test/",bootstrap,h,{},l));return p;
}
static void settle(Page& p) {
    for(unsigned i=0;i<4096 && p.active();++i) {
        p.step();p.take_markup();
        if(p.ready() && p.next_wake_us()!=0) {
            if(p.stats().errors)std::cerr<<"Selector case "<<case_number<<": "<<p.error()<<'\n';
            assert(!p.stats().errors);return;
        }
    }
    std::cerr<<"Selector case "<<case_number<<": "<<p.error()<<" active="<<p.active()<<" ready="<<p.ready()<<" errors="<<p.stats().errors<<'\n';
    assert(false && "selector page did not settle");
}
static void measure() {
    std::string body="<div id=root>";
    for(unsigned i=0;i<32;++i)body+="<section><p class='target'>"+std::to_string(i)+"</p></section>";
    body+="</div><script>let n=0;document.body.onclick=()=>{let root=document.getElementById('root');"
        "for(let i=0;i<32;++i){if(root.querySelector('.target').textContent!=='0')throw Error('wrong query')}"
        "location.href='/count/'+(++n)};</script>";
    auto p=open(std::move(body));settle(p);assert(p.click(1));settle(p);assert(p.take_navigation());
    const auto before=p.stats().markup_turns;const auto begin=std::clock();
    for(unsigned i=0;i<32;++i) {
        assert(p.click(1));settle(p);const auto nav=p.take_navigation();
        assert(nav && nav->url=="https://example.test/count/"+std::to_string(i+2));
    }
    std::cout<<"{\"queries\":1024,\"cpu_ms\":"<<1000.0*double(std::clock()-begin)/CLOCKS_PER_SEC
        <<",\"vm_peak_bytes\":"<<p.stats().peak_bytes<<",\"markup_batches\":"<<p.stats().markup_turns-before<<"}\n";
}
int main(int argc,char** argv) {
    assert(argc==2 || (argc==3 && std::string_view(argv[2])=="--measure"));
    std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});
    assert(!bootstrap.runtime.empty());if(argc==3){measure();return 0;}
    {
        auto p=open(R"HTML(<div id=root class=reader><section><img id=first class="page alpha" data-src="a,b > c"></section>
        <img id=second class="page" data-src="next"></div><img id=outside class=page><script>
        function check(ok,label){if(!ok)throw Error(label)}
        const root=document.getElementById('root'),first=document.getElementById('first'),second=document.getElementById('second');
        check(document.querySelector('img')===first,'tree first');
        check(root.querySelector('.reader')===null,'exclude query root');
        let list=document.querySelectorAll('img.page, #first');
        check(list.length===3 && list[0]===first && list[1]===second,'tree order and duplicate groups');
        check(list.item(0)===first && list.item(999)===null && list.item(-1)===null,'collection item');
        check(root.querySelector('body img')===first,'ancestors outside query root');
        check(root.querySelector(':scope > img[data-src]')===second,'scope child');
        check(root.querySelectorAll(':scope').length===0 && document.querySelector(':scope')===document.documentElement,'scope root');
        check(root.querySelector('img[data-src="a,b > c"]')===first,'quoted punctuation');
        check(first.matches('section > img.page.alpha[data-src]'),'compound');
        check(first.closest('.reader')===root && root.closest('.reader')===root,'closest');
        const detached=document.createElement('div');detached.className='reader';
        check(detached.matches('.reader') && detached.closest(':scope')===detached,'detached matching');
        const before=root.querySelectorAll('img');root.appendChild(document.createElement('img'));
        check(before.length===2 && root.querySelectorAll('img').length===3,'static results');
        check(root.getElementsByTagName('img')[0]===first && root.getElementsByTagName('div').length===0,'tag descendants');
        first.className='page\talpha\n漫画';
        check(first.matches('.page.漫画') && root.getElementsByClassName('page alpha')[0]===first,'class whitespace');
        first.className='a\u00a0b';check(first.matches('.a\u00a0b') && !first.matches('.a'),'CSS ASCII whitespace');
        first.setAttribute('data-v','Hello-word end');
        check(first.matches('[data-v^="Hello"][data-v$=end][data-v*="-word"][data-v~=end]'),'attribute operators');
        check(!first.matches('[data-v~="word end"]') && !first.matches('[data-v^=""]'),'empty and multiword values');
        first.setAttribute('data-v','en-US');check(first.matches('[data-v|=en]'),'language prefix');
        first.setAttribute('data-v','Hello');check(first.matches('[data-v=hello i]') && !first.matches('[data-v=hello s]'),'ASCII flag');
        first.setAttribute('data-v','K');check(!first.matches('[data-v=k i]'),'ASCII flag is not Unicode folding');
        first.setAttribute('data-v','AK');check(first.matches('[data-v="aK" i]'),'mixed ASCII folding');
        const input=document.createElement('input');input.type='CHECKBOX';check(input.matches('input[type=checkbox]'),'HTML enum case');
        check(!input.matches('[type=checkbox s]'),'explicit case flag');
        </script>)HTML");settle(p);
    }
    {
        auto p=open(R"HTML(<div class=a><span id=far><section><span id=near><p id=t>Text</p></span></section></span></div><script>
        const t=document.getElementById('t');
        if(document.querySelector('div > span p')!==t || !t.matches('div > span p'))throw Error('ancestor backtracking');
        if(document.querySelector('div > span > p')!==null)throw Error('child relation');
        if(t.closest('section > span')!==document.getElementById('near'))throw Error('closest child relation');
        </script>)HTML");settle(p);
    }
    {
        auto p=open(R"HTML(<script>
        function rejects(source,kind=DOMException){let caught=false;try{document.querySelectorAll(source)}catch(e){caught=e instanceof kind && (kind!==DOMException || e.name==='SyntaxError' && e.code===12 && !(e instanceof SyntaxError))}if(!caught)throw Error('bad selector '+source)}
        for(const s of ['', ' ', ',', 'div,', '>p','div >','div..a','[x=]','[x="bad]','[x=hello q]', 'div + p',':not(div)', '#123', '.a\\:b'])rejects(s);
        rejects('x'.repeat(513),RangeError);rejects(Array(10).fill('p').join(','),RangeError);
        if(document.querySelectorAll('.missing').length || document.querySelector('.missing')!==null)throw Error('miss');
        const t=document.createElement('p');t.textContent='Recovered';document.body.appendChild(t);
        if(document.querySelector('p')!==t)throw Error('recovery');
        </script>)HTML");settle(p);
    }
    {
        auto p=open(R"HTML(<script>
        let host=document.createElement('div'),leaf=host;
        for(let i=0;i<130;++i){let next=document.createElement('div');leaf.appendChild(next);leaf=next}
        let caught=false;try{host.querySelectorAll('aside div')}catch(e){caught=e instanceof RangeError}
        if(!caught)throw Error('selector work budget');
        leaf.id='end';if(host.querySelector('#end')!==leaf)throw Error('budget recovery');
        </script>)HTML",true);settle(p);
    }
    {
        auto p=open(R"HTML(<div class=controls><button type=button data-label="第A集">Read</button></div><script>
        document.querySelectorAll('.controls > button').forEach(button=>button.addEventListener('click',e=>{
          if(!e.target.matches('button[type=BUTTON]') || !e.target.closest('.controls'))throw Error('event query');
          if(document.querySelector('.controls button[type=BUTTON]')!==e.target ||
             document.getElementsByClassName('controls')[0]!==e.target.parentNode)throw Error('nested event query');
          if(document.querySelector('button[data-label="第a集" i]')!==e.target)throw Error('mixed-case event query');
          location.href='/bound-event';
        }));</script>)HTML");settle(p);assert(p.click(1));settle(p);
        const auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/bound-event");
    }
    std::cout<<"selectors PASS: tree order, compound/attribute/list/child/descendant selectors, scope, closest, backtracking and budgets\n";
}
