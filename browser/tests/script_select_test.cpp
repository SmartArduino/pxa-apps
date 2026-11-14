#include "../browser_script.hpp"
#include "../browser_core.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static unsigned number=0;
static Page open(std::string body) {
    ++number;Page p;Limits l;l.heap_bytes=512*1024;l.turn_us=200000;
    assert(p.open("<html><body>"+body+"</body></html>","https://example.test/start",bootstrap,{},{},l));return p;
}
static std::string settle(Page& p) {
    std::string result;
    for(unsigned turn=0;turn<4096 && p.active();++turn) {
        p.step();if(auto html=p.take_markup())result=std::move(*html);
        if(p.ready() && p.next_wake_us()!=0) {
            if(p.stats().errors) {std::cerr<<"Select case "<<number<<": "<<p.error()<<'\n';if(auto nav=p.take_navigation())std::cerr<<nav->url<<'\n';}
            assert(!p.stats().errors);return result;
        }
    }
    assert(false && "select did not settle");return {};
}
static const char* check="function check(v,name){if(!v){location.href='/failed/'+name;throw Error(name)}}";
static std::string script(std::string text){return "<script>"+std::string(check)+text+"</script>";}
static browser::Document document(std::string_view html) {
    browser::Document d;d.clear("https://example.test/start");browser::Parser p(d,true);p.feed(html);p.finish();return d;
}
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});
    {
        auto p=open("<select id=s name=chapter><optgroup disabled><option value=skip>Skip</optgroup><option> A &amp; B <option value=dup>Two<option value=dup>Three</select><p id=ok></p>"+script(R"JS(
const s=document.getElementById('s');check(s.type==='select-one' && s.length===4 && s.selectedIndex===1 && s.value==='A & B','defaults');
check(s.options.item(0)===s.options[0] && s.options.item(9)===null && s.options[0].form===null,'options');
s.selectedIndex=-1;check(s.value==='' && s.selectedOptions.length===0,'empty');
s.value='dup';check(s.selectedIndex===2 && s.selectedOptions.length===1 && s.options[2].selected,'first-duplicate');
s.selectedIndex=3;check(s.value==='dup' && s.options[3].index===3,'index');
s.value='missing';check(s.selectedIndex===-1,'missing');
s.options[1].selected=false;check(s.selectedIndex===1,'option-reset');
s.options[1].disabled=true;check(s.selectedIndex===1,'disabled-retains-selection');
s.selectedIndex=-1;s.size=2;check(s.size===2 && s.selectedIndex===-1,'size-list');
s.size=1;check(s.selectedIndex===2,'size-fallback');
document.getElementById('ok').textContent='Basics PASS';)JS"));
        assert(settle(p).find("Basics PASS")!=std::string::npos);
    }
    {
        auto p=open("<select id=s><option id=a value=a>A<optgroup id=g><option id=b value=b selected>B</optgroup></select><p id=ok></p>"+script(R"JS(
const s=document.getElementById('s'),a=document.getElementById('a'),b=document.getElementById('b'),g=document.getElementById('g');
check(b.selected && !a.selected,'read-initial-dirty');
a.defaultSelected=true;check(a.selected && !b.selected,'read-default-change');
const c=new Option('C','c',false,true);s.appendChild(c);check(c.selected && !a.selected && !b.selected,'read-append');
c.remove();check(a.selected && !b.selected,'read-remove');
s.size=2;s.selectedIndex=-1;check(!a.selected && !b.selected,'read-explicit-none');
g.disabled=true;a.disabled=true;s.size=1;check(!a.selected && !b.selected,'read-disabled-none');
g.disabled=false;check(b.selected && !a.selected,'read-enabled-fallback');
s.multiple=true;a.selected=true;check(a.selected && b.selected,'read-multiple');
s.multiple=false;check(a.selected && !b.selected,'read-single');
s.innerHTML='<option id=x value=x>X<option id=y selected value=y>Y';
const x=document.getElementById('x'),y=document.getElementById('y');check(y.selected && !x.selected,'read-html-replacement');
const f=new DocumentFragment(),z=new Option('Z','z',false,true);f.appendChild(z);s.insertBefore(f,s.firstChild);
check(z.selected && !y.selected && !x.selected,'read-fragment');
z.remove();check(x.selected && !y.selected,'read-removed-fragment-option');
s.textContent='';check(z.selected && !s.options.length,'detached-option-retains-state');
document.getElementById('ok').textContent='Selection reads PASS';)JS"));
        assert(settle(p).find("Selection reads PASS")!=std::string::npos);
    }
    {
        auto p=open("<select id=s><option value=old>Old</select><p id=ok></p>"+script(R"JS(
const s=document.getElementById('s'),o=new Option('New','new',true,false);
check(o instanceof Option && o.defaultSelected && !o.selected && o.value==='new' && o.index===0,'constructor');
s.add(o,0);check(s.value==='old' && s.options[0]===o && !o.selected,'insert-preserves');
o.selected=true;check(s.value==='new' && o.defaultSelected,'selected');
o.defaultSelected=false;check(o.selected && !o.defaultSelected,'dirty-default');
s.multiple=true;s.options[1].selected=true;check(s.type==='select-multiple' && s.selectedOptions.length===2,'multiple');
s.multiple=false;check(s.selectedIndex===0 && !s.options[1].selected,'remove-multiple');
s.remove();check(s.value==='old' && !o.parentNode && o.selected,'remove');
const added=new Option('Last','last',false,true);s.add(added);check(s.value==='last','append-selected');
s.insertBefore(added,s.firstChild);check(s.value==='last' && s.selectedIndex===0,'move');
added.innerHTML=' A <b>B</b> &amp; C ';added.removeAttribute('value');
check(added.text==='A B & C' && added.value==='A B & C' && added.outerHTML.includes('<b>B</b>'),'option-text');
document.getElementById('ok').textContent='Mutation PASS';)JS"));
        assert(settle(p).find("Mutation PASS")!=std::string::npos);
    }
    {
        auto p=open("<select id=s><option value=a>A<option value=b>B</select><p id=ok></p>"+script(R"JS(
const s=document.getElementById('s');s.selectedIndex=-1;s.appendChild(document.createTextNode('ignored'));
check(s.selectedIndex===-1,'text-preserves-none');
s.multiple=true;check(s.selectedIndex===-1,'multiple-preserves-none');
s.innerHTML='<optgroup disabled><option selected value=x>X</optgroup><option value=y>Y';
check(s.length===2 && s.value==='x' && s.options[0].selected,'fragment');
s.multiple=false;s.options[0].selected=false;check(s.value==='y','disabled-fallback');
const old=s.firstChild;let caught=false;try{s.innerHTML='<option>x</option>'.repeat(49)}catch(e){caught=e instanceof RangeError}
check(caught && s.firstChild===old && s.value==='y','fragment-rollback');
document.getElementById('ok').textContent='Fragment PASS';)JS"));
        assert(settle(p).find("Fragment PASS")!=std::string::npos);
    }
    {
        auto p=open("<select name=q oninput=\"document.getElementById('ok').textContent='Input '+this.selectedIndex\" onchange=\"document.getElementById('ok').textContent+=' Change '+this.value\"><option value=same>One<option value=skip disabled>Skip<option value=same>Three</select><p id=ok></p>");
        auto d=document(settle(p));assert(d.fields.size()==1 && d.fields[0].script_node);const auto id=d.fields[0].script_node;
        const auto token=std::to_string(d.select_options[2].script_node),disabled=std::to_string(d.select_options[1].script_node);
        assert(token!="0" && disabled!="0");
        p.change_field(id,token,false);auto html=settle(p);assert(html.find("Input 2 Change same")!=html.npos);
        d=document(html);assert(browser::selection_indices(d,d.fields[0])=="2");
        for(const auto& value:{token,disabled,token+","+token,std::string("4294967295"),std::string("-1"),std::string("0"),disabled+","+token})
            {p.change_field(id,value,false);assert(settle(p).empty());}
    }
    {
        auto p=open("<select id=s onchange=\"document.getElementById('ok').textContent='Choice '+this.selectedIndex+' '+this.value\"><option value=a>A<option value=b>B<option value=c>C</select>"
            "<button onclick=\"s.insertBefore(s.lastChild,s.firstChild)\">Move</button><button onclick=\"s.remove(0)\">Remove</button><p id=ok></p>"+
            script("const s=document.getElementById('s');"));
        const auto old=document(settle(p));const auto field=old.fields[0].script_node;
        const auto token=std::to_string(old.select_options[2].script_node);assert(field && token!="0" && old.links.size()==2);
        p.click(old.links[0].script_node);settle(p); // the displayed native document may still have the old order
        p.change_field(field,token,false);assert(settle(p).find("Choice 0 c")!=std::string::npos);
        p.click(old.links[1].script_node);settle(p);
        p.change_field(field,token,false);assert(settle(p).empty()); // a removed option cannot target its replacement
    }
    {
        std::string attrs;for(int i=0;i<22;++i)attrs+=" data-extra-"+std::to_string(i)+"=unused";
        auto p=open("<form><select"+attrs+" name=q multiple><option"+attrs+" value=kept selected>Caption</select></form>"+script("void 0;"));
        auto d=document(settle(p));assert(d.fields.size()==1 && d.fields[0].name=="q" && d.fields[0].multiple);
        assert(d.select_options.size()==1 && d.select_options[0].selected && d.select_options[0].script_node && d.select_options[0].value=="kept");
    }
    {
        auto p=open("<form id=f method=post action=/echo><input name=q value=first><select name=tag multiple><option selected>A &amp; B<option value=two selected>Two<option value=two selected>Duplicate<optgroup disabled><option selected value=no>No</optgroup></select><select name=skip disabled><option selected value=bad>Bad</select><select name=none size=2><option>None</select><button name=go value=send>Send</button></form>"+script("document.getElementById('f').submit();"));
        settle(p);const auto nav=p.take_navigation();assert(nav && nav->post && nav->body=="q=first&tag=A+%26+B&tag=two&tag=two");
    }
    std::cout<<"script select PASS: defaults, dirty selection, Option, mutation, fragments, native changes, duplicate values and exact form data\n";
}
