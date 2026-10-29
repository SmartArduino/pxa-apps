// SPDX-License-Identifier: MIT
#include "audio.hpp"
#include "model.hpp"
#include "ui.hpp"
#include <memory>

using namespace pxa;
using namespace pxa::ui;
using namespace pxa::ui::literals;
namespace sibo {
enum class Screen {
  home,
  poetry,
  recite,
  list0,
  list1,
  detail,
  reader,
  pinyin,
  quiz,
  feedback,
  loading,
  picture,
  about,
  count
};
enum class Job {
  stories,
  mengxue,
  mathear,
  book,
  story,
  daily,
  random,
  search,
  pinyin,
  lesson,
  read
};
struct ListContext {
  std::string title;
  std::vector<Entry> rows;
  unsigned page = 0;
  bool stories = false;
};
struct Slot {
  State<std::string> title{""}, detail{""};
  State<bool> shown{false};
};
struct ListPanel {
  std::array<Slot, 6> slots;
  State<std::string> title{""}, counter{""};
  State<bool> previous{false}, next{false}, categories{false};
};
struct App;
struct AppView : ViewModifiers {
  static constexpr Capacity capacity{640, 384, 128, 0, 3};
  App *app;
  explicit AppView(App *a) : app(a) {}
  template <class Page> bool render(Page &page, uint32_t parent);
};

struct App {
  Environment<> environment;
  Context *ctx = nullptr;
  TaskScope network, images, saving;
  AudioPlayer player;
  std::array<Flag, size_t(Screen::count)> screens;
  std::array<std::function<void()>, 128> callbacks;
  State<bool> ready{false}, resume_visible{false}, is_poem{false},
      has_quiz{false}, has_image{false}, audio_active{false};
  State<std::string> subtitle{"把好奇心，装进小书包"}, summary{""},
      resume_title{""};
  State<std::string> item_title{""}, item_detail{""}, preview{""},
      favorite_label{"收藏起来"}, audio_status{""}, pause_label{"暂停"},
      notice{""};
  State<std::string> reader_text{""}, reader_counter{""}, reader_next{"下一页"},
      reader_title{"一起读一读"};
  State<bool> reader_previous{false}, pinyin_previous{false},
      pinyin_next_visible{false};
  State<std::string> pinyin_counter{""};
  std::array<Label, 32> han, py;
  State<std::string> question_text{""}, question_counter{""}, feedback_text{""},
      feedback_next{"下一题"};
  std::array<Label, 4> answers;
  std::array<Flag, 4> answer_visible;
  State<std::string> loading_text{"正在打开书包…"}, picture_caption{""},
      about_summary{""};
  std::array<ListPanel, 2> panels;
  CanvasRef hero_canvas, cover_canvas;
  CanvasCommands<2400> drawing;
  std::optional<Asset> cover;
  Screen screen = Screen::home, loading_from = Screen::home,
         detail_from = Screen::home, reader_from = Screen::detail,
         list_base = Screen::home;
  std::vector<ListContext> lists;
  std::vector<Entry> story_items, course_assets, favorites;
  std::vector<Category> story_categories;
  Entry selected, bookmark;
  std::vector<std::string> pages;
  std::vector<std::vector<Cell>> pinyin_pages;
  std::vector<Question> questions;
  unsigned list_panel = 0, reader_index = 0, pinyin_index = 0,
           question_index = 0, round_score = 0, bookmark_page = 0;
  std::array<uint32_t, 4> cache_ages{};
  uint32_t cache_clock = 0;
  unsigned read_count = 0, answered_count = 0, correct_count = 0, revision = 0,
           saved_revision = 0;
  bool save_running = false, from_cache = false;
  unsigned canvas_width = 200, pinyin_columns = 4, pinyin_rows = 2;
  uint64_t generation = 0;
  std::array<std::byte, 2048> network_chunk{};

