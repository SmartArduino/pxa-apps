#include "../engine.hpp"
#include <cassert>
#include <string>

using pinyin_lite::CharacterEngine;
using namespace std::string_view_literals;
static std::string candidate(const CharacterEngine& engine, std::size_t index) {
    std::array<char,4> bytes{};
    auto n = engine.candidate(index, bytes);
    return {bytes.data(), n};
}
static void first(CharacterEngine& engine, std::string_view text, std::string_view expected,
                  std::string_view tail = {}) {
    assert(engine.search(text)); assert(candidate(engine,0) == expected);
    engine.choose(0); assert(engine.remaining() == tail);
    assert(engine.consumed() == text.size() - tail.size());
}
int main() {
    CharacterEngine engine;
    first(engine,"ni","你"); first(engine,"hao","好"); first(engine,"shi","是");
    first(engine,"nihao","你","hao"); first(engine,"ni'hao","你","hao");
    first(engine,"ni''hao","你","hao"); first(engine,"'ni'hao","你","hao");
    first(engine,"xian","先"); first(engine,"xi'an","系","an");
    first(engine,"zhongguo","中","guo"); first(engine,"lve","略"); first(engine,"nve","虐");
    first(engine,"lv","率"); first(engine,"nv","女"); first(engine,"ni'","你");
    assert(engine.search("zh")); assert(!candidate(engine,0).empty());
    engine.choose(0); assert(engine.remaining().empty());
    assert(!engine.search("")); assert(!engine.search("''''")); assert(!engine.search("NI"));
    assert(!engine.search("ni\0hao"sv));
    assert(!engine.search(std::string(65,'n')));
    assert(engine.search(std::string(64,'n'))); engine.choose(0); assert(engine.remaining().size()==63);
    engine.reset(); assert(candidate(engine,0).empty() && engine.remaining().empty());
    // Enumerate the entire table; no truncation, invalid UTF-8, duplicate per
    // reading, or unaddressable candidate at a page boundary is acceptable.
    std::size_t total=0;
    for (const auto& syllable : pinyin_lite::table::syllables) {
        assert(engine.search(syllable.spelling)==syllable.count);
        std::string previous;
        for (std::size_t i=0;i<syllable.count;++i) {
            auto text=candidate(engine,i); assert(text.size()==3);
            auto a=static_cast<unsigned char>(text[0]), b=static_cast<unsigned char>(text[1]),
                 c=static_cast<unsigned char>(text[2]);
            assert(a>=0xe4 && a<=0xe9 && (b&0xc0)==0x80 && (c&0xc0)==0x80);
            assert(text!=previous); previous=text;
            ++total;
        }
        assert(candidate(engine,syllable.count).empty());
        std::array<char,3> too_small{'X','Y','Z'};
        assert(!engine.candidate(0,too_small) && too_small[0]=='X');
        engine.choose(syllable.count-1); assert(engine.remaining().empty());
    }
    assert(total==std::size(pinyin_lite::table::characters));
    // Partial spellings span several syllables. Compare every visible and
    // far-page candidate against a linear reference, including exact-spelling
    // precedence, instead of merely checking that UTF-8 is well formed.
    for (const auto& row : pinyin_lite::table::syllables) {
        auto spelling = std::string_view(row.spelling);
        for (std::size_t length=1; length<=spelling.size(); ++length) {
            auto prefix=spelling.substr(0,length);
            auto count=engine.search(prefix);
            const auto* exact=std::find_if(std::begin(pinyin_lite::table::syllables),
                std::end(pinyin_lite::table::syllables),[prefix](const auto& s) { return prefix==s.spelling; });
            std::size_t index=0;
            for (const auto& syllable : pinyin_lite::table::syllables) {
                if (exact!=std::end(pinyin_lite::table::syllables) ? &syllable!=exact :
                    !std::string_view(syllable.spelling).starts_with(prefix)) continue;
                for (std::size_t i=0;i<syllable.count;++i) {
                    auto text=candidate(engine,index++); assert(text.size()==3);
                    auto cp=((static_cast<unsigned char>(text[0])&15)<<12) |
                            ((static_cast<unsigned char>(text[1])&63)<<6) |
                            (static_cast<unsigned char>(text[2])&63);
                    assert(cp==pinyin_lite::table::characters[syllable.offset+i]);
                }
            }
            assert(count==index && candidate(engine,count).empty());
        }
    }
    // Deterministic malformed/partial spelling and index stress under sanitizers.
    std::uint32_t random=42;
    for (unsigned trial=0;trial<10000;++trial) {
        auto next=[&]{ random=random*1664525+1013904223; return random; };
        std::string text;
        auto length=next()%68;
        for (unsigned i=0;i<length;++i) text+= "abcdefghijklmnopqrstuvwxzy'!?"[next()%29];
        auto n=engine.search(text);
        if (n) { assert(!candidate(engine,n-1).empty()); engine.choose(next()%n); }
        assert(candidate(engine,n).empty());
        assert(engine.remaining().size()<=64);
    }
}
