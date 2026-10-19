#include "browser_script_wire.hpp"
#include "browser_script_library.hpp"
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <ctime>
#include <memory>
#include <type_traits>

namespace {
namespace js=browser::script;
namespace wire=browser::script::wire;
struct ScriptService {
    js::Page page;
    js::Storage storage;
    browser::Cookies cookies;
    wire::Key key{};
    std::uint64_t generation=0;
    bool bound=false,opening=false;
    wire::Upload incoming;
    std::string url,html,problem;
    js::Bootstrap bootstrap;
    js::Metrics metrics;
    js::Limits limits;
    unsigned module=0;
    std::uint32_t loading_call=0,library_id=0;
    enum class LibraryState : unsigned char { idle, hashing, loading, miss, ready };
    LibraryState library_state=LibraryState::idle;
    std::string library_bytecode;
    std::array<std::uint32_t,4> known{};
    std::vector<std::string> cookie_changes;
    std::size_t cookie_bytes=0;
    wire::Output outgoing_kind=wire::Output::none;
    std::string outgoing;
    std::size_t outgoing_offset=0;
    std::optional<std::string> markup;
    // Hashing and asset I/O never overlap. Reuse the same storage, with explicit
    // union lifetimes, instead of retaining another digest beside the I/O buffer.
    union AssetScratch {
        std::array<std::byte,1024> bytes;
        js::SourceDigest digest;
        AssetScratch():bytes{}{}
    } asset_scratch;
    static_assert(std::is_trivially_destructible_v<js::SourceDigest>);
    std::array<std::byte,1044> packet{};
    wire::Encoder encoded;
    pxa::Context* context=nullptr;

