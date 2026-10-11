#include <pxa/libcpp_diagnostics.hpp>
#include "reader_state.hpp"
#include "reader_catalog.hpp"
#include "reader_paging.hpp"
#include <pxa/ui_theme.hpp>
#include <cstdio>
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/canvas.hpp>
#include <pxa/ui_display.hpp>
#include <pxa/ui_widgets.hpp>
#include <pxa/ui_refresh.hpp>

#if defined(READER_VALIDATE) && defined(__wasm__)
extern "C" unsigned char __stack_low[];
inline void reader_stack_probe(bool initialize) {
  constexpr std::size_t bytes = 8192;
  volatile unsigned char* memory = __stack_low;
  for (std::size_t i = 0; i < bytes; ++i)
    if (initialize) memory[i] = 0xa5;
    else if (memory[i] != 0xa5) std::abort();
}
#else
inline void reader_stack_probe(bool) {}
#endif

using namespace pxa::ui;
struct ReaderApp;
struct ReaderPointer {
  ReaderApp *app;
  void operator()(const CanvasPointer &) const;
};
struct ReaderApp {
  static constexpr pxa::game::LoopOptions loop_options{50000, 50, 1};
  enum Screen {
    shelf,
    search,
    sources,
    reading,
    read_menu,
    settings,
    toc,
    source_add,
    book_menu,
    remove_confirm
  };
  enum Action {
    home,
    discover,
    source_list,
    options,
    open_book,
    next_page,
    previous_page,
    menu,
    contents,
    download,
    font_less,
    font_more,
    gap_change,
    theme_change,
    auto_toggle,
    auto_speed,
    back,
    search_go,
    source_select,
    add_source,
    import_source,
    source_toggle,
    remove_source,
    list_previous,
    list_next,
    open_chapter,
    bookmark,
    remove_book,
    cancel_job,
    search_classic,
    text_edit,
    manage_book,
    confirm_remove
  };
  struct ControlAction {
    ReaderApp* app = nullptr;
    Action action = home;
    int arg = 0;
    void operator()() const;
    void operator()(const CanvasPointer&) const;
  };
  struct Controls {
    static constexpr Capacity capacity{48, 2, 16};
    ReaderApp* app;
    std::array<ControlAction, 16> callbacks{};
    template<class Owner> [[gnu::noinline]] bool render(Owner& owner, std::uint32_t parent);
  };
  std::size_t visible_rows = 1;
  State<std::uint32_t> ui_revision{0};
  State<std::string> header_text{""}, subtitle_text{""};
  int header_width = 0, header_font = 12, subtitle_width = 0, subtitle_font = 12;
  bool controls_dirty = true, rendered_busy = false;
  UiAppearance system_appearance;
  std::string_view header_title() const {
    if (screen == reading && !state.shelf.empty())
      return reading_notice && notice_deadline ? std::string_view(status) : std::string_view(state.shelf[active].title);
    return screen == shelf ? "书页" : screen == search ? "发现" : screen == sources ? "书源管理" :
           screen == source_add ? "添加书源" : screen == settings ? "阅读偏好" : screen == toc ? "章节目录" :
           screen == read_menu ? "正在阅读" : screen == book_menu ? "管理书籍" : "移除书籍";
  }
  std::string_view subtitle() const {
    if (notice_deadline || busy) return status;
    if ((screen == read_menu || screen == book_menu || screen == remove_confirm) && !state.shelf.empty()) return state.shelf[active].title;
    return screen == shelf ? "轻点阅读 · 长按管理书籍" : screen == search ? "搜索书名或作者 · 轻点切换书源" :
           screen == sources ? "免费经典文学，也支持自定义书源" : screen == settings ? "找到最舒服的阅读方式" : std::string_view(status);
  }

