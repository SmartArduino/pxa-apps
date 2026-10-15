#include "browser_core.hpp"
#include "browser_search.hpp"
#include "browser_image_view.hpp"
#include "browser_geometry.hpp"
#include "browser_scrollbar.hpp"
#include "browser_cookies.hpp"
#include "browser_sites.hpp"
#include "browser_site_forms.hpp"
#include "browser_encoding.hpp"
#include <pxa/ui_volume_keys.hpp>
#include <cstdio>
#include <ctime>
#include <utility>
#include <pxa/app.hpp>
#include <pxa/canvas.hpp>
#include <pxa/canvas_geometry.hpp>
#include <pxa/events.hpp>
#include <pxa/fs.hpp>
#include <pxa/http.hpp>
#include <pxa/image_frames.hpp>
#include <pxa/libcpp_diagnostics.hpp>
#include <pxa/log.hpp>
#include <pxa/ui_geometry.hpp>
#include <pxa/ui_widgets.hpp>

#ifndef PXA_BROWSER_START_URL
#define PXA_BROWSER_START_URL ""
#endif
using namespace pxa::ui;
namespace {
// This small clock helper is called from input and streamed reads. Inlining
// also avoids a permanent AOT function-table entry on constrained Hosts.
[[gnu::always_inline]] inline std::uint64_t millis() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return std::uint64_t(t.tv_sec) * 1000 + t.tv_nsec / 1000000;
}
#ifdef PXA_BROWSER_PROFILE
std::uint64_t micros() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return std::uint64_t(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}
#endif
struct App;
struct Pointer {
    App *app;
    void operator()(const CanvasPointer &) const;
};
struct App {
    static constexpr pxa::game::LoopOptions loop_options{20000, 20, 1};
    pxa::Context *context = nullptr;
    DisplayMetrics metrics;
    pxa::Insets system_insets;
    CanvasRef canvas;
    CanvasCommands<4000> draw;
    State<std::string> input{""};
    State<bool> capture_volume_keys{false};
    State<bool> password_input{false};
    browser::Cookies cookies;
    bool show_images = true, show_gif = true;
    enum class MenuPage : std::uint8_t { tools, history, directory };
    MenuPage menu_page = MenuPage::tools;
    bool volume_scroll = false;
    int volume_direction = 0;
    std::uint64_t volume_repeat_at = 0;
    TextInputRef editor;
    State<TextInputBox> editor_box{TextInputBox{{}, false, Font::body, 0x17334aff}};
    pxa::Permission permission;
    browser::Document document;
    browser::Layout page;
    browser::History history;
    std::vector<std::string> bookmarks;
    std::string url = "about:home", status;
    std::uint64_t status_until = 0;
    std::array<std::byte, 4096> receive{};
    enum class ImageState : std::uint8_t { unseen, loading, ready, failed };
    struct ImageCache {
        int index = -1;
        std::optional<pxa::DecodedImage> asset;
        std::uint64_t next_frame = 0;
    };
    std::array<ImageCache, 2> images;
    int animation_slot = -1;
    std::array<ImageState, browser::max_images> image_states{};
    browser::ImageView image_view;
    int loading_image = -1;
    bool foreground = true;
    int scroll = 0, body_font_dp = 15, edit_field = -1, restore_scroll = 0;
    bool busy = false, dirty = true, layout_dirty = true, editing = false, menu = false,
         bookmark_save = false, bookmarks_ready = false, focus_pending = false,
         preferences_dirty = false, window_pending = true, body_dirty = false, mobile_ua = false;
    int menu_scroll = 0;
    browser::SearchEngine search_engine = browser::SearchEngine::bing;
    PressGesture press;
    int last_x = 0, last_y = 0;
    bool hold_enabled = false;
    bool scrollbar_dragging = false;
    enum class ExitState : std::uint8_t { none, saving, queued };
    ExitState exit_state = ExitState::none;
    std::uint64_t scrollbar_until = 0;
    std::uint64_t last_progress = 0;
#ifdef PXA_BROWSER_PROFILE
    struct Profile {
        std::uint64_t encode_us = 0, submit_us = 0, layout_us = 0;
        std::uint32_t renders = 0, layouts = 0, bytes = 0, max_submit_us = 0;
        bool pending = false;
    } profile;
    void report_profile() {
        char text[256];
        std::snprintf(text, sizeof(text),
                      "BROWSER PERF renders=%u layouts=%u encode_us=%llu submit_us=%llu "
                      "layout_us=%llu bytes=%u max_submit_us=%u scroll=%d",
                      profile.renders, profile.layouts,
                      static_cast<unsigned long long>(profile.encode_us),
                      static_cast<unsigned long long>(profile.submit_us),
                      static_cast<unsigned long long>(profile.layout_us), profile.bytes,
                      profile.max_submit_us, menu ? menu_scroll : scroll);
        log(text);
        profile.pending = false;
    }
#endif
    enum class Action {
        address,
        back,
        forward,
        home,
        reload,
        menu,
        bookmark,
        font_less,
        font_more,
        add_site,
        exit,
        link,
        field,
        bookmark_open,
        search_engine,
        search,
        page_mode,
        image,
        image_close,
        image_less,
        image_fit,
        image_more,
        volume_keys, history_list, history_open, history_clear, directory, site_open,
        image_setting, gif_setting, clear_sessions
    };
    Action pressed_action = Action::address;
    int pressed_index = -1;
    struct Hit {
        PixelRect box;
        Action action;
        int index = 0;
    };
    std::array<Hit, 96> hits{};
    std::size_t hit_count = 0;
    PixelRect toolbar, content, address, status_box;
    int toolbar_y = 0, row = 0, pad = 0, font = 15;
    auto view() {
        return CaptureVolumeKeys(Stack(Canvas(canvas).on_pointer(Pointer{this}),
                     TextInput(input, editor)
                         .box(editor_box)
                         .max_bytes(browser::max_url)
                         .single_line()
                         .on_submit([this] { finish_edit(true); })
                         .password(password_input))
            .fill()
            .fill_height(), capture_volume_keys,
            [this](const VolumeKeyEvent &event) { volume_key(event.key); });
    }
    int dp(int n) const { return std::max(1, int(std::int64_t(n) * metrics.density_q16 / 65536)); }
    void geometry() {
        const auto boxes = browser::viewport(metrics, system_insets);
        toolbar = boxes.toolbar;
        content = boxes.content;
        address = boxes.address;
        status_box = boxes.status;
        pad = dp(6);
        row = toolbar.height;
        font =
            std::clamp(int(std::int64_t(dp(body_font_dp)) * metrics.font_scale_q16 / 65536), 8, 64);
        toolbar_y = toolbar.y;
        sync_editor();
#ifdef PXA_BROWSER_TRACE
        char layout[160];
        std::snprintf(layout, sizeof(layout),
            "BROWSER VIEWPORT address=%d,%d,%d,%d content=%d,%d,%d,%d toolbar=%d,%d,%d,%d status=%d,%d,%d,%d",
            address.x, address.y, address.width, address.height,
            content.x, content.y, content.width, content.height,
            toolbar.x, toolbar.y, toolbar.width, toolbar.height,
            status_box.x, status_box.y, status_box.width, status_box.height);
        log(layout);
#endif
    }
    void sync_editor() {
        auto density = std::max(1u, metrics.density_q16);
        editor_box.set(
            TextInputBox{{int(std::int64_t(address.x) * 65536 / density),
                          int(std::int64_t(address.y) * 65536 / density),
                          std::max(1, int(std::int64_t(address.width - dp(32)) * 65536 / density)),
                          std::max(1, int(std::int64_t(address.height) * 65536 / density))},
                         editing,
                         Font::body,
                         0x17334aff});
    }
    void log(std::string_view s) { (void)context->log().write(pxa::LogLevel::info, s); }
    void set_status(auto &&message, unsigned duration_ms = 4000) {
        status = std::forward<decltype(message)>(message);
        reveal_status(duration_ms);
    }
    // Share the clock conversion across message types instead of duplicating
    // it in every literal/string instantiation. This is a notification path.
    [[gnu::noinline]] void reveal_status(unsigned duration_ms = 4000) {
        status_until = millis() + duration_ms;
        dirty = true;
    }
    void fail(pxa::Error error) {
        exit_state = ExitState::none;
        busy = false;
        layout_dirty = true;
        set_status("加载失败 (" + std::to_string(int(error)) + ") · 可重试", 8000);
        dirty = true;
        log("BROWSER ERROR code=" + std::to_string(int(error)));
    }
    void start(pxa::Task<void> task, bool foreground = true) {
        auto result = foreground ? context->foreground_tasks().start(std::move(task))
                                 : context->tasks().start(std::move(task));
        if (!result) {
            if (loading_image >= 0) {
                image_states[loading_image] = ImageState::failed;
                for (auto &image : images)
                    if (image.index == loading_image && !image.asset) image.index = -1;
                loading_image = -1;
            }
            fail(result.error());
        }
    }
    void stop() {
        const bool interrupted = busy || loading_image >= 0;
        const int cancelled_frame = animation_slot;
        context->foreground_tasks().cancel();
        animation_slot = -1;
        if (cancelled_frame >= 0) {
            auto &image = images[cancelled_frame];
            image_states[image.index] = ImageState::unseen;
            image.asset.reset();
            image.index = -1;
        }
        if (loading_image >= 0) {
            image_states[loading_image] = ImageState::failed;
            for (auto &image : images)
                if (image.index == loading_image && !image.asset) image.index = -1;
        }
        loading_image = -1;
        busy = false;
        if (interrupted) set_status("已停止");
        dirty = true;
    }
    void clear_images() {
        image_view = {};
        scrollbar_dragging = false;
        scrollbar_until = 0;
        for (auto &image : images) {
            image.asset.reset();
            image.index = -1;
        }
        image_states.fill(ImageState::unseen);
    }
    void rebuild() {
#ifdef PXA_BROWSER_PROFILE
        auto begin = micros();
#endif
        geometry();
        page.build(document, content.width, font);
        if (url == "about:home")
            page.height = home_height();
        layout_dirty = false;
        scroll = std::clamp(scroll, 0, std::max(0, page.height - content.height));
        menu_scroll = std::clamp(menu_scroll, 0, std::max(0, scroll_height() - content.height));
        dirty = true;
#ifdef PXA_BROWSER_PROFILE
        profile.layout_us += micros() - begin;
        ++profile.layouts;
#endif
    }
    void home(bool record = true, int position = 0) {
        press.reset();
        stop();
        clear_images();
        url = "about:home";
        document.clear("https://pxa.doit.am/");
        scroll = position;
        set_status("PXA 浏览器 · 轻量网页阅读");
        layout_dirty = true;
        menu = false;
        if (record)
            history.visit(url);
    }
    void anchor(bool reading_start = false) {
        auto hash = url.find('#');
        if (hash == url.npos && !reading_start)
            return;
        // Login forms can follow a site's large logo/navigation. Start at
        // their first editable input while retaining explicit fragments and
        // restored history positions. No site-specific scroll coordinates.
        if (hash == url.npos && reading_start && std::ranges::any_of(document.fields,
                [](const auto &field) { return field.type == browser::FieldType::password; })) {
            for (const auto &piece : page.pieces) {
                const auto field = document.spans[piece.span].field;
                if (field >= 0 && (document.fields[field].type == browser::FieldType::text ||
                                  document.fields[field].type == browser::FieldType::password)) {
                    scroll = std::clamp(int(piece.y), 0, std::max(0, page.height-content.height));
                    dirty = true; return;
                }
            }
        }
        // MediaWiki exposes a stable content id. On an initial article load,
        // start there instead of making small-screen readers pass site menus.
        // Explicit fragments and restored history positions take precedence.
        auto name = hash == url.npos ? std::string_view("mw-content-text")
                                     : std::string_view(url).substr(hash + 1);
        for (auto &a : document.anchors)
            if (browser::fragment_matches(a.name, name)) {
                for (auto &p : page.pieces)
                    if (p.offset >= a.offset) {
                        scroll = std::clamp(int(p.y), 0, std::max(0, page.height - content.height));
                        dirty = true;
                        return;
                    }
            }
    }
    void navigate(std::string target, bool record = true, std::string body = {}, bool post = false,
                  int position = 0) {
        press.reset();
        if (editing)
            finish_edit(false);
        if (target == "about:home") {
            home(record, position);
            return;
        }
        auto normalized = browser::resolve(document.base, target, true);
        if (!normalized) {
            set_status("网址无效：仅支持 HTTP/HTTPS");
            dirty = true;
            return;
        }
        if (!busy && browser::transport_url(*normalized) == browser::transport_url(url) &&
            normalized->find('#') != normalized->npos) {
            history.position(scroll);
            url = std::move(*normalized);
            if (record)
                history.visit(url);
            anchor();
            dirty = true;
            return;
        }
        auto initiator = url;
        stop();
        clear_images();
        menu = false;
        url = std::move(*normalized);
        scroll = 0;
        restore_scroll = position;
        document.clear(url);
        layout_dirty = true;
        if (record)
            history.visit(post ? std::string(browser::transport_url(url)) : url);
        busy = true;
        set_status("正在连接…");
        log("BROWSER NAV " + url.substr(0, url.find_first_of("?#")));
        start(fetch(std::move(body), post, std::move(initiator)));
    }
    pxa::Task<void> fetch(std::string body, bool post, std::string initiator) {
        std::uint64_t first = 0, head = 0;
        std::size_t received = 0;
        if (!permission) {
            auto granted = co_await context->permissions().acquire("net.client", "web");
            if (!granted)
                co_return std::unexpected(granted.error());
            permission = std::move(*granted);
        }
        const auto begin = millis(); // Permission-dialog time is not network latency.
        // Filter unused policy headers before the response-head budget. Retain
        // repeated Set-Cookie fields individually, including on redirects.
        static constexpr std::array<std::string_view, 4> wanted_headers{
            "content-type", "content-encoding", "location", "set-cookie"};
        for (unsigned redirect = 0; redirect <= 5; ++redirect) {
            const auto cookie = cookies.request(url, millis() / 1000);
            std::array<pxa::NetHeaderView, 8> headers{
                {{"Accept", "text/html, text/plain;q=0.9, application/rss+xml;q=0.8, "
                            "text/xml;q=0.7"},
                 {"Accept-Encoding", "identity"},
                 {"User-Agent", browser::site_user_agent(url, browser::user_agent(mobile_ua))},
                 {"Save-Data", "on"},
                 {"Content-Type", "application/x-www-form-urlencoded"}}};
            std::size_t header_count = post ? 5 : 4;
            if (!cookie.empty()) headers[header_count++] = {"Cookie", cookie};
            auto origin = [](std::string_view value) {
                return value.substr(0, value.find_first_of("/?#", value.find("://") + 3));
            };
            const bool http_source = initiator.starts_with("http:") || initiator.starts_with("https:");
            if (http_source && !(initiator.starts_with("https:") && url.starts_with("http:"))) {
                // Origin-only referrer avoids leaking queries and path tokens.
                headers[header_count++] = {"Referer", origin(initiator)};
                if (post) headers[header_count++] = {"Origin", origin(initiator)};
            }
            auto exchange = co_await context->http().open(
                {.url = browser::transport_url(url),
                 .method = post ? pxa::HttpMethod::post : pxa::HttpMethod::get,
                 .headers = std::span{headers}.first(header_count),
                 .wanted_headers = wanted_headers,
                 .body = std::as_bytes(std::span{body.data(), body.size()}),
                 .options = {.buffer_bytes = 4096,
                             .header_bytes = 4096,
                             .max_body_bytes = 262144,
                             .timeout_ms = 20000}},
                permission);
            if (!exchange)
                co_return std::unexpected(exchange.error());
            auto response = co_await std::move(*exchange).response();
            if (!response)
                co_return std::unexpected(response.error());
            head = millis() - begin;
            const auto code = response->status_code;
            for (auto value : response->headers.values("set-cookie"))
                cookies.receive(url, value, millis() / 1000, std::time(nullptr));
            if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
                auto location = response->headers.first("location");
                if (!location || redirect == 5)
                    co_return std::unexpected(pxa::Error::protocol_error);
                auto next = browser::resolve(url, *location);
                if (!next)
                    co_return std::unexpected(pxa::Error::invalid_argument);
                // Never downgrade HTTPS through a redirect or forward a POST
                // body to a different origin. Cookies are matched for each hop.
                if ((url.starts_with("https:") && next->starts_with("http:")) ||
                    (post && origin(url) != origin(*next) && code != 303))
                    co_return std::unexpected(pxa::Error::denied);
                if (code == 303 || ((code == 301 || code == 302) && post)) {
                    post = false;
                    body.clear();
                }
                url = std::move(*next);
                document.base = url;
                continue;
            }
            history.replace_current(url);
            auto encoding = response->headers.first("content-encoding");
            if (encoding && !pxa::http_header_equal(browser::trim(*encoding), "identity")) {
                set_status("网站返回压缩内容；请使用纯文本或轻量页面");
                busy = false;
                dirty = true;
                co_return pxa::Result<void>{};
            }
            auto type = response->headers.first("content-type");
            std::string mime = type ? browser::lowercase(*type) : "text/html";
            if (mime.starts_with("image/png") || mime.starts_with("image/jpeg") || mime.starts_with("image/gif")) {
                if (!show_images || (!show_gif && mime.starts_with("image/gif"))) {
                    busy = false; set_status("图片加载已关闭 · 可在菜单开启"); dirty = true;
                    co_return pxa::Result<void>{};
                }
                document.images.push_back({url, 240, 150});
                document.append("图片", browser::Style::muted, -1, -1, 0);
                images[0].index = 0;
                image_states[0] = ImageState::loading;
                loading_image = 0;
                auto decoded = co_await read_image(response->body, 0, 0, begin);
                loading_image = -1;
                busy = false;
                layout_dirty = true;
                dirty = true;
                if (!decoded) {
                    log("BROWSER IMAGE ERROR index=0 code=" +
                        std::to_string(int(decoded.error())));
                    images[0].index = -1;
                    image_states[0] = ImageState::failed;
                    set_status("图片未加载 · 轻点可重试");
                }
                co_return pxa::Result<void>{};
            }
            bool plain = mime.starts_with("text/plain");
            const bool wml = mime.starts_with("text/vnd.wap.wml") ||
                (mime.starts_with("application/octet-stream") && browser::transport_url(url).ends_with(".wml"));
            bool rss = mime.starts_with("application/rss+xml") || (!wml && mime.starts_with("text/xml")) ||
                       mime.starts_with("application/xml");
            if (!plain && !rss && !wml && !mime.starts_with("text/html") &&
                !mime.starts_with("application/xhtml+xml")) {
                set_status("暂不支持此文件类型");
                busy = false;
                dirty = true;
                co_return pxa::Result<void>{};
            }
            set_status("HTTP " + std::to_string(code) + " · 正在读取");
            dirty = true;
            // Declared encodings only need enough bytes to recognize a BOM.
            // Do not hold a short streamed paragraph until 1 KiB arrives.
            const std::size_t prescan_bytes = !plain && !rss && !wml &&
                !browser::content_charset(mime) ? 1024 : 3;
            std::size_t prefix_size = 0;
            while (prefix_size < prescan_bytes) {
                auto n = co_await response->body.read_some(std::span{receive}.subspan(prefix_size, prescan_bytes - prefix_size));
                if (!n) co_return std::unexpected(n.error());
                if (!*n) break;
                if (!first) first = millis() - begin;
                received += *n;
                prefix_size += *n;
            }
            auto prefix = std::string_view(reinterpret_cast<const char *>(receive.data()), prefix_size);
            const auto encoding_type = browser::sniff_encoding(mime, prefix, !plain && !rss && !wml);
            if (encoding_type == browser::TextEncoding::unsupported) {
                set_status("暂不支持此页面的文字编码");
                busy = false; dirty = true;
                co_return pxa::Result<void>{};
            }
            // An external package asset adds no mapped code/linear data or
            // permanent cache. Allocate only for a legacy page, release before
            // layout, and use the existing receive buffer for ordinary UTF-8.
            std::vector<std::byte> legacy_map;
            if (encoding_type == browser::TextEncoding::gb18030) {
                set_status("正在读取中文编码表"); dirty = true;
                legacy_map.resize(browser::gb_map_bytes);
                for (std::size_t at = 0; at < legacy_map.size();) {
                    auto part = std::span{legacy_map}.subspan(at);
                    auto read = co_await context->assets().read("assets/gb18030.bin", at,
                        part.first(std::min<std::size_t>(4064, part.size())));
                    if (!read) co_return std::unexpected(read.error());
                    if (!read->bytes || read->total_bytes != legacy_map.size())
                        co_return std::unexpected(pxa::Error::protocol_error);
                    at += read->bytes;
                }
            }
            browser::Gb18030Decoder decoder(legacy_map);
            std::string converted;
            if (!legacy_map.empty()) converted.reserve(1545);
            if (!plain && !rss && !wml) browser::site_forms(document, url);
            browser::Parser parser(document);
            std::optional<browser::RssParser> feed;
            if (rss)
                feed.emplace(document);
            auto consume = [&](std::string_view chunk) {
                do {
                    auto part = chunk;
                    if (!legacy_map.empty()) {
                        part = chunk.substr(0, 512);
                        converted.clear(); decoder.feed(part, converted);
                        chunk.remove_prefix(part.size()); part = converted;
                    } else chunk = {};
                    if (feed) feed->feed(part);
                    else if (plain) parser.feed_plain(part);
                    else parser.feed(part);
                } while (!chunk.empty());
            };
            if (prefix.starts_with("\xef\xbb\xbf")) prefix.remove_prefix(3);
            consume(prefix);
            for (;;) {
                auto n = co_await response->body.read_some(receive);
                if (!n)
                    co_return std::unexpected(n.error());
                if (!*n)
                    break;
                if (!first)
                    first = millis() - begin;
                received += *n;
                auto chunk = std::string_view(reinterpret_cast<const char *>(receive.data()), *n);
                consume(chunk);
                if (millis() - last_progress >= 100) {
                    last_progress = millis();
                    layout_dirty = true;
                    set_status("已读取 " + std::to_string(received / 1024) + " KiB");
                }
                if (document.truncated)
                    break;
            }
            if (!legacy_map.empty()) {
                converted.clear(); decoder.finish(converted);
                if (feed) feed->feed(converted);
                else if (plain) parser.feed_plain(converted);
                else parser.feed(converted);
                // Legacy form encoding requires an encoder as well. Avoid
                // silently sending UTF-8 to servers expecting GBK/GB18030.
                for (auto &form : document.forms) form.supported = false;
                std::vector<std::byte>{}.swap(legacy_map);
                std::string{}.swap(converted);
            }
            if (feed)
                feed->finish();
            else
                parser.finish();
            layout_dirty = true;
            scroll = restore_scroll;
            rebuild();
            anchor(restore_scroll == 0);
            busy = false;
            set_status((document.truncated || page.truncated ? "内容已截断 · "
                      : document.links_limited              ? "部分链接已省略 · "
                      : code >= 400                        ? "HTTP " + std::to_string(code) + " · "
                                                           : "") +
                     std::to_string(received / 1024) + " KiB · " +
                     std::to_string(millis() - begin) + " ms");
            if (document.text.empty())
                set_status("页面无可读文字 · 可能依赖 JavaScript");
            char timing[320];
            std::snprintf(timing, sizeof(timing),
                          "BROWSER DONE status=%u head_ms=%llu first_ms=%llu total_ms=%llu "
                          "received=%zu text=%zu spans=%zu links=%zu",
                          code, static_cast<unsigned long long>(head),
                          static_cast<unsigned long long>(first),
                          static_cast<unsigned long long>(millis() - begin), received,
                          document.text.size(), document.spans.size(), document.links.size());
#ifdef PXA_BROWSER_TRACE
            // Structural diagnostics identify login/error views without
            // logging form values, response bodies, cookies or page titles.
            std::uint32_t title_id = 2166136261u;
            for (unsigned char c : document.title) title_id = (title_id ^ c) * 16777619u;
            const auto used = std::char_traits<char>::length(timing);
            std::snprintf(timing + used, sizeof(timing)-used, " forms=%zu fields=%zu title_id=%08x",
                          document.forms.size(), document.fields.size(), unsigned(title_id));
#endif
            log(timing);
#ifdef PXA_BROWSER_PROFILE
            profile.pending = true;
#endif
            dirty = true;
            co_return pxa::Result<void>{};
        }
        co_return std::unexpected(pxa::Error::protocol_error);
    }
    auto visible_begin() const {
        auto begin =
            std::lower_bound(page.pieces.begin(), page.pieces.end(), std::max(0, scroll - font * 2),
                             [](const auto &piece, int y) { return int(piece.y) < y; });
        // A tall image may start far above the viewport. Its next piece is
        // after the entire image, so at most one predecessor needs checking.
        if (begin != page.pieces.begin()) {
            auto previous = std::prev(begin);
            const auto &span = document.spans[previous->span];
            if (span.image >= 0 &&
                int(previous->y) + browser::Layout::image_size(document.images[span.image],
                                                               content.width, font)
                                       .second >
                    scroll)
                begin = previous;
        }
        return begin;
    }
    bool image_visible(int index) const {
        for (auto it = visible_begin();
             it != page.pieces.end() && int(it->y) < scroll + content.height; ++it)
            if (document.spans[it->span].image == index)
                return true;
        return false;
    }
    const pxa::Asset *cached_image(int index) const {
        for (const auto &item : images)
            if (item.index == index && item.asset)
                return &item.asset->image();
        return nullptr;
    }
    void begin_image(int index, bool manual = false) {
        if (index < 0 || std::size_t(index) >= document.images.size())
            return;
        if (!show_images || (!show_gif && browser::gif_url(document.images[index].source))) {
            set_status("图片加载已关闭 · 可在菜单开启"); dirty = true; return;
        }
        if (cached_image(index)) {
            // A linked picture remains a usable link after loading.
            if (manual)
                for (const auto &span : document.spans)
                    if (span.image == index && span.link >= 0) {
                        action(Action::link, span.link);
                        return;
                    }
            if (manual)
                preview_image(index);
            return;
        }
        if (busy)
            return;
        // An advance borrows its cache entry. Cancel before reusing any slot.
        if (animation_slot >= 0) stop();
        ImageCache *slot = nullptr;
        for (auto &item : images)
            if (item.index < 0) {
                slot = &item;
                break;
            }
        if (!slot)
            for (auto &item : images)
                if (!image_visible(item.index)) {
                    slot = &item;
                    break;
                }
        if (!slot && manual)
            slot = &images[0];
        if (!slot)
            return;
        if (slot->index >= 0)
            image_states[slot->index] = ImageState::unseen;
        slot->asset.reset();
        slot->index = index;
        image_states[index] = ImageState::loading;
        loading_image = index;
        busy = true;
        set_status("正在加载图片…");
        dirty = true;
        start(load_image(index, unsigned(slot - images.data())));
    }
    void load_visible_image() {
        if (!show_images || !foreground || busy || editing || menu || image_view.active() || press.active() ||
            url == "about:home")
            return;
        for (auto it = visible_begin();
             it != page.pieces.end() && int(it->y) < scroll + content.height; ++it) {
            int index = document.spans[it->span].image;
            if (index >= 0 && image_states[index] == ImageState::unseen &&
                (show_gif || !browser::gif_url(document.images[index].source))) {
                begin_image(index);
                return;
            }
        }
    }
    void preview_image(int index) {
        if (!cached_image(index))
            return;
        const int paused = loading_image;
        // Preview keeps the same cache entry: an in-flight frame can finish
        // safely. Only another download needs cancelling here.
        if (busy || paused >= 0) stop();
        if (paused >= 0)
            image_states[paused] = ImageState::unseen;
        if (!cached_image(index)) {
            begin_image(index);
            return;
        }
        image_view.open(index);
        reveal_status();
        scrollbar_until = 0;
        menu = false;
        dirty = true;
        char message[48];
        std::snprintf(message, sizeof(message), "BROWSER IMAGE VIEW index=%d", index);
        log(message);
    }
    pxa::Task<void> load_image(int index, unsigned slot) {
        auto result = co_await fetch_image(index, slot);
        loading_image = -1;
        busy = false;
        dirty = true;
        if (!result) {
            images[slot].index = -1;
            image_states[index] = ImageState::failed;
            set_status("图片未加载 · 轻点可重试");
            log("BROWSER IMAGE ERROR index=" + std::to_string(index) +
                " code=" + std::to_string(int(result.error())));
        }
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> fetch_image(int index, unsigned slot) {
        auto target = document.images[index].source;
        if (url.starts_with("https:") && target.starts_with("http:"))
            co_return std::unexpected(pxa::Error::denied);
        const auto begin = millis();
        static constexpr std::array<std::string_view, 4> wanted{"content-type", "content-encoding",
                                                                "location", "set-cookie"};
        std::array<pxa::NetHeaderView, 6> headers{
            {{"Accept", "image/png, image/jpeg);q=0.9, image/gif;q=0.8"},
             {"Accept-Encoding", "identity"},
             {"User-Agent", browser::user_agent(mobile_ua)},
             {"Save-Data", "on"}}};
        for (unsigned redirect = 0; redirect <= 5; ++redirect) {
            const auto cookie = cookies.request(target, millis() / 1000);
            headers[2].value = browser::site_user_agent(target, browser::user_agent(mobile_ua));
            std::size_t header_count = 4;
            if (!cookie.empty()) headers[header_count++] = {"Cookie", cookie};
            if (!(url.starts_with("https:") && target.starts_with("http:"))) {
                auto source = std::string_view(url);
                headers[header_count++] = {"Referer", source.substr(0, source.find_first_of("/?#", source.find("://")+3))};
            }
            auto exchange = co_await context->http().open({.url = browser::transport_url(target),
                                                           .headers = std::span{headers}.first(header_count),
                                                           .wanted_headers = wanted,
                                                           .options = {.buffer_bytes = 4096,
                                                                       .header_bytes = 1024,
                                                                       .max_body_bytes = 131072,
                                                                       .timeout_ms = 15000}},
                                                          permission);
            if (!exchange)
                co_return std::unexpected(exchange.error());
            auto response = co_await std::move(*exchange).response();
            if (!response)
                co_return std::unexpected(response.error());
            auto code = response->status_code;
            for (auto value : response->headers.values("set-cookie"))
                cookies.receive(target, value, millis() / 1000, std::time(nullptr));
            if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
                auto location = response->headers.first("location");
                if (!location || redirect == 5)
                    co_return std::unexpected(pxa::Error::protocol_error);
                auto next = browser::resolve(target, *location);
                if (!next)
                    co_return std::unexpected(pxa::Error::invalid_argument);
                if (target.starts_with("https:") && next->starts_with("http:"))
                    co_return std::unexpected(pxa::Error::denied);
                target = std::move(*next);
                continue;
            }
            if (code != 200)
                co_return std::unexpected(pxa::Error::protocol_error);
            auto encoding = response->headers.first("content-encoding");
            if (encoding && !pxa::http_header_equal(browser::trim(*encoding), "identity"))
                co_return std::unexpected(pxa::Error::unsupported);
            auto type = response->headers.first("content-type");
            auto mime =
                type ? browser::lowercase(browser::trim(type->substr(0, type->find(';')))) : "";
            if (!mime.empty() && mime != "image/png" && mime != "image/jpeg" && mime != "image/gif" &&
                mime != "application/octet-stream")
                co_return std::unexpected(pxa::Error::unsupported);
            if (!show_gif && mime == "image/gif") {
                image_states[index] = ImageState::failed;
                images[slot].index = -1; set_status("GIF 加载已关闭");
                co_return pxa::Result<void>{};
            }
            co_return co_await read_image(response->body, index, slot, begin);
        }
        co_return std::unexpected(pxa::Error::protocol_error);
    }
    pxa::Task<void> read_image(pxa::HttpExchange &body, int index, unsigned slot,
                               std::uint64_t begin) {
        auto decoded = co_await context->assets().images().decode(std::move(body),
            {.max_width = std::uint16_t(std::clamp(content.width, 1, 512)),
             .max_height = 512,
             .max_pixels = 32768,
             .max_encoded_bytes = 131072,
             .formats = show_gif ? pxa::ImageFormats::any :
                                  pxa::ImageFormats::png | pxa::ImageFormats::jpeg},
            {.max_pixels = 8192, .max_encoded_bytes = 65536});
        if (!decoded)
            co_return std::unexpected(decoded.error());
        const auto info = decoded->image().descriptor();
        document.images[index].width = info.source_width;
        document.images[index].height = info.source_height;
        images[slot].asset.emplace(std::move(*decoded));
        images[slot].next_frame = millis() + std::max(30u, images[slot].asset->frame_info().delay_ms);
        image_states[index] = ImageState::ready;
        layout_dirty = true;
        set_status("图片 " + std::to_string(info.source_width) + "×" + std::to_string(info.source_height));
        log("BROWSER IMAGE DONE index=" + std::to_string(index) +
            " received=" + std::to_string(info.source_bytes) + " pixels=" + std::to_string(info.width) +
            "x" + std::to_string(info.height) + " resident=" + std::to_string(info.resident_bytes) +
            " total_ms=" + std::to_string(millis() - begin));
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> advance_image(unsigned slot) {
        auto &image = images[slot];
        auto advanced = co_await image.asset->advance();
        animation_slot = -1;
        if (!advanced) {
            // A failed advance can have consumed the Host state. Release this
            // owner; the next explicit tap can retry the download safely.
            log("BROWSER GIF ERROR code=" + std::to_string(int(advanced.error())));
            image_states[image.index] = ImageState::failed;
            image.asset.reset();
            image.index = -1;
            body_dirty = true;
            co_return pxa::Result<void>{};
        }
        if (*advanced) {
            image.next_frame = millis() + std::max(30u, image.asset->frame_info().delay_ms);
            body_dirty = true;
#ifdef PXA_BROWSER_TRACE
            log("BROWSER GIF FRAME index=" + std::to_string(image.index) +
                " iteration=" + std::to_string(image.asset->frame_info().iteration) +
                " last=" + std::to_string(image.asset->frame_info().last));
#endif
        }
        co_return pxa::Result<void>{};
    }
    void animate_visible_images() {
        if (!foreground || !show_images || !show_gif || busy || editing || menu ||
            press.active() || animation_slot >= 0) return;
        for (unsigned slot = 0; slot < images.size(); ++slot) {
            auto &image = images[slot];
            if (!image.asset || image.asset->frame_info().last || millis() < image.next_frame ||
                (image_view.active() ? image_view.index != image.index : !image_visible(image.index)))
                continue;
            animation_slot = int(slot);
            auto started = context->foreground_tasks().start(advance_image(slot));
            if (!started) {
                animation_slot = -1;
                image.next_frame = millis() + 100; // bounded retry, no catch-up
            }
            break;
        }
    }
    void edit(int field = -1) {
        press.reset();
        image_view = {};
        editing = true;
        password_input.set(field >= 0 && document.fields[field].type == browser::FieldType::password);
        capture_volume_keys.set(false);
        volume_direction = 0;
        edit_field = field;
        menu = false;
        input.set(field >= 0 ? document.fields[field].value :
                  (url == "about:home" || field == -2 ? "" : url));
        sync_editor();
        dirty = true;
        focus_pending = true;
    }
    void finish_edit(bool accept) {
        if (!editing)
            return;
        std::string value = input.get();
        int field = edit_field;
        if (accept && field == -2) {
            auto target = browser::resolve({}, browser::trim(value), true);
            if (!target || target->find('?') != target->npos) {
                set_status("请输入网站地址 · 暂不保存含查询参数的网址");
                dirty = true;
                return;
            }
            if (std::ranges::find(bookmarks, *target) != bookmarks.end())
                set_status("网站已在主页");
            else if (bookmarks.size() >= 8) {
                set_status("网站已满 · 可打开网站后在菜单中移除");
                dirty = true;
                return;
            } else {
                bookmarks.push_back(std::move(*target));
                preferences_dirty = true;
                layout_dirty = true;
                set_status("已添加到主页");
            }
        }
        editing = false;
        focus_pending = false;
        edit_field = -1;
        (void)editor.hide_keyboard();
        sync_editor();
        dirty = true;
        input.set(""); // Do not keep the last password in a hidden editor.
        if (!accept)
            return;
        if (field == -2) {
            home();
            set_status("网站已保存到主页");
        } else if (field < 0) {
            auto target = browser::address_target(value, search_engine);
            if (target)
                navigate(std::move(*target));
            else
                set_status("请输入有效的网址或较短的搜索关键词");
        } else if (std::size_t(field) < document.fields.size()) {
            if (value.size() > 512) {
                set_status("表单文字最多 512 字节");
                return;
            }
            document.fields[field].value = std::move(value);
            set_status("已更新输入");
        }
    }
    pxa::Task<void> load_bookmarks() {
        auto file = co_await context->fs().open("bookmarks.txt", pxa::OpenMode::read);
        bookmarks_ready = true;
        if (!file) {
            co_return pxa::Result<void>{};
        }
        std::string bytes;
        std::array<std::byte, 512> chunk{};
        for (;;) {
            auto n = file->read(chunk);
            if (!n)
                co_return std::unexpected(n.error());
            if (!*n)
                break;
            if (bytes.size() + *n > 8448)
                break;
            bytes.append(reinterpret_cast<const char *>(chunk.data()), *n);
        }
        for (std::size_t at = 0; at < bytes.size() && bookmarks.size() < 8;) {
            auto end = bytes.find('\n', at);
            if (end == bytes.npos)
                break;
            auto line = std::string_view(bytes).substr(at, end - at);
            if (line.starts_with("#prefs ")) {
                int saved_font = 15, saved_engine = 0;
                auto tail = line.substr(7);
                auto [font_end, font_error] =
                    std::from_chars(tail.data(), tail.data() + tail.size(), saved_font);
                tail = browser::trim(tail.substr(std::size_t(font_end - tail.data())));
                auto [engine_end, engine_error] =
                    std::from_chars(tail.data(), tail.data() + tail.size(), saved_engine);
                if (font_error == std::errc{} && engine_error == std::errc{} &&
                    engine_end == tail.data() + tail.size()) {
                    body_font_dp = std::clamp(saved_font, 12, 24);
                    search_engine = saved_engine == 1 ? browser::SearchEngine::duckduckgo
                                                      : browser::SearchEngine::bing;
                    layout_dirty = true;
                }
            } else if (line.starts_with("#ua ")) {
                mobile_ua = line == "#ua mobile";
            } else if (line == "#images 0") {
                show_images = false;
            } else if (line == "#gif 0") {
                show_gif = false;
            } else if (line == "#volume-scroll 1") {
                volume_scroll = true;
            } else if (auto value = browser::resolve({}, line))
                bookmarks.push_back(std::move(*value));
            at = end + 1;
        }
        dirty = layout_dirty = true;
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> load_window() {
        auto value = co_await context->window().snapshot();
        if (!value)
            co_return std::unexpected(value.error());
        on_window_changed(*value);
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> save_bookmarks(std::string bytes) {
        auto file = co_await context->fs().open("bookmarks.tmp", pxa::OpenMode::write |
                                                                     pxa::OpenMode::create |
                                                                     pxa::OpenMode::truncate);
        if (!file) {
            bookmark_save = false;
            co_return std::unexpected(file.error());
        }
        auto remaining = std::as_bytes(std::span{bytes.data(), bytes.size()});
        while (!remaining.empty()) {
            auto n = file->write(remaining);
            if (!n || !*n) {
                bookmark_save = false;
                co_return std::unexpected(n ? pxa::Error::io_error : n.error());
            }
            remaining = remaining.subspan(*n);
        }
        auto result = file->close();
        if (result)
            result = co_await context->fs().replace("bookmarks.tmp", "bookmarks.txt");
        bookmark_save = false;
        co_return result;
    }
    void bookmark() {
        if (url == "about:home" || bookmark_save || !bookmarks_ready)
            return;
        if (url.find('?') != url.npos) {
            set_status("含查询参数的网址可能有隐私信息，暂不收藏");
            dirty = true;
            return;
        }
        auto it = std::ranges::find(bookmarks, url);
        if (it != bookmarks.end()) {
            bookmarks.erase(it);
            set_status("已移除书签");
        } else if (bookmarks.size() < 8) {
            bookmarks.push_back(url);
            set_status("已收藏");
        } else {
            set_status("书签已满 (8)");
            dirty = true;
            return;
        }
        preferences_dirty = true;
        dirty = layout_dirty = true;
    }
    pxa::Result<void> on_start(pxa::Context &ctx, std::span<const std::byte> config) {
        context = &ctx;
        if (auto value = decode_start_display(config))
            metrics = *value;
        auto error = [](void *self, pxa::Error e) noexcept { static_cast<App *>(self)->fail(e); };
        ctx.tasks().on_error(this, error);
        ctx.foreground_tasks().on_error(this, error);
        (void)ctx.window().fullscreen(pxa::WindowBarMode::hidden, pxa::WindowBarMode::transient);
        geometry();
#ifdef PXA_BROWSER_TRACE
        log("BROWSER METRICS width=" + std::to_string(metrics.width) +
            " height=" + std::to_string(metrics.height) +
            " density=" + std::to_string(metrics.density_q16) + " content=" + std::to_string(content.x) +
            "," + std::to_string(content.y) + "," + std::to_string(content.width) + "," +
            std::to_string(content.height) + " toolbar=" + std::to_string(toolbar_y));
#endif
        home();
        start(load_bookmarks(), false);
        if (std::string_view(PXA_BROWSER_START_URL).size())
            navigate(PXA_BROWSER_START_URL);
        return {};
    }
    void on_background(pxa::Context &) {
        capture_volume_keys.set(false);
        volume_direction = 0;
        scrollbar_dragging = false;
        scrollbar_until = 0;
        foreground = false;
        if (editing)
            finish_edit(false);
        stop();
        press.reset();
        // Background browsers keep no live GIF decoder. Foreground visibility
        // reloads those entries; ordinary static cached pictures remain valid.
        for (auto &image : images)
            if (image.asset && !image.asset->frame_info().last) {
                image_states[image.index] = ImageState::unseen;
                image.asset.reset();
                image.index = -1;
            }
    }
    void on_foreground(pxa::Context &) {
        foreground = true;
        dirty = true;
    }
    void on_window_changed(const pxa::WindowMetrics &window) {
        window_pending = false;
        system_insets = window.system_bar_insets;
        layout_dirty = true;
        dirty = true;
        press.reset();
        scrollbar_dragging = false;
    }
    pxa::BackAction on_back() {
        press.reset();
        scrollbar_dragging = false;
        if (exit_state == ExitState::queued)
            return pxa::BackAction::close;
        if (exit_state == ExitState::saving)
            return pxa::BackAction::stay;
        if (image_view.active()) {
            action(Action::image_close);
            return pxa::BackAction::stay;
        }
        if (editing) {
            finish_edit(false);
            return pxa::BackAction::stay;
        }
        if (menu) {
            if (menu_page == MenuPage::history) { menu_page = MenuPage::tools; menu_scroll = 0; }
            else menu = false;
            dirty = true;
            return pxa::BackAction::stay;
        }
        if (history.back()) {
            action(Action::back);
            return pxa::BackAction::stay;
        }
        return pxa::BackAction::close;
    }
    void on_error(pxa::Context &, pxa::Error e) { fail(e); }
    pxa::Result<bool> on_event(pxa::Context &, const pxa::Event &event) {
        if (event.is<DisplayMetrics>())
            if (auto value = decode_display_metrics(event)) {
                metrics = *value;
                layout_dirty = true;
                dirty = true;
                press.reset();
                scrollbar_dragging = false;
            }
        if (event.is<pxa::PermissionRevoked>()) {
            stop();
            permission.reset();
            set_status("网络权限已撤销");
        }
        return false;
    }
    void on_frame(pxa::Context &, pxa::game::FrameTick tick) {
        if (press.advance(tick.timestamp_us, press_options())) long_press();
        capture_volume_keys.set(volume_scroll && foreground && !editing &&
                                exit_state == ExitState::none);
        if (!capture_volume_keys.get()) volume_direction = 0;
        if (volume_direction && millis() >= volume_repeat_at) {
            scroll_by(volume_direction * std::max(font, content.height - 2 * font));
            volume_repeat_at = millis() + 180;
        }
        // ESP publishes the first window snapshot after on_start returns.
        // A metrics event usually arrives first; query once if it has not.
        if (window_pending) {
            window_pending = false;
            start(load_window(), false);
        }
        if (preferences_dirty && bookmarks_ready && !bookmark_save) {
            preferences_dirty = false;
            bookmark_save = true;
            std::string bytes = "#prefs " + std::to_string(body_font_dp) + " " +
                                std::to_string(int(search_engine)) + "\n";
            if (mobile_ua)
                bytes += "#ua mobile\n";
            if (volume_scroll)
                bytes += "#volume-scroll 1\n";
            if (!show_images) bytes += "#images 0\n";
            if (!show_gif) bytes += "#gif 0\n";
            for (auto &b : bookmarks) {
                bytes += b;
                bytes += '\n';
            }
            start(save_bookmarks(std::move(bytes)), false);
        }
        if (exit_state == ExitState::saving && bookmarks_ready &&
            !preferences_dirty && !bookmark_save) {
            exit_state = ExitState::queued;
            if (auto result = context->window().request_back(); !result)
                fail(result.error());
            return;
        }
        if (exit_state == ExitState::queued) return;
        if (scrollbar_until && !scrollbar_dragging && millis() >= scrollbar_until) {
            scrollbar_until = 0;
            body_dirty = true;
        }
        if (focus_pending && editing && editor.mounted()) {
            focus_pending = false;
            auto r = editor.show_keyboard();
            if (!r)
                fail(r.error());
        }
        if (layout_dirty) {
            rebuild();
            if (page.truncated)
                set_status("排版已达容量上限 · 可减小字号");
        }
        load_visible_image();
        animate_visible_images();
        if (status_until || (busy && foreground)) {
            const auto now = millis();
            if (busy && foreground) status_until = now + 4000;
            else if (now >= status_until) {
                status_until = 0;
                // Erase the overlay once. No layout change or periodic repaint.
                dirty = true;
            }
        }
        if ((dirty || body_dirty) && canvas.mounted()) {
#ifdef PXA_BROWSER_PROFILE
            const auto begin = micros(), submitted = profile.submit_us;
#endif
            render(!dirty);
            dirty = false;
            body_dirty = false;
#ifdef PXA_BROWSER_PROFILE
            profile.encode_us += micros() - begin - (profile.submit_us - submitted);
            ++profile.renders;
            profile.bytes += std::uint32_t(draw.bytes_used());
#endif
        }
#ifdef PXA_BROWSER_PROFILE
        if (profile.pending)
            report_profile();
#endif
    }
    void hit(PixelRect box, Action a, int index = 0) {
        if (hit_count < hits.size())
            hits[hit_count++] = {box, a, index};
    }
    CanvasRegion region(PixelRect pixels) const {
        return surface_to_canvas_region(pixels, metrics);
    }
    void rect(PixelRect pixels, std::uint32_t color, int radius = 0) {
        auto box = region(pixels);
        if (box.width > 0 && box.height > 0)
            draw.rect(
                box.x, box.y, box.width, box.height, color,
                std::uint16_t(std::clamp(surface_to_canvas_length(radius, metrics), 0, 65535)));
    }
    void clip(PixelRect pixels) { draw.clip(region(pixels)); }
    void line(int x1, int y1, int x2, int y2, std::uint32_t color, int stroke = 1) {
        draw.line(
            surface_to_canvas_coordinate(x1, metrics), surface_to_canvas_coordinate(y1, metrics),
            surface_to_canvas_coordinate(x2, metrics), surface_to_canvas_coordinate(y2, metrics),
            color, std::uint16_t(std::clamp(surface_to_canvas_length(stroke, metrics), 1, 65535)));
    }
    void label(PixelRect box, std::string_view text, std::uint32_t color, int pixels = 0,
               int align = 0, bool middle = false) {
        const auto size = std::clamp(pixels ? pixels : font, 8, 128);
        if (middle) {
            const auto inset = std::max(0, (box.height - size * 5 / 4) / 2);
            box.y += inset;
            box.height -= inset;
        }
        if (box.width > 0 && box.height > 0)
            draw.text_sized(region(box), color, browser::line_prefix(text, box.width, size),
                            std::uint16_t(size), std::uint8_t(align));
    }
    int fitted_font(std::string_view text, int width, int wanted) const {
        while (wanted > 8 && browser::line_prefix(text, width, wanted).size() < text.size())
            --wanted;
        return wanted;
    }
    void button(PixelRect box, std::string_view text, Action a, int i = 0, int align = 1) {
        const bool enabled =
            (a != Action::back || history.back()) && (a != Action::forward || history.forward());
        const bool active =
            (a == Action::menu && menu) || (a == Action::home && url == "about:home" && !menu);
        rect(box, !enabled ? 0xf0f3f7ff : active ? 0xdbeaf7ff : 0xe8eef3ff, dp(6));
        auto text_box = box;
        text_box.x += dp(4);
        text_box.width = std::max(1, text_box.width - dp(8));
        const auto wanted = std::min(font, dp(13));
        // URLs may be arbitrarily long. Clip them at a readable size instead
        // of shrinking an entire bookmark to the smallest available font.
        label(text_box, text, enabled ? 0x234765ff : 0x9baab7ff,
              (a == Action::bookmark_open || a == Action::history_open) ? wanted : fitted_font(text, text_box.width, wanted),
              align, true);
        if (enabled) {
            if (menu) {
                const int bottom = std::min(box.y + box.height, content.y + content.height);
                box.y = std::max(box.y, content.y);
                box.height = std::max(0, bottom - box.y);
            }
            if (box.height > 0) hit(box, a, i);
        }
    }
    void navigation_button(PixelRect box, Action action) {
        const bool enabled = (action != Action::back || history.back()) &&
                             (action != Action::forward || history.forward()) &&
                             (action != Action::image_less || image_view.zoom > 1) &&
                             (action != Action::image_more || image_view.zoom < 8);
        const bool active = (action == Action::menu && menu) ||
                            (action == Action::home && url == "about:home" && !menu);
        const auto background = !enabled ? 0xf0f3f7ff : active ? 0xdbeaf7ff : 0xe8eef3ff;
        const auto color = enabled ? 0x234765ff : 0x9baab7ff;
        rect(box, background, dp(6));
        const int size = std::max(8, std::min(dp(18), std::min(box.width, box.height) - dp(10)));
        const int x = box.x + box.width / 2, y = box.y + box.height / 2, r = size / 2;
        const int stroke = std::max(2, dp(2));
        auto segment = [&](int x1, int y1, int x2, int y2) { line(x1, y1, x2, y2, color, stroke); };
        switch (action) {
        case Action::back:
        case Action::image_close:
        case Action::forward: {
            const int direction = action == Action::forward ? 1 : -1;
            const int tip = x + direction * r / 2, shoulder = x - direction * r / 2;
            segment(shoulder, y - r * 3 / 4, tip, y);
            segment(tip, y, shoulder, y + r * 3 / 4);
            break;
        }
        case Action::home:
            segment(x - r, y - r / 4, x, y - r);
            segment(x, y - r, x + r, y - r / 4);
            segment(x - r * 2 / 3, y - r / 4, x - r * 2 / 3, y + r);
            segment(x + r * 2 / 3, y - r / 4, x + r * 2 / 3, y + r);
            segment(x - r * 2 / 3, y + r, x + r * 2 / 3, y + r);
            rect({x - stroke / 2, y + r / 3, stroke, r * 2 / 3}, color);
            break;
        case Action::reload:
            if (busy) {
                rect({x - r * 2 / 3, y - r * 2 / 3, r * 4 / 3, r * 4 / 3}, color, dp(1));
            } else {
                // Build the arc and arrow in the same Canvas coordinates:
                // LVGL's radius is the outer edge, so the arrow must meet
                // the stroke centreline. End at 315 degrees with a clockwise
                // tangent and a clear right-angle head at the upper right.
                const auto width = surface_to_canvas_length(stroke, metrics);
                const int radius = std::max(3, surface_to_canvas_length(r * 8 / 9, metrics));
                const int cx = surface_to_canvas_coordinate(x, metrics) - width / 2;
                const int cy = surface_to_canvas_coordinate(y, metrics) + width / 2;
                const int diagonal = ((radius - width / 2) * 181 + 128) / 256;
                const int end_x = cx + diagonal, end_y = cy - diagonal;
                const int tip_x = end_x + width, tip_y = end_y + width;
                const int wing = std::max(4, radius * 2 / 3);
                draw.arc(cx, cy, radius, 45, 315, color, std::uint16_t(width));
                draw.line(end_x, end_y, tip_x, tip_y, color, std::uint16_t(width));
                draw.line(tip_x - wing, tip_y, tip_x, tip_y, color, std::uint16_t(width));
                draw.line(tip_x, tip_y - wing, tip_x, tip_y, color, std::uint16_t(width));
            }
            break;
        case Action::menu:
            for (int dx : {-r * 2 / 3, 0, r * 2 / 3})
                rect({x + dx - stroke / 2, y - stroke / 2, stroke, stroke}, color, stroke);
            break;
        case Action::image_more:
        case Action::image_less:
            segment(x - r * 2 / 3, y, x + r * 2 / 3, y);
            if (action == Action::image_more)
                segment(x, y - r * 2 / 3, x, y + r * 2 / 3);
            break;
        case Action::image_fit:
            for (int dx : {-1, 1})
                for (int dy : {-1, 1}) {
                    segment(x + dx * r, y + dy * r, x + dx * r / 3, y + dy * r);
                    segment(x + dx * r, y + dy * r, x + dx * r, y + dy * r / 3);
                }
            break;
        default:
            break;
        }
        if (enabled)
            hit(box, action);
    }
    int home_row() const { return std::max(dp(32), font * 2); }
    int home_height() const {
        return dp(8) + int(bookmarks.size() + 2 + (bookmarks.size() < 8)) * (home_row() + dp(6));
    }
    int scroll_height() const {
        if (!menu) return page.height;
        const auto count = menu_page == MenuPage::history ? history.entries().size() + 1 :
            menu_page == MenuPage::directory ? browser::sites.size() + 1 : bookmarks.size() + 9;
        return int(count) * (dp(32) + dp(3));
    }
    int &scroll_position() { return menu ? menu_scroll : scroll; }
    browser::Scrollbar scrollbar() const {
        return {content.y, content.height, scroll_height(), menu ? menu_scroll : scroll, dp(20)};
    }
    void reveal_scrollbar() {
        if (!scrollbar_until) body_dirty = true;
        scrollbar_until = millis() + 900;
    }
    void scroll_by(int amount) {
        if (image_view.active()) {
            if (const auto *image = cached_image(image_view.index)) {
                const auto info = image->descriptor();
                body_dirty |= image_view.pan(0, -amount, info.width, info.height,
                                              content.width, content.height);
            }
            return;
        }
        const auto next = int(std::clamp(std::int64_t(scroll_position()) + amount,
                                          std::int64_t(0), std::int64_t(scrollbar().range())));
        if (next != scroll_position()) {
            scroll_position() = next;
            body_dirty = true;
            reveal_scrollbar();
        }
    }
    void volume_key(VolumeKey key) {
        if (!capture_volume_keys.get()) return;
        if (key == VolumeKey::up_released || key == VolumeKey::down_released) {
            const int released = key == VolumeKey::up_released ? -1 : 1;
            if (volume_direction == released) volume_direction = 0;
            return;
        }
        const int direction = key == VolumeKey::up ? -1 : 1;
        if (volume_direction == direction) return; // Hardware auto-repeat is handled by the loop.
        volume_direction = direction;
        volume_repeat_at = millis() + 350;
        scroll_by(direction * std::max(font, content.height - 2 * font));
    }
    pxa::Result<void> present(std::span<const CanvasRegion> damage = {}) {
#ifdef PXA_BROWSER_PROFILE
        const auto begin = micros();
#endif
        auto result = canvas.present(draw, damage);
#ifdef PXA_BROWSER_PROFILE
        const auto elapsed = std::uint32_t(micros() - begin);
        profile.submit_us += elapsed;
        profile.max_submit_us = std::max(profile.max_submit_us, elapsed);
#endif
        return result;
    }
    void render_home() {
        int y = content.y + dp(8) - scroll;
        const int height = home_row();
        PixelRect box{content.x + pad, y, std::max(1, content.width - 2 * pad), height};
        rect(box, 0x156ca8ff, dp(8));
        label(box, "搜索或输入网址", 0xffffffff, std::min(font, dp(15)), 1, true);
        auto touch = [&](PixelRect area, Action action, int index = 0) {
            const int bottom = std::min(area.y + area.height, content.y + content.height);
            area.y = std::max(area.y, content.y);
            area.height = std::max(0, bottom - area.y);
            hit(area, action, index);
        };
        touch(box, Action::search);
        y += height + dp(6);
        box.y = y;
        rect(box, 0xe8eef3ff, dp(8));
        label(box, "网站导航", 0x234765ff, std::min(font, dp(14)), 1, true);
        touch(box, Action::directory);
        y += height + dp(6);
        for (std::size_t i = 0; i < bookmarks.size(); ++i, y += height + dp(6)) {
            if (y + height <= content.y || y >= content.y + content.height) continue;
            box.y = y;
            rect(box, 0xe8eef3ff, dp(8));
            std::string_view title = bookmarks[i];
            if (auto scheme = title.find("://"); scheme != title.npos) title.remove_prefix(scheme + 3);
            if (title.ends_with('/')) title.remove_suffix(1);
            label({box.x + dp(8), box.y, box.width - dp(16), box.height}, title,
                  0x234765ff, std::min(font, dp(14)), 0, true);
            touch(box, Action::bookmark_open, int(i));
        }
        if (bookmarks.size() < 8 && y < content.y + content.height) {
            box.y = y;
            label(box, "+ 添加网站", 0x718599ff, std::min(font, dp(13)), 1, true);
            touch(box, Action::add_site);
        }
    }
    void render(bool body_only) {
        draw.reset();
        hit_count = 0;
        bool dense = false;
        rect({0, 0, int(metrics.width), int(metrics.height)}, 0xf7f9fcff);
        if (content.width < dp(80) || content.height < dp(20) || toolbar.width < dp(80)) {
            label(content, "屏幕空间不足", 0x17334aff);
            (void)present();
            return;
        }
        rect(address, 0xe8eef3ff, dp(7));
        if (!editing) {
            label(
                {address.x + dp(8), address.y, std::max(1, address.width - dp(16)), address.height},
                url == "about:home" ? "搜索或输入网址…" : std::string_view(url), 0x365675ff,
                std::min(font, dp(13)), 0, true);
            hit(address, Action::address);
        } else
            button({address.x + address.width - dp(30), address.y, dp(30), address.height}, "完成",
                   Action::address);
        if (image_view.active()) {
            if (const auto *image = cached_image(image_view.index)) {
                const auto info = image->descriptor();
                const auto box = image_view.clamp(info.width, info.height,
                                                   content.width, content.height);
                rect(content, 0xe1e8efff, dp(4));
                clip(content);
                draw.image(region({content.x + box[0], content.y + box[1], box[2], box[3]}),
                           image->handle());
                draw.pop_clip();
            }
        } else if (menu) {
            clip(content);
            int y = content.y - menu_scroll, h = dp(32);
            int half = (content.width - pad) / 2;
            if (menu_page == MenuPage::history) {
                button({content.x, y, content.width, h}, "清空浏览历史", Action::history_clear);
                y += h + dp(3);
                auto entries = history.entries();
                for (std::size_t n = entries.size(); n > 0; --n, y += h + dp(3))
                    if (y + h > content.y && y < content.y + content.height) {
                        const auto &entry = entries[n-1];
                        std::string title = n-1 == history.current() ? "● " : "";
                        title += entry.url == "about:home" ? "主页" : entry.url;
                        button({content.x, y, content.width, h}, title, Action::history_open, int(n-1), 0);
                    }
            } else if (menu_page == MenuPage::directory) {
                label({content.x, y, content.width, h}, "轻量网站 · 已验证入口", 0x718599ff, std::min(font, dp(13)), 1, true);
                y += h + dp(3);
                for (std::size_t n=0; n<browser::sites.size(); ++n, y += h + dp(3))
                    if (y+h > content.y && y < content.y+content.height)
                        button({content.x,y,content.width,h}, browser::sites[n].name, Action::site_open, int(n), 0);
            } else {
            button({content.x, y, half, h}, url == "about:home" ? "添加网站" :
                std::ranges::find(bookmarks, url) != bookmarks.end() ? "移除网站" : "收藏此页",
                Action::bookmark);
            button({content.x + half + pad, y, half, h}, "退出浏览器", Action::exit);
            y += h + dp(3);
            button({content.x, y, half, h}, "字号 −", Action::font_less);
            button({content.x + half + pad, y, half, h}, "字号 +", Action::font_more);
            y += h + dp(3);
            button({content.x, y, content.width, h},
                   search_engine == browser::SearchEngine::bing ? "搜索：必应" : "搜索：DuckDuckGo",
                   Action::search_engine);
            y += h + dp(3);
            button({content.x, y, content.width, h},
                   mobile_ua ? "页面模式：手机兼容" : "页面模式：轻量", Action::page_mode);
            y += h + dp(3);
            button({content.x, y, content.width, h},
                   volume_scroll ? "音量键翻页：开" : "音量键翻页：关", Action::volume_keys);
            y += h + dp(3);
            button({content.x, y, content.width, h}, "浏览历史", Action::history_list);
            y += h + dp(3);
            button({content.x, y, content.width, h}, show_images ? "加载图片：开" : "加载图片：关", Action::image_setting);
            y += h + dp(3);
            button({content.x, y, content.width, h}, show_gif ? "加载 GIF：开" : "加载 GIF：关", Action::gif_setting);
            y += h + dp(3);
            button({content.x, y, content.width, h}, "清除登录会话", Action::clear_sessions);
            y += h + dp(3);
            for (std::size_t i = 0; i < bookmarks.size(); ++i, y += h + dp(3))
                if (y + h > content.y && y < content.y + content.height)
                    button({content.x, y, content.width, h}, bookmarks[i], Action::bookmark_open,
                           int(i), 0);
            if (bookmarks.empty() && y + font < content.y + content.height)
                label({content.x, y, content.width, font * 2}, "暂无书签", 0x7a8c9cff);
            }
            draw.pop_clip();
        } else {
            clip(content);
            if (url == "about:home") {
                render_home();
            } else {
                auto begin = visible_begin();
                unsigned displayed = 0;
                for (auto it = begin; it != page.pieces.end() && displayed < 56; ++it) {
                    auto &p = *it;
                    int y = content.y + int(p.y) - scroll;
                    if (y >= content.y + content.height)
                        break;
                    auto &span = document.spans[p.span];
                    if (span.image >= 0) {
                        auto [w, h] = browser::Layout::image_size(document.images[span.image],
                                                                  content.width, font);
                        PixelRect box{content.x + (content.width - w) / 2, y, w, h};
                        if (y + h <= content.y)
                            continue;
                        if (const auto *image = cached_image(span.image))
                            draw.image(region(box), image->handle());
                        else {
                            // Clip the placeholder too: a long comic must not
                            // cover navigation or the system's gesture area.
                            rect(box, 0xe8eef3ff, dp(4));
                            int top = std::max(y, content.y),
                                bottom = std::min(y + h, content.y + content.height);
                            label({box.x + dp(4), top, box.width - dp(8),
                                   std::min(bottom - top, font * 2)},
                                  std::string_view(document.text).substr(span.offset, span.size),
                                  0x587088ff, font, 1, true);
                            if (bottom - top >= font * 3)
                                label({box.x + dp(4), top + font * 2, box.width - dp(8),
                                       font * 3 / 2},
                                      image_states[span.image] == ImageState::loading
                                          ? "正在加载…"
                                          : "轻点加载图片",
                                      0x1469acff, std::min(font, dp(13)), 1, true);
                        }
                        PixelRect touch = box;
                        touch.y = std::max(y, content.y);
                        touch.height = std::min(y + h, content.y + content.height) - touch.y;
                        hit(touch, Action::image, span.image);
                        ++displayed;
                        continue;
                    }
                    int size = browser::Layout::font(span.style, font);
                    if (y + size * 3 / 2 <= content.y)
                        continue;
                    if (draw.bytes_used() + p.size + (span.field >= 0 ? 300 : 100) > 3000) {
                        dense = true;
                        break;
                    }
                    PixelRect box{content.x + p.x, y,
                                  std::min(content.width - int(p.x), std::max(int(p.width), size)),
                                  size * 3 / 2};
                    auto value = std::string_view(document.text).substr(p.offset, p.size);
                    auto color = span.link >= 0                        ? 0x1469acff
                                 : span.style == browser::Style::muted ? 0x7a8c9cff
                                                                       : 0x21394dff;
                    if (span.field >= 0 && std::size_t(span.field) < document.fields.size()) {
                        auto &field = document.fields[span.field];
                        box.width = content.width;
                        box.height = size * 2;
                        const bool submit = field.type == browser::FieldType::submit;
                        rect(box, submit ? 0x156ca8ff : 0xe8eef3ff, dp(5));
                        if (submit)
                            label(box, value, 0xffffffff, size, 1, true);
                        else
                            label({box.x + dp(4), box.y, box.width / 2 - dp(4), box.height}, value,
                                  color, size, 0, true);
                        if (field.type == browser::FieldType::text && !field.value.empty())
                            label({box.x + box.width / 2, box.y, box.width / 2, box.height},
                                  field.value, 0x1469acff, size, 0, true);
                        if (!field.value.empty() && field.type != browser::FieldType::submit &&
                            field.type != browser::FieldType::hidden &&
                            field.type != browser::FieldType::text) {
                            // Field value is visible in the editor; the document row
                            // shows a compact indicator without duplicating secrets.
                            label({box.x + box.width - dp(36), box.y, dp(36), box.height},
                                  field.type == browser::FieldType::checkbox
                                      ? (field.checked ? "✓" : "○")
                                      : "…",
                                  0x1469acff, size, 1, true);
                        }
                        PixelRect touch = box;
                        touch.y = std::max(touch.y, content.y);
                        touch.height =
                            std::min(box.y + box.height, content.y + content.height) - touch.y;
                        hit(touch, Action::field, span.field);
                    } else {
                        label(box, value, color, size);
                        if (span.link >= 0) {
                            line(box.x, box.y + size + dp(2), box.x + box.width,
                                 box.y + size + dp(2), 0x91b9d9ff);
                            PixelRect touch = box;
                            touch.y = std::max(touch.y, content.y);
                            touch.height =
                                std::min(box.y + box.height, content.y + content.height) - touch.y;
                            hit(touch, Action::link, span.link);
                        }
                    }
                    ++displayed;
                }
                if (displayed == 56)
                    dense = true;
            }
            draw.pop_clip();
        }
        if (!image_view.active() && (scrollbar_dragging || scrollbar_until) &&
            scrollbar().range() > 0) {
            const auto bar = scrollbar();
            const int width = dp(scrollbar_dragging ? 6 : 3);
            if (scrollbar_dragging)
                rect({content.x + content.width - width, content.y, width, content.height},
                     0xdbe4edff, width / 2);
            rect({content.x + content.width - width, bar.thumb_top(), width, bar.thumb_height()},
                 scrollbar_dragging ? 0x365675ff : 0x8da7bcff, width / 2);
        }
        char image_status[96]{};
        if (image_view.active()) {
            const auto &image = document.images[image_view.index];
            std::snprintf(image_status, sizeof(image_status), "%u×%u · 适配×%d · 拖动查看",
                          unsigned(image.width), unsigned(image.height), image_view.zoom);
        }
        const bool status_below = status_box.y >= toolbar.y + toolbar.height;
        if (status_until && !editing && !menu && status_box.height) {
            if (!status_below) rect(status_box, 0xe8eef3ff, dp(4));
            label({status_box.x + dp(4), status_box.y,
                   std::max(0, status_box.width - dp(8)), status_box.height},
                  image_view.active() ? std::string_view(image_status)
                  : dense ? "本屏内容过密 · 可增大字号或滚动" : std::string_view(status),
                  0x718599ff, std::min(font, dp(10)), 1, true);
        }
        constexpr std::array actions{Action::back, Action::forward, Action::home, Action::reload,
                                     Action::menu};
        constexpr std::array image_actions{Action::image_close, Action::image_less,
                                           Action::image_fit, Action::image_more};
        const std::span<const Action> buttons = image_view.active()
            ? std::span<const Action>(image_actions) : std::span<const Action>(actions);
        for (std::size_t i = 0; i < buttons.size(); ++i) {
            int x = toolbar.x + int(i) * toolbar.width / int(buttons.size());
            int w = int(i + 1) * toolbar.width / int(buttons.size()) - x + toolbar.x;
            navigation_button({x + dp(1), toolbar_y, std::max(1, w - dp(2)), row}, buttons[i]);
        }
        // The Host retains a complete command list, so future exposure and
        // system overlays still repaint correctly. During a drag only the
        // content, scrollbar and status line need rasterization. No backing
        // bitmap or second command cache is required.
        const std::array damage{
            region({std::min(content.x, toolbar.x), content.y,
                    std::max(content.x + content.width, toolbar.x + toolbar.width) -
                        std::min(content.x, toolbar.x), toolbar_y - content.y}),
            region(status_box)};
        auto result = present(body_only ? std::span<const CanvasRegion>(damage).first(status_below ? 2 : 1)
                                        : std::span<const CanvasRegion>{});
        if (!result)
            log("BROWSER DRAW ERROR " + std::to_string(int(result.error())));
    }
    void action(Action a, int i = 0) {
        switch (a) {
        case Action::address:
            if (editing)
                finish_edit(true);
            else
                edit();
            break;
        case Action::back:
        case Action::forward: {
            history.position(scroll);
            auto entry = history.move(a == Action::back ? -1 : 1);
            if (entry)
                navigate(entry->url, false, {}, false, entry->scroll);
            break;
        }
        case Action::home:
            history.position(scroll);
            home();
            break;
        case Action::reload:
            if (busy)
                stop();
            else
                navigate(url, false);
            break;
        case Action::menu:
            if (editing)
                finish_edit(false);
            menu = !menu;
            menu_page = MenuPage::tools;
            menu_scroll = 0;
            scrollbar_until = 0;
            dirty = true;
            break;
        case Action::history_list:
            history.position(scroll);
            menu_page = MenuPage::history; menu_scroll = 0; break;
        case Action::history_open:
            history.position(scroll);
            if (i >= 0) if (auto entry = history.select(std::size_t(i)))
                navigate(entry->url, false, {}, false, entry->scroll);
            break;
        case Action::history_clear:
            history.clear(url, scroll); menu_scroll = 0; set_status("浏览历史已清空"); break;
        case Action::directory:
            menu = true; menu_page = MenuPage::directory; menu_scroll = 0; break;
        case Action::site_open:
            if (i >= 0 && std::size_t(i) < browser::sites.size()) {
                history.position(scroll); navigate(std::string(browser::sites[i].url));
            }
            break;
        case Action::clear_sessions:
            if (busy) stop();
            cookies.clear();
            for (auto &field : document.fields)
                if (field.type == browser::FieldType::password) field.value.clear();
            set_status("登录会话已清除"); break;
        case Action::image_setting:
        case Action::gif_setting:
            stop();
            if (a == Action::image_setting) show_images = !show_images;
            else show_gif = !show_gif;
            clear_images(); preferences_dirty = true;
            set_status("图片设置已保存"); break;
        case Action::add_site:
            if (bookmarks_ready && !bookmark_save) edit(-2);
            break;
        case Action::exit:
            stop();
            if (editing) finish_edit(false);
            exit_state = ExitState::saving;
            set_status("正在退出…");
            break;
        case Action::bookmark:
            if (url == "about:home") {
                if (bookmarks_ready && !bookmark_save) edit(-2);
            } else bookmark();
            break;
        case Action::font_less:
        case Action::font_more:
            body_font_dp = std::clamp(body_font_dp + (a == Action::font_more ? 1 : -1), 12, 24);
            layout_dirty = true;
            preferences_dirty = true;
            break;
        case Action::search:
            edit();
            break;
        case Action::link:
            if (i >= 0 && std::size_t(i) < document.links.size()) {
                auto target = browser::wml_target(document, document.links[i].target);
                if (target) {
                    history.position(scroll);
                    navigate(std::move(*target));
                } else set_status("此链接需要暂不支持的 WML 操作");
            }
            break;
        case Action::image:
            begin_image(i, true);
            break;
        case Action::image_close:
            image_view = {};
            set_status("已返回网页 · 长按图片可预览");
            break;
        case Action::image_fit:
            image_view.fit();
            reveal_status();
            break;
        case Action::image_less:
        case Action::image_more:
            if (const auto *image = cached_image(image_view.index)) {
                const auto info = image->descriptor();
                image_view.resize(a == Action::image_more ? 1 : -1, info.width, info.height,
                                  content.width, content.height);
                reveal_status();
            }
            break;
        case Action::bookmark_open:
            if (i >= 0 && std::size_t(i) < bookmarks.size()) {
                auto target = bookmarks[i];
                history.position(scroll);
                navigate(std::move(target));
            }
            break;
        case Action::search_engine:
            search_engine = search_engine == browser::SearchEngine::bing
                                ? browser::SearchEngine::duckduckgo
                                : browser::SearchEngine::bing;
            set_status("搜索引擎已切换");
            preferences_dirty = true;
            dirty = true;
            break;
        case Action::page_mode:
            mobile_ua = !mobile_ua;
            set_status("下次加载生效 · 简化效果由网站决定");
            preferences_dirty = true;
            break;
        case Action::volume_keys:
            volume_scroll = !volume_scroll;
            volume_direction = 0;
            preferences_dirty = true;
            set_status(volume_scroll ? "音量 + 向上 · 音量 − 向下 · 长按连续翻页" : "音量键恢复调节音量");
            break;
        case Action::field:
            if (i >= 0 && std::size_t(i) < document.fields.size()) {
                auto &field = document.fields[i];
                if (field.type == browser::FieldType::submit) {
                    auto request = browser::submit(document, field.form, i);
                    if (request)
                        navigate(std::move(request->url), true, std::move(request->body),
                                 request->post);
                    else {
                        set_status("该表单暂不支持");
                        dirty = true;
                    }
                } else if (field.type == browser::FieldType::checkbox) {
                    field.checked = !field.checked;
                    dirty = true;
                } else
                    edit(i);
            }
            break;
        }
        dirty = true;
    }
    PressOptions press_options() const {
        return {.long_press=std::chrono::microseconds{hold_enabled ? 500000 : 0}, .slop=6};
    }
    const Hit* hit_at(int x, int y) const {
        for (std::size_t n=hit_count; n>0; --n)
            if (hits[n-1].box.contains(x,y) &&
                (!menu || hits[n-1].action==Action::address ||
                 (hits[n-1].action>=Action::back && hits[n-1].action<=Action::menu) || content.contains(x,y)))
                return &hits[n-1];
        return nullptr;
    }
    void long_press() {
#ifdef PXA_BROWSER_TRACE
        log("BROWSER LONG action=" + std::to_string(int(pressed_action)) +
            " index=" + std::to_string(pressed_index));
#endif
        if (pressed_action==Action::bookmark_open && !bookmark_save && bookmarks_ready &&
            pressed_index>=0 && std::size_t(pressed_index)<bookmarks.size()) {
            bookmarks.erase(bookmarks.begin()+pressed_index);
            preferences_dirty=layout_dirty=dirty=true;
            set_status("已移除网站");
        } else if (pressed_action==Action::image && cached_image(pressed_index)) {
            preview_image(pressed_index);
        } else if (pressed_action>=Action::back && pressed_action<=Action::menu) {
            constexpr std::array labels{"返回", "前进", "首页", "刷新页面", "更多工具"};
            const auto label=pressed_action==Action::reload && busy ? "停止加载" :
                labels[unsigned(pressed_action)-unsigned(Action::back)];
            (void)context->window().toast(label);
        } else if (pressed_action>=Action::image_close && pressed_action<=Action::image_more) {
            constexpr std::array labels{"返回网页", "缩小", "适合屏幕", "放大"};
            (void)context->window().toast(labels[unsigned(pressed_action)-unsigned(Action::image_close)]);
        }
    }
    void pointer(const CanvasPointer &e) {
        int x = canvas_to_surface_coordinate(e.x, metrics),
            y = canvas_to_surface_coordinate(e.y, metrics);
#ifdef PXA_BROWSER_TRACE
        log("BROWSER POINTER phase=" + std::to_string(e.phase) + " x=" + std::to_string(x) +
            " y=" + std::to_string(y));
#endif
        press.rebase(e.generation); // Page filters stale Canvas events before dispatch.
        if (e.action() == PointerPhase::down) {
            if (press.active()) return;
            (void)press.update(e, press_options());
            if (!press.owns(e)) return;
            last_x = x; last_y = y;
            pressed_index = -1; hold_enabled = false;
            if (const auto* selected=hit_at(x,y)) {
                pressed_action=selected->action; pressed_index=selected->index;
                hold_enabled=(pressed_action==Action::bookmark_open && !bookmark_save && bookmarks_ready) ||
                    (pressed_action==Action::image && cached_image(pressed_index)) ||
                    (pressed_action>=Action::back && pressed_action<=Action::menu) ||
                    (pressed_action>=Action::image_close && pressed_action<=Action::image_more);
            }
            scrollbar_dragging = !image_view.active() && content.contains(x, y) &&
                x >= content.x + content.width - dp(14) && scrollbar().range() > 0;
            if (scrollbar_dragging) {
                const auto bar = scrollbar();
                hold_enabled = false;
                last_y = y >= bar.thumb_top() && y < bar.thumb_top() + bar.thumb_height()
                    ? y - bar.thumb_top() : bar.thumb_height() / 2;
                scroll_position() = bar.position_at(y, last_y);
                body_dirty = true;
                reveal_scrollbar();
            }
#ifdef PXA_BROWSER_PROFILE
            profile = {};
#endif
            return;
        }
        if (!press.owns(e)) return;
        const bool held = press.long_pressed();
        const auto down_x=canvas_to_surface_coordinate(press.start_x(),metrics);
        const auto down_y=canvas_to_surface_coordinate(press.start_y(),metrics);
        const auto result=press.update(e,press_options());
        if (scrollbar_dragging) {
            if (e.action() == PointerPhase::move) {
                const int next = scrollbar().position_at(y, last_y);
                body_dirty |= next != scroll_position();
                scroll_position() = next;
            }
            if (e.ended()) {
                scrollbar_dragging = false;
                body_dirty = true;
            }
            reveal_scrollbar();
            return;
        }
        if (e.action() == PointerPhase::move) {
            if (held) return;
            if (press.dragged() && content.contains(down_x, down_y)) {
                if (image_view.active()) {
                    if (const auto *image = cached_image(image_view.index)) {
                        const auto info = image->descriptor();
                        body_dirty |= image_view.pan(x - last_x, y - last_y, info.width,
                                                     info.height, content.width, content.height);
                    }
                } else if (menu) {
                    const int next = std::clamp(
                        menu_scroll + last_y - y, 0,
                        std::max(0, scroll_height() - content.height));
                    if (next != menu_scroll) reveal_scrollbar();
                    body_dirty |= next != menu_scroll;
                    menu_scroll = next;
                } else {
                    const int next = std::clamp(scroll + last_y - y, 0,
                                                std::max(0, page.height - content.height));
                    if (next != scroll) reveal_scrollbar();
                    body_dirty |= next != scroll;
                    scroll = next;
                }
            }
            last_x = x;
            last_y = y;
            return;
        }
        if (e.ended()) {
#ifdef PXA_BROWSER_PROFILE
            profile.pending = true;
#endif
            if (result==PressResult::long_press) { long_press(); return; }
            if (result!=PressResult::tap) return;
            if (const auto* selected=hit_at(x,y); selected && selected->action==pressed_action &&
                selected->index==pressed_index) {
#ifdef PXA_BROWSER_TRACE
                log("BROWSER HIT action=" + std::to_string(int(selected->action)) +
                    " index=" + std::to_string(selected->index));
#endif
                action(selected->action,selected->index);
            }
        }
    }
};
void Pointer::operator()(const CanvasPointer &event) const { app->pointer(event); }
} // namespace
PXA_APPLICATION(App)
