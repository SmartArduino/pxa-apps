#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace browser::script {
// Private wire selector. Only original-page execution is accepted; network
// responses remain untrusted source text, never signed bootstrap bytecode.
enum class Module : unsigned { page };
// App policy, not Host/ABI limits. Nothing reserves these budgets by default.
struct Limits {
    std::size_t heap_bytes = 256 * 1024;
    // The device's native owner task is smaller than the Wasm linear C stack.
    // The WASI native nesting guard and AOT checks protect that separate stack.
    std::size_t stack_bytes = sizeof(void*) == 4 ? 16 * 1024 : 256 * 1024;
    std::size_t html_bytes = 32 * 1024;
    std::size_t source_bytes = 48 * 1024;
    std::size_t response_bytes = 16 * 1024;
    std::size_t script_bytes = 32 * 1024;
    std::size_t fragment_bytes = 8192;
    unsigned nodes = 192, scripts = 12, fragment_nodes = 96;
    std::uint64_t turn_us = 50000;
};
struct Metrics { int width = 296, height = 240; double density = 1; std::string user_agent = "PXA-Browser"; };
// Only signed app assets may supply bytecode. Network responses remain source.
struct CompiledScript {
    enum class State : unsigned char { source, loading, bytecode };
    State state=State::source;
    std::string bytes;
};
struct Hooks {
    void* owner = nullptr;
    std::uint64_t (*now_us)(void*) = nullptr;
    void (*trace)(void*,std::string_view) = nullptr;
    CompiledScript (*compiled_script)(void*,std::uint32_t,std::string_view) = nullptr;
    std::string (*cookie_get)(void*, std::string_view) = nullptr;
    void (*cookie_set)(void*, std::string_view, std::string_view) = nullptr;
    std::optional<std::string> (*storage_get)(void*, std::string_view, std::string_view, bool) = nullptr;
    bool (*storage_set)(void*, std::string_view, std::string_view, const std::string*, bool, bool) = nullptr;
};
struct Request {
    std::uint32_t id = 0;
    bool script = false, post = false;
    std::string url, body, content_type;
    // The Host admits at most 60 s per exchange. Use that allowance when the
    // page leaves XHR.timeout at its default zero; a short implicit timeout
    // breaks long polling. Explicit nonzero page timeouts still take priority.
    std::uint32_t timeout_ms = 60000;
};
struct Response {
    std::uint32_t id = 0;
    unsigned status = 0;
    std::string body;
    bool failed = false;
};
struct Navigation { std::string url, body; bool post = false; };
// App-owned, version-pinned bytecode from signed package assets only. Never
// populate it from a page/network response: QuickJS bytecode is trusted.
struct Bootstrap { std::string runtime; };
struct Stats {
    std::size_t current_bytes = 0, peak_bytes = 0;
    std::uint32_t turns = 0, errors = 0, timeouts = 0;
    std::uint64_t max_turn_us = 0, initialization_us = 0, max_initialization_step_us = 0;
    std::uint64_t max_markup_us = 0;
    std::uint32_t markup_turns = 0;
};
// Bounded HTML capture is separate from the VM. Static pages never create one.
class Capture {
    std::string bytes_;
    enum class Mode : std::uint8_t { text, tag, comment, script, style };
    std::size_t tag_start_ = 0;
    Mode mode_ = Mode::text;
    unsigned char match_ = 0;
    char quote_ = 0;
    bool limited_ = false;
public:
    void append(std::string_view part, std::size_t limit = 32768);
    bool has_script() const noexcept;
    bool limited() const noexcept { return limited_; }
    std::string take() noexcept { return std::move(bytes_); }
};
bool same_origin(std::string_view a, std::string_view b);
bool script_url_allowed(std::string_view page, std::string_view target, bool external_script);

// Browser-session storage. Allocates only on first write; separate namespaces,
// exact origin matching and one shared 8 KiB quota. No automatic disk writes.
class Storage {
    struct Entry { std::string origin, key, value; bool session; };
    std::vector<Entry> entries_;
public:
    std::optional<std::string> get(std::string_view url, std::string_view key, bool session) const;
    bool set(std::string_view url, std::string_view key, const std::string* value, bool session, bool clear_all = false);
    void clear() { std::vector<Entry>{}.swap(entries_); }
};
// Owns one page realm. Destruction releases all JS roots and pending responses.
// Network completion only queues bytes. step() and take_markup() enter the VM
// on idle foreground frames; snapshots advance in bounded chunks.
class Page {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    Page();
    ~Page();
    Page(Page&&) noexcept;
    Page& operator=(Page&&) noexcept;
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
    // Takes ownership without executing JS. step() initializes one trusted
    // stage at a time before evaluating any website code.
    bool open(std::string html, std::string url, Bootstrap bootstrap,
              Hooks hooks = {}, Metrics metrics = {}, Limits limits = {});
    void step();
    void complete(Response response);
    std::optional<Request> take_request();
    std::optional<Navigation> take_navigation();
    std::optional<std::string> take_markup();
    bool click(std::uint32_t node);
    void change_field(std::uint32_t node, std::string value, bool checked);
    void pause(bool value);
    bool pending(std::uint32_t id) const noexcept;
    bool active() const noexcept;
    bool initialized() const noexcept;
    bool set_heap_limit(std::size_t bytes);
    bool ready() const noexcept;
    std::string_view error() const noexcept;
    Stats stats() const noexcept;
    std::uint64_t next_wake_us() const noexcept;
};
} // namespace browser::script
