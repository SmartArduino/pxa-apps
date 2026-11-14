#include "../browser_script.hpp"
#include "../browser_core.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static unsigned cases=0;
static Page open(std::string html) {
    ++cases;Page p;Limits l;l.heap_bytes=512*1024;l.turn_us=200000;
    assert(p.open("<html><body>"+html+"</body></html>","https://example.test/start",bootstrap,{},{},l));return p;
}
static std::string settle(Page &p) {
    std::string html;
    for(unsigned turn=0;turn<4096 && p.active();++turn) {
        p.step();if(auto out=p.take_markup())html=std::move(*out);
        if(p.ready() && p.next_wake_us()!=0) {
            if(p.stats().errors)std::cerr<<"Textarea case "<<cases<<": "<<p.error()<<'\n';
            assert(!p.stats().errors);return html;
        }
    }
    assert(false && "textarea page did not settle");return {};
}
static browser::Document document(std::string_view html) {
    browser::Document d;d.clear("https://example.test/start");browser::Parser p(d,true);p.feed(html);p.finish();return d;
}
int main(int argc,char **argv) {
    assert(argc==2);std::ifstream file(argv[1],std::ios::binary);
    bootstrap.runtime.assign(std::istreambuf_iterator<char>(file),{});assert(!bootstrap.runtime.empty());
    {
        auto p=open(R"HTML(<textarea id=t name=m value="ignored">
初始 &amp; &lt;b&gt;
第二行</textarea><p id=ok></p><script>
const t=document.getElementById('t');function check(v){if(!v)throw Error('textarea semantics')}
check(t.value==='初始 & <b>\n第二行' && t.defaultValue===t.value && t.type==='textarea');
t.setAttribute('value','unused');check(t.value==='初始 & <b>\n第二行');
t.defaultValue='\nA\r\nB';check(t.value==='\nA\nB' && t.defaultValue==='\nA\r\nB' && t.textLength===4);
check(t.outerHTML.includes('>\nA\r\nB</textarea>'));
t.value=t.value;t.defaultValue='changed';check(t.value==='\nA\nB' && t.defaultValue==='changed');
t.value='编辑\r\n第二\r第三';check(t.value==='编辑\n第二\n第三' && t.textContent==='changed');
check(t.outerHTML.includes('>changed</textarea>'));
t.readOnly=true;check(t.getAttribute('readonly')==='');t.readOnly=false;check(!t.hasAttribute('readonly'));
let caught=false;try{t.value='x'.repeat(4097)}catch(e){caught=e instanceof RangeError}
check(caught && t.value==='编辑\n第二\n第三');
document.getElementById('ok').textContent='Value PASS';</script>)HTML");
        auto html=settle(p);auto d=document(html);assert(html.find("Value PASS")!=html.npos);
        assert(d.fields.size()==1 && d.fields[0].value=="编辑\n第二\n第三" && d.fields[0].script_node);
    }
    {
        auto p=open(R"HTML(<form id=f method=post action=/echo><textarea id=t name=m>Default</textarea>
<textarea name=r readonly>Read-only
value</textarea><textarea name=no disabled>Skip</textarea>
<input type=hidden name=q value=ok></form><p id=ok></p><script>
const t=document.getElementById('t');t.oninput=function(){
if(this.value!=='编辑\n第二行' || this.defaultValue!=='Default')throw Error('input event');
this.value+='\nnormalized';};t.onchange=()=>document.getElementById('ok').textContent='Events PASS';
</script>)HTML");
        auto d=document(settle(p));assert(d.fields.size()==3);
        const auto id=d.fields[0].script_node;assert(id);p.change_field(id,"编辑\n第二行",false);
        const auto html=settle(p);d=document(html);
        assert(html.find("Events PASS")!=html.npos && d.fields[0].value=="编辑\n第二行\nnormalized");
    }
    {
        auto p=open(R"HTML(<form id=f method=post action=/echo><textarea name=m value=bad>
First
Second</textarea><textarea name=r readonly>Read
only</textarea><textarea name=no disabled>Skip</textarea>
<textarea hidden name=h>Hidden
value</textarea></form><script>document.getElementById('f').submit()</script>)HTML");
        settle(p);auto nav=p.take_navigation();assert(nav && nav->post && nav->body==
            "m=First%0D%0ASecond&r=Read%0D%0Aonly&h=Hidden%0D%0Avalue");
    }
    {
        auto p=open(R"HTML(<form id=f method=post><textarea id=t name=m wrap=hard cols=3>abcdef</textarea></form>
<script>const f=document.getElementById('f'),t=document.getElementById('t');let caught=false;
try{f.submit()}catch(e){caught=e instanceof TypeError}if(!caught)throw Error('hard wrap');
t.setAttribute('wrap','soft');t.value='!'.repeat(3000);caught=false;
try{f.submit()}catch(e){caught=e instanceof RangeError}if(!caught)throw Error('body budget');
t.value='recovered';f.submit();</script>)HTML");
        settle(p);const auto nav=p.take_navigation();assert(nav && nav->body=="m=recovered");
    }
    {
        auto p=open(R"HTML(<textarea id=t disabled onclick="invalid syntax )">Original</textarea><p id=ok></p>
<script>const t=document.getElementById('t');t.value='\nPreserved';t.disabled=false;
t.setAttribute('onclick',"document.getElementById('ok').textContent='Enabled PASS'");
document.getElementById('ok').textContent=t.value==='\nPreserved'?'Enable ready':'bad';</script>)HTML");
        auto d=document(settle(p));assert(d.fields[0].value=="\nPreserved" && d.fields[0].script_node);
    }
    std::cout<<"script textarea PASS: values/defaults, dirty state, projection, events, successful controls, CRLF and budgets\n";
}
