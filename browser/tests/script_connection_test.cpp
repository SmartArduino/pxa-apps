#include "../browser_script.hpp"
#include <cassert>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static Page open(std::string body) {
    Page p;Limits l;l.heap_bytes=512*1024;l.turn_us=200000;
    assert(p.open("<html><body>"+body+"</body></html>","https://example.test/",bootstrap,{},{},l));return p;
}
static std::string settle(Page& p) {
    std::string result;
    for(unsigned i=0;i<4096 && p.active();++i) {
        p.step();if(auto html=p.take_markup())result=std::move(*html);
        if(p.ready() && p.next_wake_us()!=0){if(p.stats().errors)std::cerr<<p.error()<<'\n';assert(!p.stats().errors);return result;}
    }
    assert(false && "connection page did not settle");return {};
}
static void measure() {
    for(bool attached:{false,true}) {
        std::string body;
        for(int i=0;i<40;++i)body+="<p>Unchanged paragraph</p>";
        body+="<button onclick=update()>Update</button><script>var n=0,box=document.createElement('div');";
        if(attached)body+="document.body.appendChild(box);";
        body+="function update(){++n;box.textContent='Staged '+n;box.style.color=n%2?'red':'blue';"
            "box.classList.toggle('odd');box.dataset.count=n;box.onclick=n%2?update:null;"
            "box.innerHTML='<b>Prepared '+n+'</b>';location.href='/count/'+n}</script>";
        auto p=open(body);assert(!settle(p).empty());const auto before=p.stats();const auto begin=std::clock();
        for(unsigned i=0;i<48;++i){assert(p.click(1));auto html=settle(p);assert(attached?!html.empty():html.empty());auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/count/"+std::to_string(i+1));}
        std::cout<<"{\"attached\":"<<(attached?"true":"false")<<",\"callbacks\":48,\"cpu_ms\":"
            <<1000.0*double(std::clock()-begin)/CLOCKS_PER_SEC<<",\"markup_batches\":"<<p.stats().markup_turns-before.markup_turns<<",\"vm_peak_bytes\":"<<p.stats().peak_bytes<<"}\n";
    }
}
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});assert(!bootstrap.runtime.empty());
    if(argc==3){assert(std::string_view(argv[2])=="--measure");measure();return 0;}
    {
        auto p=open(R"HTML(<div id=host><span id=leaf>Text</span></div><p id=proof></p><script>
function check(v){if(!v)throw Error('connection semantics')}
const host=document.getElementById('host'),leaf=document.getElementById('leaf'),text=leaf.firstChild;
check(document.isConnected && document.getRootNode()===document && host.isConnected && text.isConnected);
check(document.contains(document) && document.contains(text) && host.contains(host) && host.contains(text));
check(!text.contains(host) && !host.contains(null) && !host.contains(undefined));
let caught=false;try{host.contains({})}catch(e){caught=e instanceof TypeError}check(caught);
caught=false;try{host.contains()}catch(e){caught=e instanceof TypeError}check(caught);
check(host.parentElement===document.body && text.parentElement===leaf && document.parentElement===null);
host.remove();check(!host.isConnected && !text.isConnected && text.getRootNode()===host && host.contains(text));
const other=document.createElement('div');other.appendChild(host);check(text.getRootNode({composed:true})===other && !leaf.isConnected);
document.body.appendChild(other);check(text.getRootNode()===document && text.isConnected);
leaf.remove();check(!text.isConnected && text.getRootNode()===leaf && !host.contains(text));
host.insertBefore(leaf,null);check(text.isConnected && host.contains(text));
document.getElementById('proof').textContent='Connection PASS';</script>)HTML");
        assert(settle(p).find("Connection PASS")!=std::string::npos);
    }
    {
        auto p=open(R"HTML(<button onclick=stage()>Stage</button><button onclick=mount()>Mount</button><button onclick=unmount()>Remove</button><p id=proof>Visible</p><script>
let n=0,box=document.createElement('div'),leaf=document.createElement('span');box.appendChild(leaf);
function stage(){++n;leaf.textContent='Prepared '+n;box.innerHTML='<span>Staged '+n+'</span>';box.setAttribute('title','offscreen '+n);box.removeAttribute('title');box.style.color='green';box.classList.toggle('odd');box.dataset.count=n;delete box.dataset.count;box.onclick=()=>{box.textContent='Handler PASS'};box.addEventListener('click',noop);box.removeEventListener('click',noop);box.firstChild.firstChild.textContent='Staged '+n}
function noop(){}function mount(){document.body.appendChild(box)}function unmount(){box.remove()}
</script>)HTML");
        assert(!settle(p).empty());auto batches=p.stats().markup_turns;
        for(int i=0;i<8;++i){assert(p.click(1));assert(settle(p).empty() && p.stats().markup_turns==batches);}
        assert(p.click(2));auto html=settle(p);assert(html.find("Staged 8")!=html.npos && html.find("color:green")!=html.npos && html.find("data-pxa-click=\"4\"")!=html.npos);
        assert(p.click(4));assert(settle(p).find("Handler PASS")!=std::string::npos);
        assert(p.click(3));assert(settle(p).find("Handler PASS")==std::string::npos);batches=p.stats().markup_turns;
        assert(p.click(1));assert(settle(p).empty() && p.stats().markup_turns==batches);
        assert(p.click(2));assert(settle(p).find("Staged 9")!=std::string::npos);
    }
    {
        auto p=open(R"HTML(<button onclick=update()>Update</button><p id=proof>Before</p><script>
const ghost=document.createElement('div');function update(){ghost.setAttribute('data-value',{toString(){document.getElementById('proof').textContent='Conversion PASS';return 'offscreen'}})}
</script>)HTML");
        settle(p);assert(p.click(1));assert(settle(p).find("Conversion PASS")!=std::string::npos);
    }
    {
        auto p=open(R"HTML(<button onclick=stage()>Stage</button><button onclick=mount()>Mount</button><script>
const s=document.createElement('select');s.name='q';s.add(new Option('A','a'));s.add(new Option('B','b'));
function stage(){s.selectedIndex=1;s.options[0].selected=false;s.options[1].label='Prepared B'}function mount(){document.body.appendChild(s)}
</script>)HTML");
        settle(p);const auto batches=p.stats().markup_turns;assert(p.click(1));assert(settle(p).empty() && p.stats().markup_turns==batches);
        assert(p.click(2));const auto html=settle(p);assert(html.find("selected value=\"b\" label=\"Prepared B\"")!=html.npos);
    }
    {
        auto p=open(R"HTML(<button onclick=mount()>Mount</button><script>
const box=document.createElement('div'),script=document.createElement('script');script.src='/staged.js';box.appendChild(script);
function mount(){document.body.appendChild(box)}
</script>)HTML");
        settle(p);assert(!p.take_request());assert(p.click(1));settle(p);const auto r=p.take_request();assert(r && r->script && r->url=="https://example.test/staged.js");
        p.complete({r->id,200,"document.body.appendChild(document.createTextNode('Script PASS'))"});assert(settle(p).find("Script PASS")!=std::string::npos);
    }
    std::cout<<"connection PASS: roots, containment, reparenting, detached idle, mount/unmount, handlers, conversion effects, selects and scripts\n";
}
