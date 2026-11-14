#include "../browser_core.hpp"
#include <cassert>
#include <iostream>
using namespace browser;
static Document parse(std::string_view html,std::size_t chunk,bool scripted=false) {
    Document d;d.clear("https://example.test/start");Parser p(d,scripted);
    for(std::size_t at=0;at<html.size();at+=chunk)p.feed(html.substr(at,chunk));
    p.finish();return d;
}
int main() {
    static_assert(sizeof(Field)==104 && sizeof(Span)==24 && sizeof(Piece)==16);
    assert(encode("a\nb\rc\r\nd",true)=="a%0D%0Ab%0D%0Ac%0D%0Ad");
    assert(encode("a\nb",false)=="a%0Ab");
    for(std::size_t chunk:{1,2,7,4096}) {
        auto d=parse("<form method=post><label>留言<textarea name=m value=ignored style='white-space:normal'>\r\n"
            "第一行 &amp; <b>文字</b>\r\n\t第二行\r第三行</textarea></label><button name=go value=yes>发送</button></form>",chunk);
        assert(d.forms[0].supported && d.fields.size()==2 && d.fields[0].type==FieldType::textarea);
        assert(d.fields[0].label=="留言" && d.fields[0].value=="第一行 & <b>文字</b>\n\t第二行\n第三行");
        assert(d.text.find("第一行")==d.text.npos && d.text.find("第二行")==d.text.npos);
        assert(submit(d,0,1)->body=="m="+encode(d.fields[0].value,true)+"&go=yes");
        auto read=parse("<form method=post><textarea name=q readonly>\n只读\n内容</textarea>"
            "<textarea disabled name=ignored>不会提交</textarea></form>",chunk);
        assert(read.forms[0].supported && read.fields.size()==1 && read.fields[0].type==FieldType::readonly_textarea);
        assert(read.text.find("只读\n内容")!=read.text.npos && submit(read,0)->body=="q="+encode("只读\n内容",true));
        assert(std::ranges::none_of(read.spans,[](const Span &s){return s.field>=0;}));
        auto hidden=parse("<form method=post><input hidden name=a value=secret><div hidden>"
            "<textarea name=q>\n秘密\n内容</textarea><input type=checkbox name=c value=yes checked>"
            "<input type=checkbox name=unchecked><input disabled name=no value=bad></div>"
            "<textarea style='display:none' readonly name=r>more</textarea><input hidden type=submit name=go value=bad></form>",chunk);
        assert(hidden.forms[0].supported && hidden.fields.size()==5);
        assert(submit(hidden,0)->body=="a=secret&q="+encode("秘密\n内容",true)+"&c=yes&r=more");
        assert(hidden.text.empty());
        auto password=parse("<form><input hidden type=password name=p value=secret></form>",chunk);
        assert(!submit(password,0) && password.text.empty());
        auto exact=parse("<form method=post><textarea name=q>"+std::string(512,'x')+"</textarea></form>",chunk);
        assert(exact.forms[0].supported && exact.fields[0].value.size()==512 && submit(exact,0));
        auto large=parse("<form method=post><textarea name=q>"+std::string(511,'x')+"中tail</textarea>After</form>",chunk);
        assert(!large.forms[0].supported && large.fields[0].value.size()==511 && !submit(large,0));
        assert(large.text==std::string(511,'x')+"中tail\nAfter\n");
        auto private_large=parse("<form method=post><textarea hidden name=q>"+std::string(513,'x')+"</textarea></form>",chunk);
        assert(!private_large.forms[0].supported && private_large.text.empty());
        auto eof=parse("<form method=post><textarea name=q>\n未闭合 &amp; 完成",chunk);
        assert(eof.fields[0].value=="未闭合 & 完成" && eof.forms[0].supported);
        auto hard=parse("<form><textarea name=q wrap=hard cols=3>abcdef</textarea></form>",chunk);
        assert(!hard.forms[0].supported && hard.text.find("abcdef")!=hard.text.npos);
        auto script=parse("<textarea name=q data-pxa-field=42>\n\nNew</textarea>",chunk,true);
        assert(script.fields[0].value=="\nNew" && script.fields[0].script_node==42);
        const std::pair<std::string_view,std::string_view> newlines[] = {
            {"&#13;X","\nX"},{"&#10;X","X"},{"A\r&#10;B","A\n\nB"},
            {"A&#13;\rB","A\nB"},{"&#13;\r\nX","\nX"},{"\r&#10;X","\nX"},
            {"A&#13;word\nB","A\nword\nB"},
        };
        for(const auto &[source,expected]:newlines) {
            auto value=parse("<form><textarea name=q>"+std::string(source)+"</textarea></form>",chunk);
            assert(value.fields[0].value==expected);
        }
        auto space=parse("<form>A<textarea hidden name=q>secret\r</textarea> B</form>",chunk);
        assert(space.text=="A B\n");
        std::string fields="<form method=post>";
        for(std::size_t i=0;i<max_fields;++i)fields+="<textarea name=q></textarea>";
        auto full=parse(fields+"<button type=reset>Reset</button></form>",chunk);
        assert(full.fields.size()==max_fields && full.forms[0].supported);
        full=parse(fields+"<textarea name=extra></textarea></form>",chunk);
        assert(!full.forms[0].supported);
    }
    std::cout<<"textarea PASS: streaming values, UTF-8 bounds, CRLF submission, readonly/disabled/hidden controls and quotas\n";
}
