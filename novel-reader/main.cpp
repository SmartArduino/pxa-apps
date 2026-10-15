#include <pxa/libcpp_diagnostics.hpp>
#include "reader_state.hpp"
#include "reader_catalog.hpp"
#include "reader_paging.hpp"
#include "reader_window.hpp"
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
  static constexpr pxa::game::LoopOptions loop_options{16000, 16, 1};
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
    remove_confirm,
    cache_settings
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
    search_history,
    text_edit,
    manage_book,
    confirm_remove,
    clear_search,
    clear_history,
    cache_mode,
    erase_cache,
    cache_options
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
           screen == source_add ? "添加书源" : screen == settings ? (metrics.shape==2 ? "偏好" : "阅读偏好") : screen == toc ? "章节目录" :
           screen == read_menu ? "正在阅读" : screen == book_menu ? "管理书籍" : screen==cache_settings ? "下载与缓存" : "移除书籍";
  }
  std::string_view subtitle() const {
    if (notice_deadline || busy) return status;
    if ((screen == read_menu || screen == book_menu || screen == remove_confirm) && !state.shelf.empty()) return state.shelf[active].title;
    return screen == shelf ? "轻点阅读 · 长按管理书籍" : screen == search ? "搜索书名或作者 · 轻点切换书源" : screen==cache_settings ? "按需阅读只保存进度，不保存正文" :
           screen == sources ? "阅读网站，也支持自定义书源" : screen == settings ? "找到最舒服的阅读方式" : std::string_view(status);
  }

  pxa::Context *context = nullptr;
  reader::SavedState state;
  pxa::ui::DisplayMetrics metrics;
  CanvasRef canvas;
  CanvasCommands<reader::page_command_capacity> draw;
  pxa::Permission network_permission;
  State<std::string> input{""};
  Screen screen = shelf, return_screen = shelf, cache_return = shelf;
  TextInputRef input_ref;
  State<TextInputBox> input_box{TextInputBox{{}, false, Font::body, 0x253430ff}};
  std::vector<reader::Book> results;
  std::vector<reader::Chapter> chapters;
  std::optional<reader::web::TocCursor> toc_cursor;
  std::size_t chapter_count() const {return toc_cursor && toc_cursor->total?toc_cursor->total:chapters.size();}
  bool more_chapters() const {return toc_cursor && !toc_cursor->complete();}
  void trace_remote_toc() {
#ifdef READER_VALIDATE
    log("READER-REMOTE-TOC chapters="+reader::number_string(chapters.size())+" total="+reader::number_string(toc_cursor->total)+
      " pages="+reader::number_string(toc_cursor->next)+"/"+reader::number_string(toc_cursor->pages));
#endif
  }
  std::array<std::uint32_t, 24> previous{};
  reader::PageNumbers page_numbers;
  std::size_t previous_count = 0;
  reader::Page page;
  reader::ReadingWindow window;
  std::string chapter_text;
  std::string page_text, status = "正在加载书架…", next_results;
  std::size_t active = 0, source_index = 0, list_start = 0;
  bool dirty = true, busy = false, page_busy = false, save_busy = false,
       save_pending = false, cancelled = false, reading_partial = false,
       state_ready = false, system_dark = false, publishing = false, sized_text_supported = false,
       reading_notice = false, transient = false, progress_pending = false,
       body_dirty = true;
  Screen canvas_screen = remove_confirm;
  std::string footer_label;
  std::uint64_t progress_deadline = 0;
  struct PendingTurn {
    std::uint32_t position=0,chapter=0;
    int direction=0;
    bool chapter_change=false;
    std::uint32_t number=0;
    bool numbered=false;
  } pending_turn;
#ifdef READER_VALIDATE
  std::uint32_t cache_hits=0, cache_reads=0, body_presents=0, footer_presents=0;