  unsigned width() {
    auto d = environment.display().get();
    return unsigned(uint64_t(d.width) * 65536 / std::max(1u, d.density_q16));
  }
  unsigned height() {
    auto d = environment.display().get();
    return unsigned(uint64_t(d.height) * 65536 / std::max(1u, d.density_q16));
  }
  void show(Screen value) {
    screen = value;
    for (unsigned i = 0; i < screens.size(); ++i)
      screens[i].value.set(i == unsigned(value));
  }
  void message(std::string value) { notice.set(std::move(value)); }
  void cancel() {
    ++generation;
    network.cancel();
    images.cancel();
  }
  void refresh_summary() {
    summary.set("读完 " + std::to_string(read_count) + " 篇 · 答对 " +
                std::to_string(correct_count) + " 题");
    resume_visible.set(!bookmark.title.empty());
    resume_title.set("继续读 · " + bookmark.title);
    about_summary.set(summary.get() + "\n收藏 " +
                      std::to_string(favorites.size()) + " / 12 篇");
  }
  static bool same(const Entry &a, const Entry &b) {
    return a.kind == b.kind && a.id == b.id;
  }
  void detail_update() {
    item_title.set(selected.title);
    item_detail.set(selected.detail);
    is_poem.set(selected.kind == Kind::poem);
    auto first = study::reading_pages(selected.body, 28, 3);
    preview.set(first.front());
    favorite_label.set(
        std::any_of(favorites.begin(), favorites.end(),
                    [&](const auto &e) { return same(e, selected); })
            ? "已收藏 · 再按移除"
            : "收藏起来");
    has_quiz.set(selected.kind == Kind::poem || !selected.questions.empty());
    has_image.set(selected.course || !selected.image.empty());
    message("");
  }
  void opened(Entry entry, Screen from) {
    selected = std::move(entry);
    detail_from = from;
    detail_update();
    show(Screen::detail);
  }
  void open_entry(Entry entry) {
    player.stop();
    images.cancel();
    cover.reset();
    if (entry.kind == Kind::category) {
      if (!lists.empty())
        lists.pop_back();
      if (lists.empty())
        return;
      auto &list = lists.back();
      list.rows.clear();
      for (const auto &item : story_items)
        if (entry.id.empty() || item.category == entry.id)
          list.rows.push_back(item);
      list.title = entry.title;
      list.page = 0;
      paint_list();
      return;
    }
    if (entry.kind == Kind::dynasty) {
      begin(Job::search, entry.title);
      return;
    }
    if ((entry.kind == Kind::mengxue || entry.kind == Kind::mathear) &&
        entry.body.empty()) {
      selected = std::move(entry);
      begin(Job::book);
      return;
    }
    if (entry.kind == Kind::story && entry.body.empty()) {
      selected = std::move(entry);
      begin(Job::story);
      return;
    }
    opened(std::move(entry), screen);
  }
  void push_list(std::string title, std::vector<Entry> entries,
                 bool stories = false) {
    lists.push_back({std::move(title), std::move(entries), 0, stories});
    paint_list();
  }
  void root_list(std::string title, std::vector<Entry> entries, Screen base,
                 bool stories = false) {
    lists.clear();
    list_base = base;
    push_list(std::move(title), std::move(entries), stories);
  }
  void paint_list() {
    if (lists.empty()) {
      show(list_base);
      return;
    }
    list_panel ^= 1;
    auto &view = panels[list_panel];
    auto &list = lists.back();
    const auto total = std::max<size_t>(1, (list.rows.size() + 5) / 6);
    list.page = std::min<unsigned>(list.page, unsigned(total - 1));
    view.title.set(list.title);
    view.counter.set(std::to_string(list.page + 1) + " / " +
                     std::to_string(total) + " 页 · " +
                     std::to_string(list.rows.size()) + " 项");
    for (unsigned i = 0; i < 6; ++i) {
      auto index = list.page * 6 + i;
      bool valid = index < list.rows.size();
      auto &slot = view.slots[i];
      slot.shown.set(valid);
      slot.title.set(valid ? list.rows[index].title : "");
      slot.detail.set(valid ? list.rows[index].detail : "");
    }
    view.previous.set(list.page > 0);
    view.next.set(list.page + 1 < total);
    view.categories.set(list.stories && !story_categories.empty());
    show(list_panel ? Screen::list1 : Screen::list0);
  }
  void list_turn(int delta) {
    if (lists.empty())
      return;
    auto &l = lists.back();
    if (delta < 0 && l.page)
      --l.page;
    else if (delta > 0 && (l.page + 1) * 6 < l.rows.size())
      ++l.page;
    paint_list();
  }
  void list_pick(unsigned index) {
    if (lists.empty())
      return;
    index += lists.back().page * 6;
    if (index < lists.back().rows.size())
      open_entry(lists.back().rows[index]);
  }
  void categories() {
    std::vector<Entry> rows;
    for (const auto &c : story_categories) {
      Entry e;
      e.kind = Kind::category;
      e.id = c.id;
      e.title = c.name;
      rows.push_back(std::move(e));
    }
    push_list("故事小书架", std::move(rows));
  }
  void authors() {
    std::vector<Entry> rows;
    for (auto name :
         {"李白", "杜甫", "白居易", "王维", "苏轼", "李清照", "孟浩然",
          "王昌龄", "杜牧", "刘禹锡", "杨万里", "陆游", "辛弃疾", "纳兰性德"}) {
      Entry e;
      e.kind = Kind::dynasty;
      e.title = name;
      rows.push_back(std::move(e));
    }
    root_list("找一位诗人", std::move(rows), Screen::poetry);
  }
  void back() {
    if (screen == Screen::loading) {
      cancel();
      if (loading_from == Screen::list0 || loading_from == Screen::list1)
        paint_list();
      else
        show(loading_from);
      return;
    }
    if (screen == Screen::reader) {
      show(reader_from);
      return;
    }
    if (screen == Screen::pinyin || screen == Screen::quiz ||
        screen == Screen::feedback || screen == Screen::picture) {
      images.cancel();
      cover.reset();
      show(Screen::detail);
      return;
    }
    if (screen == Screen::detail) {
      cancel();
      player.stop();
      if (detail_from == Screen::list0 || detail_from == Screen::list1)
        paint_list();
      else
        show(detail_from);
      return;
    }
    if (screen == Screen::list0 || screen == Screen::list1) {
      if (!lists.empty())
        lists.pop_back();
      paint_list();
      return;
    }
    player.stop();
    cancel();
    refresh_summary();
    show(Screen::home);
    draw_hero();
  }
  void reader_update() {
    reader_text.set(pages[reader_index]);
    reader_counter.set(std::to_string(reader_index + 1) + " / " +
                       std::to_string(pages.size()) + " 页");
    reader_previous.set(reader_index > 0);
    reader_next.set(reader_index + 1 < pages.size() ? "下一页" : "读完啦");
  }
  void read(std::string body, std::string title, Screen from = Screen::detail,
            unsigned index = 0) {
    reader_from = from;
    reader_title.set(std::move(title));
    pages = study::reading_pages(body, width() > 48 ? (width() - 48) / 10 : 12,
                                 height() > 166 ? (height() - 166) / 26 : 3);
    reader_index = std::min<unsigned>(index, unsigned(pages.size() - 1));
    reader_update();
    show(Screen::reader);
  }
  void remember() {
    bookmark = selected;
    bookmark_page = reader_index;
    refresh_summary();
    save();
  }
  void next_page() {
    if (reader_index + 1 < pages.size()) {
      ++reader_index;
      reader_update();
      if (reader_from == Screen::detail && reader_title.get() == selected.title)
        remember();
    } else {
      if (reader_from == Screen::detail &&
          reader_title.get() == selected.title) {
        ++read_count;
        reader_index = 0;
        remember();
      }
      show(reader_from);
    }
  }
  void read_selected(unsigned index = 0) {
    read(selected.body, selected.title, Screen::detail, index);
    remember();
  }
  void resume() {
    player.stop();
    opened(bookmark, Screen::home);
    read_selected(bookmark_page);
  }
  void toggle_favorite() {
    auto it = std::find_if(favorites.begin(), favorites.end(),
                           [&](const auto &e) { return same(e, selected); });
    if (it != favorites.end())
      favorites.erase(it);
    else {
      if (favorites.size() == 12) {
        message("书包装满啦，先移除一篇收藏吧。");
        return;
      }
      size_t favorite_bytes = serialize(selected).size();
      for (const auto &e : favorites)
        favorite_bytes += serialize(e).size();
      if (selected.body.size() > 8192 || favorite_bytes > 96 * 1024) {
        message("这篇较长，可以用“继续读”保留阅读位置。");
        return;
      }
      favorites.push_back(selected);
    }
    detail_update();
    refresh_summary();
    save();
  }
  void pinyin_update() {
    for (unsigned i = 0; i < 32; ++i) {
      bool valid = i < pinyin_pages[pinyin_index].size();
      han[i].value.set(valid ? pinyin_pages[pinyin_index][i].han : "");
      py[i].value.set(valid ? pinyin_pages[pinyin_index][i].py : "");
    }
    pinyin_counter.set("拼音 " + std::to_string(pinyin_index + 1) + " / " +
                       std::to_string(pinyin_pages.size()));
    pinyin_previous.set(pinyin_index > 0);
    pinyin_next_visible.set(pinyin_index + 1 < pinyin_pages.size());
  }
  void open_pinyin() {
    if (selected.pinyin.empty()) {
      begin(Job::pinyin);
      return;
    }
    pinyin_pages.clear();
    std::vector<Cell> current;
    const auto capacity = pinyin_columns * pinyin_rows;
    for (const auto &cell : selected.pinyin) {
      if (cell.han == "\n") {
        while (current.size() % pinyin_columns)
          current.push_back({"", ""});
      } else
        current.push_back(cell);
      if (current.size() >= capacity) {
        pinyin_pages.push_back(std::move(current));
        current.clear();
      }
    }
    if (!current.empty())
      pinyin_pages.push_back(std::move(current));
    if (pinyin_pages.empty()) {
      message("这首诗还没有拼音。");
      return;
    }
    pinyin_index = 0;
    pinyin_update();
    show(Screen::pinyin);
  }
  void quiz_update() {
    auto &q = questions[question_index];
    question_text.set(q.text);
    question_counter.set("小挑战 " + std::to_string(question_index + 1) +
                         " / " + std::to_string(questions.size()));
    for (unsigned i = 0; i < 4; ++i) {
      answers[i].value.set(q.options[i]);
      answer_visible[i].value.set(i < q.count);
    }
    show(Screen::quiz);
  }
  void quiz() {
    questions = make_quiz(selected);
    if (questions.empty()) {
      message("这篇暂时没有小挑战，先读一读吧。");
      return;
    }
    question_index = round_score = 0;
    quiz_update();
  }
  void answer(unsigned index) {
    if (screen != Screen::quiz || index >= questions[question_index].count)
      return;
    auto &q = questions[question_index];
    bool correct = index == q.answer;
    ++answered_count;
    if (correct) {
      ++correct_count;
      ++round_score;
    }
    feedback_text.set(
        std::string(correct ? "答对啦！\n\n" : "再想一想，也在进步。\n\n") +
        "答案：" + q.options[q.answer] +
        (q.explanation.empty() ? "" : "\n\n" + q.explanation));
    feedback_next.set(question_index + 1 < questions.size() ? "下一题"
                                                            : "看看收获");
    save();
    show(Screen::feedback);
  }
  void next_question() {
    if (question_index + 1 < questions.size()) {
      ++question_index;
      quiz_update();
    } else {
      refresh_summary();
      read("这一轮答对 " + std::to_string(round_score) + " / " +
               std::to_string(questions.size()) +
               " 题。\n\n带着今天的小发现，再读一遍《" + selected.title +
               "》吧！",
           "收获小星星");
    }
  }
  void audio_changed() {
    audio_status.set(player.status +
                     (player.active ? " · " + player.position() : ""));
    audio_active.set(player.active);
    pause_label.set(player.paused ? "继续听" : "暂停");
  }
  void listen() {
    if (!selected.audio.empty())
      player.start(*ctx, selected.audio);
    else if (selected.kind == Kind::poem)
      begin(Job::read);
    else
      message("这篇暂时没有朗读音频。");
  }
  void lesson() {
    if (!selected.meta.empty())
      read(selected.meta, "诗意小课堂");
    else
      begin(Job::lesson);
  }
  void begin(Job job, std::string query = {}) {
    auto previous = screen;
    cancel();
    player.stop();
    loading_from = previous;
    loading_text.set(job == Job::lesson
                         ? "正在准备诗意讲解…\n第一次可能需要稍等一会儿。"
                         : "正在翻开新的一页…\n内容会留一份轻量缓存。");
    show(Screen::loading);
    auto tag = generation;
    if (!network.start(load(job, std::move(query), tag))) {
      show(previous);
      message("没有启动成功，请再试一次。");
    }
  }
  Task<std::string> read_file(std::string path, size_t limit) {
    auto file = co_await ctx->fs().open(path);
    if (!file)
      co_return std::unexpected(file.error());
    std::array<std::byte, 2048> chunk{};
    std::string bytes;
    for (;;) {
      auto n = file->read(chunk);
      if (!n)
        co_return std::unexpected(n.error());
      if (!*n)
        break;
      if (bytes.size() + *n > limit)
        co_return std::unexpected(Error::resource_limit);
      bytes.append(reinterpret_cast<const char *>(chunk.data()), *n);
    }
    co_return bytes;
  }
  Task<void> write_file(std::string path, std::string bytes, std::string temp) {
    auto file = co_await ctx->fs().open(
        temp, OpenMode::write | OpenMode::create | OpenMode::truncate);
    if (!file)
      co_return std::unexpected(file.error());
    auto remaining = std::as_bytes(std::span{bytes.data(), bytes.size()});
    while (!remaining.empty()) {
      auto n = file->write(
          remaining.first(std::min<size_t>(4096, remaining.size())));
      if (!n || !*n)
        co_return std::unexpected(Error::io_error);
      remaining = remaining.subspan(*n);
    }
    auto closed = file->close();
    if (!closed)
      co_return closed;
    co_return co_await ctx->fs().replace(temp, path);
  }
  Task<std::string> download(std::string address, std::string body) {
    auto permission = co_await ctx->permissions().acquire("net.client", "web");
    if (!permission)
      co_return std::unexpected(permission.error());
    static constexpr std::array<std::string_view, 1> wanted{"content-type"};
    static constexpr std::array<NetHeaderView, 1> headers{
        {{"content-type", "application/x-www-form-urlencoded"}}};
    auto request = co_await ctx->http().open(
        {.url = address,
         .method = body.empty() ? HttpMethod::get : HttpMethod::post,
         .headers = headers,
         .wanted_headers = wanted,
         .body = std::as_bytes(std::span{body.data(), body.size()}),
         .options = {.buffer_bytes = 4096,
                     .header_bytes = 512,
                     .max_body_bytes = json_limit,
                     .timeout_ms = 90000}},
        *permission);
    if (!request)
      co_return std::unexpected(request.error());
    auto response = co_await std::move(*request).response();
    if (!response)
      co_return std::unexpected(response.error());
    if (response->status_code != 200)
      co_return std::unexpected(Error::not_found);
    std::string bytes;
    for (;;) {
      auto n = co_await response->body.read_some(network_chunk);
      if (!n)
        co_return std::unexpected(n.error());
      if (!*n)
        break;
      if (bytes.size() + *n > json_limit)
        co_return std::unexpected(Error::resource_limit);
      bytes.append(reinterpret_cast<char *>(network_chunk.data()), *n);
    }
    if (!study::complete_json(bytes))
      co_return std::unexpected(Error::protocol_error);
    co_return compact(bytes);
  }
  Task<std::string> fetch(std::string address, std::string body = {},
                          bool fresh = false) {
    if (!trusted(address) || body.size() > 3500)
      co_return std::unexpected(Error::invalid_argument);
    std::string key = address + "?" + body;
    // Four independent slots with recency, rather than hashing two related
    // requests onto one slot (catalog and book must coexist for offline use).
    std::optional<std::string> cached;
    int found = -1, empty = -1;
    for (unsigned i = 0; i < 4; ++i) {
      auto file =
          co_await read_file("cache-" + std::to_string(i) + ".bin", 100 * 1024);
      if (!file) {
        if (empty < 0)
          empty = int(i);
        continue;
      }
      auto newline = file->find('\n');
      if (newline == file->npos) {
        if (empty < 0)
          empty = int(i);
        continue;
      }
      if (file->substr(0, newline) == key &&
          study::complete_json(std::string_view(*file).substr(newline + 1))) {
        found = int(i);
        cached = file->substr(newline + 1);
        cache_ages[i] = ++cache_clock;
        break;
      }
    }
    if (cached && !fresh) {
      from_cache = true;
      co_return std::move(*cached);
    }
    auto bytes = co_await download(address, body);
    if (!bytes) {
      if (cached) {
        from_cache = true;
        co_return std::move(*cached);
      }
      co_return std::unexpected(bytes.error());
    }
    from_cache = false;
    if (bytes->size() + key.size() + 1 <= 96 * 1024) {
      unsigned slot = found >= 0 ? unsigned(found)
                      : empty >= 0
                          ? unsigned(empty)
                          : unsigned(std::min_element(cache_ages.begin(),
                                                      cache_ages.end()) -
                                     cache_ages.begin());
      auto saved = co_await write_file("cache-" + std::to_string(slot) + ".bin",
                                       key + "\n" + *bytes, "cache.tmp");
      if (saved) {
        cache_ages[slot] = ++cache_clock;
        std::string order = "[";
        for (unsigned i = 0; i < 4; ++i) {
          if (i)
            order += ",";
          order += std::to_string(cache_ages[i]);
        }
        order += "]";
        (void)co_await write_file("cache-order.json", std::move(order),
                                  "cache-order.tmp");
      }
    }
    co_return std::move(*bytes);
  }
  void failed(uint64_t tag) {
    if (tag != generation)
      return;
    auto previous = loading_from;
    if (previous == Screen::list0 || previous == Screen::list1)
      paint_list();
    else
      show(previous);
    message("暂时没有取到内容。请检查网络，或先读课本古诗。\n已缓存的内容可以离"
            "线继续读。");
  }
  Task<void> load(Job job, std::string query, uint64_t tag) {
    std::string address, body;
    if (job == Job::stories)
      address = std::string(bases[0]) + "/catalog.json";
    else if (job == Job::mengxue || job == Job::mathear)
      address = std::string(bases[1]) + "/catalog.json";
    else if (job == Job::book || job == Job::story)
      address = selected.path;
    else if (job == Job::daily || job == Job::random)
      address = std::string(api) + "/api/poem/" +
                (job == Job::daily ? "daily" : "random");
    else if (job == Job::search)
      address = std::string(api) + "/api/poem/search?q=" + encode(query) +
                "&limit=12";
    else {
      if (selected.course) {
        if (course_assets.empty()) {
          auto index = co_await fetch(std::string(bases[3]) + "/index.json");
          if (tag != generation)
            co_return Result<void>{};
          if (index)
            course_assets = parse_poems(*index, true);
        }
        auto asset =
            std::find_if(course_assets.begin(), course_assets.end(),
                         [&](const auto &e) { return e.id == selected.id; });
        if (asset != course_assets.end()) {
          auto data = co_await fetch(asset->path);
          if (tag != generation)
            co_return Result<void>{};
          if (data && enrich(*data, selected)) {
            selected.audio = asset->audio;
            selected.image = std::string(api) + "/api/poem/course_cover/" +
                             selected.id + ".jpg";
            if (job == Job::read) {
              show(Screen::detail);
              player.start(*ctx, selected.audio);
            } else if (job == Job::pinyin) {
              show(Screen::detail);
              if (selected.pinyin.empty()) {
                failed(tag);
                co_return Result<void>{};
              }
              open_pinyin();
            } else {
              show(Screen::detail);
              if (selected.meta.empty()) {
                failed(tag);
                co_return Result<void>{};
              }
              lesson();
            }
            co_return Result<void>{};
          }
        }
      }
      address = std::string(api) + "/api/poem/" +
                (job == Job::pinyin   ? "pinyin"
                 : job == Job::lesson ? "lesson"
                                      : "read");
      body = job == Job::pinyin ? "lines=" + encode(joined(selected.lines))
                                : form(selected);
      if (job == Job::read)
        body += "&part=poem";
    }
    auto bytes =
        co_await fetch(address, body, job == Job::daily || job == Job::random);
    if (tag != generation)
      co_return Result<void>{};
    if (!bytes) {
      failed(tag);
      co_return Result<void>{};
    }
    if (job == Job::stories) {
      story_items = parse_catalog(*bytes, Kind::story, &story_categories);
      if (story_items.empty())
        failed(tag);
      else
        root_list("故事小书架", story_items, Screen::home, true);
    } else if (job == Job::mengxue || job == Job::mathear) {
      auto rows = parse_catalog(*bytes, job == Job::mengxue ? Kind::mengxue
                                                            : Kind::mathear);
      if (rows.empty())
        failed(tag);
      else
        root_list(job == Job::mengxue ? "蒙学诵读" : "小学听题本",
                  std::move(rows), Screen::recite);
    } else if (job == Job::book) {
      auto rows = parse_tracks(*bytes, selected.kind);
      if (rows.empty())
        failed(tag);
      else
        push_list(selected.title, std::move(rows));
    } else if (job == Job::story) {
      if (!parse_story(*bytes, selected))
        failed(tag);
      else
        opened(std::move(selected), loading_from);
    } else if (job == Job::search) {
      auto rows = parse_poems(*bytes);
      if (rows.empty())
        failed(tag);
      else
        push_list(query + "的诗", std::move(rows));
    } else if (job == Job::daily || job == Job::random) {
      Entry e;
      if (!parse_poem(study::json(*bytes), e))
        failed(tag);
      else
        opened(std::move(e), Screen::poetry);
    } else if (job == Job::read) {
      auto root = study::json(*bytes);
      auto url = str(root.get("url"), 1024);
      if (url.starts_with("/"))
        url = std::string(origin) + url;
      if (!trusted(url))
        failed(tag);
      else {
        selected.audio = url;
        show(Screen::detail);
        player.start(*ctx, url);
      }
    } else {
      if (!enrich(*bytes, selected) ||
          (job == Job::pinyin ? selected.pinyin.empty()
                              : selected.meta.empty()))
        failed(tag);
      else {
        show(Screen::detail);
        if (job == Job::pinyin)
          open_pinyin();
        else
          lesson();
      }
    }
    co_return Result<void>{};
  }
  Task<void> save_loop() {
    while (saved_revision != revision) {
      auto rev = revision;
      std::string bytes =
          "{\"read\":" + std::to_string(read_count) +
          ",\"answered\":" + std::to_string(answered_count) +
          ",\"correct\":" + std::to_string(correct_count) +
          ",\"page\":" + std::to_string(bookmark_page) + ",\"bookmark\":" +
          (bookmark.title.empty() ? "null" : serialize(bookmark)) +
          ",\"favorites\":[";
      bool first = true;
      for (auto &item : favorites) {
        if (!first)
          bytes += ",";
        first = false;
        bytes += serialize(item);
      }
      bytes += "]}";
      auto saved = co_await write_file("personal.json", std::move(bytes),
                                       "personal.tmp");
      if (!saved) {
        message("存储空间不足，刚才的记录还未保存。后续操作时会再试。");
        save_running = false;
        co_return Result<void>{};
      }
      saved_revision = rev;
    }
    save_running = false;
    co_return Result<void>{};
  }
  void save() {
    ++revision;
    if (save_running)
      return;
    saving.reap();
    save_running = true;
    if (!saving.start(save_loop())) {
      save_running = false;
      message("记录暂未保存，请稍后再试。");
    }
  }
  Task<void> clear_cache() {
    for (unsigned i = 0; i < 4; ++i)
      (void)co_await ctx->fs().remove("cache-" + std::to_string(i) + ".bin");
    (void)co_await ctx->fs().remove("cache.tmp");
    (void)co_await ctx->fs().remove("cache-order.json");
    (void)co_await ctx->fs().remove("cache-order.tmp");
    cache_ages.fill(0);
    cache_clock = 0;
    course_assets.clear();
    message("在线缓存清理好了，收藏与阅读位置都保留着。");
    co_return Result<void>{};
  }
  Task<void> initialize() {
    auto bytes = co_await read_file("personal.json", 256 * 1024);
    if (bytes && study::complete_json(*bytes)) {
      auto root = study::json(*bytes);
      read_count = std::min<uint64_t>(root.get("read").number(), 1000000);
      answered_count =
          std::min<uint64_t>(root.get("answered").number(), 1000000);
      correct_count =
          std::min<uint64_t>(root.get("correct").number(), answered_count);
      bookmark_page = std::min<uint64_t>(root.get("page").number(), 4096);
      auto value = root.get("bookmark");
      if (value)
        deserialize(
            std::string_view(reinterpret_cast<const char *>(value.p), value.n),
            bookmark);
      media::json::Array a(root.get("favorites"));
      Value v;
      while (a.next(v) && favorites.size() < 12) {
        Entry e;
        if (deserialize(
                std::string_view(reinterpret_cast<const char *>(v.p), v.n),
                e) &&
            e.body.size() <= 8192)
          favorites.push_back(std::move(e));
      }
    }
    auto order = co_await read_file("cache-order.json", 256);
    if (order && study::complete_json(*order)) {
      media::json::Array ages(study::json(*order));
      Value age;
      unsigned i = 0;
      while (ages.next(age) && i < 4) {
        cache_ages[i++] =
            unsigned(std::min<uint64_t>(age.number(), 1000000000));
      }
      cache_clock = *std::max_element(cache_ages.begin(), cache_ages.end());
    }
    refresh_summary();
    show(Screen::home);
    ready.set(true);
    draw_hero();
    co_return Result<void>{};
  }
  void draw_hero() {
    if (!hero_canvas.mounted())
      return;
    drawing.reset();
    const int x = int(canvas_width) / 2;
    drawing.rect(0, 0, canvas_width, 72, 0xf6e5bfff, 18)
        .rect(x - 51, 18, 48, 42, white, 5)
        .rect(x + 3, 18, 48, 42, 0xe0f1e5ff, 5)
        .line(x, 20, x, 61, green, 3)
        .line(x - 40, 30, x - 14, 30, 0xe3b85cff, 3)
        .line(x - 40, 39, x - 20, 39, 0xe3b85cff, 3)
        .line(x + 14, 31, x + 39, 31, 0x75b49bff, 3)
        .line(x + 14, 40, x + 34, 40, 0x75b49bff, 3)
        .rect(x - 8, 5, 16, 16, 0xf3ba49ff, 8)
        .rect(15, 18, 5, 5, green, 2)
        .rect(canvas_width - 24, 48, 5, 5, 0xd98b65ff, 2);
    (void)hero_canvas.present(drawing);
  }
  void picture() {
    cancel();
    loading_from = Screen::detail;
    loading_text.set("正在打开插图…");
    show(Screen::loading);
    auto tag = generation;
    std::string url = selected.image;
    if (selected.course)
      url = std::string(api) + "/api/poem/course_cover/" + selected.id + ".jpg";
    if (!images.start(load_picture(std::move(url), tag))) {
      show(Screen::detail);
      message("插图暂时无法打开。");
    }
  }
  Task<void> load_picture(std::string url, uint64_t tag) {
    if (!trusted(url)) {
      failed(tag);
      co_return Result<void>{};
    }
    auto grant = co_await ctx->permissions().acquire("net.client", "web");
    if (!grant) {
      failed(tag);
      co_return Result<void>{};
    }
    static constexpr std::array<std::string_view, 1> wanted{"content-type"};
    auto request =
        co_await ctx->http().open({.url = url,
                                   .wanted_headers = wanted,
                                   .options = {.buffer_bytes = 4096,
                                               .header_bytes = 512,
                                               .max_body_bytes = 128 * 1024,
                                               .timeout_ms = 20000}},
                                  *grant);
    if (!request) {
      failed(tag);
      co_return Result<void>{};
    }
    auto response = co_await std::move(*request).response();
    if (!response || response->status_code != 200) {
      failed(tag);
      co_return Result<void>{};
    }
    auto decoded = co_await ctx->assets().decode_image(
        std::move(response->body),
        {.max_width = uint16_t(canvas_width),
         .max_height = 150,
         .max_pixels = 32768,
         .max_encoded_bytes = 128 * 1024,
         .formats = ImageFormats::jpeg | ImageFormats::png});
    if (tag != generation)
      co_return Result<void>{};
    if (!decoded) {
      failed(tag);
      co_return Result<void>{};
    }
    cover.emplace(std::move(*decoded));
    picture_caption.set(selected.title);
    show(Screen::picture);
    drawing.reset();
    drawing.image({0, 0, int(canvas_width), 150}, *cover, ImageFit::contain);
    (void)cover_canvas.present(drawing);
    co_return Result<void>{};
  }
  Result<void> on_start(Context &context, std::span<const std::byte> config) {
    ctx = &context;
    auto result = environment.initialize(config);
    if (!result)
      return result;
    canvas_width = std::clamp(width() > 22 ? width() - 22 : 120, 120u, 280u);
    pinyin_columns =
        std::clamp((width() > 36 ? width() - 36 : 120) / 45, 3u, 6u);
    pinyin_rows =
        std::clamp((height() > 150 ? height() - 150 : 58) / 58, 1u, 3u);
    player.changed = [this] { audio_changed(); };
    (void)ctx->window().fullscreen(WindowBarMode::hidden,
                                   WindowBarMode::hidden);
    return ctx->tasks().start(initialize());
  }
  Result<bool> on_event(Context &, const Event &event) {
    network.reap();
    images.reap();
    saving.reap();
    if (event.service == 4 && event.opcode == 0x8001) {
      player.tick();
      return true;
    }
    return environment.update(event);
  }
  void on_foreground(Context &context) {
    (void)context.window().fullscreen(WindowBarMode::hidden,
                                      WindowBarMode::hidden);
    draw_hero();
  }
  BackAction on_back() {
    if (screen == Screen::home)
      return BackAction::close;
    back();
    return BackAction::stay;
  }
  void on_background(Context &) {
    player.stop();
    if (screen == Screen::loading)
      back();
    else
      cancel();
  }
  auto view() {
    return SafeArea(environment.display(), AppView(this))
        .background(paper)
        .padding(11_dp)
        .enabled(ready);
  }
};

template <class Page> bool AppView::render(Page &page, uint32_t parent) {
  auto &a = *app;
  Painter p(page, a.callbacks);
  auto root = p.box(parent);
  p.fill(root, true);
  auto stage = [&](Screen s) {
    return p.frame(root, a.screens[unsigned(s)].value);
  };
  auto header = [&](uint32_t f, auto &title) {
    p.header(f, title, [&a] { a.back(); });
  };
  auto notice = [&](uint32_t b) { p.text(b, a.notice, Font::caption, muted); };
  {
    auto f = stage(Screen::home), b = p.scroll(p.body(f));
    p.canvas(b, a.hero_canvas, a.canvas_width, 72);
    p.text(b, a.subtitle, Font::title, green);
    p.text(b, a.summary, Font::caption, muted);
    auto resume = p.button(b, a.resume_title, [&a] { a.resume(); }, true);
    p.visible(resume, a.resume_visible);
    p.button(
        b, "诗词小天地",
        [&a] {
          a.show(Screen::poetry);
          a.message("");
        },
        true);
    p.button(b, "故事小书架", [&a] { a.begin(Job::stories); });
    p.button(b, "读出声来", [&a] {
      a.show(Screen::recite);
      a.message("");
    });
    p.button(b, "我的收藏",
             [&a] { a.root_list("我的收藏", a.favorites, Screen::home); });
    p.button(b, "小书包与说明", [&a] {
      a.refresh_summary();
      a.show(Screen::about);
    });
    notice(b);
    auto title = std::string_view("四博学习宝");
    p.header(
        f, title,
        [&a] {
          a.player.stop();
          (void)a.ctx->window().request_back();
        },
        true);
  }
  {
    auto f = stage(Screen::poetry), b = p.scroll(p.body(f));
    p.text(b, "把一首诗，读成一幅画", Font::title, green);
    p.text(b, "75 首课本古诗，原文离线可读。", Font::body, muted);
    p.button(
        b, "课本古诗 · 75 首",
        [&a] { a.root_list("课本古诗", course(), Screen::poetry); }, true);
    p.button(b, "今天读一首", [&a] { a.begin(Job::daily); });
    p.button(b, "偶遇一首诗", [&a] { a.begin(Job::random); });
    p.button(b, "找一位诗人", [&a] { a.authors(); });
    notice(b);
    auto title = std::string_view("诗词小天地");
    header(f, title);
  }
  {
    auto f = stage(Screen::recite), b = p.scroll(p.body(f));
    p.text(b, "耳朵听，脑袋想", Font::title, green);
    p.text(b, "跟着声音慢慢读，听完再想一想。", Font::body, muted);
    p.button(b, "蒙学诵读", [&a] { a.begin(Job::mengxue); }, true);
    p.button(b, "小学听题本", [&a] { a.begin(Job::mathear); });
    notice(b);
    auto title = std::string_view("读出声来");
    header(f, title);
  }
  for (unsigned panel = 0; panel < 2; ++panel) {
    auto &v = a.panels[panel];
    auto f = stage(panel ? Screen::list1 : Screen::list0),
         b = p.scroll(p.body(f));
    p.text(b, v.counter, Font::caption, muted);
    for (unsigned i = 0; i < 6; ++i) {
      auto n = p.button_root(b, [&a, i] { a.list_pick(i); }, i == 0);
      p.text(n, v.slots[i].title, Font::title, i == 0 ? white : ink);
      p.text(n, v.slots[i].detail, Font::caption, i == 0 ? white : muted);
      p.visible(n, v.slots[i].shown);
    }
    auto next = p.button(b, "下一页", [&a] { a.list_turn(1); });
    p.visible(next, v.next);
    auto prev = p.button(b, "上一页", [&a] { a.list_turn(-1); });
    p.visible(prev, v.previous);
    auto categories = p.button(b, "换个故事分类", [&a] { a.categories(); });
    p.visible(categories, v.categories);
    notice(b);
    header(f, v.title);
  }
  {
    auto f = stage(Screen::detail), b = p.scroll(p.body(f));
    p.text(b, a.item_title, Font::headline, green);
    p.text(b, a.item_detail, Font::caption, muted);
    p.text(b, a.preview, Font::body);
    p.button(b, "开始阅读", [&a] { a.read_selected(); }, true);
    p.button(b, "听一听朗读", [&a] { a.listen(); });
    p.text(b, a.audio_status, Font::caption, muted);
    auto pause = p.button(b, a.pause_label, [&a] { a.player.pause_resume(); });
    p.visible(pause, a.audio_active);
    auto stop = p.button(b, "停止朗读", [&a] { a.player.stop(); });
    p.visible(stop, a.audio_active);
    auto py = p.button(b, "拼音伴读", [&a] { a.open_pinyin(); });
    p.visible(py, a.is_poem);
    auto lesson = p.button(b, "诗意小课堂", [&a] { a.lesson(); });
    p.visible(lesson, a.is_poem);
    auto quiz = p.button(b, "来个小挑战", [&a] { a.quiz(); });
    p.visible(quiz, a.has_quiz);
    auto pic = p.button(b, "看看插图", [&a] { a.picture(); });
    p.visible(pic, a.has_image);
    p.button(b, a.favorite_label, [&a] { a.toggle_favorite(); });
    notice(b);
    auto title = std::string_view("发现一本好书");
    header(f, title);
  }
  {
    auto f = stage(Screen::reader), wrap = p.body(f), b = p.scroll(wrap);
    p.text(b, a.reader_counter, Font::caption, green);
    auto text = p.text(b, a.reader_text, Font::title);
    p.padding(text, 8, 5, 8, 5);
    auto row = p.box(wrap, true);
    auto next = p.button(row, a.reader_next, [&a] { a.next_page(); }, true);
    p.ok &= p.tx.u16(next, protocol::grow, 1);
    auto prev = p.button(row, "上一页", [&a] {
      if (a.reader_index) {
        --a.reader_index;
        a.reader_update();
        if (a.reader_title.get() == a.selected.title)
          a.remember();
      }
    });
    p.visible(prev, a.reader_previous);
    p.ok &= p.tx.u16(prev, protocol::grow, 1);
    header(f, a.reader_title);
  }
  {
    auto f = stage(Screen::pinyin), wrap = p.body(f), b = p.scroll(wrap);
    p.text(b, a.pinyin_counter, Font::caption, green);
    for (unsigned row = 0; row < a.pinyin_rows; ++row) {
      auto r = p.box(b, true, 1);
      for (unsigned col = 0; col < a.pinyin_columns; ++col) {
        unsigned i = row * a.pinyin_columns + col;
        auto cell = p.box(r, false, 3);
        p.ok &= p.tx.logical_px(cell, protocol::width, 44);
        auto py = p.text(cell, a.py[i].value, Font::body, green),
             han = p.text(cell, a.han[i].value, Font::title);
        p.ok &= p.tx.u8(py, protocol::text_align, 1) &&
                p.tx.u8(han, protocol::text_align, 1);
      }
    }
    auto next = p.button(
        wrap, "下一页",
        [&a] {
          if (a.pinyin_index + 1 < a.pinyin_pages.size()) {
            ++a.pinyin_index;
            a.pinyin_update();
          }
        },
        true);
    p.visible(next, a.pinyin_next_visible);
    auto prev = p.button(wrap, "上一页", [&a] {
      if (a.pinyin_index) {
        --a.pinyin_index;
        a.pinyin_update();
      }
    });
    p.visible(prev, a.pinyin_previous);
    auto title = std::string_view("拼音伴读");
    header(f, title);
  }
  {
    auto f = stage(Screen::quiz), b = p.scroll(p.body(f));
    p.text(b, a.question_counter, Font::caption, green);
    p.text(b, a.question_text, Font::title);
    for (unsigned i = 0; i < 4; ++i) {
      auto n = p.button(b, a.answers[i].value, [&a, i] { a.answer(i); });
      p.visible(n, a.answer_visible[i].value);
    }
    auto title = std::string_view("来个小挑战");
    header(f, title);
  }
  {
    auto f = stage(Screen::feedback), wrap = p.body(f), b = p.scroll(wrap);
    p.text(b, a.feedback_text, Font::title);
    p.button(wrap, a.feedback_next, [&a] { a.next_question(); }, true);
    p.button(wrap, "分页看讲解", [&a] {
      a.read(a.feedback_text.get(), "慢慢想一想", Screen::feedback);
    });
    auto title = std::string_view("收获小星星");
    header(f, title);
  }
  {
    auto f = stage(Screen::loading), b = p.scroll(p.body(f));
    p.text(b, "小书页，翻呀翻", Font::headline, green);
    p.text(b, a.loading_text);
    auto title = std::string_view("四博学习宝");
    header(f, title);
  }
  {
    auto f = stage(Screen::picture), b = p.scroll(p.body(f));
    p.text(b, a.picture_caption, Font::title, green);
    p.canvas(b, a.cover_canvas, a.canvas_width, 150);
    p.button(b, "回到书页", [&a] { a.back(); }, true);
    auto title = std::string_view("诗和故事里的画");
    header(f, title);
  }
  {
    auto f = stage(Screen::about), b = p.scroll(p.body(f));
    p.text(b, "我的小书包", Font::headline, green);
    p.text(b, a.about_summary);
    p.button(
        b, "阅读使用说明",
        [&a] {
          a.read("欢迎来到四博学习宝！\n\n诗词、儿童故事、蒙学诵读、小学听题，"
                 "都在这个小书包里。\n\n上下键选择，确认键打开。每页右上角都可"
                 "以返回，也支持触摸。\n\n75首课本古诗原文离线可读。故事、拼音"
                 "、讲解、插图和朗读需联网，应用不会自动开启设备WiFi。\n\n最近"
                 "读取的4份内容各缓存至多96KB，收藏最多12篇。内部存储即可使用，"
                 "不依赖SD卡。音频直接播放，不保存音频文件。\n\n阅读时可翻页，"
                 "首页“继续读”会保留上次正文与阅读位置。\n\n在线诗词和故事由内"
                 "容服务提供，家长可以先陪孩子挑选适合的内容。",
                 "使用说明", Screen::about);
        },
        true);
    p.button(b, "来源与许可", [&a] {
      a.read("四博学习宝 PXA "
             "版\n\n学习内容与接口参考原四博学习宝。公开服务：wtoy.doit."
             "am。\n\n原项目、chinese-poetry 数据和本应用代码遵循 MIT，minimp3 "
             "遵循 "
             "CC0。完整声明随安装包保留。\n\n音频、故事和配图按需访问原服务，未"
             "打包进安装包；可用性取决于服务。\n\n独立教材账号服务不包含在本应"
             "用中。",
             "来源与许可", Screen::about);
    });
    p.button(b, "清理在线缓存", [&a] {
      a.network.cancel();
      (void)a.network.start(a.clear_cache());
    });
    notice(b);
    auto title = std::string_view("小书包与说明");
    header(f, title);
  }
  return p.ok && p.tx.valid();
}
} // namespace sibo
PXA_APPLICATION(sibo::App)
