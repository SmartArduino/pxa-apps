#include <pxa_control_capture.h>
#include "../reader_disk.hpp"
#include <cassert>
#include <deque>
#include <cstdio>
using namespace pxa;
struct Command {unsigned service,op;std::uint64_t token;std::vector<std::byte> body;};
static std::deque<Command> commands;
static unsigned ticks,finished;static Error last_error=Error::internal;
static std::array<std::byte,5000> source;
extern "C" std::int32_t pxa_test_control(const std::uint8_t* data,std::uint32_t size){
 auto p=reinterpret_cast<const std::byte*>(data);assert(size>=20);
 commands.push_back({wire::get16(p),wire::get16(p+2),wire::get64(p+4),{p+20,p+size}});return 0;
}
extern "C" std::int32_t pxa_io(std::uint64_t,std::uint32_t,std::uint8_t*,std::uint32_t){assert(false);return -1;}
struct App {
 manga::AsyncDisk disk;static inline App* current=nullptr;
 Result<void> on_start(Context&){current=this;return {};}
 Task<void> write(Context& c){auto r=co_await disk.write(c,"result.bin",source);++finished;if(!r)last_error=r.error();co_return Result<void>{};}
 Result<bool> on_event(Context& c,const Event& e){
  if(e.service!=99)return false;
  if(e.opcode==1){auto r=c.foreground_tasks().start(write(c));assert(r);}
  if(e.opcode==2)c.foreground_tasks().cancel();
  if(e.opcode==3)++ticks;
  return true;
 }
};
using Runtime=AppRuntime<App>;
static void event(unsigned service,unsigned op,std::uint64_t token=0,std::span<const std::byte> body={}){
 std::vector<std::byte> packet(20+body.size());wire::put16(packet.data(),service);wire::put16(packet.data()+2,op);wire::put64(packet.data()+4,token);wire::put32(packet.data()+12,body.size());std::copy(body.begin(),body.end(),packet.begin()+20);
 assert(Runtime::event(reinterpret_cast<const std::uint8_t*>(packet.data()),packet.size())>=0);
}
static Command take(unsigned service,unsigned op){
 for(auto i=commands.begin();i!=commands.end();++i)if(i->service==service&&i->op==op){auto c=std::move(*i);commands.erase(i);return c;}
 std::fprintf(stderr,"missing %u/%u; queued:",service,op);for(auto& c:commands)std::fprintf(stderr," %u/%u",c.service,c.op);std::fprintf(stderr,"\n");assert(false&&"missing expected request");return {};
}
static void reply(const Command& c,int status=0,std::uint64_t value=0){
 std::array<std::byte,12> body{};wire::put32(body.data(),std::uint32_t(status));unsigned n=4;
 if(!status&&((c.service==5&&c.op==20)||(c.service==4&&c.op==2))){n=12;wire::put64(body.data()+4,value);}
 if(!status&&c.service==5&&c.op==21){n=8;wire::put32(body.data()+4,value);}
 event(c.service,c.op,c.token,std::span{body}.first(n));
}
static void clean(){commands.clear();assert(App::current->disk.pending==0);}
int main(){
 source.fill(std::byte{42});assert(Runtime::start(nullptr,0)==0);
 // A cancelled begin may still complete successfully: the detached owner must
 // abort that late session rather than lose either a handle or its temporary.
 event(99,1);auto begin=take(5,20);for(unsigned i=0;i<20;++i)event(99,3);assert(ticks==20);
 event(99,2);reply(begin,0,UINT64_C(0x100000001));auto abort=take(5,23);assert(wire::get64(abort.body.data())==UINT64_C(0x100000001));reply(abort);assert(!finished);clean();
 // Cancellation must not release payload ownership while a worker writes it.
 event(99,1);reply(take(5,20),0,UINT64_C(0x200000001));auto chunk=take(5,21);assert(chunk.body.size()==2056);
 event(99,2);auto cancel=take(5,23);source.fill(std::byte{17});for(auto b:std::span{chunk.body}.subspan(8))assert(b==std::byte{42});
 reply(cancel,int(Error::would_block));auto clock=take(4,2);reply(chunk,0,2048);reply(clock);reply(take(5,23));assert(!finished);clean();
 // Whole-file publication uses bounded chunks and commits only after all bytes.
 event(99,1);auto opening=take(5,20);assert(wire::get64(opening.body.data()+8)==5000);reply(opening,0,UINT64_C(0x300000001));
 for(unsigned n:{2048u,2048u,904u}){auto c=take(5,21);assert(c.body.size()==n+8);reply(c,0,n);}
 assert(finished==0);reply(take(5,22));assert(finished==1);clean();
 // Invalid short acknowledgement is never published and still cleans up.
 event(99,1);reply(take(5,20),0,UINT64_C(0x400000001));reply(take(5,21),0,0);assert(finished==2&&last_error==Error::protocol_error);reply(take(5,23));clean();
 event(99,1);take(5,20);Runtime::stop(0);assert(task_pool_stats().active_slots==0);
 puts("Async disk: progress during delayed I/O, late begin cancellation, copied chunks, abort retry, atomic commit, malformed acknowledgement and teardown PASS");
}
