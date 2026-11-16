// Observable DOM event behavior under the same bounded VM used by the app.
#include "../browser_script.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static Page open(std::string body) {
    Page p;Limits limits;limits.heap_bytes=512*1024;limits.turn_us=200000;
    assert(p.open("<html><body>"+body+"</body></html>","https://example.test/",bootstrap,{},{},limits));return p;
}
static std::string settle(Page& p) {
    std::string html;
    for(unsigned i=0;i<4096 && p.active();++i) {
        p.step();if(auto value=p.take_markup())html=std::move(*value);
        if(p.ready() && p.next_wake_us()!=0){if(p.stats().errors){std::cerr<<p.error()<<'\n';if(auto n=p.take_navigation())std::cerr<<n->url<<'\n';}assert(!p.stats().errors);return html;}
    }
    std::cerr<<p.error()<<'\n';assert(false && "event did not settle");return {};
}
static void check(const char* source) {
    static unsigned number=0;std::cout<<"event options case "<<++number<<'\n'<<std::flush;
    auto p=open("<p id=proof>Before</p><script>function ok(value,message){if(!value)throw Error(message)}\ntry {"+
        std::string(source)+"\ndocument.getElementById('proof').textContent='Event PASS'}catch(error){location.href='/failure/'+encodeURIComponent(error.message);throw error}</script>");
    assert(settle(p).find("Event PASS")!=std::string::npos);
}
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});assert(!bootstrap.runtime.empty());
    check(R"JS(
      const e=new Event('probe'),data={count:1};
      ok(!e.bubbles && !e.cancelable && !e.defaultPrevented && e.eventPhase===Event.NONE,'defaults');
      e.preventDefault();ok(!e.defaultPrevented,'noncancelable');
      const custom=new CustomEvent('probe',{bubbles:true,cancelable:true,detail:data});
      ok(custom instanceof Event && custom instanceof CustomEvent && custom.detail===data,'custom event');
      custom.returnValue=false;ok(custom.defaultPrevented && !custom.returnValue,'returnValue');
      custom.cancelBubble=true;custom.cancelBubble=false;ok(custom.cancelBubble,'cancelBubble is sticky');
      let calls=0;const one=new CustomEvent('detail',{get detail(){++calls;return 42}});ok(calls===1 && one.detail===42,'detail getter');
      ok(new Event('probe',null).type==='probe' && new CustomEvent('probe',null).detail===null,'null dictionary');
      for(const constructor of [Event,CustomEvent]){let threw=false;try{new constructor()}catch(e){threw=e instanceof TypeError}ok(threw,'missing type')}
    )JS");
    check(R"JS(
      const t=document.createElement('p'),seen=[];
      function late(){seen.push('late')}
      function removed(){seen.push('removed')}
      t.addEventListener('probe',()=>{seen.push('first');t.removeEventListener('probe',removed);t.addEventListener('probe',late)});
      t.addEventListener('probe',removed);t.dispatchEvent(new Event('probe'));
      ok(seen.join(',')==='first','removed and newly added listeners');
      t.dispatchEvent(new Event('probe'));ok(seen.join(',')==='first,first,late','later invocation');
      const object={handleEvent(e){ok(this===object && e.currentTarget===t,'object receiver');seen.push('object')}};
      t.addEventListener('object',object);object.handleEvent=function(){ok(this===object,'replacement receiver');seen.push('replaced')};
      t.dispatchEvent(new Event('object'));ok(seen[seen.length-1]==='replaced','dynamic handleEvent');t.removeEventListener('object',object);
      let count=0;function once(){++count;t.dispatchEvent(new Event('once'))}
      t.addEventListener('once',once,{once:true});t.dispatchEvent(new Event('once'));t.dispatchEvent(new Event('once'));ok(count===1,'remove before recursive once');
      function failing(){++count;throw Error('expected')}
      t.addEventListener('throw',failing,{once:true});try{t.dispatchEvent(new Event('throw'))}catch(e){ok(e.message==='expected','exception')}
      t.dispatchEvent(new Event('throw'));ok(count===2,'throwing once released');
      const options=function(){};options.once=true;t.addEventListener('function-options',()=>++count,options);
      t.dispatchEvent(new Event('function-options'));t.dispatchEvent(new Event('function-options'));ok(count===3,'callable options dictionary');
    )JS");
    check(R"JS(
      const t=document.createElement('p'),seen=[];function both(e){seen.push(e.eventPhase)}
      t.addEventListener('probe',both);t.addEventListener('probe',both,{once:true});t.addEventListener('probe',both,true);
      t.dispatchEvent(new Event('probe'));ok(seen.join(',')==='2,2','capture identity and duplicate options');
      t.removeEventListener('probe',both,{capture:false});t.dispatchEvent(new Event('probe'));ok(seen.length===3,'remove capture matching');
      t.removeEventListener('probe',both,true);t.dispatchEvent(new Event('probe'));ok(seen.length===3,'capture removed');
      const callbacks=[];for(let i=0;i<16;++i){const fn=()=>{};callbacks.push(fn);t.addEventListener('quota',fn,{once:true})}
      t.addEventListener('quota',callbacks[0],{passive:true});let limited=false;
      try{t.addEventListener('quota',()=>{})}catch(e){limited=e instanceof RangeError}ok(limited,'quota unchanged');
      t.dispatchEvent(new Event('quota'));for(const fn of callbacks)t.addEventListener('quota',fn);t.dispatchEvent(new Event('quota'));
    )JS");
    check(R"JS(
      const p=document.createElement('div'),t=document.createElement('p'),seen=[];p.appendChild(t);
      p.addEventListener('probe',e=>seen.push('capture:'+e.eventPhase),true);
      p.addEventListener('probe',e=>seen.push('bubble:'+e.eventPhase));
      t.addEventListener('probe',e=>{seen.push('target:'+e.eventPhase);ok(e.target===t && e.currentTarget===t,'event target')});
      const e=new Event('probe');t.dispatchEvent(e);ok(seen.join(',')==='capture:1,target:2','nonbubbling capture');
      seen.length=0;t.dispatchEvent(new Event('probe',{bubbles:true}));ok(seen.join(',')==='capture:1,target:2,bubble:3','phase order');
      ok(e.currentTarget===null && e.eventPhase===0 && e.target===t,'dispatch cleanup');
      let parentCalls=0;p.addEventListener('stop',()=>++parentCalls);
      t.addEventListener('stop',e=>{e.stopPropagation();seen.push('stop')});t.addEventListener('stop',()=>seen.push('same'));
      t.dispatchEvent(new Event('stop',{bubbles:true}));ok(!parentCalls && seen.slice(-2).join(',')==='stop,same','stopPropagation');
      t.addEventListener('immediate',e=>{seen.push('immediate');e.stopImmediatePropagation()});t.addEventListener('immediate',()=>seen.push('bad'));
      p.addEventListener('immediate',()=>seen.push('bad'));t.dispatchEvent(new Event('immediate',{bubbles:true}));ok(seen[seen.length-1]==='immediate','stopImmediatePropagation');
    )JS");
    check(R"JS(
      const t=document.createElement('p');let count=0;
      t.addEventListener('probe',e=>{e.preventDefault();ok(!e.defaultPrevented,'passive cancellation');++count},{passive:true});
      t.addEventListener('probe',e=>{e.preventDefault();ok(e.defaultPrevented,'active cancellation');++count});
      const e=new Event('probe',{cancelable:true});ok(t.dispatchEvent(e)===false && count===2,'dispatch return');
      t.addEventListener('nested',e=>{let rejected=false;try{t.dispatchEvent(e)}catch(error){rejected=error.name==='InvalidStateError'}ok(rejected,'recursive same event');e.stopImmediatePropagation()},{once:true});
      const again=new Event('nested');t.dispatchEvent(again);ok(again.eventPhase===0 && !again.cancelBubble,'stop cleanup');t.dispatchEvent(again);
      t.addEventListener('throw',()=>{throw Error('expected')},{once:true,passive:true});const failed=new Event('throw',{cancelable:true});
      try{t.dispatchEvent(failed)}catch(error){ok(error.message==='expected','caught callback')}
      failed.preventDefault();ok(failed.currentTarget===null && failed.eventPhase===0 && failed.defaultPrevented,'exception cleanup');t.dispatchEvent(failed);
    )JS");
    check(R"JS(
      const old=document.createElement('div'),other=document.createElement('div'),t=document.createElement('p'),seen=[];old.appendChild(t);
      old.addEventListener('probe',()=>{seen.push('capture');other.appendChild(t)},true);
      old.addEventListener('probe',()=>seen.push('old'));other.addEventListener('probe',()=>seen.push('new'));
      t.addEventListener('probe',()=>seen.push('target'));t.dispatchEvent(new Event('probe',{bubbles:true}));
      ok(seen.join(',')==='capture,target,old' && t.parentNode===other,'path is stable during DOM moves');
      const inline=document.createElement('p');inline.onprobe=()=>inline.addEventListener('probe',()=>seen.push('late'));
      inline.dispatchEvent(new Event('probe'));ok(seen[seen.length-1]==='old','inline additions wait');
    )JS");
    {
      auto p=open(R"HTML(<div id=p><button id=t>Run</button></div><p id=proof>Before</p><script>
      try {const t=document.getElementById('t'),parent=document.getElementById('p'),seen=[];
      function listener(label,phase,receiver){return function(e){if(e.target!==t || e.currentTarget!==receiver || this!==receiver || e.eventPhase!==phase){location.href='/native-phase/'+label+'/'+e.eventPhase+'/'+(e.target===t)+'/'+(e.currentTarget===receiver)+'/'+(this===receiver);throw Error('native phase')}seen.push(label+phase)}}
      window.addEventListener('click',listener('W',1,window),true);document.addEventListener('click',listener('D',1,document),true);
      parent.addEventListener('click',listener('P',1,parent),true);t.addEventListener('click',listener('T',2,t),true);t.addEventListener('click',listener('T',2,t));
      parent.addEventListener('click',listener('P',3,parent));document.addEventListener('click',listener('D',3,document));
      window.addEventListener('click',listener('W',3,window));window.addEventListener('click',function(){document.getElementById('proof').textContent=seen.join(',')});
      }catch(error){location.href='/native-setup/'+encodeURIComponent(error.message);throw error}
      </script>)HTML");
      std::cout<<"native event phases\n"<<std::flush;
      settle(p);assert(p.click(2));assert(settle(p).find("W1,D1,P1,T2,T2,P3,D3,W3")!=std::string::npos);
    }
    {
      std::cout<<"trusted lazy initialization\n"<<std::flush;
      auto p=open(R"HTML(<button onclick="document.getElementById('proof').textContent='Intrinsics PASS'">Run</button><p id=proof>Before</p><script>
      function forbidden(){throw Error('replaced intrinsic executed')}
      Object.entries=Object.defineProperties=Symbol=forbidden;
      Object.defineProperty(Object.prototype,'get',{get:forbidden,configurable:true});
      </script>)HTML");
      settle(p);assert(p.click(1));assert(settle(p).find("Intrinsics PASS")!=std::string::npos);
    }
    {
      std::cout<<"queued native edits\n"<<std::flush;
      auto p=open(R"HTML(<input id=a><input id=b><p id=proof>Before</p><script>window.onload=()=>{}</script>)HTML");
      settle(p);p.change_field(1,"FIRST",false);p.change_field(2,"SECOND",false);
      for(unsigned i=0;i<256 && p.action_pending();++i){p.step();if(p.action_pending())assert(!p.take_markup());}
      assert(!p.action_pending());const auto html=settle(p);assert(html.find("FIRST")!=html.npos && html.find("SECOND")!=html.npos);
    }
    {
      std::cout<<"consumed hit target\n"<<std::flush;
      auto p=open(R"HTML(<p id=t>Once</p><script>document.getElementById('t').addEventListener('click',()=>{},{once:true})</script>)HTML");
      assert(settle(p).find("data-pxa-click=\"1\"")!=std::string::npos);assert(p.click(1));
      assert(settle(p).find("data-pxa-click=")==std::string::npos);
    }
    {
      std::cout<<"once listener closures\n"<<std::flush;
      auto p=open(R"HTML(<button onclick=cycle()>Cycle</button><script>
      const t=document.createElement('p');let calls=0;function cycle(){const data=new Array(1024).fill(0);
      t.addEventListener('tick',()=>{if(data[0]!==0)throw Error('closure');++calls},{once:true});t.dispatchEvent(new Event('tick'))}
      </script>)HTML");
      settle(p);assert(p.click(1));settle(p);const auto before=p.stats().current_bytes;
      for(unsigned i=0;i<128;++i){assert(p.click(1));assert(settle(p).empty());}
      const auto after=p.stats();assert(after.current_bytes<=before+1024 && !after.errors);
      std::cout<<"once listener memory: before="<<before<<" after="<<after.current_bytes<<" peak="<<after.peak_bytes<<'\n';
    }
    std::cout<<"event options PASS: capture, once, passive, objects, mutation, propagation, native phases and release\n";
}
