#include "../browser_script_wire.hpp"
#include <cassert>
#include <iostream>
namespace w=browser::script::wire;
int main() {
    std::array<std::byte,1024> packet{};
    w::Header h;h.op=w::Op::open;h.generation=9;h.total=2000;h.key[0]=std::byte{99};h.encode(packet);
    for(std::size_t n=0;n<w::header_bytes;++n)assert(!w::Header::decode(std::span(packet).first(n)));
    auto decoded=w::Header::decode(packet);assert(decoded && decoded->key==h.key && decoded->generation==9);
    auto corrupt=packet;corrupt[7]=std::byte{1};assert(!w::Header::decode(corrupt));
    corrupt=packet;corrupt[6]=std::byte{1};assert(!w::Header::decode(corrupt));
    for(auto mode:{w::PollMode::drain,w::PollMode::update,w::PollMode::callbacks}) {
        auto poll=h;poll.op=w::Op::poll;poll.mode=mode;poll.total=0;
        poll.encode(std::span(packet).first(w::header_bytes));
        auto roundtrip=w::Header::decode(std::span(packet).first(w::header_bytes));
        assert(roundtrip && roundtrip->mode==mode);
    }
    packet[6]=std::byte{3};assert(!w::Header::decode(std::span(packet).first(w::header_bytes)));
    h.total=100;h.encode(packet);assert(!w::Header::decode(packet)); // declared length cannot hold chunk
    h.total=2000;h.encode(packet);
    w::Upload upload;assert(upload.append(h,std::span(packet).subspan(w::header_bytes)));
    const auto first=upload.data.size();h.offset=std::uint32_t(first+1);
    assert(!upload.append(h,std::span(packet).first(2)) && upload.data.size()==first);
    h.offset=std::uint32_t(first);h.op=w::Op::response;assert(!upload.append(h,std::span(packet).first(2)));
    h.op=w::Op::open;assert(upload.append(h,std::span(packet).first(2000-first>1024?1024:2000-first)));
    h.offset=std::uint32_t(upload.data.size());assert(upload.append(h,std::span(packet).first(2000-upload.data.size())));
    assert(upload.complete());upload.clear();assert(upload.data.empty());

    browser::Cookies jar;
    jar.receive("https://a.test/a/b","session=secret; Path=/; Secure; HttpOnly",10,1700000000);
    jar.receive("https://a.test/a/b","visible=中文; Path=/a",10,1700000000);
    w::Encoder e;w::cookies(e,jar);w::Reader r{e.data};browser::Cookies clone;
    assert(w::cookies(r,clone) && r.done() && clone.size()==2);
    assert(clone.request("https://a.test/a/b",10,true)=="visible=中文");
    clone.receive("https://a.test/a/b","session=overwrite; Path=/",10,1700000000,true);
    assert(clone.request("https://a.test/a/b",10).find("session=secret")!=std::string::npos);
    assert(clone.request("https://b.test/a/b",10).empty());
    auto entries=std::vector(jar.snapshot().begin(),jar.snapshot().end());entries.push_back(entries[0]);
    assert(!clone.restore(std::move(entries)) && clone.size()==2);
    for(std::size_t n=0;n<e.data.size();++n) {
        w::Reader partial{std::string_view(e.data).substr(0,n)};browser::Cookies other;
        assert(!w::cookies(partial,other));
    }

    browser::script::Limits limits; e.data.clear();w::limits(e,limits);
    w::Reader lr{e.data};browser::script::Limits restored;assert(w::limits(lr,restored) && lr.done());
    assert(restored.heap_bytes==limits.heap_bytes && restored.turn_us==limits.turn_us);
    limits.heap_bytes=8*1024*1024;e.data.clear();w::limits(e,limits);lr={e.data};assert(!w::limits(lr,restored));
    w::Summary summary;summary.generation=9;summary.active=true;summary.ready=true;summary.pending[2]=17;
    summary.error="脚本内存达到上限";summary.counters.max_turn_us=0x1234567890ull;
    e.data.clear();summary.encode(e);r={e.data};auto result=w::Summary::decode(r);
    assert(result && r.done() && result->ready && result->pending[2]==17 && result->error==summary.error &&
           result->counters.max_turn_us==summary.counters.max_turn_us);
    for(std::size_t n=0;n<e.data.size();++n){w::Reader partial{std::string_view(e.data).substr(0,n)};assert(!w::Summary::decode(partial));}
    std::cout<<"script wire passed: framing, chunk order, limits, Cookie isolation/HttpOnly, truncated inputs, statistics\n";
}
