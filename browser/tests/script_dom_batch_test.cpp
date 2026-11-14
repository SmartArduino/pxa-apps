#include "../browser_script.hpp"
#include "../browser_core.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static Page open(std::string body,std::size_t heap=512*1024) {
    Page p;Limits l;l.heap_bytes=heap;l.turn_us=200000;
    assert(p.open("<html><body>"+body+"</body></html>","https://example.test/",bootstrap,{},{},l));return p;
}
static std::string settle(Page& p) {
    std::string result;
    for(unsigned i=0;i<4096 && p.active();++i) {
        p.step();if(auto html=p.take_markup())result=std::move(*html);
        if(p.ready() && p.next_wake_us()!=0){if(p.stats().errors)std::cerr<<p.error()<<'\n';assert(!p.stats().errors);return result;}
    }
    assert(false && "batch page did not settle");return {};
}
static const char* check="function check(v,name){if(!v){location.href='/failed/'+name;throw Error(name)}}";
static std::string script(std::string text){return "<script>"+std::string(check)+text+"</script>";}
static browser::Document document(std::string_view html) {
    browser::Document d;d.clear("https://example.test/");browser::Parser p(d,true);p.feed(html);p.finish();return d;
}
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});assert(!bootstrap.runtime.empty());
    {
        auto p=open("<div id=host><span id=last>Last</span></div><p id=proof></p>"+script(R"JS(
const fragment=document.createDocumentFragment(),C=DocumentFragment,list=fragment.childNodes,host=document.getElementById('host');
check(fragment instanceof C && fragment.constructor===C && fragment.nodeType===11 && fragment.nodeName==='#document-fragment','constructor');
check(fragment.parentNode===null && !fragment.isConnected && fragment.tagName===undefined,'detached');
let caught=false;try{C()}catch(e){caught=e instanceof TypeError}check(caught,'new-required');
const a=document.createElement('p'),b=document.createElement('p');a.id=b.id='same';a.className='chapter';a.textContent='First';b.textContent='Second';
fragment.appendChild(a);fragment.appendChild(b);check(fragment.getElementById('same')===a && fragment.querySelector('.chapter')===a && fragment.querySelectorAll('p').length===2,'queries');
check(a.parentElement===null && a.getRootNode()===fragment && fragment.contains(a),'roots');
check(host.insertBefore(fragment,host.firstChild)===fragment && fragment.childNodes===list && list.length===0,'transfer');
check(host.childNodes[0]===a && host.childNodes[1]===b && host.childNodes[2].id==='last' && a.isConnected && a.parentElement===host,'order');
check(document.getElementById('same')===a && fragment.getElementById('same')===null && !fragment.contains(a),'query-after');
DocumentFragment=function(){throw Error('replaced global')};const next=document.createDocumentFragment();check(next instanceof C,'factory-original');
next.textContent='Text';check(host.appendChild(next)===next && next.textContent==='' && host.lastChild.nodeType===3,'text-transfer');
document.getElementById('proof').textContent='Batch PASS';)JS"));
        const auto html=settle(p);assert(html.find("Batch PASS")!=html.npos && html.find("#document-fragment")==html.npos);
    }
    {
        auto p=open("<button onclick=empty()>Empty</button><div id=host>Keep</div>"+script(R"JS(
const f=new DocumentFragment(),host=document.getElementById('host');function empty(){check(host.appendChild(f)===f,'empty-append');host.insertBefore(f,host.firstChild)}
)JS"));
        settle(p);const auto batches=p.stats().markup_turns;assert(p.click(1));assert(settle(p).empty() && p.stats().markup_turns==batches);
    }
    {
        auto p=open("<div id=host></div><p id=proof></p>"+script(R"JS(
const host=document.getElementById('host'),f=new DocumentFragment(),a=document.createElement('p'),b=document.createElement('p');a.textContent='A';b.textContent='B';a.onclick=()=>{};f.appendChild(a);f.appendChild(b);const source=f.childNodes;
for(let i=0;i<191;i++)host.appendChild(document.createTextNode(''));
let caught=false;try{host.appendChild(f)}catch(e){caught=e instanceof RangeError}
check(caught && source.length===2 && source[0]===a && a.parentNode===f && host.childNodes.length===191 && typeof a.onclick==='function','quota-rollback');
host.removeChild(host.firstChild);host.appendChild(f);check(host.childNodes.length===192 && source.length===0 && host.childNodes[190]===a && host.childNodes[191]===b,'quota-recovery');
const other=new DocumentFragment(),inside=document.createElement('div');other.appendChild(inside);caught=false;
try{inside.appendChild(other)}catch(e){caught=e.name==='HierarchyRequestError'}check(caught && other.firstChild===inside && inside.parentNode===other,'cycle');
caught=false;try{document.createTextNode('x').appendChild(other)}catch(e){caught=e.name==='HierarchyRequestError'}check(caught && other.firstChild===inside,'text-parent');
caught=false;try{other.appendChild(document)}catch(e){caught=e.name==='HierarchyRequestError'}check(caught && document.parentNode===null,'document-child');
caught=false;try{host.insertBefore(other,document.createElement('p'))}catch(e){caught=e.name==='NotFoundError'}check(caught && other.firstChild===inside,'missing-reference');
document.getElementById('proof').textContent='Rollback PASS';)JS"));
        assert(settle(p).find("Rollback PASS")!=std::string::npos);
    }
    {
        auto p=open("<form id=form method=post action=/send><input name=q type=radio checked value=old></form><p id=proof></p>"+script(R"JS(
const form=document.getElementById('form'),f=new DocumentFragment();
for(const value of ['one','two']){const wrapper=document.createElement('div'),r=document.createElement('input');r.type='radio';r.name='q';r.value=value;r.checked=true;wrapper.appendChild(r);f.appendChild(wrapper)}
check(f.querySelectorAll('input')[0].checked && f.querySelectorAll('input')[1].checked,'staged-radios');form.appendChild(f);
const radios=form.querySelectorAll('input');check(!radios[0].checked && !radios[1].checked && radios[2].checked,'radio-owner');form.submit();
)JS"));
        settle(p);const auto nav=p.take_navigation();assert(nav && nav->post && nav->body=="q=two");
    }
    {
        auto p=open("<select id=s name=q><option value=a>A</select>"+script(R"JS(
const s=document.getElementById('s'),f=new DocumentFragment(),b=new Option('B','b',false,true),group=document.createElement('optgroup'),c=new Option('C','c');group.disabled=true;group.appendChild(c);f.appendChild(b);f.appendChild(group);
s.insertBefore(f,s.firstChild);check(s.length===3 && s.value==='b' && s.selectedIndex===0 && s.options[2].value==='a','select-insertion');
s.selectedIndex=-1;s.appendChild(new DocumentFragment());check(s.selectedIndex===-1,'empty-select');s.value='b';
)JS"));
        auto d=document(settle(p));assert(d.fields.size()==1 && d.select_options.size()==3 && d.select_options[0].selected && d.select_options[1].disabled && d.fields[0].value=="b");
    }
    {
        auto p=open("<button onclick=mount()>Mount</button><div id=host></div><p id=proof></p>"+script(R"JS(
const f=new DocumentFragment();for(const id of ['first','second']){const s=document.createElement('script');s.id=id;s.src='/'+id+'.js';f.appendChild(s)}
function mount(){document.getElementById('host').appendChild(f);check(f.childNodes.length===0,'script-transfer')}
)JS"));
        settle(p);assert(!p.take_request());assert(p.click(1));settle(p);const auto one=p.take_request(),two=p.take_request();assert(one && two && one->script && two->script);
        p.complete({one->id,200,"if(!document.getElementById('second'))throw Error('incomplete batch');document.getElementById('proof').textContent='Script one'"});
        p.complete({two->id,200,"document.getElementById('proof').textContent+=' two'"});assert(settle(p).find("Script one two")!=std::string::npos);
    }
    {
        auto p=open("<div id=host></div><p id=proof></p>"+script(R"JS(
const host=document.getElementById('host'),f=new DocumentFragment();for(let i=0;i<4;i++){const p=document.createElement('p');for(let j=0;j<100;j++)p.appendChild(document.createTextNode(''));host.appendChild(p)}
const r=document.createElement('input');r.type='radio';r.name='q';r.checked=true;f.appendChild(r);for(let i=0;i<110;i++)f.appendChild(document.createTextNode(''));
let caught=false;try{host.appendChild(f)}catch(e){caught=e instanceof RangeError}check(caught && f.firstChild===r && r.checked && f.childNodes.length===111 && host.childNodes.length===4,'radio-preflight');
document.getElementById('proof').textContent='Preflight PASS';)JS"),1024*1024);
        assert(settle(p).find("Preflight PASS")!=std::string::npos);
    }
    std::cout<<"DOM batch PASS: fragment constructors, queries, live source arrays, order/identity, rollback, cycles, radios, selects and scripts\n";
}
