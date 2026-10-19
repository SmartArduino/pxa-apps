#include "browser_script.hpp"
#include "browser_core.hpp"
#include "browser_script_markup.hpp"
#include "vendor/quickjs/quickjs.h"
#include <array>
#ifdef PXA_SCRIPT_DIAGNOSTICS
#include <cstdio>
#endif
#include <chrono>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace browser::script {
namespace {
// Native tests can observe phases. Production Wasm includes diagnostics only
// when requested by the same build flag as the UI/service trace output.
#if defined(__wasi__) && !defined(PXA_BROWSER_TRACE)
constexpr bool trace_enabled=false;
#else
constexpr bool trace_enabled=true;
#endif
using namespace detail;
#ifdef PXA_SCRIPT_INTERPRETED
// Intrinsic registration and the signed bridge are much slower under WAMR.
// This allowance never applies to downloaded scripts or event handlers.
constexpr std::uint64_t bootstrap_turn_us=1000000;
constexpr unsigned dom_batch_nodes=2;
#else
constexpr std::uint64_t bootstrap_turn_us=200000;
constexpr unsigned dom_batch_nodes=8;
#endif
// The lexer and compiler phase boundaries poll this deadline. Keep a final
// check for work that completes without another poll; parsing is not resumable.
constexpr std::uint64_t compile_turn_us = 2000000;
struct Value {
    JSContext* ctx = nullptr;
    JSValue value = JS_UNDEFINED;
    Value() = default;
    Value(JSContext* c, JSValue v) : ctx(c), value(v) {}
    ~Value() { if (ctx) JS_FreeValue(ctx, value); }
    Value(Value&& other) noexcept : ctx(std::exchange(other.ctx, nullptr)), value(other.value) {}
    Value& operator=(Value&& other) noexcept {
        if (this != &other) { if (ctx) JS_FreeValue(ctx, value); ctx = std::exchange(other.ctx, nullptr); value = other.value; }
        return *this;
    }
    Value(const Value&) = delete;
    JSValue release() { ctx = nullptr; return value; }
};
struct CString {
    JSContext* ctx;
    std::size_t size = 0;
    const char* data;
    CString(JSContext* c, JSValueConst v) : ctx(c), data(JS_ToCStringLen(c, &size, v)) {}
    ~CString() { if (data) JS_FreeCString(ctx, data); }
    std::string_view view() const { return data ? std::string_view(data, size) : std::string_view{}; }
};
}

