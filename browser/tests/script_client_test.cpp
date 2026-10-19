#include "../browser_script_client.hpp"
#include <cassert>
#include <iostream>

namespace w=browser::script::wire;
struct Call {std::uint64_t token;w::Header header;std::string part;};
static std::vector<Call> calls;
static unsigned cancelled=0;
namespace pxa {
std::uint64_t Transport::next_token() noexcept {return ++token_;}
Result<void> Transport::send(std::uint16_t service,std::uint16_t op,std::uint64_t token,
                             std::span<const std::byte> payload) noexcept {
    if(service==1){assert(op==1);++cancelled;return {};}
    assert(service==7 && op==1);
    wire::Records records(payload);auto name=records.take(1,w::endpoint.size());
    auto data=records.take(2);assert(name && data && records.empty());
    auto h=w::Header::decode(*data);assert(h);
    calls.push_back({token,*h,std::string(w::text(data->subspan(w::header_bytes)))});
    return {};
}
}
extern "C" unsigned short __wasi_random_get(unsigned char* p,std::size_t n) {
    for(std::size_t i=0;i<n;++i)p[i]=static_cast<unsigned char>(i+1);
    return 0;
}
static void finish(browser::script::Client& client,w::Summary summary,
                   w::Output kind=w::Output::none,std::string_view output={}) {
    const auto call=calls.back();
    std::array<std::byte,8> accepted{};pxa::wire::put32(accepted.data()+4,std::uint32_t(call.token));
    assert(client.event({7,1,call.token,accepted}));
    w::Encoder body;summary.encode(body);body.u8(unsigned(kind));body.u8(kind!=w::Output::none);
    body.u32(0);body.u32(output.size());body.data+=output;
    std::vector<std::byte> result(8+body.data.size());
    pxa::wire::put16(result.data()+4,3);pxa::wire::put16(result.data()+6,body.data.size());
    std::memcpy(result.data()+8,body.data.data(),body.data.size());
    assert(client.event({7,0x8002,call.token,result}));
}
static w::Summary state() {
    w::Summary s;s.generation=calls.back().header.generation;s.active=true;s.initialized=true;s.ready=true;
    return s;
}
int main() {
    pxa::Transport transport;browser::Cookies cookies;
    {
        browser::script::Client client(transport,cookies);
        assert(calls.empty());client.maintain();assert(calls.empty()); // static page is lazy
        assert(client.open(std::string(3000,'x'),"https://a.test/a",0,{},{}));
        client.step();assert(calls.back().header.op==w::Op::open);
        std::string upload;
        for(unsigned n=0;n<10;++n) {
            const auto call=calls.back();assert(call.header.offset==upload.size());upload+=call.part;
            finish(client,state());if(upload.size()==call.header.total)break;
        }
        assert(upload.ends_with(std::string(3000,'x')));
        const auto idle=calls.size();client.step();client.maintain();assert(calls.size()==idle);
        client.pause(true);client.click(3);client.step();assert(calls.back().header.op==w::Op::click);
        auto s=state();s.wake=0;finish(client,s);
        const auto paused=calls.size();client.step();assert(calls.size()==paused);
        client.pause(false);client.step();assert(calls.back().header.mode==w::PollMode::update);
        auto mode_state=state();mode_state.wake=0;finish(client,mode_state);
        client.step(false);assert(calls.back().header.mode==w::PollMode::callbacks);
        mode_state=state();mode_state.wake=0;finish(client,mode_state);
        client.step();assert(calls.back().header.mode==w::PollMode::update);
        // Replace a page before its old asynchronous reply arrives.
        const auto old=state();assert(client.open("new","https://a.test/b",0,{},{}));
        finish(client,old,w::Output::markup,"stale markup");
        assert(!client.take_markup() && calls.back().header.generation==old.generation+1);
        finish(client,state());assert(client.active());
        // Close with a poll in flight, then reopen with close still in flight.
        s=state();s.wake=0;client.click(4);client.step();finish(client,s);client.step();
        client.close();assert(!client.active());finish(client,state());
        assert(calls.back().header.op==w::Op::close);
        assert(client.open("third","https://a.test/c",0,{},{}));finish(client,state());
        assert(calls.back().header.op==w::Op::open);finish(client,state());assert(client.active());
        // Clearing storage must precede a queued replacement page.
        client.click(5);client.step();client.clear_storage();
        assert(client.open("fourth","https://a.test/d",0,{},{}));finish(client,state());
        assert(calls.back().header.op==w::Op::clear);
        s=state();s.active=false;finish(client,s);assert(calls.back().header.op==w::Op::open);
        finish(client,state());assert(client.active());
        client.close();s=state();s.active=false;finish(client,s);assert(!client.active());
    }
    {
        browser::script::Client client(transport,cookies);
        assert(client.open("pending","https://a.test/",0,{},{}));client.step();
    }
    assert(cancelled==1);
    std::cout<<"script client passed: lazy start, chunks, idle, pause, stale replies, close/reopen, clear, cancellation\n";
}