#endif
  std::uint64_t auto_deadline = 0, frame_time = 0, notice_deadline = 0;
  std::uint32_t next_offset = 0, cached_bytes = 0;
  std::uint64_t press_time = 0;
  int pressed_book = -1, press_x = 0, press_y = 0;
  std::string search_query, result_query;
  unsigned reading_theme() const { return state.settings.theme == 3 ? (system_dark ? 2 : 0) : state.settings.theme; }
  void cancel_network() {
    context->foreground_tasks().cancel();
    busy = false; page_busy = false; publishing = false; auto_deadline = 0;
    page_numbers.running=false;
    save();
  }
  std::array<std::byte, 4096> net_packet{};
  std::array<pxa::NetHeader, 3> net_headers{};
  static constexpr std::size_t page_read_bytes = reader::ReadingWindow::capacity;
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
                       if(screen==search) action(search_go,0);
                       else if(screen==source_add) action(import_source,0);
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
      dirty = true; controls_dirty = true; body_dirty = true;
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
    if(screen==reading && !page.count)body_dirty=true;
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
      finish_turn(false);
      error("任务", r.error());
    }
  }
  void finish_turn(bool success) {
    if(!pending_turn.direction || state.shelf.empty())return;
    auto& book=state.shelf[active];
    if(!success) {
      book.position=pending_turn.position;book.chapter=pending_turn.chapter;
      if(pending_turn.numbered) {
        page_numbers.current=pending_turn.number;page_numbers.current_offset=book.position;page_numbers.current_boundary=true;
      } else page_numbers.current_offset=UINT32_MAX;
    } else {
      if(!pending_turn.chapter_change) {
        if(pending_turn.direction>0) {
          if(previous_count==previous.size()){std::move(previous.begin()+1,previous.end(),previous.begin());--previous_count;}
          previous[previous_count++]=pending_turn.position;
        } else if(previous_count)--previous_count;
      }
      progress_pending=true;progress_deadline=frame_time+1000000;
    }
    pending_turn={};
  }
  void begin_turn(int direction,bool chapter_change=false) {
    const auto& book=state.shelf[active];
    pending_turn={book.position,book.chapter,direction,chapter_change,page_numbers.current,
        page_numbers.current_offset==book.position && page_numbers.current_boundary};
  }
  void change(Screen value) {
    if(progress_pending){progress_pending=false;save();}
    if (value != search && value != source_add)
      (void)input_ref.hide_keyboard();
    if (screen == source_add && value != source_add) input.set(search_query);
    screen = value;
    controls_dirty = true;
    pressed_book = -1;
    list_start = 0;
    auto_deadline = 0;
    dirty = true;
    body_dirty = true;
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
    pressed_book = -1;
    (void)input_ref.hide_keyboard();
    auto_deadline = 0;
    state.settings.automatic = false;
    busy = false;
    page_busy = false;
    page_numbers.running=false;
    notice("已暂停");
    save();
    progress_pending=false;
  }
  void on_foreground(pxa::Context &) {
    auto_deadline = 0;
    dirty = true;
    if(screen==reading && state_ready && !page.count && !busy && !page_busy)start(load_page());
  }
  void resume_reading() {
    change(reading);
    if(!page.count && !busy && !page_busy)start(load_page());
  }
  pxa::BackAction on_back() {
    if(screen==cache_settings){change(cache_return);return pxa::BackAction::stay;}
    if (screen == book_menu || screen == remove_confirm) { change(shelf); return pxa::BackAction::stay; }
    if (screen == shelf)
      return pxa::BackAction::close;
    if (screen == reading)
      change(read_menu);
    else if (screen == read_menu || screen == toc)
      resume_reading();
    else if (screen == settings)
      {if(return_screen==read_menu)resume_reading();else change(shelf);}
    else if (screen == source_add)
      change(sources);
    else
      change(shelf);
    return pxa::BackAction::stay;
  }
  void on_error(pxa::Context &, pxa::Error e) { error("界面", e); }
  pxa::Result<bool> on_event(pxa::Context &, const pxa::Event &event) {
    if (event.is<UiAppearance>()) {
      if (auto appearance = decode_appearance(event)) { system_appearance = *appearance; system_dark = appearance->dark; dirty = true; controls_dirty = true; body_dirty=true; }
    }
    if (event.is<DisplayMetrics>()) {
      auto value = decode_display_metrics(event);
      if (value) {
        pressed_book = -1;
        metrics = *value;
        reset_page_numbers(page_numbers.origin);
        controls_dirty = true;
        body_dirty = true;
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
    if (screen == shelf && pressed_book >= 0 && frame_time >= press_time &&
        frame_time - press_time >= 550000) {
      const int index = pressed_book;
      pressed_book = -1;
      action(manage_book, index);
    }
    if(progress_pending && frame_time>=progress_deadline){progress_pending=false;save();}
    if (notice_deadline && frame_time >= notice_deadline) {
      notice_deadline = 0;
      dirty = true;
      if(screen==search && results.empty())controls_dirty=true;
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
    if (state_ready && !transient && screen==reading && page.count && !busy && (!page_busy || page_numbers.go_previous) &&
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
      if(text.size()>reader::SavedState::max_state_bytes) {
        notice("书架记录超出上限，原保存记录仍保留");break;
      }
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
    bool loaded = false,migrated=false;
    auto f = co_await context->fs().open("reader-state.json");
    if (f) {
      std::string text;
      auto buffer = std::span{stream_buffer}.first(1024);
      while (text.size() <= reader::SavedState::max_state_bytes) {
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
      migrated=loaded && (reader::Json(text).get("version").number().value_or(4)<4 ||
        reader::Json(text).get("sourceRevision").number().value_or(1)<reader::current_source_revision);
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
    if (!loaded || migrated) save();
    busy = false;
    if (status == "正在加载书架…")
      status = "欢迎来到书页";
    dirty = true;
    co_return pxa::Result<void>{};
  }
  pxa::Task<pxa::NetResponse>
  request(std::string_view url, std::uint32_t maximum,
          std::span<const pxa::NetHeaderView> headers = {},
          std::span<const std::string_view> wanted = {},
          std::string_view form = {}) {
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
    if(!form.empty()){req.method=pxa::HttpMethod::post;req.body=std::as_bytes(std::span{form.data(),form.size()});}
    auto response = co_await context->net().request(req, network_permission,
                                                    net_packet, net_headers);
    // The app owns the authority for the full request/body lifetime.
    if (!response)
      co_return std::unexpected(response.error());
    co_return std::move(*response);
  }
  pxa::Task<std::string> fetch(std::string url,
                               std::uint32_t maximum = reader::max_json_bytes,
                               std::string form = {},std::string_view referer = {},std::string_view user_agent = {}) {
    std::array<pxa::NetHeaderView,3> headers{};std::size_t count=0;
    if(!form.empty())headers[count++]={"content-type","application/x-www-form-urlencoded"};
    if(!referer.empty())headers[count++]={"referer",referer};
    if(!user_agent.empty())headers[count++]={"user-agent",user_agent};
    auto response = co_await request(url, maximum,std::span{headers}.first(count),{},form);
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
    if(state.sources.empty()){results.clear();next_results.clear();notice("请先到书源页添加书源");co_return pxa::Result<void>{};}
    if (source_index >= state.sources.size()) source_index = 0;
    auto key=explicit_url.empty()?std::string(reader::web::trim(input.get())):result_query;
    if(key.empty()){results.clear();next_results.clear();notice("输入书名或作者后搜索");controls_dirty=true;co_return pxa::Result<void>{};}
    if(key.size()>96){notice("关键词过长，请缩短到 96 字节以内");co_return pxa::Result<void>{};}
    input.set(key);result_query=key;
    busy = true;
    controls_dirty = true;
    results.clear();next_results.clear();list_start=0;
    notice("正在搜索…");
    auto source = state.sources[source_index];
    const auto html_rules=source.kind==reader::SourceKind::html?reader::web::rules(reader::source_rules(source)):std::nullopt;
    if(source.kind==reader::SourceKind::html && !html_rules){busy=false;notice("HTML 书源规则无效，请重新导入");co_return pxa::Result<void>{};}
    if (!source.enabled) {
      busy = false;
      notice("该书源已停用，请切换或启用书源");
      co_return pxa::Result<void>{};
    }
    state.remember_search(key);save();
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
      if (marker != url.npos) {
        const auto address=reader::search_address(url,key);
        if(!address){busy=false;notice("搜索地址过长，请缩短关键词");co_return pxa::Result<void>{};}
        url=*address;
      }
      else if (source.kind == reader::SourceKind::gutenberg && !key.empty())
        url += "&search=" + reader::percent(key);
    }
    std::string form;
    if(html_rules && explicit_url.empty()) {
      if(!html_rules->post_body.empty()) {
        auto encoded=reader::search_address(html_rules->post_body,key);
        if(!encoded){busy=false;notice("搜索请求过长，请缩短关键词");co_return pxa::Result<void>{};}
        form=std::move(*encoded);
      }
    }
    auto body = co_await fetch(url,reader::max_json_bytes,std::move(form),html_rules?std::string_view(html_rules->referer):std::string_view{},
      html_rules?std::string_view(html_rules->user_agent):std::string_view{});
    busy = false;
    if (!body) {
      notice(body.error() == pxa::Error::denied ? "请允许网络权限，或切换书源" : "书源暂时无响应，可重试或切换书源");
      log("READER-SEARCH failed=" + reader::number_string(int(body.error())));
      co_return pxa::Result<void>{};
    }
    if(html_rules) {
      results=reader::web::books(*body,source,*html_rules,url);
      if(!html_rules->next.empty()) {
        reader::web::Element document{{},*body};
        next_results=reader::web::url(url,reader::web::field(document,html_rules->next));
      }
    } else if (!reader::Json(*body).valid()) {
      notice("书源返回了无效 JSON");
      co_return pxa::Result<void>{};
    } else {
    auto response_source=source;response_source.url=url;
    results = reader::parse_books(*body, response_source);
    auto next=reader::Json(*body).get("next").string(512).value_or("");
    next_results=reader::web::url(url,next);
    }
    if((source.kind==reader::SourceKind::json || (html_rules && html_rules->post_body.empty())) &&
       source.url.find("{{key}}")==source.url.npos)
      std::erase_if(results,[&](const auto& b){return !reader::contains_folded(b.title,key) && !reader::contains_folded(b.author,key);});
    list_start = 0;
    notice(results.empty()
               ? "没有找到作品，试试作者名或切换书源"
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
    chapters.clear();toc_cursor.reset();
    previous_count = 0;
    reset_page_numbers();
    page = {};
    page_text.clear();
    window.clear();std::string{}.swap(chapter_text);transient=false;
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
      } else if(!state.settings.offline) {
        transient=true;reset_page_numbers();cached_bytes=book.total;
        book.validator.clear(); // Validators guard one live session; reopen may adopt a newer edition.
        if(book.toc.empty())start(load_page());else start(stream_chapter());
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
      chapters.clear();toc_cursor.reset();
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
  std::optional<reader::web::Rules> book_rules() const {
    const auto& book=state.shelf[active];
    for(const auto& source:state.sources)
      if(source.url==book.source && source.kind==reader::SourceKind::html)
        return reader::web::rules(reader::source_rules(source));
    return {};
  }
  std::vector<reader::Chapter> parse_chapters(std::string_view raw) const {
    const auto& book=state.shelf[active];
    if(!book.source.empty()) {
      if(const auto rules=book_rules())return reader::web::chapters(raw,*rules,book.toc);
      return {};
    }
    return reader::parse_toc(raw,book.toc);
  }
  pxa::Task<void> ensure_chapters(std::size_t through) {
    const auto& book=state.shelf[active];
    const auto rules=book_rules();
    if(chapters.empty() && !toc_cursor) {
      auto raw=co_await fetch(book.toc,reader::max_json_bytes,{},rules?std::string_view(rules->referer):std::string_view{},
        rules?std::string_view(rules->user_agent):std::string_view{});
      if(!raw)co_return std::unexpected(raw.error());
      if(!rules || !rules->paged_toc) {
        chapters=parse_chapters(*raw);
        co_return chapters.empty()?pxa::Result<void>{std::unexpected(pxa::Error::invalid_argument)}:pxa::Result<void>{};
      }
      if(rules->paged_toc->html) {
        auto part=reader::web::html_toc_page(*raw,*rules,book.toc,0);
        if(!part)co_return std::unexpected(pxa::Error::invalid_argument);
        reader::web::TocCursor cursor;
        if(!cursor.append(chapters,std::move(*part)))co_return std::unexpected(pxa::Error::invalid_argument);
        toc_cursor=std::move(cursor);
        trace_remote_toc();
      } else {
        const auto id=reader::web::field({{},*raw},rules->paged_toc->id,96);
        if(id.empty())co_return std::unexpected(pxa::Error::invalid_argument);
        toc_cursor=reader::web::TocCursor{id};
      }
    }
    if(!toc_cursor)co_return pxa::Result<void>{};
    if(!rules || !rules->paged_toc)co_return std::unexpected(pxa::Error::invalid_argument);
    const auto& paged=*rules->paged_toc;
    while(!toc_cursor->complete() && chapters.size()<=through) {
      const auto address=reader::web::toc_page_address(paged,book.toc,toc_cursor->id,toc_cursor->next);
      if(!address)co_return std::unexpected(pxa::Error::invalid_argument);
      auto raw=co_await fetch(*address,reader::max_json_bytes,{},rules->referer,rules->user_agent);
      if(!raw)co_return std::unexpected(raw.error());
      auto part=paged.html?reader::web::html_toc_page(*raw,*rules,*address,toc_cursor->next):reader::web::toc_page(*raw,paged,book.toc);
      if(!part || !toc_cursor->append(chapters,std::move(*part)))
        co_return std::unexpected(pxa::Error::invalid_argument);
      trace_remote_toc();
    }
    co_return pxa::Result<void>{};
  }
  pxa::Task<void> load_toc_rows(std::size_t before) {
    busy=true;controls_dirty=true;notice("正在读取后续目录…");
    struct Guard {
      ReaderApp& app;std::size_t before;bool committed=false;
      ~Guard(){if(!committed)app.list_start=before;app.busy=false;app.controls_dirty=true;app.dirty=true;}
    } guard{*this,before};
    auto loaded=co_await ensure_chapters(list_start+std::max<std::size_t>(1,visible_rows)-1);
    if(!loaded)notice("后续目录暂时无法读取，可再试下一组");
    else {guard.committed=true;notice("目录已更新");}
    co_return pxa::Result<void>{};
  }
  bool decode_chapter(std::string& content) const {
    if(!state.shelf[active].source.empty()) {
      const auto rules=book_rules();if(!rules)return false;
      content=reader::web::content(content,*rules);
    } else if(reader::Json(content).valid()) {
      if(auto text=reader::Json(content).get("text").string(65536))content=std::move(*text);
    }
    return !content.empty() && reader::valid_utf8(content);
  }
  pxa::Task<std::string> read_chapter(const reader::Chapter& chapter) {
    const auto rules=book_rules();
    std::string output,address=chapter.url;
    std::vector<std::uint64_t> seen;
    std::uint32_t pages=0;
    for(std::uint32_t page=0;page<128;++page) {
      if(rules && rules->paged_content) {
        const auto hash=reader::hash(address);
        if(std::find(seen.begin(),seen.end(),hash)!=seen.end())co_return std::unexpected(pxa::Error::invalid_argument);
        seen.push_back(hash);
      }
      auto raw=co_await fetch(address,65536,{},rules?std::string_view(rules->referer):std::string_view{},
        rules?std::string_view(rules->user_agent):std::string_view{});
      if(!raw)co_return std::unexpected(raw.error());
      std::string next;
      if(rules) {
        auto part=reader::web::chapter_part(*raw,*rules,chapter.url,address,page);
        if(!part || (page && pages!=part->pages))co_return std::unexpected(pxa::Error::invalid_argument);
        pages=part->pages;next=std::move(part->next);*raw=std::move(part->text);
      } else if(!decode_chapter(*raw))co_return std::unexpected(pxa::Error::invalid_argument);
      if(raw->size()>65536-output.size())co_return std::unexpected(pxa::Error::limit_exceeded);
      if(page==0)output=std::move(*raw);
      else {
        const auto needed=output.size()+raw->size();
        if(needed>output.capacity())output.reserve(std::min<std::size_t>(65536,std::max(needed,output.capacity()*2)));
        output+=*raw;
      }
      if(next.empty())co_return output;
      if(output.size()==65536)co_return std::unexpected(pxa::Error::limit_exceeded);
      if(output.size()==output.capacity())output.reserve(std::min<std::size_t>(65536,output.size()+1));
      output+='\n';address=std::move(next);
    }
    co_return std::unexpected(pxa::Error::limit_exceeded);
  }
  pxa::Task<void> stream_chapter(bool last_page=false) {
    if(page_busy || state.shelf.empty())co_return pxa::Result<void>{};
    page_busy=true;
    struct Guard { ReaderApp& app;~Guard(){app.page_busy=false;app.finish_turn(false);} } guard{*this};
    auto& book=state.shelf[active];
    if(chapters.size()<=book.chapter) {
      auto loaded=co_await ensure_chapters(book.chapter);
      if(!loaded || chapters.empty()){notice("无法读取目录，请检查书源规则或重新添加书源",true);co_return pxa::Result<void>{};}
      controls_dirty=true;
    }
    if(book.chapter>=chapters.size())book.chapter=0;
    auto content=co_await read_chapter(chapters[book.chapter]);
    if(!content) {notice("章节加载失败，可重试；请检查网站或章节大小",true);co_return pxa::Result<void>{};}
    chapter_text=chapters[book.chapter].title+"\n\n"+*content+"\n";
    cached_bytes=book.total=std::uint32_t(chapter_text.size());
    if(book.position>=cached_bytes)book.position=0;
    previous_count=0;window.clear();reset_page_numbers();
    if(last_page) {
      std::uint32_t at=0;
      while(at<cached_bytes) {
        const auto part=reader::paginate(std::string_view(chapter_text).substr(at),reader::Layout(metrics,false),state.settings,
            !at || chapter_text[at-1]=='\n');
        if(!part.consumed || at+part.consumed>=cached_bytes)break;
        if(previous_count==previous.size()){std::move(previous.begin()+1,previous.end(),previous.begin());--previous_count;}
        previous[previous_count++]=at;at+=std::uint32_t(part.consumed);
      }
      book.position=at;
    }
    prepare_page();
    co_return pxa::Result<void>{};
  }
  bool page_available() const {
    const auto& book=state.shelf[active];
    return book.url.starts_with("sample:") || (!chapter_text.empty() && transient) || window.covers(book.position,cached_bytes);
  }
  void prepare_page() {
    auto& book=state.shelf[active];
    auto text=book.url.starts_with("sample:")?reader::sample_text.substr(std::min<std::size_t>(book.position,reader::sample_text.size())):
        transient && !chapter_text.empty()?std::string_view(chapter_text).substr(book.position):window.at(book.position);
    bool paragraph=book.url.starts_with("sample:")? !book.position || reader::sample_text[book.position-1]=='\n':
        transient && !chapter_text.empty()? !book.position || chapter_text[book.position-1]=='\n':window.paragraph_at(book.position);
    if(!book.position && reader::hardwrapped_source(book.url)) {
      reader::ChapterScanner scanner;std::uint32_t first_heading=UINT32_MAX;
      scanner.append(text,[&](std::string_view,std::uint32_t offset){if(first_heading==UINT32_MAX)first_heading=offset;});
      if(first_heading!=UINT32_MAX && first_heading) {
        reset_page_numbers(first_heading);book.position=first_heading;text.remove_prefix(first_heading);paragraph=true;
      }
    }
    const reader::Layout geometry(metrics,false);
    page=reader::paginate(text,geometry,state.settings,paragraph,reader::hardwrapped_source(book.url));
    next_offset=book.position+std::uint32_t(page.consumed);
    reader::copy_page_text(text,page_text,page);
    reader::balance_page(page,page_text,geometry,state.settings);
    if(book.url.starts_with("sample:") || (transient && !chapter_text.empty() && !page_numbers.finished)) {
      const auto full=book.url.starts_with("sample:")?reader::sample_text:std::string_view(chapter_text);
      page_numbers.reset();page_numbers.available=cached_bytes;
      page_numbers.consume(full,geometry,state.settings,book.position,false,true,false);
      page_numbers.finished=page_numbers.exact_total=true;
#ifdef READER_VALIDATE
      log("READER-PAGES total="+reader::number_string(page_numbers.total)+" bytes="+reader::number_string(cached_bytes)+" complete=1");
#endif
    }
    if(transient && book.toc.empty() && page_numbers.current_offset==book.position && page_numbers.current_boundary) {
      page_numbers.offset=book.position;page_numbers.pages=page_numbers.current-1;page_numbers.checkpoint();
    }
    if(book.position==page_numbers.origin) {
      page_numbers.current=1;page_numbers.current_offset=book.position;page_numbers.current_boundary=true;
    }
    body_dirty=dirty=true;auto_deadline=0;
    finish_turn(true);
#ifdef READER_VALIDATE
    log("READER-PAGE offset="+reader::number_string(book.position)+" next="+reader::number_string(next_offset)+" lines="+reader::number_string(page.count));
    log("READER-CACHE hits="+reader::number_string(cache_hits)+" reads="+reader::number_string(cache_reads)+" raw_capacity="+reader::number_string(window.text.capacity())+" display_capacity="+reader::number_string(page_text.capacity())+" transient="+reader::number_string(transient));
#endif
  }
  pxa::Task<bool> fill_window(std::uint32_t position) {
    auto& book=state.shelf[active];
    const auto desired=position>reader::ReadingWindow::history_bytes?position-reader::ReadingWindow::history_bytes:0;
    const auto start=desired?desired-1:0;
    window.clear();window.offset=start;window.text.reserve(reader::ReadingWindow::capacity);
#ifdef READER_VALIDATE
    ++cache_reads;
#endif
    if(transient) {
      const auto range="bytes="+reader::number_string(start)+"-"+reader::number_string(start+reader::ReadingWindow::capacity-1);
      const std::array<pxa::NetHeaderView,3> headers{{{"range",range},{"accept-encoding","identity"},{"if-range",book.validator}}};
      constexpr std::array<std::string_view,3> wanted{"content-range","etag","last-modified"};
      auto response=co_await request(book.url,reader::chunk_bytes,std::span{headers}.first(book.validator.empty()?2:3),wanted);
      if(!response){error("加载正文",response.error());co_return false;}
      if(response->status_code!=200 && response->status_code!=206){notice("书源暂时不可用，请重试",true);co_return false;}
      std::optional<reader::ContentRange> bounds;std::string validator;
      for(const auto& h:response->headers) {
        if(h.name_view()=="content-range")bounds=reader::content_range(h.value_view());
        if(h.name_view()=="etag" && !h.value_view().starts_with("W/"))validator=h.value_view();
        else if(h.name_view()=="last-modified" && validator.empty())validator=h.value_view();
      }
      if(!book.validator.empty() && book.validator!=validator) {notice("正文已更新，请重新打开作品",true);co_return false;}
      std::uint32_t response_start=0;
      if(response->status_code==206 && bounds && bounds->start==start) {response_start=start;book.total=bounds->total;}
      else if(response->status_code==200 && response->body_length_known() && response->body_length<=reader::chunk_bytes)book.total=std::uint32_t(response->body_length);
      else {notice("书源不支持按需分段阅读，可尝试下载离线",true);co_return false;}
      std::size_t received=0;
      if(response->body)while(true) {
        auto n=response->body.read(stream_buffer);if(!n){error("正文",n.error());co_return false;}if(!*n)break;
        const auto at=response_start+received;
        const auto left=std::max<std::size_t>(start,at),right=std::min<std::size_t>(start+reader::ReadingWindow::capacity,at+*n);
        if(right>left)window.text.append(reinterpret_cast<const char*>(stream_buffer.data())+left-at,right-left);
        received+=*n;
      }
      if((bounds && received!=bounds->end-bounds->start+1) || (!bounds && received!=book.total)) {window.clear();notice("正文响应不完整，请重试",true);co_return false;}
      if(validator.size()<=128)book.validator=std::move(validator);
      cached_bytes=book.total;
    } else {
      auto f=co_await context->fs().open(filename(book,reading_partial));
      if(!f){error("读取",f.error());co_return false;}
      if(!co_await f->seek(start)){notice("阅读位置超出正文",true);co_return false;}
      window.text.resize(reader::ReadingWindow::capacity);
      auto bytes=std::as_writable_bytes(std::span{window.text.data(),window.text.size()});std::size_t used=0;
      while(used<bytes.size()) {auto n=f->read(bytes.subspan(used));if(!n){error("读取",n.error());window.clear();co_return false;}if(!*n)break;used+=*n;}
      window.text.resize(used);if(!close_file(*f))co_return false;
    }
    if(desired) {
      window.paragraph=!window.text.empty() && (window.text[0]=='\n' || window.text[0]=='\r');
      if(!window.text.empty())window.text.erase(0,1);++window.offset;
      while(!window.text.empty() && (static_cast<unsigned char>(window.text[0])&0xc0)==0x80) {
        window.text.erase(0,1);++window.offset;window.paragraph=false;
      }
    }
    bool invalid=false;const auto valid=reader::utf8_prefix(window.text,invalid);window.text.resize(valid);
    if(invalid || window.text.empty()){window.clear();notice("正文为空或不是有效 UTF-8",true);co_return false;}
    co_return true;
  }
  pxa::Task<void> load_page() {
    if(page_busy || publishing || state.shelf.empty())co_return pxa::Result<void>{};
    if(transient && !state.shelf[active].toc.empty() && chapter_text.empty()) {
      co_await stream_chapter();co_return pxa::Result<void>{};
    }
    if(page_available()){prepare_page();co_return pxa::Result<void>{};}
    page_busy=true;
    struct Guard {ReaderApp& app;~Guard(){app.page_busy=false;app.finish_turn(false);}} guard{*this};
    const auto filled=co_await fill_window(state.shelf[active].position);
    if(filled && *filled)prepare_page();
    co_return pxa::Result<void>{};
  }
  pxa::Task<void> locate_stream_previous() {
    page_busy=true;
    struct Guard {ReaderApp& app;~Guard(){app.page_busy=false;}} guard{*this};
    auto& book=state.shelf[active];const auto target=book.position;
    reader::PageNumbers scan=page_numbers;
    if(scan.origin>=target)scan.reset();
    scan.locate(target-1);
    while(scan.offset<target) {
      if(!window.covers(scan.offset,cached_bytes)) {
        const auto filled=co_await fill_window(scan.offset);
        if(!filled || !*filled)co_return pxa::Result<void>{};
      }
      if(!scan.offset && reader::hardwrapped_source(book.url)) {
        reader::ChapterScanner headings;std::uint32_t first=UINT32_MAX;
        headings.append(window.text,[&](std::string_view,std::uint32_t offset){if(first==UINT32_MAX)first=offset;});
        if(first!=UINT32_MAX && first && first<target)scan.reset(first);
      }
      auto text=window.at(scan.offset);
      const auto used=scan.consume(text,reader::Layout(metrics,false),state.settings,target-1,
          reader::hardwrapped_source(book.url),std::uint64_t(window.offset)+window.text.size()>=cached_bytes,true);
      if(scan.current_offset==target-1) {
        book.position=scan.current_start;page_numbers.current=scan.current;
        page_numbers.current_offset=book.position;page_numbers.current_boundary=true;
        progress_pending=true;progress_deadline=frame_time+1000000;prepare_page();
        co_return pxa::Result<void>{};
      }
      if(!used)break;
      if(!co_await context->clock().yield())co_return pxa::Result<void>{};
    }
    notice("无法定位上一页，请重试",true);
    co_return pxa::Result<void>{};
  }
  void turn(int direction) {
    if (page_busy || state.shelf.empty())
      return;
    if(!page.count){if(!busy)start(load_page());return;}
    auto &b = state.shelf[active];
    const auto old_position=b.position;
    const bool numbered=page_numbers.current_offset==old_position && page_numbers.current_boundary;
    if (direction > 0) {
      if(transient && !b.toc.empty() && next_offset>=cached_bytes && (b.chapter+1<chapter_count() || more_chapters())) {
        begin_turn(1,true);
        ++b.chapter;b.position=0;std::string{}.swap(chapter_text);window.clear();
        start(stream_chapter());return;
      }
      if (next_offset <= b.position || ((b.complete || transient) && next_offset >= b.total) || (!b.complete && next_offset >= cached_bytes)) {
        state.settings.automatic = false;
        notice(b.complete || transient ? "已经读到书末" : "请等待正文缓存后再翻页", true);
        return;
      }
      begin_turn(1);
      b.position = next_offset;
    } else {
      if(transient && !b.toc.empty() && !b.position && b.chapter) {
        begin_turn(-1,true);
        --b.chapter;b.position=0;std::string{}.swap(chapter_text);window.clear();
        start(stream_chapter(true));return;
      }
      if (!previous_count && b.position>(transient?0:page_numbers.origin)) {
        if (b.url.starts_with("sample:") || (transient && !chapter_text.empty())) {
          const auto text=b.url.starts_with("sample:")?reader::sample_text:std::string_view(chapter_text);
          page_numbers.locate(b.position-1);
          page_numbers.consume(text.substr(page_numbers.offset),reader::Layout(metrics,false),state.settings,b.position-1,false,true,true);
          b.position=page_numbers.current_start;page_numbers.current_offset=b.position;page_numbers.current_boundary=true;
          progress_pending=true;progress_deadline=frame_time+1000000;prepare_page();
          return;
        }
        if(transient) {start(locate_stream_previous());return;}
        page_numbers.go_previous=true; page_busy=true; dirty=true;
        return;
      }
      if(!previous_count && b.position==old_position){notice("已经是本次阅读起点",true);return;}
      if (previous_count) {
        begin_turn(-1);
        b.position = previous[previous_count-1];
      }
    }
    if (b.position!=old_position) {
      if (numbered) {
        page_numbers.current=std::uint32_t(std::max(1,int(page_numbers.current)+direction));
        page_numbers.current_offset=b.position;
      } else page_numbers.current_offset=UINT32_MAX;
    }
    if(page_available()) {
#ifdef READER_VALIDATE
      ++cache_hits;
#endif
      prepare_page();
    } else start(load_page());
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
    if (root.get("format").string() == "pxa-reader-source-1" || root.get("format").string() == "pxa-reader-source-2") {
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
      else if(kind=="html" && reader::web::rules(*body)) {
        added.kind=reader::SourceKind::html;added.rules=*body;
      }
      else if (kind && kind != "json") {
        notice("暂不支持该书源解析器");
        co_return pxa::Result<void>{};
      }
    } else if (root.get("books").raw().empty()) {
      notice("需要 books 目录或书源规则配置");
      co_return pxa::Result<void>{};
    }
    if (reader::origin(added.url).empty()) {
      notice("书源搜索链接无效");
      co_return pxa::Result<void>{};
    }
    if(std::any_of(state.sources.begin(),state.sources.end(),[&](const auto& value){return value.url==added.url;})) {
      notice("该书源已存在，请先移除旧规则再添加");co_return pxa::Result<void>{};
    }
    state.sources.push_back(std::move(added));
    source_index = state.sources.size() - 1;
    results.clear();next_results.clear();result_query.clear();list_start=0;
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
    chapters.clear();toc_cursor.reset();
    busy = false;
    save();
    change(shelf);
    notice("已移除书籍及本地缓存");
    co_return pxa::Result<void>{};
  }
  pxa::Task<void> erase_body_cache() {
    if(state.shelf.empty())co_return pxa::Result<void>{};
    auto& book=state.shelf[active];
    if(book.url.starts_with("sample:")){notice("内置短篇不占下载空间");co_return pxa::Result<void>{};}
    busy=true;
    for(bool partial:{false,true}) {
      auto removed=co_await context->fs().remove(filename(book,partial));
      if(!removed && removed.error()!=pxa::Error::not_found){busy=false;error("清除缓存",removed.error());co_return pxa::Result<void>{};}
    }
    book.complete=false;book.validator.clear();
    if(!book.toc.empty()){book.position=0;book.chapter=0;}
    window.clear();std::string{}.swap(chapter_text);chapters.clear();toc_cursor.reset();page={};cached_bytes=0;transient=true;
    previous_count=0;reset_page_numbers(book.position);save();busy=false;body_dirty=true;
    notice(book.toc.empty() ? "正文缓存已清除，书架和进度仍保留" : "正文缓存已清除，分章书籍将从首章打开");
    co_return pxa::Result<void>{};
  }
  pxa::Task<void> download_current() {
    if (busy)
      co_return pxa::Result<void>{};
    busy = true;
    cancelled = false;
    auto &book = state.shelf[active];
    const auto resume_chapter=book.chapter,resume_position=book.position;
    const bool chapter_progress=transient && !book.toc.empty();
    transient=false;std::string{}.swap(chapter_text);window.clear();
    auto path = filename(book, true);
    std::uint32_t written = 0, total = 0;
    bool good = true, first = true, replace_existing = book.complete;
    std::string validator;
    reader::Utf8Validator text_validator;
    reader::ChapterScanner scanner;
    chapters.clear();toc_cursor.reset();
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
      chapters.clear();toc_cursor.reset();
      auto loaded = co_await ensure_chapters(SIZE_MAX);
      if (!loaded) {
        error("目录", loaded.error());
        good = false;
      } else {
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
          auto content = co_await read_chapter(chapter);
          if (!content) {
            error("章节", content.error());
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
        book.chapter=0;
        if(chapter_progress && resume_chapter<chapters.size())book.position=std::min(total-1,chapters[resume_chapter].offset+resume_position);
        reading_partial = false;
        window.clear();
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
