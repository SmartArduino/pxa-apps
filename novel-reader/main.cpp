#include "reader_state.hpp"
#include <cstdio>
#include <pxa/app.hpp>
#include <pxa/canvas.hpp>
#include <pxa/ui_display.hpp>

using namespace pxa::ui;
struct InputBox {
  CanvasRegion region{};
  bool visible = false;
  Font font = Font::body;
  std::uint32_t foreground = 0x253430ff;
  bool operator==(const InputBox &b) const {
    return visible == b.visible && font == b.font &&
           foreground == b.foreground && region.x == b.region.x &&
           region.y == b.region.y && region.width == b.region.width &&
           region.height == b.region.height;
  }
};
static bool write_input_box(Transaction<> &tx, std::uint32_t node,
                            const InputBox &box) {
  std::array<std::byte, 8> color{};
  color[0] = std::byte{1};
  pxa::wire::put32(color.data() + 4, box.foreground);
  return tx.u8(node, protocol::visible, box.visible) &&
         tx.u16(node, protocol::font_role, static_cast<std::uint16_t>(box.font)) &&
         tx.property(node, protocol::foreground, color) &&
         tx.u8(node, protocol::position, 1) &&
         tx.logical_px(node, protocol::x, box.region.x) &&
         tx.logical_px(node, protocol::y, box.region.y) &&
         tx.logical_px(node, protocol::width, std::max(1, box.region.width)) &&
         tx.logical_px(node, protocol::height, std::max(1, box.region.height));
}
struct ReaderInput {
  static constexpr Capacity capacity{1, 2, 2, 0, 1};
  State<std::string> *value;
  State<InputBox> *box;
  TextInputRef *ref;
  void *app;
  void (*submit)(void *);
  void operator()() { submit(app); }
  template <class P> bool render(P &p, std::uint32_t parent) {
    auto node = p.create(parent, protocol::control, protocol::text_input);
    return node && p.attach_text_input(*ref, node) &&
           write_input_box(p.transaction(), node, box->get()) &&
           write_string_text(p.transaction(), node, value->get()) &&
           p.transaction().u64(node, protocol::event_mask, (1u << 5) | 1u) &&
           p.template bind_source<std::string, write_string_text>(*value,
                                                                  node) &&
           p.template bind<InputBox, write_input_box>(*box, node) &&
           p.on_text(node, *value) && p.on_click(node, *this);
  }
};
struct ReaderApp;
struct ReaderPointer {
  ReaderApp *app;
  void operator()(const CanvasPointer &) const;
};
struct ReaderApp {
  static constexpr pxa::game::LoopOptions loop_options{250000, 250, 1};
  enum Screen {
    shelf,
    search,
    sources,
    reading,
    read_menu,
    settings,
    toc,
    source_add
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
    text_edit
  };
  struct Hit {
    reader::Rect box;
    Action action;
    int arg = 0;
  };
  std::array<Hit, 64> hits{};
  std::size_t hit_count = 0, visible_rows = 1;
  pxa::Context *context = nullptr;
  reader::SavedState state;
  pxa::ui::DisplayMetrics metrics;
  CanvasRef canvas;
  CanvasCommands<4000> draw;
  pxa::Permission network_permission;
  std::uint64_t ui_features = 0;
  State<std::string> input{""};
  Screen screen = shelf, return_screen = shelf;
  TextInputRef input_ref;
  State<InputBox> input_box{InputBox{}};
  std::vector<reader::Book> results;
  std::vector<reader::Chapter> chapters;
  std::array<std::uint32_t, 64> previous{};
  std::size_t previous_count = 0;
  reader::Page page;
  std::string page_text, status = "正在加载书架…", next_results;
  std::size_t active = 0, source_index = 0, list_start = 0;
  bool dirty = true, busy = false, page_busy = false, save_busy = false,
       save_pending = false, cancelled = false, reading_partial = false,
       state_ready = false, publishing = false;
  std::uint64_t auto_deadline = 0, frame_time = 0, notice_deadline = 0;
  std::uint32_t next_offset = 0;
  std::array<std::byte, 4096> net_packet{};
  std::array<pxa::NetHeader, 3> net_headers{};
  std::array<std::byte, 16384> page_buffer{};
  std::array<std::byte, 4096> stream_buffer{};
  float native_density() const { return float(metrics.density_q16) / 65536.f; }
  reader::Layout layout() const { return reader::Layout(metrics); }
  auto view() {
    return Stack(Canvas(canvas).on_pointer(ReaderPointer{this}),
                 ReaderInput{&input, &input_box, &input_ref, this,
                             [](void *ptr) {
                               auto self = static_cast<ReaderApp *>(ptr);
                               (void)self->input_ref.hide_keyboard();
                               self->dirty = true;
                             }})
        .fill()
        .fill_height();
  }
  void log(std::string_view message) {
    (void)context->log().write(pxa::LogLevel::info, message);
  }
  void notice(std::string value) {
    status = std::move(value);
    if (context)
      log("READER-STATUS " + status);
    notice_deadline = frame_time + 4000000;
    dirty = true;
  }
  void error(std::string_view operation, pxa::Error e) {
    log("READER-ERROR operation=" + std::string(operation) +
        " code=" + std::to_string(int(e)));
    if (e == pxa::Error::quota_exceeded) {
      notice("存储配额不足，请移除已下载书籍");
      return;
    }
    notice(std::string(operation) + "失败 (" + std::to_string(int(e)) + ")");
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
    screen = value;
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
    for (std::size_t at = 0; at + 4 <= config.size();) {
      auto tag = pxa::wire::get16(config.data() + at),
           size = pxa::wire::get16(config.data() + at + 2);
      at += 4;
      if (size > config.size() - at)
        break;
      if (tag == 8) {
        auto env = config.subspan(at, size);
        for (std::size_t p = 0; p + 4 <= env.size();) {
          auto t = pxa::wire::get16(env.data() + p),
               n = pxa::wire::get16(env.data() + p + 2);
          p += 4;
          if (n > env.size() - p)
            break;
          if (t == 10 && n == 8)
            ui_features = pxa::wire::get64(env.data() + p);
          p += n;
        }
      }
      at += size;
    }
    (void)ctx.window().fullscreen(pxa::WindowBarMode::hidden,
                                  pxa::WindowBarMode::transient);
    if (!(ui_features & (1ull << 11)))
      notice("请更新 Host 以支持可调字号");
    start(load_state(), false);
    return {};
  }
  void on_background(pxa::Context &) {
    (void)input_ref.hide_keyboard();
    auto_deadline = 0;
    state.settings.automatic = false;
    busy = false;
    page_busy = false;
    notice("已暂停");
    save();
  }
  void on_foreground(pxa::Context &) {
    auto_deadline = 0;
    dirty = true;
  }
  pxa::BackAction on_back() {
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
    if (event.service == 3 && event.opcode == 0x8002) {
      auto value = decode_display_metrics(event.payload);
      if (value) {
        metrics = *value;
        dirty = true;
        if (screen == reading)
          start(load_page());
      }
    }
    if (event.service == 11 && event.opcode == 0x8001) {
      context->foreground_tasks().cancel();
      network_permission.reset();
      busy = false;
      notice("网络权限已关闭，已下载内容仍可阅读");
    }
    return false;
  }
  void on_frame(pxa::Context &, pxa::game::FrameTick tick) {
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
          write_file(*f, std::as_bytes(std::span{text.data(), text.size()}));
      f->close();
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
  static pxa::Result<void> write_file(pxa::File &file,
                                      std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
      auto n =
          file.write(bytes.first(std::min<std::size_t>(4096, bytes.size())));
      if (!n)
        return std::unexpected(n.error());
      if (!*n)
        return std::unexpected(pxa::Error::io_error);
      bytes = bytes.subspan(*n);
    }
    return {};
  }
  pxa::Task<void> load_state() {
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
      f->close();
      if (!state.decode(text)) {
        auto backup = co_await context->fs().rename("reader-state.json",
                                                    "reader-state.bad");
        notice(backup ? "书架记录无效，已备份为 reader-state.bad"
                      : "书架记录无效，请检查存储");
      }
    }
    if (state.shelf.empty())
      state.shelf.push_back({"林间书屋", "书页原创 · CC0", "sample:book", "", 0,
                             std::uint32_t(reader::sample_text.size()), true});
    state_ready = true;
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
    req.timeout_ms = 30000;
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
    if (busy)
      co_return pxa::Result<void>{};
    busy = true;
    notice("正在搜索…");
    auto source = state.sources[source_index];
    if (!source.enabled) {
      busy = false;
      notice("该书源已停用，请切换或启用书源");
      co_return pxa::Result<void>{};
    }
    auto key = input.get();
    auto url = explicit_url.empty() ? source.url : explicit_url;
    if (explicit_url.empty()) {
      if (key == "西游记")
        key = "西遊記";
      if (key == "红楼梦")
        key = "紅樓夢";
      if (key == "三国演义")
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
      error("搜索", body.error());
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
               : "找到 " + std::to_string(results.size()) + " 部作品");
    log("READER-SEARCH results=" + std::to_string(results.size()));
    co_return pxa::Result<void>{};
  }
  void open(std::size_t index, bool from_results) {
    if (busy)
      return;
    context->foreground_tasks().cancel();
    page_busy = false;
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
    page = {};
    page_text.clear();
    busy = true;
    change(reading);
    start(open_current());
  }
  pxa::Task<void> open_current() {
    auto &book = state.shelf[active];
    reading_partial = !book.complete;
    if (book.url.starts_with("sample:")) {
      busy = false;
      index_text(reader::sample_text, 0);
      start(load_page());
      co_return pxa::Result<void>{};
    }
    auto file = co_await context->fs().stat(filename(book));
    if (file) {
      busy = false;
      book.total = std::uint32_t(file->size);
      book.complete = true;
      reading_partial = false;
      start(index_book());
      start(load_page());
    } else {
      busy = false;
      book.complete = false;
      reading_partial = true;
      start(download_current());
    }
    co_return pxa::Result<void>{};
  }
  void index_text(std::string_view data, std::uint32_t base) {
    for (std::size_t at = 0; at < data.size();) {
      auto end = data.find('\n', at);
      if (end == data.npos)
        end = data.size();
      auto line = data.substr(at, end - at);
      while (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);
      if (reader::chapter_heading(line) &&
          chapters.size() < reader::max_chapters)
        chapters.push_back(
            {reader::shorten(line, 192), "", base + std::uint32_t(at)});
      at = end < data.size() ? end + 1 : end;
    }
  }
  pxa::Task<void> index_book() {
    auto f = co_await context->fs().open(filename(state.shelf[active]));
    if (!f)
      co_return pxa::Result<void>{};
    chapters.clear();
    auto bytes = std::span{stream_buffer};
    std::string pending;
    pending.reserve(256);
    std::uint32_t offset = 0, line_start = 0, blocks = 0;
    bool overflow = false;
    auto emit = [&] {
      while (!pending.empty() && pending.back() == '\r')
        pending.pop_back();
      if (!overflow && reader::chapter_heading(pending) &&
          chapters.size() < reader::max_chapters)
        chapters.push_back({reader::shorten(pending, 192), "", line_start});
      pending.clear();
      overflow = false;
    };
    while (true) {
      auto n = f->read(bytes);
      if (!n || !*n)
        break;
      for (std::uint32_t i = 0; i < *n; ++i) {
        char c = char(bytes[i]);
        if (c == '\n') {
          emit();
          line_start = offset + 1;
        } else if (pending.size() < 240)
          pending += c;
        else
          overflow = true;
        ++offset;
      }
      if (++blocks % 8 == 0) {
        auto yielded = co_await f->seek(offset);
        if (!yielded)
          break;
      }
    }
    emit();
    dirty = true;
    log("READER-INDEX chapters=" + std::to_string(chapters.size()));
    co_return pxa::Result<void>{};
  }
  pxa::Task<void> load_page() {
    if (page_busy || publishing || state.shelf.empty())
      co_return pxa::Result<void>{};
    page_busy = true;
    auto &book = state.shelf[active];
    if (book.url.starts_with("sample:"))
      page_text = std::string(reader::sample_text.substr(
          std::min<std::size_t>(book.position, reader::sample_text.size())));
    else {
      auto f = co_await context->fs().open(filename(book, reading_partial));
      if (!f) {
        page_busy = false;
        error("读取", f.error());
        co_return pxa::Result<void>{};
      }
      auto seek = co_await f->seek(book.position);
      if (!seek) {
        page_busy = false;
        error("翻页", seek.error());
        co_return pxa::Result<void>{};
      }
      std::size_t used = 0;
      while (used < page_buffer.size()) {
        auto n = f->read(std::span{page_buffer}.subspan(used));
        if (!n) {
          page_busy = false;
          error("读取", n.error());
          co_return pxa::Result<void>{};
        }
        if (!*n)
          break;
        used += *n;
      }
      page_text.assign(reinterpret_cast<const char *>(page_buffer.data()),
                       used);
      f->close();
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
    page = reader::paginate(page_text, layout(), state.settings);
    next_offset = book.position + std::uint32_t(page.consumed);
    page_busy = false;
    dirty = true;
    auto_deadline = 0;
    if (page.invalid_utf8)
      notice("正文编码不是有效 UTF-8，暂无法阅读");
    log("READER-PAGE offset=" + std::to_string(book.position) + " next=" +
        std::to_string(next_offset) + " lines=" + std::to_string(page.count));
    co_return pxa::Result<void>{};
  }
  void turn(int direction) {
    if (page_busy || state.shelf.empty())
      return;
    auto &b = state.shelf[active];
    if (direction > 0) {
      if (next_offset <= b.position || (b.complete && next_offset >= b.total)) {
        state.settings.automatic = false;
        notice(b.complete ? "已经读到书末" : "请等待正文缓存后再翻页");
        return;
      }
      if (previous_count == previous.size()) {
        std::move(previous.begin() + 1, previous.end(), previous.begin());
        --previous_count;
      }
      previous[previous_count++] = b.position;
      b.position = next_offset;
    } else {
      if (!previous_count && b.position) {
        start(previous_from_file());
        return;
      }
      if (previous_count)
        b.position = previous[--previous_count];
    }
    save();
    start(load_page());
  }
  pxa::Task<void> previous_from_file() {
    if (page_busy)
      co_return pxa::Result<void>{};
    page_busy = true;
    auto &b = state.shelf[active];
    auto target = b.position;
    auto start_at = target > page_buffer.size()
                        ? target - std::uint32_t(page_buffer.size())
                        : 0;
    std::string text;
    if (b.url.starts_with("sample:"))
      text =
          std::string(reader::sample_text.substr(start_at, target - start_at));
    else {
      auto f = co_await context->fs().open(filename(b, reading_partial));
      if (!f) {
        page_busy = false;
        co_return pxa::Result<void>{};
      }
      auto seek = co_await f->seek(start_at);
      if (!seek) {
        page_busy = false;
        co_return pxa::Result<void>{};
      }
      std::size_t used = 0, limit = target - start_at;
      while (used < limit) {
        auto n = f->read(std::span{page_buffer}.subspan(used, limit - used));
        if (!n || !*n)
          break;
        used += *n;
      }
      text.assign(reinterpret_cast<const char *>(page_buffer.data()), used);
    }
    std::size_t skip = 0;
    while (skip < text.size() &&
           (static_cast<unsigned char>(text[skip]) & 0xc0) == 0x80)
      ++skip;
    std::size_t at = skip, last = skip;
    while (at < text.size()) {
      auto p = reader::paginate(std::string_view(text).substr(at), layout(),
                                state.settings);
      if (!p.consumed)
        break;
      last = at;
      at += p.consumed;
    }
    b.position = start_at + std::uint32_t(last);
    page_busy = false;
    save();
    start(load_page());
    co_return pxa::Result<void>{};
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
    if (busy)
      co_return pxa::Result<void>{};
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
    auto f = co_await context->fs().open(path, pxa::OpenMode::write |
                                                   pxa::OpenMode::create |
                                                   pxa::OpenMode::truncate);
    if (!f) {
      busy = false;
      error("下载", f.error());
      co_return pxa::Result<void>{};
    }
    std::uint32_t written = 0, total = 0;
    bool good = true, first = true, replace_existing = book.complete;
    std::string validator;
    reader::Utf8Validator text_validator;
    if (!book.toc.empty()) {
      notice("读取目录…");
      auto raw = co_await fetch(book.toc);
      if (!raw) {
        error("目录", raw.error());
        good = false;
      } else {
        auto list = reader::parse_toc(*raw, book.toc);
        chapters.clear();
        if (list.empty()) {
          notice("书源目录为空或格式无效");
          good = false;
        }
        for (auto &chapter : list) {
          notice("下载 " + std::to_string(chapters.size() + 1) + "/" +
                 std::to_string(list.size()));
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
          chapters.push_back({chapter.title, "", written});
          auto write = write_file(
              *f, std::as_bytes(std::span{text.data(), text.size()}));
          if (!write) {
            error("写入", write.error());
            good = false;
            break;
          }
          written += std::uint32_t(text.size());
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
      while (good) {
        auto range = "bytes=" + std::to_string(written) + "-" +
                     std::to_string(written + reader::chunk_bytes - 1);
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
        if (!written)
          validator = current_validator;
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
            auto write = write_file(*f, std::span{bytes}.first(*n));
            if (!write) {
              error("写入", write.error());
              good = false;
              break;
            }
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
        if (!replace_existing)
          book.total = total;
        notice("已缓存 " + std::to_string(written / 1024) + " / " +
               std::to_string((total + 1023) / 1024) + " KiB");
        if (!replace_existing) {
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
    f->close();
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
          (void)co_await context->clock().now();
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
      notice("下载完成，可离线阅读");
      save();
      start(index_book());
      start(load_page());
      log("READER-DOWNLOAD complete=1 bytes=" + std::to_string(total));
    } else
      notice(status +
             (replace_existing ? " · 保留原有离线版本" : " · 可重试下载"));
    busy = false;
    co_return pxa::Result<void>{};
  }
  void render();
  void pointer(const CanvasPointer &);
  void action(Action, int);
};
void ReaderPointer::operator()(const CanvasPointer &event) const {
  app->pointer(event);
}
#include "reader_ui.inc"
PXA_APPLICATION(ReaderApp)
