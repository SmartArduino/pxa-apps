#include "pxa_control_capture.h"
#include <pxa/ime.hpp>
#include "../engine.hpp"
#include "../../pinyin-ime/session.hpp"
#include <cassert>
#include <cstring>

static std::array<std::byte,4096> packet;
static std::size_t packet_size;
extern "C" std::int32_t pxa_test_control(const std::uint8_t* bytes,std::uint32_t size) {
    assert(size<=packet.size()); std::memcpy(packet.data(),bytes,size); packet_size=size; return 0;
}
extern "C" std::int32_t pxa_io(std::uint64_t,std::uint32_t,std::uint8_t*,std::uint32_t) { return -3; }

static pxa::ime::Edit edit() { return static_cast<pxa::ime::Edit>(pxa::wire::get16(packet.data()+20+14)); }
static std::string_view preedit() {
    return {reinterpret_cast<const char*>(packet.data()+20+32),pxa::wire::get16(packet.data()+20+20)};
}
static std::string_view commit() {
    return {reinterpret_cast<const char*>(packet.data()+20+32)+preedit().size(),pxa::wire::get16(packet.data()+20+22)};
}
static std::string_view candidate(unsigned index=0) {
    const auto* p=packet.data()+20+32+preedit().size()+commit().size();
    assert(index<pxa::wire::get16(packet.data()+20+24));
    while (index--) p+=2+pxa::wire::get16(p);
    return {reinterpret_cast<const char*>(p+2),pxa::wire::get16(p)};
}
int main() {
    PinyinSession<pinyin_lite::CharacterEngine> session;
    pxa::Transport transport; transport.phase(pxa::Phase::event);
    pxa::ime::Input input{}; input.session=42;
    auto send=[&](pxa::ime::Action action,std::string_view text={},std::uint32_t value=0) {
        input.action=action; input.text=text; input.value=value; ++input.sequence;
        assert(session.process(input,transport)); assert(packet_size>=52);
        assert(pxa::wire::get64(packet.data()+20)==input.session);
        assert(pxa::wire::get32(packet.data()+20+8)==input.sequence);
    };
    using enum pxa::ime::Action;
    send(reset); assert(preedit().empty());
    send(text,"nihao"); assert(preedit()=="nihao" && candidate()=="你");
    send(choose); assert(commit()=="你" && preedit()=="hao" && candidate()=="好");
    send(confirm); assert(commit()=="好" && preedit().empty());
    send(backspace); assert(edit()==pxa::ime::Edit::backspace);
    send(text,"ni"); send(text,"!",1); assert(commit()=="你!" && preedit().empty());
    send(text,"ni'hao"); send(text,"，",1); assert(commit()=="你好，");
    send(text,"zzzz"); send(confirm); assert(commit()=="zzzz" && preedit().empty());
    send(text,"shi"); send(page,{},12);
    assert(pxa::wire::get32(packet.data()+20+16)==12 && !candidate().empty());
    send(choose,{},100000); assert(edit()==pxa::ime::Edit::none && preedit()=="shi");
    send(backspace); assert(preedit()=="sh");
    send(reset); send(text,std::string(64,'n')); send(text,"i"); assert(preedit().size()==64);
    ++input.session; send(text,"hao"); assert(preedit()=="hao" && candidate()=="好");
    input.layout=pxa::ime::Layout::nine; send(reset);
    send(text,"6"); send(text,"4");
    for (std::size_t i=0;i<ime_t9_count();++i) if (std::string_view(ime_t9_pinyin(i))=="ni") { send(choose_pinyin,{},i); break; }
    assert(preedit()=="ni" && candidate()=="你");
    send(text,"'"); send(text,"4"); send(text,"2"); send(text,"6");
    for (std::size_t i=0;i<ime_t9_count();++i) if (std::string_view(ime_t9_pinyin(i))=="ni'hao") { send(choose_pinyin,{},i); break; }
    assert(preedit()=="ni'hao"); send(choose); assert(commit()=="你" && preedit()=="hao");
    send(choose); assert(commit()=="好" && !ime_t9_len());
    input.language=pxa::ime::Language::english; send(text,"hi"); assert(commit()=="hi");
    input.sensitive=true; input.language=pxa::ime::Language::chinese;
    send(text,"password"); assert(commit()=="password" && preedit().empty());
    send(confirm,{},1); assert(edit()==pxa::ime::Edit::submit);
}
