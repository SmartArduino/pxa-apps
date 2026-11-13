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
    }
    auto d=parse("<form method=post><input name=a><input name=b><input name=c><input name=d></form>",4096);
    for(int i=0;i<3;++i)d.fields[i].value=std::string(512,'x');
    d.fields[3].value=std::string(501,'x');
    assert(submit(d,0)->body.size()==2048);d.fields[3].value+='x';assert(!submit(d,0));
    assert(*form_query_url("https://example.test/p?old=1#part","")=="https://example.test/p?#part");
    assert(!form_query_url("https://example.test/",std::string(max_url,'x')));
    auto fallback=parse("<form method=unknown enctype=unknown action=/fallback><input name=q value=ok></form>",4096);
    assert(submit(fallback,0)->url=="https://example.test/fallback?q=ok");
    std::cout<<"forms PASS: streaming order, submitter values, encoding, GET fragments and exact budgets\n";
}
