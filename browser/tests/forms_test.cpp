#include "../browser_core.hpp"
#include <cassert>
#include <iostream>
using namespace browser;
static Document parse(std::string_view html,std::size_t chunk) {
    Document d;d.clear("https://example.test/start");Parser p(d);
    for(std::size_t at=0;at<html.size();at+=chunk)p.feed(html.substr(at,chunk));
    p.finish();return d;
}
int main() {
    assert(encode("A B+*!~'()中",true)=="A+B%2B*%21%7E%27%28%29%E4%B8%AD");
    assert(encode("*~",false)=="%2A~"); // ordinary URL encoding is unchanged
    for(std::size_t chunk:{1,3,64,4096}) {
        auto d=parse("<form action='/echo?stale=1#result' method=GET><div><input name=q value='A B*~'></div>"
                     "<input name=q value=last><input type=SUBMIT name=go value=send>"
                     "<button name=other value=ignored>Other</button></form>",chunk);
        auto sent=submit(d,0,2);assert(sent && !sent->post);
        assert(sent->url=="https://example.test/echo?q=A+B*%7E&q=last&go=send#result");
        auto invalid=parse("<form method=POST><button type=IMAGE name=go value=yes>Send</button></form>",chunk);
        assert(invalid.forms[0].supported && submit(invalid,0,0)->body=="go=yes");
        auto caption=parse("<style>.hide{display:none}</style><form action=/send method=post>"
            "<input type=hidden name=q value=ok><button name=go value=token style='white-space:pre'>"
            " \n 发送 <b>中文 &amp; English</b><span class=hide>不显示</span><br>"
            "<img alt='完成'><a href=/wrong>!</a>\n </button><p>After</p></form>",chunk);
        assert(caption.fields.size()==2 && caption.fields[1].label=="发送 中文 & English 完成!");
        assert(caption.fields[1].value=="token" && caption.links.empty() && caption.images.empty());
        assert(submit(caption,0,1)->body=="q=ok&go=token");
        assert(caption.text=="发送 中文 & English 完成!\nAfter\n");
        Layout layout;layout.build(caption,80,15);
        std::size_t controls=0;std::uint32_t after=0;
        for(const auto &piece:layout.pieces) {
            if(caption.spans[piece.span].field==1) {
                ++controls;assert(piece.x==0 && piece.width==80 && piece.size>0 && piece.size<caption.fields[1].label.size());
            } else if(piece.size && caption.text.substr(piece.offset,piece.size).starts_with("After")) after=piece.y;
        }
        assert(controls==1 && after>=30);
        auto inactive=parse("<form method=post><button disabled onclick=\"location.href='/wrong'\" name=no value=bad>禁用</button>"
            "<input disabled onclick=\"location.href='/wrong'\" type=submit>"
            "<button type=button>普通</button><button type=reset>重置</button>"
            "<button name=go>发送</button></form>",chunk);
        assert(inactive.fields.size()==1 && inactive.fields[0].label=="发送" && inactive.links.empty());
        assert(submit(inactive,0,0)->body=="go=");
        auto ending=parse("<form><button name=first value=1><b>First<button name=second value=2>Second</form>After",chunk);
        assert(ending.fields.size()==2 && ending.fields[0].label=="First" && ending.fields[1].label=="Second");
        assert(ending.text=="First\nSecond\nAfter\n");
        auto eof=parse("<form method=post><button name=go value=ok>未闭合 &amp; 完成",chunk);
        assert(eof.fields[0].label=="未闭合 & 完成" && submit(eof,0,0)->body=="go=ok");
        const std::string long_caption=std::string(126,'x')+"中tail";
        auto bounded=parse("<form><button name=go value=ok>"+long_caption+"</button>After</form>",chunk);
        assert(bounded.fields[0].label==std::string(126,'x') && !bounded.truncated);
        assert(bounded.text==std::string(126,'x')+"\nAfter\n");
        layout.build(bounded,1,15);assert(layout.pieces[0].size==0 && layout.pieces[0].width==1);
        auto fallback_button=parse("<form><button aria-label=Accessible value=payload> \n </button></form>",chunk);
        assert(fallback_button.fields[0].label=="Accessible");
        auto full=parse("<form><button name=go>"+std::string(128,'x')+std::string(5000,'z')+
            "&amp;中</button><button name=next value=2>Next</button></form>",chunk);
        assert(full.fields[0].label==std::string(128,'x') && full.fields[1].label=="Next" && !full.truncated);
        auto input=parse("<form><input name=q placeholder='"+std::string(128,'x')+"'></form>",chunk);
        layout.build(input,80,15);assert(layout.pieces.size()==1 && layout.pieces[0].width==80);
    }
    auto d=parse("<form method=post><input name=a><input name=b><input name=c><input name=d></form>",4096);
    for(int i=0;i<3;++i)d.fields[i].value=std::string(512,'x');
    d.fields[3].value=std::string(501,'x');
    assert(submit(d,0)->body.size()==2048);d.fields[3].value+='x';assert(!submit(d,0));
    assert(*form_query_url("https://example.test/p?old=1#part","")=="https://example.test/p?#part");
    assert(!form_query_url("https://example.test/",std::string(max_url,'x')));
    auto fallback=parse("<form method=unknown enctype=unknown action=/fallback><input name=q value=ok></form>",4096);
    assert(submit(fallback,0)->url=="https://example.test/fallback?q=ok");
    std::cout<<"forms PASS: streaming captions, single control hits, submitter values, disabled controls and exact budgets\n";
}