    static std::uint64_t now() {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    pxa::Result<void> on_start(pxa::Context& ctx) {
        context=&ctx;encoded.data.reserve(1024);
        return ctx.log().write(pxa::LogLevel::info,"BROWSER SCRIPT SERVICE START wasm");
    }
    void reset() {
        page={};cookies.clear();incoming.clear();bootstrap={};std::string{}.swap(html);problem.clear();
        library_id=0;library_state=LibraryState::idle;std::string{}.swap(library_bytecode);
        known={};cookie_changes.clear();cookie_bytes=0;markup.reset();
        std::string{}.swap(outgoing);outgoing_kind=wire::Output::none;outgoing_offset=0;opening=false;
    }
    wire::Summary summary() const {
        wire::Summary s;s.generation=generation;s.active=problem.empty() && (opening || page.active());
        s.initialized=page.initialized();s.ready=page.ready();s.counters=page.stats();
        s.wake=opening?0:page.next_wake_us();s.error=problem.empty()?std::string(page.error()):problem;
        for(unsigned i=0;i<known.size();++i)if(page.pending(known[i]))s.pending[i]=known[i];
        return s;
    }
    void select_output() {
        if(outgoing_kind!=wire::Output::none)return;
        outgoing_offset=0;
        if(!cookie_changes.empty()) {
            outgoing_kind=wire::Output::cookie;outgoing=std::move(cookie_changes.front());
            cookie_bytes-=outgoing.size();cookie_changes.erase(cookie_changes.begin());
        } else if(auto n=page.take_navigation()) {
            wire::Encoder e;e.u8(n->post);e.string(n->url);e.string(n->body);
            outgoing_kind=wire::Output::navigation;outgoing=std::move(e.data);
        } else if(auto q=page.take_request()) {
            auto slot=std::ranges::find_if(known,[&](auto id){return !page.pending(id);});
            if(slot==known.end()){problem="脚本请求达到上限";return;}
            *slot=q->id;
            wire::Encoder e;e.u32(q->id);e.u8(unsigned(q->script)|(unsigned(q->post)<<1));e.u32(q->timeout_ms);
            e.string(q->url);e.string(q->content_type);e.string(q->body);
            outgoing_kind=wire::Output::request;outgoing=std::move(e.data);
        } else if(markup) {
            outgoing_kind=wire::Output::markup;outgoing=std::move(*markup);markup.reset();
        }
    }
    void reply(std::uint32_t call,pxa::Error error=pxa::Error{},bool outputs=false) {
        encoded.data.clear();
        if(error==pxa::Error{}) {
            if(outputs)select_output();
            summary().encode(encoded);
            const auto kind=outputs?outgoing_kind:wire::Output::none;
            encoded.u8(std::uint8_t(kind));encoded.u8(kind!=wire::Output::none);
            encoded.u32(kind==wire::Output::none?0:std::uint32_t(outgoing_offset));
            encoded.u32(kind==wire::Output::none?0:std::uint32_t(outgoing.size()));
            if(kind!=wire::Output::none) {
                const auto n=std::min(wire::packet_bytes-encoded.data.size(),outgoing.size()-outgoing_offset);
                encoded.data.append(outgoing,outgoing_offset,n);outgoing_offset+=n;
                if(outgoing_offset==outgoing.size()){std::string{}.swap(outgoing);outgoing_kind=wire::Output::none;outgoing_offset=0;}
            }
        }
        auto result=pxa::encode_ipc_reply(call,std::int32_t(error),wire::bytes(encoded.data),packet);
        if(result)(void)context->transport().send(7,2,context->transport().next_token(),std::span(packet).first(*result));
    }
    void cancel_loading() {
        if(!loading_call && library_state!=LibraryState::loading)return;
        context->tasks().cancel();auto call=std::exchange(loading_call,0u);
        if(call)reply(call,pxa::Error::cancelled);
    }
    pxa::Task<void> load_bytecode(bool initial,std::string path) {
        bool good=true;
        auto& buffer=*std::construct_at(&asset_scratch.bytes);
        auto& output=initial?bootstrap.runtime:library_bytecode;
        for(std::size_t at=0;;) {
            auto part=co_await context->assets().read(path,at,buffer);
            if(!part || part->total_bytes>65536u || !part->bytes){good=false;break;}
            if(!at)output.reserve(part->total_bytes);
            output.append(reinterpret_cast<const char*>(buffer.data()),part->bytes);
            at+=part->bytes;if(at==part->total_bytes)break;
        }
        good=good && output.starts_with("PXAQJS1\n");
        if(initial) {
            if(!good)problem="脚本运行环境加载失败";
            else if(!page.open(std::move(html),url,std::move(bootstrap),hooks(),std::move(metrics),limits))problem=page.error();
            opening=false;reply(std::exchange(loading_call,0u));
        } else if(!good) {
            std::string{}.swap(output);library_state=LibraryState::miss;
        } else library_state=LibraryState::ready;
        co_return pxa::Result<void>{};
    }
    js::Hooks hooks() {
        js::Hooks h;h.owner=this;
        h.compiled_script=[](void* p,std::uint32_t id,std::string_view source){
            auto& self=*static_cast<ScriptService*>(p);
            using State=js::CompiledScript::State;
            if(self.library_id!=id) {
                if(self.library_state==LibraryState::loading)self.context->tasks().cancel();
                std::string{}.swap(self.library_bytecode);self.library_id=0;
                self.library_state=LibraryState::idle;
                if(!js::library_candidate(source.size()))return js::CompiledScript{};
                self.library_id=id;self.library_state=LibraryState::hashing;std::construct_at(&self.asset_scratch.digest);
#ifdef PXA_BROWSER_TRACE
                (void)self.context->log().write(pxa::LogLevel::info,"BROWSER JS LIBRARY HASH BEGIN");
#endif
            }
            if(self.library_state==LibraryState::hashing) {
                // A whole 25 KiB SHA pass is expensive under interpretation.
                // Advance on foreground idle polls, preserving the source slot.
                const auto begin=now(),end=begin+8000;
                do {
                    if(self.asset_scratch.digest.advance(source)) {
                        auto path=js::matching_library_path(source.size(),self.asset_scratch.digest);
                        if(path.empty())self.library_state=LibraryState::miss;
                        else {
                            self.library_state=LibraryState::loading;
#ifdef PXA_BROWSER_TRACE
                            (void)self.context->log().write(pxa::LogLevel::info,"BROWSER JS LIBRARY LOAD");
#endif
                            if(!self.context->tasks().start(self.load_bytecode(false,std::move(path))))
                                self.library_state=LibraryState::miss;
                        }
                        break;
                    }
                } while(now()<end);
#ifdef PXA_BROWSER_TRACE
                (void)self.context->log().write(pxa::LogLevel::info,"BROWSER JS LIBRARY HASH STEP us="+std::to_string(now()-begin));
#endif
                if(self.library_state==LibraryState::hashing)return js::CompiledScript{State::loading,{}};
            }
            if(self.library_state==LibraryState::loading)return js::CompiledScript{State::loading,{}};
            const bool found=self.library_state==LibraryState::ready;
            self.library_state=LibraryState::idle;self.library_id=0;
            return js::CompiledScript{found?State::bytecode:State::source,std::move(self.library_bytecode)};
        };
#ifdef PXA_BROWSER_TRACE
        h.trace=[](void* p,std::string_view phase){
            auto* self=static_cast<ScriptService*>(p);
            (void)self->context->log().write(pxa::LogLevel::info,"BROWSER JS STAGE "+std::string(phase)+" at_us="+std::to_string(now()));
        };
#endif
        h.cookie_get=[](void* p,std::string_view u){return static_cast<ScriptService*>(p)->cookies.request(u,now()/1000000,true);};
        h.cookie_set=[](void* p,std::string_view u,std::string_view value){
            auto& s=*static_cast<ScriptService*>(p);
            if(s.cookie_changes.size()>=32 || value.size()>16384-s.cookie_bytes){s.problem="脚本 Cookie 更新达到上限";return;}
            s.cookies.receive(u,value,now()/1000000,std::time(nullptr),true);
            s.cookie_changes.emplace_back(value);s.cookie_bytes+=value.size();
        };
        h.storage_get=[](void* p,std::string_view u,std::string_view k,bool session){return static_cast<ScriptService*>(p)->storage.get(u,k,session);};
        h.storage_set=[](void* p,std::string_view u,std::string_view k,const std::string* v,bool session,bool clear){return static_cast<ScriptService*>(p)->storage.set(u,k,v,session,clear);};
        return h;
    }
    bool commit(wire::Op op) {
        wire::Reader r{incoming.data};
        switch(op) {
        case wire::Op::open: {
            url=r.string(1024);metrics.user_agent=r.string(1024);metrics.width=int(r.u32());metrics.height=int(r.u32());
            metrics.density=double(r.u32())/65536;module=r.u8();
            if(!r.valid || module>unsigned(js::Module::page) || metrics.width<=0 || metrics.height<=0 || metrics.width>8192 || metrics.height>8192 ||
               metrics.density<0.25 || metrics.density>8 || !wire::limits(r,limits) || !wire::cookies(r,cookies) ||
               !js::same_origin(url,url) || r.data.size()>limits.html_bytes)return false;
            const auto offset=incoming.data.size()-r.data.size();incoming.data.erase(0,offset);html=std::move(incoming.data);
            return true;
        }
        case wire::Op::response: {
            js::Response q;q.id=r.u32();q.status=r.u32();auto failed=r.u8();q.failed=failed;
            if(failed>1 || !wire::cookies(r,cookies) || !r.valid || r.data.size()>std::max(limits.script_bytes,limits.response_bytes))return false;
            const auto offset=incoming.data.size()-r.data.size();incoming.data.erase(0,offset);q.body=std::move(incoming.data);
            page.complete(std::move(q));return true;
        }
        case wire::Op::field: {
            auto id=r.u32();auto checked=r.u8();if(!r.valid || checked>1 || r.data.size()>4096)return false;
            page.change_field(id,std::string(r.data),checked);return true;
        }
        case wire::Op::click: {auto id=r.u32();return r.done() && page.click(id);}
        case wire::Op::limit: {
            auto size=r.u32();if(!r.done())return false;
            if(!page.set_heap_limit(size))problem="设备内存紧张 · 已停止网页脚本";
            return true;
        }
        default:return false;
        }
    }
    pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& event) {
        if(event.service==7 && event.opcode==2)return true; // reply acknowledgement
        if(!event.is<pxa::IpcRequest>())return false;
        auto request=pxa::decode_ipc_request(event);
        if(!request)return true;
        auto h=wire::Header::decode(request->payload);
        if(request->endpoint!=wire::endpoint || !h){reply(request->call_id,pxa::Error::protocol_error);return true;}
        const bool new_owner=!bound || h->key!=key;
        if(new_owner) {
            if(h->op!=wire::Op::open && bound){reply(request->call_id,pxa::Error::denied);return true;}
            cancel_loading();reset();storage.clear();key=h->key;generation=0;bound=true;
        }
        if(h->op==wire::Op::open && h->offset==0 && h->generation>=generation) {
            cancel_loading();reset();generation=h->generation;opening=true;
        }
        if(h->generation!=generation && h->op!=wire::Op::clear && h->op!=wire::Op::close) {
            reply(request->call_id,pxa::Error::bad_state);return true;
        }
        if(h->op==wire::Op::clear || h->op==wire::Op::close) {
            if(h->total || request->payload.size()!=wire::header_bytes){reply(request->call_id,pxa::Error::protocol_error);return true;}
            cancel_loading();reset();generation=h->generation;
            if(h->op==wire::Op::clear)storage.clear();reply(request->call_id);return true;
        }
        if(h->op==wire::Op::poll) {
            if(h->total || request->payload.size()!=wire::header_bytes){reply(request->call_id,pxa::Error::protocol_error);return true;}
            if(h->mode!=wire::PollMode::drain && !opening && problem.empty() && outgoing_kind==wire::Output::none && cookie_changes.empty() && !markup) {
                page.step();if(h->mode==wire::PollMode::update && page.ready())markup=page.take_markup();
            }
            reply(request->call_id,pxa::Error{},true);return true;
        }
        if(!incoming.append(*h,request->payload.subspan(wire::header_bytes))) {reply(request->call_id,pxa::Error::protocol_error);return true;}
        if(incoming.complete()) {
            if(!commit(h->op)){incoming.clear();reply(request->call_id,pxa::Error::protocol_error);return true;}
            incoming.clear();
            if(h->op==wire::Op::open) {
                loading_call=request->call_id;
                if(!context->tasks().start(load_bytecode(true,"assets/browser-runtime.qjb"))){opening=false;problem="脚本资源不足";reply(std::exchange(loading_call,0u));}
                return true;
            }
        }
        reply(request->call_id);return true;
    }
};
}
PXA_SERVICE(ScriptService)
