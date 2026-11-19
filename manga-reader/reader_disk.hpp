#pragma once
#include <pxa/app.hpp>
#include <vector>
#include <memory>
#include <algorithm>

namespace manga {
// FS 0.6 keeps slow storage outside Guest callbacks. Requests live in the
// application scope: cancelling a reader task cannot abandon a newly opened
// session or leave a worker using its payload. Only one 2 KiB chunk is copied.
class AsyncDisk {
    struct Call {
        unsigned op;std::vector<std::byte> payload;
        pxa::Result<std::uint64_t> result=std::unexpected(pxa::Error::bad_state);
        std::coroutine_handle<> waiter{};bool done=false,abandoned=false;
    };
    struct Wait {
        std::shared_ptr<Call> call;
        ~Wait(){call->waiter={};call->abandoned=true;}
        bool await_ready()const{return call->done;}
        void await_suspend(std::coroutine_handle<> h){call->waiter=h;}
        auto await_resume(){return std::move(call->result);}
    };
    static pxa::Task<void> abort_request(pxa::Context& ctx,std::uint64_t id){
        std::array<std::byte,8> body{};pxa::wire::put64(body.data(),id);
        for(;;){auto event=co_await ctx.request(5,23,body);
            if(event){auto r=pxa::wire::result_body(event->payload);if(r||r.error()==pxa::Error::not_found)break;
                if(r.error()!=pxa::Error::would_block&&r.error()!=pxa::Error::busy)break;}
            else if(event.error()!=pxa::Error::would_block&&event.error()!=pxa::Error::busy)break;
            if(!co_await ctx.clock().yield())break;
        }co_return pxa::Result<void>{};
    }
    pxa::Task<void> run(pxa::Context& ctx,std::shared_ptr<Call> call){
        struct End{AsyncDisk& disk;bool active=true;~End(){if(active)--disk.pending;}} end{*this};
        auto event=co_await ctx.request(5,call->op,call->payload);
        if(!event)call->result=std::unexpected(event.error());
        else {auto body=pxa::wire::result_body(event->payload);
            if(!body)call->result=std::unexpected(body.error());
            else if(call->op==20&&body->size()==8){auto id=pxa::wire::get64(body->data());
                call->result=(id>>32)&&std::uint32_t(id)?pxa::Result<std::uint64_t>{id}:std::unexpected(pxa::Error::protocol_error);}
            else if(call->op==21&&body->size()==4)call->result=pxa::wire::get32(body->data());
            else if(call->op>=22&&body->empty())call->result=0;
            else call->result=std::unexpected(pxa::Error::protocol_error);
        }
        if(call->op==20&&call->abandoned&&call->result){auto r=co_await abort_request(ctx,*call->result);(void)r;}
        --pending;end.active=false;call->done=true;
        if(auto h=std::exchange(call->waiter,{}))h.resume();
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> cleanup(pxa::Context& ctx,std::uint64_t id){
        struct End{AsyncDisk& disk;~End(){--disk.pending;}} end{*this};
        co_return co_await abort_request(ctx,id);
    }
public:
    unsigned pending=0;
    pxa::Task<void> idle(pxa::Context& ctx){while(pending){auto r=co_await ctx.clock().yield();if(!r)co_return std::unexpected(r.error());}co_return pxa::Result<void>{};}
    pxa::Task<std::uint64_t> request(pxa::Context& ctx,unsigned op,std::span<const std::byte> body){
        auto call=std::make_shared<Call>();call->op=op;call->payload.assign(body.begin(),body.end());++pending;
        auto started=ctx.tasks().start(run(ctx,call));if(!started){--pending;co_return std::unexpected(started.error());}
        co_return co_await Wait{call};
    }
    void abort(pxa::Context& ctx,std::uint64_t id){
        if(!id||ctx.transport().phase()==pxa::Phase::stopped)return;
        ++pending;auto r=ctx.tasks().start(cleanup(ctx,id));if(!r)--pending;
    }
    struct Session {
        AsyncDisk* disk=nullptr;pxa::Context* ctx=nullptr;std::uint64_t id=0;
        Session()=default;Session(AsyncDisk& d,pxa::Context& c,std::uint64_t i):disk(&d),ctx(&c),id(i){}
        Session(const Session&)=delete;Session(Session&& s)noexcept:disk(s.disk),ctx(s.ctx),id(std::exchange(s.id,0)){}
        ~Session(){if(id)disk->abort(*ctx,id);}
        pxa::Task<void> write(std::span<const std::byte> bytes){
            std::vector<std::byte> body(2056);pxa::wire::put64(body.data(),id);
            while(!bytes.empty()){auto n=std::min<std::size_t>(2048,bytes.size());std::copy_n(bytes.begin(),n,body.begin()+8);
                auto r=co_await disk->request(*ctx,21,std::span{body}.first(n+8));if(!r)co_return std::unexpected(r.error());
                if(*r!=n)co_return std::unexpected(pxa::Error::protocol_error);bytes=bytes.subspan(n);
            }co_return pxa::Result<void>{};
        }
        pxa::Task<void> commit(){std::array<std::byte,8> body{};pxa::wire::put64(body.data(),id);
            auto r=co_await disk->request(*ctx,22,body);if(!r)co_return std::unexpected(r.error());id=0;co_return pxa::Result<void>{};}
    };
    pxa::Task<Session> begin(pxa::Context& ctx,std::string_view path,std::uint64_t bytes){
        auto ready=co_await idle(ctx);if(!ready)co_return std::unexpected(ready.error());
        std::string temporary=std::string(path)+".pending";
        if(path.empty()||temporary.size()>255||bytes>16*1024*1024)co_return std::unexpected(pxa::Error::invalid_argument);
        std::vector<std::byte> body(16+temporary.size()+path.size());pxa::wire::put16(body.data(),temporary.size());pxa::wire::put16(body.data()+2,path.size());pxa::wire::put64(body.data()+8,bytes);
        std::memcpy(body.data()+16,temporary.data(),temporary.size());std::memcpy(body.data()+16+temporary.size(),path.data(),path.size());
        auto r=co_await request(ctx,20,body);if(!r)co_return std::unexpected(r.error());co_return Session{*this,ctx,*r};
    }
    pxa::Task<void> write(pxa::Context& ctx,std::string_view path,std::span<const std::byte> bytes){
        auto f=co_await begin(ctx,path,bytes.size());if(!f)co_return std::unexpected(f.error());
        auto r=co_await f->write(bytes);if(!r)co_return std::unexpected(r.error());co_return co_await f->commit();
    }
};
}
