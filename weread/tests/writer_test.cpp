#include "../state.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using namespace weread;
template<JsonKey Key> concept AcceptedKey=requires(JsonObjectWriter& writer){writer.template key<Key>();};
static_assert(AcceptedKey<"valid_name">&&!AcceptedKey<"bad\"name">&&!AcceptedKey<"bad\nname">);
int main(){
    std::string output;
    JsonObjectWriter object(output);object.number<"maximum">(std::numeric_limits<std::uint32_t>::max());
    object.boolean<"enabled">(true);object.key<"nested">();
    JsonObjectWriter nested(output);nested.text<"text">("\"\\\n\r\t\x01 中文 𠳐");nested.close();object.close();
    assert(Json(output).valid()&&Json(output).get("maximum").number()==4294967295u);
    assert(Json(output).get("nested").get("text").string()=="\"\\\n\r\t\x01 中文 𠳐");
    assert(Json(output).get("enabled").raw()=="true");
    std::uint32_t random=17;
    for(unsigned test=0;test<512;++test){
        std::string text;
        for(unsigned i=0;i<test%200;++i){random=random*1664525+1013904223;text+=char(random%128);}
        std::string encoded;json_quote_to(encoded,text);assert(encoded==quote(text));
        assert(Json(encoded).valid()&&Json(encoded).string()==text);
    }
    for(unsigned n=0;n<=32;++n){
        SavedState state;state.account="synthetic";
        for(unsigned i=0;i<n;++i)state.remember({number(i),"参数 \\\" test",i,2,i*13,1});
        const auto encoded=state.encode();assert(Json(encoded).valid());
        SavedState copy;assert(copy.decode(encoded)&&copy.encode()==encoded);
    }
    std::cout<<"Compile-time keys, append escaping/limits, 512 quoted values and 33 state round trips passed\n";
}
