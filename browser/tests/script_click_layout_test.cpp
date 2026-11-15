#include "../browser_script.hpp"
#include "../browser_core.hpp"
#include <cassert>
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
    assert(false && "click layout did not settle");return {};
}
static browser::Document parse(std::string_view html,bool scripted=true,std::size_t chunk=0) {
    browser::Document d;d.clear("https://example.test/");browser::Parser p(d,scripted);
    if(!chunk)chunk=html.size();for(std::size_t i=0;i<html.size();i+=chunk)p.feed(html.substr(i,chunk));p.finish();return d;
}
static const browser::Span& span(const browser::Document& d,std::string_view word) {
    const auto at=d.text.find(word);assert(at!=std::string::npos);
    for(const auto& s:d.spans)if(s.offset<=at && at<s.offset+s.size)return s;
    assert(false && "word has no span");return d.spans[0];
}
static int target(const browser::Document& d,std::string_view word) {return span(d,word).link;}
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});
    {
        auto p=open(R"HTML(<style>p.chapter{color:#187a42}h2{color:#702080}button.pick{color:#993240}</style>
<p class=chapter onclick=noop()>Alpha</p><p class=chapter onclick=noop()>Beta</p>
<h2 onclick=noop()>Heading</h2><ul><li onclick=noop()>Entry</li></ul>
<button class=pick onclick=noop()>Pick</button><p>After</p><script>function noop(){}</script>)HTML");
        const auto html=settle(p);assert(html.find("<p data-pxa-click=")!=html.npos && html.find("<h2 data-pxa-click=")!=html.npos);
        const auto d=parse(html);assert(d.links.size()==5 && d.fields.empty());
        assert(d.text.find("Alpha\nBeta\nHeading")!=d.text.npos && d.text.find("• Entry")!=d.text.npos);
        assert(span(d,"Alpha").text_style.color==0x187a42ff && span(d,"Heading").style==browser::Style::heading);
        assert(span(d,"Heading").text_style.scale==125 && span(d,"Pick").text_style.color==0x993240ff);
        assert(target(d,"Alpha")==0 && target(d,"Beta")==1 && target(d,"Heading")==2 && target(d,"Entry")==3 && target(d,"Pick")==4 && target(d,"After")==-1);
    }
    {
        auto p=open(R"HTML(<div onclick="proof.textContent+='O'">Outer <span onclick="proof.textContent+='I'">Inner <b>Bold</b></span> Tail <a href=/next>Next</a> End</div><p>Outside</p><p id=proof></p><script>const proof=document.getElementById('proof');</script>)HTML");
        const auto d=parse(settle(p));assert(d.links.size()==3);
        assert(target(d,"Outer")==0 && target(d,"Inner")==1 && target(d,"Bold")==1 && target(d,"Tail")==0 && target(d,"Next")==2 && target(d,"End")==0 && target(d,"Outside")==-1);
        assert(p.click(d.links[1].script_node));assert(settle(p).find("IO")!=std::string::npos && !p.take_navigation());
        assert(p.click(d.links[2].script_node));settle(p);const auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/next");
    }
    {
        auto p=open(R"HTML(<div onclick=noop()>Before<div>Inside<div>Deep</div>Middle</div>Tail<img src=/pic.png width=24 height=24 onclick="proof.textContent='Image fired'">End<button disabled><span onclick=noop()>Disabled</span></button>Rest</div><p>Outside</p><p id=proof></p><script>const proof=document.getElementById('proof');function noop(){}</script>)HTML");
        const auto d=parse(settle(p));assert(d.images.size()==1 && d.links.size()==2);
        for(auto text:{"Before","Inside","Deep","Middle","Tail","End","Rest"})assert(target(d,text)==0);
        assert(target(d,"Disabled")==-1 && target(d,"Outside")==-1);
        const auto image=std::find_if(d.spans.begin(),d.spans.end(),[](const auto& s){return s.image==0;});assert(image!=d.spans.end() && image->link==1);
        assert(p.click(d.links[1].script_node));assert(settle(p).find("Image fired")!=std::string::npos);
    }
    {
        auto p=open(R"HTML(<style>p.chapter{font-size:20px}</style><p id=t>Priority</p><script>
const t=document.getElementById('t');t.removeAttribute('id');
for(let i=0;i<21;i++)t.setAttribute('data-extra-'+i,'x');t.id='t';t.className='chapter';t.style.color='#993240';t.onclick=()=>{};
</script>)HTML");
        const auto html=settle(p);const auto d=parse(html);
        // CSS px use the existing 15px reading base: ceil(20*100/15)=134.
        if(span(d,"Priority").text_style.color!=0x993240ff || span(d,"Priority").text_style.scale!=134)
            std::cerr<<"Priority color="<<span(d,"Priority").text_style.color<<" scale="<<span(d,"Priority").text_style.scale<<" html="<<html<<'\n';
        assert(span(d,"Priority").text_style.color==0x993240ff && span(d,"Priority").text_style.scale==134);
        assert(d.anchors.size()==1 && d.anchors[0].name=="t");
    }
    {
        std::string html;
        for(unsigned i=1;i<=browser::max_links;i++)html+="<div data-pxa-click="+std::to_string(i)+">";
        html+="Innermost";for(unsigned i=0;i<browser::max_links;i++)html+="</div>";html+="<p>Outside</p>";
        for(unsigned chunk:{1,7,31,127,4096}) {
            const auto d=parse(html,true,chunk);assert(d.links.size()==browser::max_links);
            assert(target(d,"Innermost")==int(browser::max_links)-1 && target(d,"Outside")==-1);
        }
        const auto plain=parse(html,false);assert(plain.links.empty() && target(plain,"Innermost")==-1);
        const auto leaf=parse("<p data-pxa-click=1>A<img src=/p.png width=24 height=24 data-pxa-click=2>B<br data-pxa-click=3>C</p>Outside");
        assert(target(leaf,"A")==0 && target(leaf,"B")==0 && target(leaf,"C")==0 && target(leaf,"Outside")==-1);
    }
    std::cout<<"click layout PASS: tags, paragraph/heading/list CSS, nested scope restoration, button/image dispatch, disabled contents, attribute priority, streaming and 96 deep targets\n";
}
