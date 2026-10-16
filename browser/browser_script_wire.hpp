#pragma once
#include "browser_script.hpp"
#include "browser_cookies.hpp"
#include <pxa/core.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

// App-private, versioned IPC. One request in flight; large values are streamed
// through the existing 1 KiB broker. No Guest pointers cross this boundary.
namespace browser::script::wire {
inline constexpr std::string_view endpoint="pxa.browser.script.v1";
inline constexpr std::size_t packet_bytes=1024, header_bytes=40, max_blob=65536;
enum class Op : std::uint8_t { open=1, response, field, click, limit, close, poll, clear };
enum class Output : std::uint8_t { none, cookie, request, navigation, markup };
using Key=std::array<std::byte,16>;
inline std::span<const std::byte> bytes(std::string_view s) { return std::as_bytes(std::span(s.data(),s.size())); }
inline std::string_view text(std::span<const std::byte> s) { return {reinterpret_cast<const char*>(s.data()),s.size()}; }

struct Encoder {
    std::string data;
    void u8(unsigned v) {data+=char(v);}
    void u32(std::uint32_t v) {for(unsigned i=0;i<4;++i)data+=char(v>>(i*8));}
    void u64(std::uint64_t v) {for(unsigned i=0;i<8;++i)data+=char(v>>(i*8));}
    void string(std::string_view v) {u32(std::uint32_t(v.size()));data.append(v);}
};
struct Reader {
    std::string_view data;
    bool valid=true;
    std::string_view take(std::size_t n) {
        if(n>data.size()){valid=false;return {};}
        auto value=data.substr(0,n);data.remove_prefix(n);return value;
    }
    unsigned u8() {auto v=take(1);return v.empty()?0:static_cast<unsigned char>(v[0]);}
    std::uint32_t u32() {auto v=take(4);return v.size()==4?pxa::wire::get32(bytes(v).data()):0;}
    std::uint64_t u64() {auto v=take(8);return v.size()==8?pxa::wire::get64(bytes(v).data()):0;}
    std::string_view string(std::size_t max) {auto n=u32();if(n>max){valid=false;return {};}return take(n);}
    bool done() const {return valid && data.empty();}
};
struct Header {
    Op op=Op::poll;
    bool execute=false;
    std::uint64_t generation=0;
    Key key{};
    std::uint32_t offset=0,total=0;
    void encode(std::span<std::byte> out) const {
        pxa::wire::put32(out.data(),0x31534a42);out[4]=std::byte{1};out[5]=std::byte(op);
        out[6]=std::byte(execute);out[7]=std::byte{0};pxa::wire::put64(out.data()+8,generation);
        std::copy(key.begin(),key.end(),out.begin()+16);
        pxa::wire::put32(out.data()+32,offset);pxa::wire::put32(out.data()+36,total);
    }
    static std::optional<Header> decode(std::span<const std::byte> in) {
        if(in.size()<header_bytes || in.size()>packet_bytes || pxa::wire::get32(in.data())!=0x31534a42 ||
           in[4]!=std::byte{1} || in[7]!=std::byte{0} || std::to_integer<unsigned>(in[6])>1) return {};
        Header h;h.op=Op(std::to_integer<unsigned>(in[5]));h.execute=in[6]==std::byte{1};
        if(h.op<Op::open || h.op>Op::clear) return {};
        h.generation=pxa::wire::get64(in.data()+8);std::copy_n(in.begin()+16,16,h.key.begin());
        h.offset=pxa::wire::get32(in.data()+32);h.total=pxa::wire::get32(in.data()+36);
        if(!h.generation || h.total>max_blob || h.offset>h.total || in.size()-header_bytes>h.total-h.offset) return {};
        if(h.execute && h.op!=Op::poll)return {};
        return h;
    }
};
inline void cookies(Encoder& e,const Cookies& jar) {
    e.u8(jar.snapshot().size());
    for(const auto& c:jar.snapshot()) {
        e.string(c.host);e.string(c.path);e.string(c.name);e.string(c.value);
        e.u64(c.expires);e.u8(unsigned(c.secure)|(unsigned(c.http_only)<<1));
    }
}
inline bool cookies(Reader& r,Cookies& jar) {
    auto n=r.u8();if(n>16)return false;
    std::vector<Cookies::Cookie> entries;
    for(unsigned i=0;i<n && r.valid;++i) {
        Cookies::Cookie c;c.host=r.string(4096);c.path=r.string(4096);c.name=r.string(4096);c.value=r.string(4096);
        c.expires=r.u64();auto flags=r.u8();if(flags>3)return false;
        c.secure=flags&1;c.http_only=flags&2;entries.push_back(std::move(c));
    }
    return r.valid && jar.restore(std::move(entries));
}
inline void limits(Encoder& e,const Limits& l) {
    for(auto n:{l.heap_bytes,l.stack_bytes,l.html_bytes,l.source_bytes,l.response_bytes,l.script_bytes,l.fragment_bytes})e.u32(std::uint32_t(n));
    e.u32(l.nodes);e.u32(l.scripts);e.u32(l.fragment_nodes);e.u64(l.turn_us);
}
inline bool limits(Reader& r,Limits& l) {
    l.heap_bytes=r.u32();l.stack_bytes=r.u32();l.html_bytes=r.u32();l.source_bytes=r.u32();
    l.response_bytes=r.u32();l.script_bytes=r.u32();l.fragment_bytes=r.u32();
    l.nodes=r.u32();l.scripts=r.u32();l.fragment_nodes=r.u32();l.turn_us=r.u64();
    // The signed UI owns policy, but all inputs remain independently bounded.
    return r.valid && l.heap_bytes>=128*1024 && l.heap_bytes<=4*1024*1024 &&
        l.stack_bytes>=4096 && l.stack_bytes<=256*1024 && l.html_bytes<=32768 && l.source_bytes<=65536 &&
        l.response_bytes<=65536 && l.script_bytes<=32768 && l.fragment_bytes<=32768 &&
        l.nodes<=192 && l.scripts<=12 && l.fragment_nodes<=192 && l.turn_us>=1000 && l.turn_us<=50000;
}
inline void stats(Encoder& e,const Stats& s) {
    e.u64(s.current_bytes);e.u64(s.peak_bytes);e.u32(s.turns);e.u32(s.errors);e.u32(s.timeouts);
    e.u64(s.max_turn_us);e.u64(s.initialization_us);e.u64(s.max_initialization_step_us);
    e.u64(s.max_markup_us);e.u32(s.markup_turns);
}
inline Stats stats(Reader& r) {
    Stats s;s.current_bytes=std::size_t(r.u64());s.peak_bytes=std::size_t(r.u64());s.turns=r.u32();s.errors=r.u32();s.timeouts=r.u32();
    s.max_turn_us=r.u64();s.initialization_us=r.u64();s.max_initialization_step_us=r.u64();
    s.max_markup_us=r.u64();s.markup_turns=r.u32();return s;
}
struct Summary {
    std::uint64_t generation=0,wake=UINT64_MAX;
    bool active=false,initialized=false,ready=false;
    Stats counters;
    std::array<std::uint32_t,4> pending{};
    std::string error;
    void encode(Encoder& e) const {
        e.u8(1);e.u64(generation);e.u8(unsigned(active)|(unsigned(initialized)<<1)|(unsigned(ready)<<2));
        e.u64(wake);stats(e,counters);for(auto id:pending)e.u32(id);e.string(error);
    }
    static std::optional<Summary> decode(Reader& r) {
        if(r.u8()!=1)return {};
        Summary s;s.generation=r.u64();auto f=r.u8();if(f>7)return {};
        s.active=f&1;s.initialized=f&2;s.ready=f&4;s.wake=r.u64();s.counters=stats(r);
        for(auto& id:s.pending)id=r.u32();s.error=r.string(256);
        return r.valid?std::optional(std::move(s)):std::nullopt;
    }
};

// The receiver owns at most one bounded upload. Offset mismatch, overflow and
// wrong operation fail before appending; commit only when all bytes arrive.
struct Upload {
    std::string data;
    Op op=Op::open;
    std::uint32_t total=0;
    bool append(const Header& h,std::span<const std::byte> part) {
        if(h.offset==0){data.clear();op=h.op;total=h.total;}
        if(op!=h.op || total!=h.total || h.offset!=data.size() || part.size()>total-data.size())return false;
        data.append(text(part));return true;
    }
    bool complete() const {return data.size()==total;}
    void clear(){std::string{}.swap(data);total=0;}
};
} // namespace browser::script::wire
