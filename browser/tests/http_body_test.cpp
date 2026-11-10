#include "../browser_http_body.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

static std::string source;
static std::size_t position=0,fragment=1;
static unsigned completed_in_turn=0,rounds=0;
static std::vector<std::uint64_t> wakeups;
namespace pxa {
std::uint64_t Transport::next_token() noexcept {return ++token_;}
Result<void> Transport::send(std::uint16_t service,std::uint16_t opcode,
                             std::uint64_t token,std::span<const std::byte>) noexcept {
    if(service==1){assert(opcode==1);return {};}
    assert(service==4 && opcode==2);wakeups.push_back(token);return {};
}
Result<std::uint32_t> Transport::io(std::uint64_t handle,std::uint32_t op,
                                   std::span<std::byte> output) noexcept {
    assert(handle==1 && op==1);
    const auto n=std::min({source.size()-position,output.size(),fragment});
    std::memcpy(output.data(),source.data()+position,n);position+=n;
    return std::uint32_t(n); // Always ready: deliberately stress nested Tasks.
}
Result<void> Transport::close(std::uint64_t) noexcept {return {};}
}
static pxa::Task<std::string> consume(browser::HttpBodyReader& reader,std::size_t output_size) {
    std::array<std::byte,1536> buffer{};std::string text;
    for(;;) {
        auto n=co_await reader.read_some(std::span{buffer}.first(output_size));
        assert(++completed_in_turn<=4); // Must return to Host before depth grows.
        if(!n)co_return std::unexpected(n.error());
        if(!*n)co_return text;
        text.append(reinterpret_cast<const char*>(buffer.data()),*n);
    }
}
static std::string read(const char* directory,const char* name) {
    std::ifstream f(std::string(directory)+"/"+name,std::ios::binary);
    assert(f);return {std::istreambuf_iterator<char>(f),{}};
}
int main(int argc,char** argv) {
    assert(argc==2);const auto expected=read(argv[1],"page.txt");
    for(auto [name,encoding]:{std::pair{"page.txt",browser::ContentEncoding::identity},
                             {"page.gz",browser::ContentEncoding::gzip},
                             {"page.deflate",browser::ContentEncoding::deflate}}) {
        source=read(argv[1],name);
        for(auto chunk:{1u,197u,4096u})for(auto output:{1u,31u,1536u}) {
            position=0;fragment=chunk;completed_in_turn=rounds=0;wakeups.clear();
            pxa::Transport transport;pxa::RequestTable requests;
            pxa::HttpExchange body(transport,requests,1);
            browser::HttpBodyReader reader(body,encoding,expected.size(),pxa::ClockService(transport,requests));
            auto task=consume(reader,output);auto handle=task.release();assert(handle);handle.resume();
            std::size_t at=0;
            while(!handle.done()) {
                assert(at<wakeups.size());completed_in_turn=0;++rounds;
                std::array<std::byte,12> response{};
                assert(requests.dispatch({4,2,wakeups[at++],response}));
            }
            assert(handle.promise().result && *handle.promise().result);
            assert(**handle.promise().result==expected && rounds>0);
            assert(reader.wire_bytes()==source.size());
            assert(reader.decoded()==expected.size());
            if(encoding==browser::ContentEncoding::identity)assert(reader.peak_heap()==0);
            handle.destroy();
        }
    }
    std::cout<<"HTTP body PASS: ready identity/gzip/deflate, tiny fragments, bounded native continuation turns\n";
}