  pxa::Context *context = nullptr;
  reader::SavedState state;
  pxa::ui::DisplayMetrics metrics;
  CanvasRef canvas;
  CanvasCommands<reader::page_command_capacity> draw;
  pxa::Permission network_permission;
  State<std::string> input{""};
  Screen screen = shelf, return_screen = shelf;
  TextInputRef input_ref;
  State<TextInputBox> input_box{TextInputBox{{}, false, Font::body, 0x253430ff}};
  std::vector<reader::Book> results;
  std::vector<reader::Chapter> chapters;
  std::array<std::uint32_t, 24> previous{};
  reader::PageNumbers page_numbers;
  std::size_t previous_count = 0;
  reader::Page page;
  std::string page_text, status = "正在加载书架…", next_results;
  std::size_t active = 0, source_index = 0, list_start = 0;
  bool dirty = true, busy = false, page_busy = false, save_busy = false,
       save_pending = false, cancelled = false, reading_partial = false,
       state_ready = false, system_dark = false, publishing = false, sized_text_supported = false,
       reading_notice = false;
  std::uint64_t auto_deadline = 0, frame_time = 0, notice_deadline = 0;
  std::uint32_t next_offset = 0, cached_bytes = 0;
  std::uint64_t press_time = 0;
  int pressed_book = -1, press_x = 0, press_y = 0;
  std::string search_query;
  unsigned reading_theme() const { return state.settings.theme == 3 ? (system_dark ? 2 : 0) : state.settings.theme; }
  void cancel_network() {
    context->foreground_tasks().cancel();
    busy = false; page_busy = false; publishing = false; auto_deadline = 0;
    page_numbers.running=false;
    save();
  }
  std::array<std::byte, 4096> net_packet{};
  std::array<pxa::NetHeader, 3> net_headers{};
  static constexpr std::size_t page_read_bytes = 16384;
  std::array<std::byte, 4096> stream_buffer{};
  float native_density() const { return float(metrics.density_q16) / 65536.f; }
  // Rendering, loading and background indexing share this geometry builder;
  // duplicating its density/curvature setup in each coroutine grows code maps.
  [[gnu::noinline]] reader::Layout layout() const { return reader::Layout(metrics, screen != reading); }
  void reset_page_numbers(std::uint32_t origin=0) {
    if (page_numbers.go_previous) page_busy=false;
    page_numbers.reset(origin);
  }
  auto view() {
    return Stack(Canvas(canvas).on_pointer(ReaderPointer{this}),
                 Refresh(ui_revision, [this] { return Controls{this}; }),
                 TextInput(input, input_ref).box(input_box).max_bytes(512)
                     .single_line().on_submit([this] {
                       (void)input_ref.hide_keyboard();
                       dirty = true; controls_dirty = true;
                     }))
        .fill()
        .fill_height();
  }
  std::uint32_t system_color(Color role) const {
    if (system_appearance.generation && static_cast<unsigned>(role) < system_appearance.colors.size())
      return system_appearance.colors[static_cast<unsigned>(role)];
    return role == Color::background ? (system_dark ? 0x171f25ff : 0xf5f7f8ff)
                                    : (system_dark ? 0xe4edf0ff : 0x20343bff);
  }
  std::uint32_t body_background() const {
    return state.settings.theme == 3 ? system_color(Color::background) :
        state.settings.theme == 2 ? 0x171f25ff : state.settings.theme == 1 ? 0xe5eedfff : 0xf5f0e4ff;
  }
  std::uint32_t body_foreground() const {
    return state.settings.theme == 3 ? system_color(Color::text) :
        state.settings.theme == 2 ? 0xe4edf0ff : 0x20343bff;
  }
  pxa::Task<void> load_theme() {
    if (auto value = co_await context->theme().get()) {
      system_appearance = *value; system_dark = value->dark;
      dirty = true; controls_dirty = true;
    }
    co_return pxa::Result<void>{};
  }
  void log(std::string_view message) {
    (void)context->log().write(pxa::LogLevel::info, message);
  }
  void notice(std::string value, bool reading_feedback = false) {
    status = std::move(value);
    reading_notice = reading_feedback;
#ifdef READER_VALIDATE
    if (context) log("READER-STATUS " + status);
#endif
    notice_deadline = frame_time + 4000000;
    dirty = true;
  }
  void error(std::string_view operation, pxa::Error e) {
    log("READER-ERROR operation=" + std::string(operation) +
        " code=" + reader::number_string(int(e)));
    if (e == pxa::Error::quota_exceeded) {
      notice("存储配额不足，请移除已下载书籍", true);
      return;
    }
    notice(std::string(operation) + "失败 (" + reader::number_string(int(e)) + ")", true);
  }
  void start(pxa::Task<void> task, bool foreground = true) {
    auto r = foreground ? context->foreground_tasks().start(std::move(task))
                        : context->tasks().start(std::move(task));
    if (!r) {
      busy = false;
      page_busy = false;
      error("任务", r.error());
    }
  }
  void change(Screen value) {
    if (value != search && value != source_add)
      (void)input_ref.hide_keyboard();
    if (screen == source_add && value != source_add) input.set(search_query);
    screen = value;
    controls_dirty = true;
    pressed_book = -1;
    list_start = 0;
    auto_deadline = 0;
    dirty = true;
  }
  std::string filename(const reader::Book &book, bool partial = false) const {
    return "b" + reader::cache_key(book.url.empty() ? book.toc : book.url) +
           (partial ? ".part" : ".txt");
  }
  pxa::Result<void> on_start(pxa::Context &ctx,
                             std::span<const std::byte> config) {
    reader_stack_probe(true);
    context = &ctx;
    busy = true;
    auto report = [](void *self, pxa::Error e) noexcept {
      auto app = static_cast<ReaderApp *>(self);
      app->busy = false;
      app->page_busy = false;
      app->error("任务", e);
    };
    ctx.tasks().on_error(this, report);
    ctx.foreground_tasks().on_error(this, report);
    auto display = decode_start_display(config);
    if (display)
      metrics = *display;
    auto capabilities = decode_start_ui_capabilities(config);
    if (!capabilities && capabilities.error() != pxa::Error::not_found)
      return std::unexpected(capabilities.error());
    if (auto appearance = decode_start_appearance(config)) system_dark = appearance->dark;
    sized_text_supported = capabilities && capabilities->supports(UiFeature::sized_text);
    (void)ctx.window().fullscreen(pxa::WindowBarMode::hidden,
                                  pxa::WindowBarMode::transient);
    if (!sized_text_supported)
      notice("请更新 Host 以支持可调字号");
    start(load_theme(), false);
    start(load_state(), false);
    return {};
  }
  void on_background(pxa::Context &) {
    (void)input_ref.hide_keyboard();
    auto_deadline = 0;
    state.settings.automatic = false;
    busy = false;
    page_busy = false;
    page_numbers.running=false;
    notice("已暂停");
    save();
  }
  void on_foreground(pxa::Context &) {
    auto_deadline = 0;
    dirty = true;
  }
  pxa::BackAction on_back() {
    if (screen == book_menu || screen == remove_confirm) { change(shelf); return pxa::BackAction::stay; }
    if (screen == shelf)
      return pxa::BackAction::close;
    if (screen == reading)
      change(read_menu);
    else if (screen == read_menu || screen == toc)
      change(reading);
    else if (screen == settings)
      change(return_screen == read_menu ? reading : shelf);
    else if (screen == source_add)
      change(sources);
    else
      change(shelf);
    return pxa::BackAction::stay;
  }
  void on_error(pxa::Context &, pxa::Error e) { error("界面", e); }
  pxa::Result<bool> on_event(pxa::Context &, const pxa::Event &event) {
    if (event.is<UiAppearance>()) {
      if (auto appearance = decode_appearance(event)) { system_appearance = *appearance; system_dark = appearance->dark; dirty = true; controls_dirty = true; }
    }
    if (event.is<DisplayMetrics>()) {
      auto value = decode_display_metrics(event);
      if (value) {
        metrics = *value;
        reset_page_numbers(page_numbers.origin);
        controls_dirty = true;
        if (auto capabilities = decode_ui_capabilities(event))
          sized_text_supported = capabilities->supports(UiFeature::sized_text);
        dirty = true;
        if (screen == reading)
          start(load_page());
      }
    }
    if (event.is<pxa::PermissionRevoked>() && pxa::decode_permission_revoked(event)) {
      context->foreground_tasks().cancel();
      network_permission.reset();
      busy = false;
      notice("网络权限已关闭，已下载内容仍可阅读");
    }
    return false;
  }
  void on_frame(pxa::Context &, pxa::game::FrameTick tick) {
    reader_stack_probe(false);
    frame_time = tick.timestamp_us;
    if (screen == reading && notice_deadline && frame_time >= notice_deadline) {
      notice_deadline = 0;
      dirty = true;
    }
    if (state.settings.automatic && screen == reading && !busy && !page_busy) {
      if (!auto_deadline)
        auto_deadline = tick.timestamp_us +
                        std::uint64_t(state.settings.auto_seconds) * 1000000;
      else if (tick.timestamp_us >= auto_deadline) {
        auto_deadline = 0;
        turn(1);
      }
    }
    if (dirty && canvas.mounted()) {
      render();
      dirty = false;
    }
    if (state_ready && screen==reading && page.count && !busy && (!page_busy || page_numbers.go_previous) &&
        !save_busy && !publishing && !page_numbers.running && !page_numbers.failed &&
        (page_numbers.go_previous || !page_numbers.finished || page_numbers.available!=cached_bytes ||
         page_numbers.current_offset!=state.shelf[active].position)) {
      page_numbers.running=true;
      auto started=context->foreground_tasks().start(index_book(true));
      if (!started) page_numbers.running=false;
    }
  }
  void save() {
    if (!state_ready)
      return;
    save_pending = true;
    if (!save_busy)
      start(save_state(), false);
  }
  pxa::Task<void> save_state() {
    save_busy = true;
    while (save_pending) {
      save_pending = false;
      auto text = state.encode();
      auto f = co_await context->fs().open(
          "reader-state.tmp", pxa::OpenMode::write | pxa::OpenMode::create |
                                  pxa::OpenMode::truncate);
      if (!f) {
        error("保存", f.error());
        break;
      }
      auto ok =
          co_await write_file(*f, std::as_bytes(std::span{text.data(), text.size()}));
      if (!close_file(*f)) break;
      if (!ok) {
        error("保存", ok.error());
        break;
      }
      auto moved = co_await context->fs().replace("reader-state.tmp",
                                                 "reader-state.json");
      if (!moved) {
        error("保存", moved.error());
        break;
      }
    }
    save_busy = false;
    co_return pxa::Result<void>{};
  }
  [[gnu::noinline]] bool close_file(pxa::File& file) {
    if (!file) return true;
    if (auto closed = file.close(); !closed) { error("关闭缓存",closed.error()); return false; }
    return true;
  }
  pxa::Task<void> write_file(pxa::File &file,
                                      std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
      auto n =
          file.write(bytes.first(std::min<std::size_t>(1024, bytes.size())));
      if (!n)
        co_return std::unexpected(n.error());
      if (!*n)
        co_return std::unexpected(pxa::Error::io_error);
      bytes = bytes.subspan(*n);
      // Flash writes are synchronous Host I/O. Return to the event loop after
      // each bounded block so input and the execution watchdog stay responsive.
      auto yielded = co_await context->clock().yield();
      if (!yielded) co_return std::unexpected(yielded.error());
    }
    co_return pxa::Result<void>{};
  }
  pxa::Task<void> load_state() {
    bool loaded = false;
    auto f = co_await context->fs().open("reader-state.json");
    if (f) {
      std::string text;
      auto buffer = std::span{stream_buffer}.first(1024);
      while (text.size() <= 32768) {
        auto n = f->read(buffer);
        if (!n) {
          error("读取书架", n.error());
          break;
        }
        if (!*n)
          break;
        text.append(reinterpret_cast<const char *>(buffer.data()), *n);
      }
      if (!close_file(*f)) co_return pxa::Result<void>{};
      loaded = state.decode(text);
      if (!loaded) {
        auto backup = co_await context->fs().rename("reader-state.json",
                                                    "reader-state.bad");
        notice(backup ? "书架记录无效，已备份为 reader-state.bad"
                      : "书架记录无效，请检查存储");
      }
    }
    if (!loaded && state.shelf.empty())
      state.shelf.push_back({"林间书屋", "书页原创 · CC0", "sample:book", "", 0,
                             std::uint32_t(reader::sample_text.size()), true});
    state_ready = true;
    if (!loaded) save();
    busy = false;
    if (status == "正在加载书架…")
      status = "欢迎来到书页";
    dirty = true;
    co_return pxa::Result<void>{};
  }
  pxa::Task<pxa::NetResponse>
  request(std::string_view url, std::uint32_t maximum,
          std::span<const pxa::NetHeaderView> headers = {},
          std::span<const std::string_view> wanted = {}) {
    // A cold scan may retain an unread suffix in the shared I/O scratch.
    // Stop it before any network response can reuse that storage.
    page_numbers.stop_scan=true;
    if (!network_permission) {
      auto permission = co_await context->permissions().acquire(
          "net.client", pxa::web_network_scope);
      if (!permission)
        co_return std::unexpected(permission.error());
      network_permission = std::move(*permission);
    }
    pxa::NetRequest req;
    req.url = url;
    req.max_response_bytes = maximum;
    req.timeout_ms = 8000;
    req.headers = headers;
    req.wanted_headers = wanted;
    auto response = co_await context->net().request(req, network_permission,
                                                    net_packet, net_headers);
    // The app owns the authority for the full request/body lifetime.
    if (!response)
      co_return std::unexpected(response.error());
    co_return std::move(*response);
  }
  pxa::Task<std::string> fetch(std::string url,
                               std::uint32_t maximum = reader::max_json_bytes) {
    auto response = co_await request(url, maximum);
    if (!response)
      co_return std::unexpected(response.error());
    if (response->status_code != 200)
      co_return std::unexpected(pxa::Error::unavailable);
    std::string text;
    text.reserve(std::min<std::size_t>(response->body_length, maximum));
    auto bytes = std::span{stream_buffer};
    if (response->body)
      while (true) {
        auto n = response->body.read(bytes);
        if (!n)
          co_return std::unexpected(n.error());
        if (!*n)
          break;
        if (text.size() + *n > maximum)
          co_return std::unexpected(pxa::Error::limit_exceeded);
        text.append(reinterpret_cast<const char *>(bytes.data()), *n);
      }
    co_return text;
  }
  pxa::Task<void> search_books(std::string explicit_url = {}) {
    if (source_index >= state.sources.size()) source_index = 0;
    busy = true;
    controls_dirty = true;
    notice("正在搜索…");
    auto source = state.sources[source_index];
    if (!source.enabled) {
      busy = false;
      notice("该书源已停用，请切换或启用书源");
      co_return pxa::Result<void>{};
    }
    auto key = input.get();
    if (source.kind == reader::SourceKind::classics || source.kind == reader::SourceKind::gitenberg) {
      results = reader::search_classics(key, source.kind);
      next_results.clear(); list_start = 0; busy = false;
      notice(results.empty() ? "经典书目中未找到，可切换网络书源" : "即时找到 " + reader::number_string(results.size()) + " 部经典作品");
#ifdef READER_VALIDATE
      log("READER-SEARCH results=" + reader::number_string(results.size()) + " instant=1");
#endif
      co_return pxa::Result<void>{};
    }
    auto url = explicit_url.empty() ? source.url : explicit_url;
    if (explicit_url.empty()) {
      if (source.kind == reader::SourceKind::gutenberg && key == "西游记")
        key = "西遊記";
      if (source.kind == reader::SourceKind::gutenberg && key == "红楼梦")
        key = "紅樓夢";
      if (source.kind == reader::SourceKind::gutenberg && key == "三国演义")
        key = "三國志演義";
      auto marker = url.find("{{key}}");
      if (marker != url.npos)
        url.replace(marker, 7, reader::percent(key));
      else if (source.kind == reader::SourceKind::gutenberg && !key.empty())
        url += "&search=" + reader::percent(key);
    }
    auto body = co_await fetch(url);
    busy = false;
    if (!body) {
      notice(body.error() == pxa::Error::denied ? "请允许网络权限，或切换即时搜索书源" : "书源暂时无响应，可重试或切换即时搜索书源");
      log("READER-SEARCH failed=" + reader::number_string(int(body.error())));
      co_return pxa::Result<void>{};
    }
    if (!reader::Json(*body).valid()) {
      notice("书源返回了无效 JSON");
      co_return pxa::Result<void>{};
    }
    results = reader::parse_books(*body, source);
    if (source.kind == reader::SourceKind::json &&
        source.url.find("{{key}}") == source.url.npos && !key.empty())
      std::erase_if(results, [&](auto &b) {
        return b.title.find(key) == b.title.npos &&
               b.author.find(key) == b.author.npos;
      });
    next_results = reader::Json(*body).get("next").string(512).value_or("");
    list_start = 0;
    notice(results.empty()
               ? "没有找到作品，可尝试繁体书名"
               : "找到 " + reader::number_string(results.size()) + " 部作品");
#ifdef READER_VALIDATE
    log("READER-SEARCH results=" + reader::number_string(results.size()));
#endif
    co_return pxa::Result<void>{};
  }
  void open(std::size_t index, bool from_results) {
    if (index >= (from_results ? results.size() : state.shelf.size())) return;
    cancel_network();
    if (from_results) {
      auto book = results[index];
      auto found =
          std::find_if(state.shelf.begin(), state.shelf.end(), [&](auto &b) {
            return b.url == book.url && b.toc == book.toc;
          });
      if (found == state.shelf.end()) {
        if (state.shelf.size() >= reader::max_shelf) {
          notice("书架已满，请先移除一本书");
          return;
        }
        state.shelf.push_back(book);
        active = state.shelf.size() - 1;
        save();
      } else
        active = std::size_t(found - state.shelf.begin());
    } else
      active = index;
    chapters.clear();
    previous_count = 0;
    reset_page_numbers();
    page = {};
    page_text.clear();
    busy = true;
    change(reading);
    start(open_current());
  }
  pxa::Task<void> open_current() {
    auto &book = state.shelf[active];
    reading_partial = !book.complete;
    cached_bytes = 0;
    if (book.url.starts_with("sample:")) {
      busy = false;
      cached_bytes=book.total;
      index_text(reader::sample_text, 0);
      start(load_page());
      co_return pxa::Result<void>{};
    }
    auto file = co_await context->fs().stat(filename(book));
    if (file) {
      busy = false;
      book.total = std::uint32_t(file->size);
      book.complete = true;
      cached_bytes = book.total;
      reading_partial = false;
      start(index_book());
      start(load_page());
    } else {
      busy = false;
      book.complete = false;
      reading_partial = true;
      auto partial = co_await context->fs().stat(filename(book, true));
      if (partial && partial->size) {
        cached_bytes = std::uint32_t(partial->size);
        book.total = std::max(book.total, cached_bytes);
        start(index_book());
        start(load_page());
        notice("已打开缓存，菜单中可继续下载");
      } else start(download_current());
    }
    co_return pxa::Result<void>{};
  }
  [[gnu::noinline]] void emit_chapter(std::string_view title, std::uint32_t offset) {
    if (chapters.size() < reader::max_chapters) {
      if (chapters.empty() && offset<page_read_bytes &&
          reader::hardwrapped_source(state.shelf[active].url) && page_numbers.origin!=offset &&
          (!state.shelf[active].position || state.shelf[active].position>=offset))
        reset_page_numbers(offset);
      chapters.push_back({reader::shorten(title,192), "", offset});
      if (screen == toc) controls_dirty = true;
    }
  }
  [[gnu::noinline]] void index_changed() {
    dirty = true;
#ifdef READER_VALIDATE
    log("READER-INDEX chapters=" + reader::number_string(chapters.size())+
        " cached=" + reader::number_string(cached_bytes));
#endif
  }
  void index_text(std::string_view data, std::uint32_t base) {
    reader::ChapterScanner scanner;
    auto emit = [&](std::string_view title,std::uint32_t offset) { emit_chapter(title,base+offset); };
    scanner.append(data,emit); scanner.finish(emit);
  }
  pxa::Task<void> index_book(bool numbers_only=false) {
    auto& numbers=page_numbers;
    numbers.running=true;
    numbers.stop_scan=false;
    struct Guard {
      ReaderApp& app;
      ~Guard() {
        auto& value=app.page_numbers; value.running=false;
        if (value.go_previous && value.failed) { value.go_previous=false; app.page_busy=false; }
      }
    } guard{*this};
    const auto limit=cached_bytes;
    const bool previous=numbers.go_previous;
    if (numbers.available!=limit) { numbers.reset(numbers.origin); numbers.available=limit; numbers.go_previous=previous; }
    const auto generation=numbers.generation;
    const auto l=reader::Layout(metrics,false);
    const auto settings=state.settings;
    const bool focus=numbers.finished || previous;
    const bool reflow=reader::hardwrapped_source(state.shelf[active].url);
    if (numbers_only && focus) numbers.locate(previous ? state.shelf[active].position-1 : state.shelf[active].position);
    auto f=co_await context->fs().open(filename(state.shelf[active],reading_partial));
    if (!f) { numbers.failed=true; co_return pxa::Result<void>{}; }
    reader::ChapterScanner scanner;
    if (!numbers_only) {
      chapters.clear();
      if (screen==toc) controls_dirty=true;
    } else {
      auto seek=co_await f->seek(numbers.offset ? numbers.offset-1 : 0);
      if (!seek) { numbers.failed=true; co_return pxa::Result<void>{}; }
      if (numbers.offset) {
        std::byte preceding{}; auto n=f->read(std::span{&preceding,1});
        if (!n || *n!=1) { numbers.failed=true; co_return pxa::Result<void>{}; }
        if (!numbers.cursor.y) numbers.cursor.paragraph_start=preceding==std::byte{'\n'} || preceding==std::byte{'\r'};
      }
    }
    auto emit=[&](std::string_view title,std::uint32_t offset) { emit_chapter(title,offset); };
    std::uint32_t read_offset=numbers_only ? numbers.offset : 0;
    std::size_t pending=0;
    unsigned blocks=0;
    while (read_offset<limit && !busy && !numbers.stop_scan && (!numbers_only || generation==numbers.generation)) {
      auto n=f->read(std::span{stream_buffer}.subspan(pending,std::min<std::size_t>(stream_buffer.size()-pending,limit-read_offset)));
      if (!n || !*n) { numbers.failed=true; break; }
      if (!numbers_only) scanner.append({reinterpret_cast<const char*>(stream_buffer.data()+pending),*n},emit);
      read_offset+=*n;
      std::size_t used=pending+*n;
      bool counted_end=false;
      if (generation==numbers.generation) {
        auto text=std::string_view(reinterpret_cast<const char*>(stream_buffer.data()),pending+*n);
        bool invalid=false; text=text.substr(0,reader::utf8_prefix(text,invalid));
        if (invalid) { numbers.failed=true; break; }
        const auto base=read_offset-std::uint32_t(pending+*n);
        const auto skip=std::min<std::size_t>(text.size(),numbers.offset-base);
        const auto target=previous ? state.shelf[active].position-1 : state.shelf[active].position;
        const auto before=numbers.current_offset;
        used=skip+numbers.consume(text.substr(skip),l,settings,target,reflow,read_offset>=limit,focus);
        if (before!=numbers.current_offset && screen==reading) dirty=true;
        if (!used) { numbers.failed=true; break; }
        if (!focus && read_offset>=limit && used==text.size()) {
          numbers.finished=true; numbers.exact_total=state.shelf[active].complete;
          counted_end=true;
          dirty=true;
#ifdef READER_VALIDATE
          log("READER-PAGES total="+reader::number_string(numbers.total)+" bytes="+reader::number_string(limit)+
              " complete="+reader::number_string(numbers.exact_total));
#endif
        }
      }
      if (counted_end && generation==numbers.generation) break;
      const auto target=previous ? state.shelf[active].position-1 : state.shelf[active].position;
      if (numbers_only && focus && numbers.current_offset==target) {
        if (previous) {
          auto& book=state.shelf[active]; book.position=numbers.current_start;
          numbers.current_offset=book.position; numbers.current_boundary=true;
          numbers.go_previous=false; page_busy=false; save(); start(load_page());
        }
        break;
      }
      if (numbers.failed) break;
      const auto remain=pending+*n-used;
      std::memmove(stream_buffer.data(),stream_buffer.data()+used,remain);
      pending=remain;
      if (++blocks%8==0 && !numbers_only) index_changed();
      if (!co_await context->clock().yield()) break;
    }
    if (!numbers_only && !reading_partial && scanner.offset==limit) scanner.finish(emit);
    (void)close_file(*f);
    if (!numbers_only) index_changed();
    co_return pxa::Result<void>{};
  }
  pxa::Task<void> load_page() {
    if (page_busy || publishing || state.shelf.empty())
      co_return pxa::Result<void>{};
    page_busy = true;
    auto &book = state.shelf[active];
    bool paragraph_start = book.position == 0;
    if (book.url.starts_with("sample:")) {
      paragraph_start = !book.position || reader::sample_text[std::min<std::size_t>(book.position,reader::sample_text.size())-1] == '\n';
      page_text = std::string(reader::sample_text.substr(
          std::min<std::size_t>(book.position, reader::sample_text.size())));
    } else {
      auto f = co_await context->fs().open(filename(book, reading_partial));
      if (!f) {
        page_busy = false;
        error("读取", f.error());
        co_return pxa::Result<void>{};
      }
      auto seek = co_await f->seek(book.position ? book.position-1 : 0);
      if (!seek) {
        page_busy = false;
        error("翻页", seek.error());
        co_return pxa::Result<void>{};
      }
      if (book.position) {
        std::byte previous_byte{};
        auto n = f->read(std::span{&previous_byte,1});
        if (!n || *n != 1) { page_busy = false; notice("阅读位置超出已缓存正文"); co_return pxa::Result<void>{}; }
        paragraph_start = previous_byte == std::byte{'\n'} || previous_byte == std::byte{'\r'};
      }
      page_text.resize(page_read_bytes);
      auto bytes = std::as_writable_bytes(std::span{page_text.data(), page_text.size()});
      std::size_t used = 0;
      while (used < bytes.size()) {
        auto n = f->read(bytes.subspan(used));
        if (!n) {
          page_busy = false;
          error("读取", n.error());
          co_return pxa::Result<void>{};
        }
        if (!*n)
          break;
        used += *n;
      }
      page_text.resize(used);
      if (!close_file(*f)) { page_busy = false; co_return pxa::Result<void>{}; }
      bool invalid = false;
      auto valid = reader::utf8_prefix(page_text, invalid);
      page_text.resize(valid);
      if (invalid) {
        page_text.clear();
        page_busy = false;
        page = {};
        notice("正文编码无效，仅支持 UTF-8 文本");
        co_return pxa::Result<void>{};
      }
    }
    if (!book.position && reader::hardwrapped_source(book.url)) {
      reader::ChapterScanner scanner;
      std::uint32_t first_heading = UINT32_MAX;
      scanner.append(page_text,[&](std::string_view,std::uint32_t offset) { if (first_heading == UINT32_MAX) first_heading = offset; });
      if (first_heading != UINT32_MAX && first_heading) {
        if (page_numbers.origin!=first_heading) reset_page_numbers(first_heading);
        book.position = first_heading;
        page_text.erase(0,first_heading); paragraph_start = true; save();
      }
    }
    {
      const reader::Layout reading_layout(metrics, false);
      page = reader::paginate(page_text, reading_layout, state.settings, paragraph_start, reader::hardwrapped_source(book.url));
      next_offset = book.position + std::uint32_t(page.consumed);
      reader::compact_page_text(page_text,page);
      reader::balance_page(page,page_text,reading_layout,state.settings);
    }
    if (book.url.starts_with("sample:")) {
      page_numbers.reset(); page_numbers.available=cached_bytes;
      page_numbers.consume(reader::sample_text,reader::Layout(metrics,false),state.settings,book.position,false,true,false);
      page_numbers.finished=page_numbers.exact_total=true;
#ifdef READER_VALIDATE
      log("READER-PAGES total="+reader::number_string(page_numbers.total)+" bytes="+reader::number_string(cached_bytes)+" complete=1");
#endif
    }
    if (book.position==page_numbers.origin) {
      page_numbers.current=1; page_numbers.current_offset=book.position;
      page_numbers.current_boundary=true;
    }
    page_busy = false;
    dirty = true;
    auto_deadline = 0;
    if (page.invalid_utf8)
      notice("正文编码不是有效 UTF-8，暂无法阅读");
#ifdef READER_VALIDATE
    log("READER-PAGE offset=" + reader::number_string(book.position) + " next=" +
        reader::number_string(next_offset) + " lines=" + reader::number_string(page.count));
#endif
    co_return pxa::Result<void>{};
  }
  void turn(int direction) {
    if (page_busy || state.shelf.empty())
      return;
    auto &b = state.shelf[active];
    const auto old_position=b.position;
    const bool numbered=page_numbers.current_offset==old_position && page_numbers.current_boundary;
    if (direction > 0) {
      if (next_offset <= b.position || (b.complete && next_offset >= b.total) || (!b.complete && next_offset >= cached_bytes)) {
        state.settings.automatic = false;
        notice(b.complete ? "已经读到书末" : "请等待正文缓存后再翻页", true);
        return;
      }
      if (previous_count == previous.size()) {
        std::move(previous.begin() + 1, previous.end(), previous.begin());
        --previous_count;
      }
      previous[previous_count++] = b.position;
      b.position = next_offset;
    } else {
      if (!previous_count && b.position>page_numbers.origin) {
        if (b.url.starts_with("sample:")) {
          page_numbers.locate(b.position-1);
          page_numbers.consume(reader::sample_text.substr(page_numbers.offset),reader::Layout(metrics,false),state.settings,b.position-1,false,true,true);
          b.position=page_numbers.current_start; save(); start(load_page());
          return;
        }
        page_numbers.go_previous=true; page_busy=true; dirty=true;
        return;
      }
      if (previous_count)
        b.position = previous[--previous_count];
    }
    if (b.position!=old_position) {
      if (numbered && direction>0) {
        page_numbers.current=std::uint32_t(int(page_numbers.current)+direction);
        page_numbers.current_offset=b.position;
      } else page_numbers.current_offset=UINT32_MAX;
    }
    save();
    start(load_page());
  }
  pxa::Task<void> import_remote_source() {
    if (busy)
      co_return pxa::Result<void>{};
    auto url = input.get();
    if (reader::origin(url).empty()) {
      notice("请输入完整 http/https 链接");
      co_return pxa::Result<void>{};
    }
    if (state.sources.size() >= reader::max_sources) {
      notice("书源已满（最多 8 个）");
      co_return pxa::Result<void>{};
    }
    busy = true;
    notice("正在验证书源…");
    auto body = co_await fetch(url);
    busy = false;
    if (!body) {
      error("导入书源", body.error());
      co_return pxa::Result<void>{};
    }
    reader::Json root(*body);
    if (!root.valid()) {
      notice("书源必须返回有效 JSON");
      co_return pxa::Result<void>{};
    }
    reader::Source added{reader::origin(url), url, reader::SourceKind::json,
                         true};
    if (root.get("format").string() == "pxa-reader-source-1") {
      auto name = root.get("name").string(96),
           search_url = root.get("searchUrl").string(512);
      if (!name || name->empty() || !search_url) {
        notice("书源配置缺少 name / searchUrl");
        co_return pxa::Result<void>{};
      }
      added.name = *name;
      added.url = reader::resolve(url, *search_url);
      auto kind = root.get("kind").string();
      if (kind == "gutendex")
        added.kind = reader::SourceKind::gutenberg;
      else if (kind && kind != "json") {
        notice("暂不支持该书源解析器");
        co_return pxa::Result<void>{};
      }
    } else if (root.get("books").raw().empty()) {
      notice("需要 books 目录或 pxa-reader-source-1 配置");
      co_return pxa::Result<void>{};
    }
    if (reader::origin(added.url).empty()) {
      notice("书源搜索链接无效");
      co_return pxa::Result<void>{};
    }
    state.sources.push_back(std::move(added));
    source_index = state.sources.size() - 1;
    input.set("");
    save();
    change(sources);
    notice("书源已添加，可到发现页搜索");
    co_return pxa::Result<void>{};
  }
  pxa::Task<void> erase_current() {
    if (state.shelf.empty() || active >= state.shelf.size()) co_return pxa::Result<void>{};
    busy = true;
    auto book = state.shelf[active];
    if (!book.url.starts_with("sample:")) {
      (void)co_await context->fs().remove(filename(book));
      (void)co_await context->fs().remove(filename(book, true));
    }
    state.shelf.erase(state.shelf.begin() + active);
    active = 0;
    chapters.clear();
    busy = false;
    save();
    change(shelf);
    notice("已移除书籍及本地缓存");
    co_return pxa::Result<void>{};
  }
  pxa::Task<void> download_current() {
    if (busy)
      co_return pxa::Result<void>{};
    busy = true;
    cancelled = false;
    auto &book = state.shelf[active];
    auto path = filename(book, true);
    std::uint32_t written = 0, total = 0;
    bool good = true, first = true, replace_existing = book.complete;
    std::string validator;
    reader::Utf8Validator text_validator;
    reader::ChapterScanner scanner;
    chapters.clear();
    auto emit = [&](std::string_view title,std::uint32_t offset) { if (!replace_existing) emit_chapter(title,offset); };
    // Resume only a validated representation. Scan with the existing 4 KiB
    // buffer so a UTF-8 code point split at the last range remains valid.
    if (!replace_existing && book.toc.empty() && !book.validator.empty()) {
      auto partial = co_await context->fs().open(path);
      if (partial) {
        while (true) {
          auto n = partial->read(stream_buffer);
          if (!n) { good = false; break; }
          if (!*n) break;
          if (!text_validator.append({reinterpret_cast<const char*>(stream_buffer.data()), *n})) { good = false; break; }
          scanner.append({reinterpret_cast<const char*>(stream_buffer.data()),*n},emit);
          written += *n;
          if (!co_await context->clock().yield()) { good = false; break; }
        }
        if (!close_file(*partial)) good = false;
        if (!good || written > book.total) {
          busy = false; notice("缓存无效，请从书架移除后重新添加");
          co_return pxa::Result<void>{};
        }
        total = book.total; validator = book.validator; first = !written;
        index_changed();
      }
    }
    if (!written && !replace_existing) book.position = 0;
    auto f = co_await context->fs().open(path, pxa::OpenMode::write |
        pxa::OpenMode::create | (written ? pxa::OpenMode{} : pxa::OpenMode::truncate));
    if (!f) { busy = false; error("下载", f.error()); co_return pxa::Result<void>{}; }
    if (written) {
      auto seek = co_await f->seek(written);
      if (!seek) { busy = false; error("恢复缓存", seek.error()); co_return pxa::Result<void>{}; }
    }
    if (!book.toc.empty()) {
      notice("读取目录…");
      auto raw = co_await fetch(book.toc);
      if (!raw) {
        error("目录", raw.error());
        good = false;
      } else {
        chapters = reader::parse_toc(*raw, book.toc);
        for (auto& entry : chapters) entry.offset = UINT32_MAX;
        controls_dirty = true; dirty = true;
        if (chapters.empty()) {
          notice("书源目录为空或格式无效");
          good = false;
        }
        std::size_t chapter_number = 0;
        for (auto &chapter : chapters) {
          ++chapter_number;
          if (!*f) {
            f = co_await context->fs().open(path, pxa::OpenMode::write | pxa::OpenMode::create);
            if (!f) { good = false; break; }
            auto seek = co_await f->seek(written);
            if (!seek) { good = false; break; }
          }
          notice("下载 " + reader::number_string(chapter_number) + "/" +
                 reader::number_string(chapters.size()));
          auto content = co_await fetch(chapter.url, 65536);
          if (!content) {
            error("章节", content.error());
            good = false;
            break;
          }
          if (reader::Json(*content).valid()) {
            auto text = reader::Json(*content).get("text").string(65536);
            if (text)
              *content = std::move(*text);
          }
          if (!reader::valid_utf8(*content)) {
            notice("章节必须是 UTF-8 文本");
            good = false;
            break;
          }
          auto text = chapter.title + "\n\n" + *content + "\n\n";
          if (written + text.size() > reader::max_book_bytes) {
            notice("下载超出 8 MiB 上限");
            good = false;
            break;
          }
          const auto chapter_offset = written;
          auto write = co_await write_file(
              *f, std::as_bytes(std::span{text.data(), text.size()}));
          if (!write) {
            error("写入", write.error());
            good = false;
            break;
          }
          written += std::uint32_t(text.size());
          if (!close_file(*f)) { good = false; break; }
          cached_bytes = written;
          chapter.offset = chapter_offset;
          if (screen == toc) controls_dirty = true;
          index_changed();
          book.total = written;
          save();
          if (first && !replace_existing) {
            first = false;
            book.total = written;
            reading_partial = true;
            start(load_page());
          }
        }
        total = written;
      }
    } else {
      std::string url = book.url;
      // Stable direct UTF-8 URL avoids the /ebooks redirect changing headers.
      auto marker = url.find("/ebooks/");
      if (url.starts_with("https://www.gutenberg.org/ebooks/") &&
          url.ends_with(".txt.utf-8")) {
        auto id = url.substr(marker + 8, url.size() - (marker + 8) - 10);
        url =
            "https://www.gutenberg.org/cache/epub/" + id + "/pg" + id + ".txt";
      }
      while (good && (!total || written < total)) {
        if (!*f) {
          f = co_await context->fs().open(path, pxa::OpenMode::write | pxa::OpenMode::create);
          if (!f) { error("缓存", f.error()); good = false; break; }
          auto seek = co_await f->seek(written);
          if (!seek) { error("缓存", seek.error()); good = false; break; }
        }
        const auto chunk = written ? reader::chunk_bytes : 8192u;
        auto range = "bytes=" + reader::number_string(written) + "-" +
                     reader::number_string(written + chunk - 1);
        std::array<pxa::NetHeaderView, 3> headers{
            {{"range", range},
             {"accept-encoding", "identity"},
             {"if-range", validator}}};
        constexpr std::array<std::string_view, 3> wanted{
            "content-range", "etag", "last-modified"};
        auto response = co_await request(
            url, reader::chunk_bytes,
            std::span{headers}.first(validator.empty() ? 2 : 3), wanted);
        if (!response) {
          error("下载", response.error());
          good = false;
          break;
        }
        if (response->status_code != 200 && response->status_code != 206) {
          notice("书源暂时不可用，请稍后继续缓存");
          good = false;
          break;
        }
        std::optional<reader::ContentRange> bounds;
        std::string current_validator;
        for (auto &h : response->headers) {
          if (h.name_view() == "content-range")
            bounds = reader::content_range(h.value_view());
          if (h.name_view() == "etag" && !h.value_view().starts_with("W/"))
            current_validator = std::string(h.value_view());
          else if (h.name_view() == "last-modified" &&
                   current_validator.empty())
            current_validator = std::string(h.value_view());
        }
        if (written && !validator.empty() && validator != current_validator) {
          notice("正文已变化，请重新下载");
          good = false;
          break;
        }
        if (!written) {
          validator = current_validator;
          if (!replace_existing) book.validator = validator.size() <= 128 ? validator : std::string{};
        }
        if (response->status_code == 206) {
          if (!bounds || bounds->start != written) {
            notice("书源返回的下载范围无效");
            good = false;
            break;
          }
          if (total && bounds->total != total) {
            notice("正文已变化，请重新下载");
            good = false;
            break;
          }
          total = bounds->total;
        } else if (response->status_code == 200 && written == 0 &&
                   response->body_length_known() &&
                   response->body_length <= reader::chunk_bytes)
          total = std::uint32_t(response->body_length);
        else {
          notice("书源不支持分段下载或响应过大");
          good = false;
          break;
        }
        std::uint32_t count = 0;
        auto bytes = std::span{stream_buffer};
        if (response->body)
          while (true) {
            auto n = response->body.read(bytes);
            if (!n) {
              error("下载", n.error());
              good = false;
              break;
            }
            if (!*n)
              break;
            if (!text_validator.append(
                    {reinterpret_cast<const char *>(bytes.data()), *n})) {
              notice("正文必须是有效 UTF-8 文本");
              good = false;
              break;
            }
            auto write = co_await write_file(*f, std::span{bytes}.first(*n));
            if (!write) {
              error("写入", write.error());
              good = false;
              break;
            }
            scanner.append({reinterpret_cast<const char*>(bytes.data()),*n},emit);
            count += *n;
          }
        if (!good)
          break;
        if (!count || (bounds && count != bounds->end - bounds->start + 1)) {
          notice("下载内容不完整，请重试");
          good = false;
          break;
        }
        written += count;
        if (!close_file(*f)) { good = false; break; } // Publish only closed ranges.
        if (!replace_existing)
          book.total = total;
        notice("已缓存 " + reader::number_string(written / 1024) + " / " +
               reader::number_string((total + 1023) / 1024) + " KiB");
        if (!replace_existing) {
          cached_bytes = written;
          index_changed();
          save();
          if (first) {
            first = false;
            reading_partial = true;
            start(load_page());
          } else if (page.count == 0 && !page_busy)
            start(load_page());
        }
        if (written >= total)
          break;
      }
    }
    if (!close_file(*f)) good = false;
    if (good && !text_validator.complete()) {
      notice("正文在 UTF-8 字符中截断");
      good = false;
    }
    if (good && written == total && written) {
      // Stop new partial reads and let an outstanding page read close first.
      // The FS publication contract deliberately rejects open source files.
      struct PublicationGuard {
        bool &flag;
        explicit PublicationGuard(bool &value) : flag(value) { flag = true; }
        ~PublicationGuard() { flag = false; }
      };
      {
        PublicationGuard guard(publishing);
        for (unsigned wait = 0; page_busy && wait < 16; ++wait)
          (void)co_await context->clock().yield();
        if (page_busy) {
          busy = false;
          error("等待正文读取", pxa::Error::busy);
          co_return pxa::Result<void>{};
        }
        auto moved = co_await context->fs().replace(path, filename(book));
        if (!moved) {
          busy = false;
          error("完成下载", moved.error());
          co_return pxa::Result<void>{};
        }
        book.complete = true;
        book.total = total;
        reading_partial = false;
      }
      cached_bytes = total;
      notice("下载完成，可离线阅读");
      save();
      if (book.toc.empty()) scanner.finish(emit);
      index_changed();
      start(load_page());
#ifdef READER_VALIDATE
      log("READER-DOWNLOAD complete=1 bytes=" + reader::number_string(total));
#endif
    } else
      notice(status +
             (replace_existing ? " · 保留原有离线版本" : " · 可重试下载"));
    busy = false;
    save();
    co_return pxa::Result<void>{};
  }
  void render();
  void book_pointer(int, const CanvasPointer&);
  void pointer(const CanvasPointer &);
  void action(Action, int);
};
void ReaderPointer::operator()(const CanvasPointer &event) const {
  app->pointer(event);
}
#include "reader_ui.inc"
PXA_APPLICATION(ReaderApp)