struct Page::Impl {
    struct Heap {
        struct alignas(std::max_align_t) Header { std::size_t bytes; };
        std::size_t used = 0, peak = 0, limit = 0;
        bool failed = false;
        static void* allocate(JSMallocState* state, std::size_t size) {
            auto& heap = *static_cast<Heap*>(state->opaque);
            if (!size || size > heap.limit || sizeof(Header) > heap.limit - size ||
                heap.used > heap.limit - size - sizeof(Header)) { heap.failed = true; return nullptr; }
            auto* h = static_cast<Header*>(std::malloc(sizeof(Header) + size));
            if (!h) { heap.failed = true; return nullptr; }
            h->bytes = size;
            heap.used += size + sizeof(Header); heap.peak = std::max(heap.peak, heap.used);
            state->malloc_size = heap.used; ++state->malloc_count;
            return h + 1;
        }
        static void release(JSMallocState* state, void* ptr) {
            if (!ptr) return;
            auto& heap = *static_cast<Heap*>(state->opaque);
            auto* h = static_cast<Header*>(ptr) - 1;
            heap.used -= h->bytes + sizeof(Header); state->malloc_size = heap.used;
            --state->malloc_count; std::free(h);
        }
        static void* resize(JSMallocState* state, void* ptr, std::size_t size) {
            if (!ptr) return allocate(state, size);
            if (!size) { release(state, ptr); return nullptr; }
            auto& heap = *static_cast<Heap*>(state->opaque);
            auto* old = static_cast<Header*>(ptr) - 1;
            const auto old_size = old->bytes;
            // Charge both allocations until the old storage is released.
            auto* next = allocate(state, size);
            if (!next) return nullptr;
            std::memcpy(next, ptr, std::min(size, old_size)); release(state, ptr);
            (void)heap;
            return next;
        }
        static std::size_t usable(const void* ptr) { return ptr ? (static_cast<const Header*>(ptr) - 1)->bytes : 0; }
    } heap;
    Limits limits;
    Hooks hooks;
    Metrics metrics;
    std::string html, url, problem, bootstrap, markup_buffer;
    JSRuntime* runtime = nullptr;
    JSContext* ctx = nullptr;
    // Initialization's factory and a compiled page script have disjoint
    // lifetimes. Reuse the same root; no extra persistent program/cache.
    Value bridge, bindings, prepared;
    unsigned initialization_stage = 0;
    std::size_t cursor = 0, source_bytes = 0;
    unsigned node_count = 0, script_count = 0;
    std::uint32_t next_id = 1, waiting_script = 0;
    std::array<std::uint32_t, 4> outstanding{};
    std::array<bool, 4> script_requests{};
    std::vector<Request> requests;
    std::vector<Response> responses;
    std::optional<Navigation> navigation;
    std::array<std::uint32_t, 4> clicks{};
    std::size_t click_count = 0;
    struct FieldChange { std::uint32_t id; std::string value; bool checked; };
    std::vector<FieldChange> fields;
    bool dom_dirty = true, snapshot_pending = false;
    std::uint64_t timer_at = UINT64_MAX;
    bool finished = false, stopped = false, paused = false, prepared_response = false;
    std::uint64_t deadline = 0;
    Stats counters;
    std::uint64_t markup_hash = 0;
    std::size_t markup_size = SIZE_MAX;
    ~Impl() {
        bridge = {};
        bindings = {};
        prepared = {};
        if (ctx) JS_FreeContext(ctx);
        if (runtime) JS_FreeRuntime(runtime);
    }
    std::uint64_t now() const {
        if (hooks.now_us) return hooks.now_us(hooks.owner);
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    static int interrupt(JSRuntime*, void* opaque) {
        auto& self = *static_cast<Impl*>(opaque);
        return self.heap.failed || (self.deadline && self.now() >= self.deadline);
    }
    bool check(JSValueConst result) {
        if (!JS_IsException(result)) return true;
        Value error(ctx, JS_GetException(ctx));
#ifdef PXA_SCRIPT_DIAGNOSTICS
        CString detail(ctx, error.value); std::fprintf(stderr, "JS ERROR: %.*s\n", int(detail.size), detail.data ? detail.data : "");
#endif
        ++counters.errors;
        if (heap.failed) { problem = "脚本内存达到上限"; stopped = true; }
        else if (deadline && now() >= deadline) {
            ++counters.timeouts; problem = "脚本执行超时"; stopped = true;
        }
        else problem = "部分网页脚本不受支持";
        return false;
    }
    Value string(std::string_view value) { return {ctx, JS_NewStringLen(ctx, value.data(), value.size())}; }
    Value call(const char* method, std::span<JSValue> args = {}) {
        Value function(ctx, JS_GetPropertyStr(ctx, bridge.value, method));
        if (!check(function.value)) return {ctx, JS_EXCEPTION};
        Value result(ctx, JS_Call(ctx, function.value, bridge.value, int(args.size()), args.data()));
        check(result.value); return result;
    }
    template<class... Values> Value invoke(const char* method, Values&&... values) {
        std::array<JSValue, sizeof...(values)> args{values...}; return call(method, args);
    }
    bool prepare_inline(std::uint32_t id, const char* type) {
        // Compile at the C++ entry, before dispatch adds JS call frames. Keep
        // Function's function-body grammar and cache on the existing node.
        auto event = string(type), parameter = string("event");
        Value constructor(ctx, JS_GetPropertyStr(ctx, bridge.value, "handlerConstructor"));
        if (!check(event.value) || !check(parameter.value) || !check(constructor.value)) return false;
        const std::string name = std::string("on") + type;
        auto node = invoke("handler", JS_NewUint32(ctx, id), event.value);
        if (JS_IsException(node.value)) return false;
        if (JS_IsNull(node.value)) return true;
        Value attrs(ctx, JS_GetPropertyStr(ctx, node.value, "_attrs"));
        if (!check(attrs.value)) return false;
        Value source(ctx, JS_GetPropertyStr(ctx, attrs.value, name.c_str()));
        if (!check(source.value)) return false;
        CString bytes(ctx, source.value);
        if (!bytes.data) { check(JS_EXCEPTION); return false; }
        if (bytes.size > limits.script_bytes || bytes.size > limits.source_bytes - std::min(source_bytes, limits.source_bytes)) {
            problem = "网页脚本超过容量限制"; ++counters.errors; return false;
        }
        source_bytes += bytes.size;
        JSValue args[] = {parameter.value, source.value};
        Value function(ctx, JS_CallConstructor(ctx, constructor.value, 2, args));
        if (!check(function.value)) return false;
        if (JS_SetPropertyStr(ctx, node.value, name.c_str(), function.release()) < 0) { check(JS_EXCEPTION); return false; }
        return true;
    }
    std::uint32_t queue(Request request) {
        auto slot = std::ranges::find(outstanding, 0u);
        if (slot == outstanding.end()) return 0;
        request.id = next_id++;
        if (!request.id) request.id = next_id++;
        *slot = request.id;
        script_requests[std::size_t(slot-outstanding.begin())]=request.script;
        const auto id = request.id;
        requests.push_back(std::move(request)); return id;
    }
    void retire(std::uint32_t id) {
        for (std::size_t i=0;i<outstanding.size();++i) if (outstanding[i] == id) {
            outstanding[i]=0;script_requests[i]=false;
        }
    }
    bool pending(std::uint32_t id) const { return id && std::ranges::find(outstanding, id) != outstanding.end(); }
    bool pending_script(std::uint32_t id) const {
        const auto slot=std::ranges::find(outstanding,id);
        return id && slot!=outstanding.end() && script_requests[std::size_t(slot-outstanding.begin())];
    }
    enum Operation { clock, cookie, storage, request, cancel, navigate, fragment, changed, wake };
    static JSValue native(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
        auto& self = *static_cast<Impl*>(JS_GetContextOpaque(ctx));
        auto text = [&](int at) { return CString(ctx, at < argc ? argv[at] : JS_UNDEFINED); };
        if (magic == clock) return JS_NewFloat64(ctx, double(self.now()) / 1000);
        if (magic == cookie) {
            if (argc) {
                auto value = text(0); if (!value.data) return JS_EXCEPTION;
                if (value.size > 1536) return JS_ThrowRangeError(ctx, "cookie limit");
                if (self.hooks.cookie_set) self.hooks.cookie_set(self.hooks.owner, self.url, value.view());
                return JS_UNDEFINED;
            }
            const auto value = self.hooks.cookie_get ? self.hooks.cookie_get(self.hooks.owner, self.url) : std::string{};
            return self.string(value).release();
        }
        if (magic == changed) { self.dom_dirty = true; return JS_UNDEFINED; }
        if (magic == wake) {
            double ms = -1;
            if (argc && JS_ToFloat64(ctx, &ms, argv[0])) return JS_EXCEPTION;
            self.timer_at = ms >= 0 && ms < 1e15 ? std::uint64_t(ms * 1000) : UINT64_MAX;
            return JS_UNDEFINED;
        }
        if (magic == storage) {
            int op = 0; if (argc) JS_ToInt32(ctx, &op, argv[0]);
            auto key = text(1); if (!key.data) return JS_EXCEPTION;
            if (key.size > 128) return JS_ThrowRangeError(ctx, "storage key limit");
            const bool session = argc > 3 && JS_ToBool(ctx, argv[3]);
            if (op) {
                auto value = text(2); if (!value.data) return JS_EXCEPTION;
                if (value.size > 4096) return JS_ThrowRangeError(ctx, "storage value limit");
                const std::string bytes(value.view());
                return JS_NewBool(ctx, self.hooks.storage_set && self.hooks.storage_set(self.hooks.owner, self.url,
                    op == 3 ? std::string_view{} : key.view(), op == 1 ? &bytes : nullptr, session, op == 3));
            }
            const auto value = self.hooks.storage_get ? self.hooks.storage_get(self.hooks.owner, self.url, key.view(), session) : std::nullopt;
            return value ? self.string(*value).release() : JS_NULL;
        }
        if (magic == cancel) {
            std::uint32_t id = 0; if (argc) JS_ToUint32(ctx, &id, argv[0]);
            self.retire(id); std::erase_if(self.requests, [=](auto& r) { return r.id == id; });
            return JS_UNDEFINED;
        }
        if (magic == request) {
            auto method = text(0), target = text(1), body = text(2), type = text(3);
            if (!method.data || !target.data || !body.data || !type.data) return JS_EXCEPTION;
            const bool script=method.view()=="SCRIPT";
            if ((method.view() != "GET" && method.view() != "POST" && !script) ||
                (script && (body.size || type.size)) || body.size > 8192 || type.size > 128 ||
                type.view().find_first_of("\r\n") != type.view().npos) return JS_ThrowTypeError(ctx, "request policy");
            auto address = browser::resolve(self.url, target.view());
            if (!address || !script_url_allowed(self.url, *address, script)) return JS_ThrowTypeError(ctx, "cross-origin request blocked");
            if (script && ++self.script_count>self.limits.scripts) return JS_ThrowRangeError(ctx,"script count limit");
            Request queued;
            queued.url = browser::transport_url(*address); queued.post = method.view() == "POST";queued.script=script;
            queued.body = body.view(); queued.content_type = type.view();
            double timeout = 0;
            if (argc > 4 && JS_ToFloat64(ctx, &timeout, argv[4])) return JS_EXCEPTION;
            if (!(timeout >= 0 && timeout <= 60000)) return JS_ThrowRangeError(ctx, "request timeout limit");
            if (timeout) queued.timeout_ms = std::uint32_t(std::max(1000.0, timeout));
            return JS_NewUint32(ctx, self.queue(std::move(queued)));
        }
        if (magic == navigate) {
            auto target = text(0); if (!target.data) return JS_EXCEPTION;
            auto address = browser::resolve(self.url, target.view(), true);
            if (!address || !script_url_allowed(self.url, *address, true)) return JS_ThrowTypeError(ctx, "navigation blocked");
            Navigation next; next.url = std::move(*address);
            if (argc >= 3) {
                auto method = text(1), body = text(2);
                if (!method.data || !body.data) return JS_EXCEPTION;
                if (body.size > 8192) return JS_ThrowRangeError(ctx, "form limit");
                next.post = method.view() == "post";
                if (next.post) next.body = body.view();
                else if (body.size) { next.url += next.url.find('?') == next.url.npos ? '?' : '&'; next.url += body.view(); }
            }
            self.navigation = std::move(next); return JS_UNDEFINED;
        }
        if (magic == fragment) {
            if (argc < 2) return JS_ThrowTypeError(ctx, "fragment arguments");
            auto html = text(1); if (!html.data) return JS_EXCEPTION;
            if (html.size > self.limits.fragment_bytes) return JS_ThrowRangeError(ctx, "fragment limit");
            auto begun = self.invoke("beginFragment", argv[0]);
            if (JS_IsException(begun.value)) return JS_ThrowRangeError(ctx,"fragment nesting failed");
            std::size_t at = 0; unsigned nodes = 0;
            while (at < html.size && !self.stopped) {
                if (++nodes > self.limits.fragment_nodes) { self.invoke("endFragment"); return JS_ThrowRangeError(ctx, "fragment node limit"); }
                self.build_one(html.view(), at, false);
            }
            self.invoke("endFragment"); return self.stopped ? JS_ThrowRangeError(ctx, "fragment failed") : JS_UNDEFINED;
        }
        return JS_UNDEFINED;
    }
    bool initialize_step() {
        // Only app-owned bootstrap runs here. Website code retains turn_us.
        // Native intrinsic registration is not interruptible; split the groups
        // so input/paint can run between them and report their real duration.
        deadline = now() + bootstrap_turn_us;
        switch (initialization_stage) {
        case 0: {
            heap.limit = limits.heap_bytes;
            static constexpr JSMallocFunctions allocator{Heap::allocate, Heap::release, Heap::resize, Heap::usable};
            runtime = JS_NewRuntime2(&allocator, &heap);
            if (!runtime) { problem = "脚本内存不足"; return false; }
            JS_SetMemoryLimit(runtime, limits.heap_bytes);
            JS_SetStripInfo(runtime, JS_STRIP_SOURCE);
            JS_SetMaxStackSize(runtime, limits.stack_bytes);
            // A larger ceiling does not postpone GC on small pages.
            JS_SetGCThreshold(runtime, std::min(limits.heap_bytes / 2, std::size_t(128*1024)));
            JS_SetInterruptHandler(runtime, interrupt, this);
            ctx = JS_NewContextRaw(runtime);
            if (!ctx) { problem = "脚本内存不足"; return false; }
            JS_SetContextOpaque(ctx, this);
            break;
        }
        case 1:
            if (!check(JS_AddIntrinsicBaseObjects(ctx) ? JS_EXCEPTION : JS_UNDEFINED)) return false;
            break;
        case 2:
            if (!check((JS_AddIntrinsicDate(ctx) || JS_AddIntrinsicEval(ctx) || JS_AddIntrinsicJSON(ctx)) ? JS_EXCEPTION : JS_UNDEFINED)) return false;
            break;
        case 3:
            if (!check((JS_AddIntrinsicRegExp(ctx) || JS_AddIntrinsicPromise(ctx) || JS_AddIntrinsicProxy(ctx)) ? JS_EXCEPTION : JS_UNDEFINED)) return false;
            JS_AddIntrinsicRegExpCompiler(ctx);
            break;
        case 4: {
            bindings = Value(ctx, JS_NewObject(ctx));
            if (!check(bindings.value)) return false;
            auto property = [&](const char* name, JSValue value) {
                Value held(ctx,value);
                if (!check(held.value)) return false;
                return JS_SetPropertyStr(ctx,bindings.value,name,held.release())>=0 || check(JS_EXCEPTION);
            };
            for (auto [name, operation] : std::array<std::pair<const char*, Operation>, 9>{{
                {"now",clock},{"cookie",cookie},{"storage",storage},{"request",request},
                {"cancel",cancel},{"navigate",navigate},{"fragment",fragment},{"changed",changed},{"wake",wake}}})
                if (!property(name, JS_NewCFunctionMagic(ctx, native, name, 0, JS_CFUNC_generic_magic, operation))) return false;
            if (!property("url", string(url).release()) || !property("userAgent",string(metrics.user_agent).release()) ||
                !property("width",JS_NewInt32(ctx,metrics.width)) || !property("height",JS_NewInt32(ctx,metrics.height)) ||
                !property("density",JS_NewFloat64(ctx,metrics.density)) ||
                !property("batchNodes",JS_NewInt32(ctx,dom_batch_nodes))) return false;
            if (bootstrap.starts_with("PXAQJS1\n")) {
                Value compiled(ctx, JS_ReadObject(ctx, reinterpret_cast<const std::uint8_t*>(bootstrap.data()+8), bootstrap.size()-8, JS_READ_OBJ_BYTECODE));
                if (!check(compiled.value)) return false;
                prepared = Value(ctx, JS_EvalFunction(ctx, compiled.release()));
            } else {
                prepared = Value(ctx, JS_Eval(ctx, bootstrap.c_str(), bootstrap.size(), "pxa-browser-runtime.js", JS_EVAL_TYPE_GLOBAL));
            }
            if (!check(prepared.value)) return false;
            std::string{}.swap(bootstrap);
            break;
        }
        case 5: {
            JSValue argument = bindings.value;
            bridge = Value(ctx, JS_Call(ctx, prepared.value, JS_UNDEFINED, 1, &argument));
            if (!check(bridge.value)) return false;
            bindings = {}; prepared = {};
            break;
        }
        default: return true;
        }
        ++initialization_stage;
        return true;
    }
    bool prepare_script(std::string_view source, std::uint32_t response_id=0) {
        if (source.size() > limits.script_bytes || source.size() > limits.source_bytes - std::min(source_bytes, limits.source_bytes)) {
            problem = "网页脚本超过容量限制"; ++counters.errors; return true;
        }
        CompiledScript cached;
        if(response_id && hooks.compiled_script) {
            cached=hooks.compiled_script(hooks.owner,response_id,source);
            if(cached.state==CompiledScript::State::loading)return false;
        }
        source_bytes += source.size();
        // Compilation is a separate idle turn. Source copies can be released
        // before execution, which receives the ordinary callback deadline.
        deadline = now() + compile_turn_us;
        if(cached.state==CompiledScript::State::bytecode) {
            if(!cached.bytes.starts_with("PXAQJS1\n") || cached.bytes.size()>65536) {
                problem="预编译脚本资源无效";++counters.errors;stopped=true;return true;
            }
            trace("COMPILED_LIBRARY");
            prepared=Value(ctx,JS_ReadObject(ctx,reinterpret_cast<const std::uint8_t*>(cached.bytes.data()+8),cached.bytes.size()-8,JS_READ_OBJ_BYTECODE));
#ifdef PXA_BROWSER_TRACE
            trace("COMPILED_LIBRARY_READ");
#endif
        } else {
            // A response body is already an owning, NUL-terminated string and
            // stays in its slot throughout this synchronous call. Inline HTML
            // slices need their own terminator; never borrow one past the view.
            const std::string terminated=response_id?std::string{}:std::string(source);
            const char* input=response_id?source.data():terminated.c_str();
            trace("COMPILE");
            prepared = Value(ctx, JS_Eval(ctx, input, source.size(), url.c_str(), JS_EVAL_TYPE_GLOBAL|JS_EVAL_FLAG_COMPILE_ONLY));
        }
        if (!check(prepared.value)) {
            if (counters.timeouts) problem="脚本编译超时";
            prepared={}; return true;
        }
        if (now() >= deadline) {
            prepared={};problem="脚本编译超时";stopped=true;
            ++counters.errors;++counters.timeouts;
        }
        return true;
    }
    [[gnu::noinline]] void execute_script() {
        std::uint32_t id=0;
        bool parser_script=false;
        if (prepared_response) {
            id=responses.front().id;responses.erase(responses.begin());
            parser_script=waiting_script==id;
            if (parser_script) waiting_script=0;
            retire(id);prepared_response=false;
        }
        const auto before=counters.errors;
        Value program=std::move(prepared);
        trace("EXECUTE");
        Value result(ctx,JS_EvalFunction(ctx,program.release()));
        check(result.value);
        trace("EXECUTED");
        if (id && !parser_script && !stopped)
            invoke("scriptResponse",JS_NewUint32(ctx,id),JS_NewBool(ctx,before!=counters.errors));
    }
    void build_one(std::string_view source, std::size_t& at, bool scripts) {
        const auto item = token(source, at);
        if (item.text.empty()) return;
        if (!item.tag) {
            auto decoded = browser::entities(item.text); auto value = string(decoded);
            if (check(value.value)) invoke("text", value.value);
            return;
        }
        if (item.text.starts_with('!') || item.text.starts_with('?')) return;
        const auto t = browser::tag(item.text);
        if (t.name.empty()) return;
        if (!t.closing && (t.name == "script" || t.name == "style")) {
            const auto closing = raw_end(source, t.name == "script" ? "</script" : "</style", at);
            const auto end = closing == source.npos ? source.size() : closing;
            auto body = source.substr(at, end - at); at = end;
            if (closing != source.npos) (void)token(source, at);
            if (!scripts || t.name != "script" || !executable(t)) return;
            if (++script_count > limits.scripts) { problem = "网页脚本数量达到上限"; ++counters.errors; return; }
            const auto src = t.get("src");
            if (src.empty()) prepare_script(body);
            else {
                const auto target = browser::resolve(url, src);
                if (!target || !script_url_allowed(url, *target, true)) { ++counters.errors; problem = "脚本地址已拦截"; return; }
                Request request; request.script = true; request.url = *target;
                waiting_script = queue(std::move(request));
                if (!waiting_script) { ++counters.errors; problem = "脚本请求达到上限"; }
            }
            return;
        }
        auto name = string(t.name);
        if (!check(name.value)) return;
        if (t.closing) { invoke("close", name.value); return; }
        if (scripts && ++node_count > limits.nodes) { problem = "脚本页面节点达到上限"; stopped = true; return; }
        Value attrs(ctx, JS_NewArray(ctx));
        if (!check(attrs.value)) return;
        for (std::size_t i = 0; i < t.count; ++i) {
            auto key = string(t.attrs[i].name); auto value = string(browser::entities(t.attrs[i].value));
            if (!check(key.value) || !check(value.value)) return;
            if (JS_SetPropertyUint32(ctx, attrs.value, std::uint32_t(i*2), key.release()) < 0 ||
                JS_SetPropertyUint32(ctx, attrs.value, std::uint32_t(i*2+1), value.release()) < 0) { check(JS_EXCEPTION); return; }
        }
        constexpr auto voids = " area base br col embed hr img input link meta param source track wbr "sv;
        const bool empty = trim(item.text).ends_with('/') || voids.find(" " + t.name + " ") != voids.npos;
        invoke("open", name.value, attrs.value, JS_NewBool(ctx, empty));
    }
    void trace(std::string_view phase) {
        if constexpr(trace_enabled)if(hooks.trace)hooks.trace(hooks.owner,phase);
    }
    [[gnu::noinline]] void trace_initialization(unsigned stage, std::optional<std::uint64_t> elapsed = {}) {
        if constexpr(!trace_enabled)return;
        if (!hooks.trace) return;
        // Prefix, 10-digit stage, separator and 20-digit duration fit in 64 B.
        char buffer[64];
        auto* at = buffer;
        const auto append = [&](std::string_view text) {
            at = std::copy(text.begin(), text.end(), at);
        };
        append(elapsed ? "INIT_END "sv : "INIT_BEGIN "sv);
        at = std::to_chars(at, std::end(buffer), stage).ptr;
        if (elapsed) {
            append(" us="sv);
            at = std::to_chars(at, std::end(buffer), *elapsed).ptr;
        }
        hooks.trace(hooks.owner, {buffer, std::size_t(at-buffer)});
    }
    // WAMR FAST retains a function's locals and constant pool on every call.
    // Keep phase-specific state out of the recursive engine's caller frame.
    [[gnu::noinline]] void initialize_turn() {
        const auto begin = now();
        const auto stage=initialization_stage;
        trace_initialization(stage);
        if (runtime) JS_UpdateStackTop(runtime);
        if (!initialize_step()) stopped = true;
        if (heap.failed && !stopped) { problem="脚本内存达到上限"; stopped=true; ++counters.errors; }
        const auto elapsed=now()-begin;
        counters.initialization_us+=elapsed;
        counters.max_initialization_step_us=std::max(counters.max_initialization_step_us,elapsed);
        trace_initialization(stage, elapsed);
        deadline=0;
    }
    [[gnu::noinline]] void handle_response() {
        const auto before=counters.errors;
        const auto& front=responses.front();
        const bool script=pending(front.id) && pending_script(front.id);
        if (script && !front.failed && front.status >= 200 && front.status < 300) {
            if(!prepare_script(front.body,front.id))return;
            if (prepared.ctx) {
                // Retain only the response's existing metadata until the next
                // turn. Keeping its slot also preserves parser/script order.
                prepared_response=true;
                std::string{}.swap(responses.front().body);
                return;
            }
        }
        auto response=std::move(responses.front());responses.erase(responses.begin());
        if (!pending(response.id)) return;
        const bool parser_script=waiting_script==response.id;
        retire(response.id);
        if (script) {
            if (parser_script) waiting_script=0;
            const bool failed=response.failed || response.status<200 || response.status>=300;
            if (failed && parser_script) { ++counters.errors;problem="部分脚本未能加载"; }
            if (!parser_script && !stopped)
                invoke("scriptResponse",JS_NewUint32(ctx,response.id),JS_NewBool(ctx,failed || before!=counters.errors));
        } else {
            auto text=string(response.body);
            if (check(text.value)) invoke("response",JS_NewUint32(ctx,response.id),JS_NewUint32(ctx,response.status),text.value,JS_NewBool(ctx,response.failed));
        }
    }
    [[gnu::noinline]] void build_dom_batch() {
        for (unsigned i = 0; i < dom_batch_nodes && cursor < html.size() && !stopped && !waiting_script; ++i) {
            const auto scripts_before = script_count;
            build_one(html, cursor, true);
            if (script_count != scripts_before || now() >= deadline) break;
        }
        if (cursor == html.size() && !stopped && !waiting_script && !prepared.ctx) {
            finished = true; invoke("finish");
            std::string{}.swap(html);
        }
    }
    [[gnu::noinline]] void dispatch_event() {
        dom_dirty = true;
        if (click_count) {
            // Bound autonomous loads between genuine UI actions. A long
            // comic must not exhaust its lifetime script/source allowance
            // simply because the user has turned more than twelve pages.
            script_count=0;source_bytes=0;
            const auto id = clicks[0]; std::move(clicks.begin()+1,clicks.begin()+click_count,clicks.begin()); --click_count;
            if (prepare_inline(id, "click")) invoke("click", JS_NewUint32(ctx,id));
        } else if (!fields.empty()) {
            script_count=0;source_bytes=0;
            auto field = std::move(fields.front()); fields.erase(fields.begin());
            auto value = string(field.value);
            if (check(value.value) && prepare_inline(field.id, "input") && prepare_inline(field.id, "change"))
                invoke("field", JS_NewUint32(ctx,field.id), value.value, JS_NewBool(ctx,field.checked));
        } else invoke("pump");
        if (!stopped && now() < deadline && JS_IsJobPending(runtime)) {
            JSContext* job_context = nullptr;
            if (JS_ExecutePendingJob(runtime, &job_context) < 0) check(JS_EXCEPTION);
        }
    }
    void step() {
        if (stopped || paused) return;
        if (initialization_stage < 6) { initialize_turn(); return; }
        if (finished && !prepared.ctx && responses.empty() && !click_count && fields.empty() && now() < timer_at && !JS_IsJobPending(runtime)) return;
        if (snapshot_pending && responses.empty() && !click_count && fields.empty()) return;
        JS_UpdateStackTop(runtime);
        const auto begin = now(); deadline = begin + limits.turn_us;
        if (snapshot_pending) {
            invoke("cancelMarkup"); snapshot_pending=false; dom_dirty=true;
            std::string{}.swap(markup_buffer);
            if (stopped) { deadline=0; return; }
        }
        ++counters.turns;
        if (prepared.ctx) execute_script();
        else if (!responses.empty()) handle_response();
        else if (!finished && !waiting_script) build_dom_batch();
        else if (finished) dispatch_event();
        if (heap.failed && !stopped) { problem="脚本内存达到上限"; stopped=true; ++counters.errors; }
        counters.max_turn_us = std::max(counters.max_turn_us, now()-begin);
        deadline = 0;
    }
};

Page::Page() = default;
Page::~Page() = default;
Page::Page(Page&&) noexcept = default;
Page& Page::operator=(Page&&) noexcept = default;
bool Page::open(std::string html, std::string url, Bootstrap bootstrap, Hooks hooks, Metrics metrics, Limits limits) {
    impl_.reset(new(std::nothrow) Impl);
    if (!impl_) return false;
    auto& self = *impl_; self.limits = limits; self.hooks = hooks; self.metrics = std::move(metrics);
    if (html.size() > limits.html_bytes || bootstrap.runtime.size() > 64*1024 ||
        origin(url).empty()) {
        self.problem = "脚本页面超过限制"; self.stopped = true; return false;
    }
    self.html = std::move(html); self.url = std::move(url); self.bootstrap = std::move(bootstrap.runtime);
    return true;
}
void Page::step() { if (impl_) impl_->step(); }
bool Page::set_heap_limit(std::size_t bytes) {
    if (!impl_ || bytes < 128*1024 || bytes > 4*1024*1024) return false;
    auto& self=*impl_;
    if (self.runtime && self.heap.used > bytes) JS_RunGC(self.runtime);
    if (self.heap.used > bytes) return false;
    self.limits.heap_bytes=bytes;self.heap.limit=bytes;
    if (self.runtime) JS_SetMemoryLimit(self.runtime,bytes);
    return true;
}
void Page::complete(Response response) {
    if (!impl_ || !impl_->pending(response.id) || impl_->stopped) return;
    if (response.body.size() > (impl_->pending_script(response.id) ? impl_->limits.script_bytes : impl_->limits.response_bytes)) { response.body.clear(); response.failed = true; }
    if (impl_->responses.size() < 4) impl_->responses.push_back(std::move(response));
}
std::optional<Request> Page::take_request() {
    if (!impl_ || impl_->requests.empty() || impl_->stopped) return {};
    auto request = std::move(impl_->requests.front()); impl_->requests.erase(impl_->requests.begin()); return request;
}
std::optional<Navigation> Page::take_navigation() {
    if (!impl_) return {};
    return std::exchange(impl_->navigation, {});
}
std::optional<std::string> Page::take_markup() {
    if (!ready() || impl_->paused || (!impl_->dom_dirty && !impl_->snapshot_pending)) return {};
    impl_->dom_dirty = false;
    auto& self = *impl_;
    struct Timing {
        Impl& self; std::uint64_t begin;
        ~Timing() { self.counters.max_markup_us=std::max(self.counters.max_markup_us,self.now()-begin); }
    } timing{self,self.now()};
    ++self.counters.markup_turns;
    JS_UpdateStackTop(self.runtime);
    self.deadline = self.now() + self.limits.turn_us;
    auto value = self.call("markup");
    self.deadline = 0;
    // A failed snapshot may have replaced its hit table. Stop the realm so
    // native links can continue against the last complete, visible document.
    if (JS_IsException(value.value)) { self.stopped=true; return {}; }
    if (JS_IsNull(value.value)) return {};
    Value batch_text(self.ctx,JS_GetPropertyStr(self.ctx,value.value,"text"));
    Value complete(self.ctx,JS_GetPropertyStr(self.ctx,value.value,"complete"));
    if (!self.check(batch_text.value) || !self.check(complete.value) ||
        !JS_IsString(batch_text.value) || !JS_IsBool(complete.value)) {
        self.stopped=true; return {};
    }
    CString text(self.ctx,batch_text.value);
    if (!text.data) { self.check(JS_EXCEPTION); return {}; }
    if (text.size > max_text-self.markup_buffer.size()) {
        self.problem = "脚本页面输出达到上限"; self.stopped = true; return {};
    }
    self.markup_buffer.append(text.view());
    self.snapshot_pending=!JS_ToBool(self.ctx,complete.value);
    if (self.snapshot_pending) return {};
    std::uint64_t hash = 14695981039346656037ull;
    for (const unsigned char c : self.markup_buffer) hash = (hash ^ c) * 1099511628211ull;
    const auto size=self.markup_buffer.size();
    if (size == self.markup_size && hash == self.markup_hash) {
        std::string{}.swap(self.markup_buffer);return {};
    }
    self.markup_size = size; self.markup_hash = hash;
    return std::exchange(self.markup_buffer,{});
}
bool Page::click(std::uint32_t node) {
    if (!active() || impl_->click_count == impl_->clicks.size()) return false;
    impl_->clicks[impl_->click_count++] = node; return true;
}
void Page::change_field(std::uint32_t node, std::string value, bool checked) {
    if (!active() || !node || value.size() > 4096) return;
    auto old = std::ranges::find(impl_->fields, node, &Impl::FieldChange::id);
    if (old != impl_->fields.end()) { old->value=std::move(value); old->checked=checked; }
    else if (impl_->fields.size() < 24) impl_->fields.push_back({node,std::move(value),checked});
}
void Page::pause(bool value) { if (impl_) impl_->paused = value; }
bool Page::pending(std::uint32_t id) const noexcept { return impl_ && impl_->pending(id); }
bool Page::active() const noexcept { return impl_ && !impl_->stopped; }
bool Page::initialized() const noexcept { return active() && impl_->initialization_stage==6; }
bool Page::ready() const noexcept { return active() && impl_->finished; }
std::string_view Page::error() const noexcept { return impl_ ? impl_->problem : std::string_view{}; }
Stats Page::stats() const noexcept {
    if (!impl_) return {};
    auto result = impl_->counters; result.current_bytes = impl_->heap.used; result.peak_bytes = impl_->heap.peak; return result;
}
std::uint64_t Page::next_wake_us() const noexcept {
    if(!active() || impl_->paused) return UINT64_MAX;
    const auto& s=*impl_;
    if(s.initialization_stage<6 || s.prepared.ctx || (!s.finished && !s.waiting_script) || !s.responses.empty() ||
       s.click_count || !s.fields.empty() || (s.finished && (s.dom_dirty || s.snapshot_pending)) || JS_IsJobPending(s.runtime)) return 0;
    return s.timer_at;
}
} // namespace browser::script
