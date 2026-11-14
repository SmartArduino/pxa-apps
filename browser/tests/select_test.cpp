#include "../browser_core.hpp"
#include <cassert>
#include <iostream>
using namespace browser;
static Document parse(std::string_view html,std::size_t chunk=4096,bool scripted=false) {
    Document d;d.clear("https://example.test/start");Parser p(d,scripted);
    for(std::size_t at=0;at<html.size();at+=chunk)p.feed(html.substr(at,chunk));
    p.finish();return d;
}
int main() {
    for(std::size_t chunk:{1,2,7,4096}) {
        auto d=parse("<form action=/echo method=post><input name=q value=first>"
            "<select name=chapter aria-label=Chapter><optgroup disabled label=Skip><option value=no>Disabled</optgroup>"
            "<option value=one> First &amp; <b>Page</b> <option value=two selected>第二页</select>"
            "<select name=tag multiple><option selected>A &amp; B<option value=two selected>Two<option disabled selected value=no>No</select>"
            "<select name=hidden hidden><option value=secret>Hidden</select>"
            "<select name=skip disabled><option selected value=bad>Disabled</select>"
            "<button name=go value=send>Send</button></form><p>After</p>",chunk);
        assert(d.fields.size()==6 && d.select_options.size()==8 && d.forms[0].supported);
        auto& single=d.fields[1];assert(single.type==FieldType::select && single.value=="two" && single.label=="Chapter");
        auto choices=options(d,single);assert(choices.size()==3 && choices[0].disabled && choices[1].label=="First & Page" && choices[2].label=="第二页");
        assert(submit(d,0,5)->body=="q=first&chapter=two&tag=A+%26+B&tag=two&hidden=secret&go=send");
        assert(!choose_option(d,single,0) && !choose_option(d,single,2));
        assert(choose_option(d,single,1) && single.value=="one" && selection_indices(d,single)=="1");
        auto& multiple=d.fields[2];assert(multiple.multiple && selection_indices(d,multiple)=="0,1,2");
        assert(choose_option(d,multiple,0) && multiple.value=="two" && selection_indices(d,multiple)=="1,2");
        assert(!choose_option(d,multiple,2));
        assert(submit(d,0,5)->body=="q=first&chapter=one&tag=two&hidden=secret&go=send");
        assert(d.fields[4].disabled && !choose_option(d,d.fields[4],0));
        assert(d.text.find("After")!=d.text.npos && d.text.find("Hidden")==d.text.npos);
        auto fallback=parse("<select><option disabled>Skip<option> A\r\n B &amp; 中文 </select>",chunk);
        assert(fallback.fields.size()==1 && fallback.fields[0].value=="A B & 中文" && options(fallback,fallback.fields[0])[1].selected);
        auto last=parse("<select><option selected>A<option disabled selected>B<option>C</select>",chunk);
        assert(last.fields[0].value=="B" && !options(last,last.fields[0])[0].selected);
        auto none=parse("<select size=2><option>A<option>B</select><select multiple><option>C</select>",chunk);
        assert(none.fields.size()==2 && none.fields[0].value.empty() && none.fields[1].value.empty());
        auto projection=parse("<select data-pxa-field=9><option>A<option>B</select>",chunk,true);
        assert(projection.fields[0].script_node==9 && projection.fields[0].value.empty());
        auto traps=parse("<form method=post><select name=x><option value=ok selected>Text<img src=/trap><a href=/trap> Link</a><input name=bad value=bad></select></form>",chunk);
        assert(traps.images.empty() && traps.links.empty() && traps.fields.size()==1 && submit(traps,0)->body=="x=ok");
    }
    {
        std::string body="<form method=post><select name=x>";
        for(std::size_t i=0;i<max_select_options;++i)body+="<option value="+std::to_string(i)+">Choice";
        auto full=parse(body+"</select><button>Send</button></form>");
        assert(full.select_options.size()==max_select_options && full.forms[0].supported && !full.fields[0].limited);
        assert(choose_option(full,full.fields[0],max_select_options-1));
        assert(submit(full,0)->body=="x="+std::to_string(max_select_options-1));
        auto overflow=parse(body+"<option value=bad>Over</select><button>Send</button></form><p>Readable</p>");
        assert(overflow.select_options.size()==max_select_options && overflow.fields[0].limited && !submit(overflow,0));
        assert(!choose_option(overflow,overflow.fields[0],0) && overflow.text.find("Readable")!=overflow.text.npos);
        auto long_value=parse("<form method=post><select name=x><option value='"+std::string(513,'x')+"'>Bad</select></form>");
        assert(long_value.fields[0].limited && !submit(long_value,0));
        auto long_text=parse("<form method=post><select name=x><option>"+std::string(513,'x')+"</select></form>");
        assert(long_text.fields[0].limited && !submit(long_text,0));
        auto huge_tag=parse("<form method=post><select name=x><option value=ok>OK<option data-long='"+std::string(2100,'x')+"><img src=/trap>' value=bad>Bad</select></form><p>Readable</p>",1);
        assert(huge_tag.fields[0].limited && !submit(huge_tag,0) && huge_tag.text.find("Readable")!=huge_tag.text.npos);
        assert(huge_tag.images.empty());
        auto omitted=parse("<form method=post><input data-long='"+std::string(2100,'x')+"' name=x value=bad><button>Send</button></form>",1);
        assert(!omitted.forms[0].supported && !submit(omitted,0));
        full.clear("https://example.test/",true);assert(full.fields.empty() && full.select_options.empty() && !full.select_options.capacity());
    }
    {
        auto old=parse("<select><option value=a>A<option selected value=b>B</select>");
        auto reordered=parse("<select><option value=new>Inserted<option value=a>A<option value=b>B</select>");
        assert(restore_selection(reordered,reordered.fields[0],old.select_options));
        assert(selection_indices(reordered,reordered.fields[0])=="2" && reordered.fields[0].value=="b");
        auto removed=parse("<select><option value=a>A<option value=new>Replacement</select>");
        assert(!restore_selection(removed,removed.fields[0],old.select_options) && removed.fields[0].value=="a");
    }
    std::cout<<"select PASS: streaming, defaults, single/multiple, groups, hidden/disabled, exact submission, inert descendants and budgets\n";
}
