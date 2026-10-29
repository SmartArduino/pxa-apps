// SPDX-License-Identifier: MIT
#include "audio.hpp"
#include "layout.hpp"
#include "model.hpp"
#include "picture_view.hpp"
#include "text_pages.hpp"
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
  selfcheck,
  loading,
  picture,
  card0,
  card1,
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
  read,
  vocab_catalog,
  vocab_unit,
  school_catalog,
  school_outline,
  school_item,
  science_catalog,
  science_item
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
  std::array<Slot, 24> slots;
  State<std::string> title{""}, counter{""};
  State<bool> previous{false}, next{false}, categories{false};
};
struct App;
struct AppView : ViewModifiers {
  static constexpr Capacity capacity{1100, 640, 192, 0, 3};
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
  std::array<std::function<void()>, 192> callbacks;
  Layout layout;
  State<bool> ready{false}, resume_visible{false}, is_poem{false},
      has_quiz{false}, has_image{false}, has_audio{false}, audio_active{false},
      notice_visible{false}, long_question{false}, full_word{false};
  State<std::string> subtitle{"把好奇心，装进小书包"}, summary{""},
      resume_title{""};
  State<std::string> item_title{""}, item_detail{""}, preview{""},
      favorite_label{"收藏起来"}, audio_status{""}, pause_label{"暂停"},
      notice{""}, reader_notice{""};
  State<std::string> reader_text{""}, reader_counter{""}, reader_next{"下一页"},
      reader_title{"一起读一读"};
  State<bool> reader_previous{false}, pinyin_previous{false},
      pinyin_next_visible{false};
  State<std::string> pinyin_counter{""};
  std::array<Label, 32> han, py;
  State<std::string> question_text{""}, question_counter{""}, feedback_text{""},
      feedback_next{"下一题"}, feedback_preview{""};
  std::array<Label, 4> answers;
  std::array<Flag, 4> answer_visible;
  State<bool> self_rating{false}, card_revealed{false}, card_previous{false};
  State<std::string> card_counter{""}, card_action{"翻开词义"},
      card_meaning{""};
  State<std::string> loading_text{"正在打开书包…"}, picture_caption{""},
      about_summary{""};
  State<int32_t> picture_height{150};
  State<int32_t> picture_width{218}, picture_description_width{218};
  State<bool> picture_description_visible{false}, picture_pan_vertical{false},
      picture_pan_horizontal{false};
  State<std::string> picture_description{""}, picture_plus{"放大"},
      picture_minus{"最小"};
  PictureView picture_view;
  std::function<void(const CanvasPointer &)> picture_pointer_handler;
  bool picture_drag = false, picture_moved = false;
  uint8_t picture_pointer_id = 0;
  int picture_last_x = 0, picture_last_y = 0, picture_down_x = 0,
      picture_down_y = 0;
  uint64_t picture_last_tap = 0;
  int picture_tap_x = 0, picture_tap_y = 0;
  State<int32_t> reader_button_width{218};
  std::array<ListPanel, 2> panels;
  CanvasRef hero_canvas, cover_canvas;
  CanvasCommands<2400> drawing;
  std::optional<Asset> cover;
  Screen screen = Screen::home, loading_from = Screen::home,
         detail_from = Screen::home, reader_from = Screen::detail,
         list_base = Screen::home, item_screen = Screen::detail;
  std::vector<ListContext> lists;
  std::vector<Entry> story_items, course_assets, favorites;
  std::vector<Entry> word_bank;
  std::vector<Category> story_categories;
  Entry selected, bookmark;
  std::vector<std::string> pages;
  std::vector<std::vector<Cell>> pinyin_pages;
  std::vector<Question> questions;
  unsigned list_panel = 0, reader_index = 0, pinyin_index = 0,
           question_index = 0, round_score = 0, bookmark_page = 0;
  unsigned word_index = 0, card_panel = 0;
  std::optional<size_t> bookmark_offset;
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
  void message(std::string value) {
    notice_visible.set(!value.empty());
    auto note=text_pages(value,std::max(2u,(layout.content_width>20?layout.content_width-20:24)/6),1).front();
    reader_notice.set(note+(note.size()<value.size()?"…":""));
    notice.set(std::move(value));
  }
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
    auto first = study::reading_pages(
        selected.body,
        layout.roomy ? (layout.content_width - 20) / layout.reader_unit : 28,
        3);
    preview.set(first.front());
    favorite_label.set(
        std::any_of(favorites.begin(), favorites.end(),
                    [&](const auto &e) { return same(e, selected); })
            ? "已收藏 · 再按移除"
            : "收藏起来");
    has_quiz.set(selected.kind == Kind::poem || !selected.questions.empty());
    has_image.set(selected.course || !selected.image.empty());
    has_audio.set(selected.kind == Kind::poem || !selected.audio.empty());
    message("");
  }
  void opened(Entry entry, Screen from) {
    selected = std::move(entry);
    detail_from = from;
    item_screen = Screen::detail;
    detail_update();
    show(Screen::detail);
  }
  void open_entry(Entry entry) {
    player.stop();
    images.cancel();
    clear_picture();
    if (entry.kind == Kind::subject) {
      begin(Job::school_catalog, entry.id);
      return;
    }
    if (entry.kind == Kind::vocab_pack) {
      push_list(entry.title + " · 选单元", vocab_units(entry));
      return;
    }
    if (entry.kind == Kind::vocab_unit || entry.kind == Kind::textbook ||
        entry.kind == Kind::science_book ||
        (entry.kind == Kind::school_lesson && entry.body.empty())) {
      auto kind = entry.kind;
      selected = std::move(entry);
      begin(kind == Kind::vocab_unit     ? Job::vocab_unit
            : kind == Kind::textbook     ? Job::school_outline
            : kind == Kind::science_book ? Job::science_item
                                         : Job::school_item);
      return;
    }
    if (entry.kind == Kind::word) {
      selected = std::move(entry);
      detail_from = screen;
      word_index = unsigned(
          std::find_if(word_bank.begin(), word_bank.end(),
                       [&](const auto &e) { return same(e, selected); }) -
          word_bank.begin());
      open_card();
      return;
    }
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
  void subjects() {
    std::vector<Entry> rows;
    for (auto [id, title] : {std::pair{"chinese", "语文"},
                             {"math", "数学"},
                             {"english", "英语"},
                             {"science", "科学"}}) {
      Entry e;
      e.kind = Kind::subject;
      e.id = id;
      e.title = title;
      e.detail = "按年级、上下册选课";
      rows.push_back(std::move(e));
    }
    root_list("同步小课堂", std::move(rows), Screen::home);
  }
  void open_card() {
    item_title.set(selected.title);
    item_detail.set(selected.detail);
    card_revealed.set(false);
    card_meaning.set(excerpt(selected.body));
    full_word.set(false);
    card_action.set("翻开词义");
    bool in_unit =
        word_index < word_bank.size() && same(word_bank[word_index], selected);
    card_counter.set(in_unit ? "单词 " + std::to_string(word_index + 1) +
                                   " / " + std::to_string(word_bank.size())
                             : "收藏的单词");
    card_previous.set(in_unit && word_index > 0);
    card_panel ^= 1;
    item_screen = card_panel ? Screen::card1 : Screen::card0;
    show(item_screen);
    message("");
  }
  void word_turn(int delta) {
    player.stop();
    if (word_index >= word_bank.size()) {
      message("这个单词已经看完啦，可以返回挑选其他内容。");
      return;
    }
    if (delta < 0 && word_index)
      --word_index;
    else if (delta > 0 && word_index + 1 < word_bank.size())
      ++word_index;
    else {
      paint_list();
      return;
    }
    selected = word_bank[word_index];
    open_card();
  }
  void flip_card() {
    if (card_revealed.get())
      word_turn(1);
    else {
      card_revealed.set(true);
      full_word.set(true);
      card_action.set("下一个单词");
    }
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
    const auto capacity = layout.page_size;
    const auto total =
        std::max<size_t>(1, (list.rows.size() + capacity - 1) / capacity);
    list.page = std::min<unsigned>(list.page, unsigned(total - 1));
    view.title.set(list.title);
    view.counter.set(std::to_string(list.page + 1) + " / " +
                     std::to_string(total) + " 页 · " +
                     std::to_string(list.rows.size()) + " 项");
    for (unsigned i = 0; i < capacity; ++i) {
      auto index = list.page * capacity + i;
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
    else if (delta > 0 && (l.page + 1) * layout.page_size < l.rows.size())
      ++l.page;
    paint_list();
  }
  void list_pick(unsigned index) {
    if (lists.empty())
      return;
    index += lists.back().page * layout.page_size;
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
        screen == Screen::feedback || screen == Screen::selfcheck ||
        screen == Screen::picture) {
      picture_drag = false;
      images.cancel();
      clear_picture();
      show(item_screen);
      return;
    }
    if (screen == Screen::detail || screen == Screen::card0 ||
        screen == Screen::card1) {
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
    reader_button_width.set(int32_t(
        reader_index ? (layout.content_width - 8) / 2 : layout.content_width));
    reader_next.set(reader_index + 1 < pages.size() ? "下一页" : "读完啦");
  }
  unsigned reader_columns() const {
    return std::max(2u, (layout.content_width > 16 ? layout.content_width-16 : 20) / layout.reader_unit);
  }
  unsigned reader_rows() const {
    const unsigned toolbar=layout.roomy?56:38;
    const unsigned reserved=2*layout.margin+layout.header+7+toolbar+20+16+10+24;
    const unsigned available=layout.height>reserved?layout.height-reserved:30;
    return std::clamp(available/(layout.reader_unit==14?42:30),1u,64u);
  }
  std::string excerpt(std::string_view text) const {
    auto p=text_pages(text,reader_columns(),std::min(3u,reader_rows()));
    return p.front()+(p.size()>1?"…":"");
  }
  void feedback(std::string text) {
    feedback_preview.set(excerpt(text));feedback_text.set(std::move(text));
  }
  void read_question() {
    auto &q=questions[question_index];
    auto text=q.text;
    for(unsigned i=0;i<q.count;++i)
      text+="\n\n"+std::string(1,char('A'+i))+". "+q.options[i];
    read(std::move(text),"完整题目",Screen::quiz);
  }
  void read(std::string body, std::string title, Screen from = Screen::detail,
            unsigned index = 0) {
    reader_from = from;
    reader_title.set(std::move(title));
    message("");
    pages = text_pages(body, reader_columns(), reader_rows());
    reader_index = std::min<unsigned>(index, unsigned(pages.size() - 1));
    reader_update();
    show(Screen::reader);
  }
  void remember() {
    auto previous = std::move(bookmark);
    auto previous_page = bookmark_page;
    auto previous_offset=bookmark_offset;
    bookmark = selected;
    bookmark_page = reader_index;
    bookmark_offset=0;
    for(unsigned i=0;i<reader_index;++i)*bookmark_offset+=pages[i].size();
    if (personal_snapshot().size() > 28 * 1024) {
      bookmark = std::move(previous);
      bookmark_page = previous_page;
      bookmark_offset=previous_offset;
      message("这篇较长，阅读位置暂未保存。可以移除一些收藏后再试。");
    }
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
    size_t offset=bookmark_offset.value_or(0);
    if(!bookmark_offset) {
      auto old=study::reading_pages(bookmark.body,
          layout.roomy?(layout.content_width-20)/layout.reader_unit:
          width()>48?(width()-48)/10:12,layout.reader_rows);
      for(size_t i=0;i<std::min<size_t>(bookmark_page,old.size());++i)offset+=old[i].size();
    }
    opened(bookmark, Screen::home);
    read(selected.body,selected.title,Screen::detail);
    size_t at=0;
    for(unsigned i=0;i<pages.size();++i) {
      if(at>offset)break;
      reader_index=i;at+=pages[i].size();
    }
    reader_update();remember();
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
      if (selected.body.size() > 8192 || favorite_bytes > 20 * 1024) {
        message("这篇较长，可以用“继续读”保留阅读位置。");
        return;
      }
      favorites.push_back(selected);
      if (personal_snapshot().size() > 28 * 1024) {
        favorites.pop_back();
        message("小书包快满啦，先移除一篇收藏再试。");
        return;
      }
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
    question_text.set(excerpt(q.text));
    bool lengthy=excerpt(q.text)!=q.text;
    for(unsigned i=0;i<q.count;++i)lengthy|=excerpt(q.options[i])!=q.options[i];
    long_question.set(lengthy);
    question_counter.set("小挑战 " + std::to_string(question_index + 1) +
                         " / " + std::to_string(questions.size()));
    for (unsigned i = 0; i < 4; ++i) {
      answers[i].value.set(excerpt(q.options[i]));
      answer_visible[i].value.set(i < q.count);
    }
    show(Screen::quiz);
  }
  void quiz() {
    questions = selected.kind == Kind::word ? vocab_quiz(word_bank, word_index)
                                            : make_quiz(selected);
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
    if (q.self_check) {
      feedback("对照一下自己的答案\n\n答案：" + q.solution +
                        (q.explanation.empty() ? "" : "\n\n" + q.explanation));
      self_rating.set(true);
      show(Screen::selfcheck);
      return;
    }
    record_answer(index == q.answer);
  }
  void record_answer(bool correct) {
    auto &q = questions[question_index];
    ++answered_count;
    if (correct) {
      ++correct_count;
      ++round_score;
    }
    feedback(
        std::string(correct ? "答对啦！\n\n" : "再想一想，也在进步。\n\n") +
        "答案：" + (q.self_check ? q.solution : q.options[q.answer]) +
        (q.explanation.empty() ? "" : "\n\n" + q.explanation));
    feedback_next.set(question_index + 1 < questions.size() ? "下一题"
                                                            : "看看收获");
    save();
    self_rating.set(false);
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
           "收获小星星", item_screen);
    }
  }
  void audio_changed() {
    audio_status.set(player.status +
                     (player.active ? " · " + player.position() : "") +
                     (player.cache.label.empty() ? "" : " · " + player.cache.label));
    audio_active.set(player.active);
    pause_label.set(player.paused ? "继续听" : "暂停");
  }
  void listen() {
    if (!selected.audio.empty())
      player.start(*ctx, selected.audio, selected.audio_start,
                   selected.audio_end);
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
      if (!n || !*n) {
        (void)file->close();
        (void)co_await ctx->fs().remove(temp);
        co_return std::unexpected(Error::io_error);
      }
      remaining = remaining.subspan(*n);
    }
    auto closed = file->close();
    if (!closed) {
      (void)co_await ctx->fs().remove(temp);
      co_return closed;
    }
    auto replaced = co_await ctx->fs().replace(temp, path);
    if (!replaced)
      (void)co_await ctx->fs().remove(temp);
    co_return replaced;
  }
  size_t cache_budget() const {
    const auto records =
        std::min<size_t>(28 * 1024, personal_snapshot().size());
    return std::min<size_t>(62 * 1024, 64 * 1024 - 2 * records - 1024);
  }
  Task<void> trim_cache(size_t incoming = 0, int replacing = -1) {
    // Gen4 grants 64 KiB of private files. Caches are disposable and must
    // leave room for both sides of an atomic personal-record replacement.
    std::array<size_t, 4> sizes{};
    size_t total = incoming + player.cache.disk_bytes();
    for (unsigned i = 0; i < 4; ++i) {
      if (int(i) == replacing)
        continue;
      auto file =
          co_await read_file("cache-" + std::to_string(i) + ".bin", 100 * 1024);
      if (file) {
        sizes[i] = file->size();
        total += sizes[i];
      }
    }
    const auto budget = cache_budget();
    while (total > budget) {
      int oldest = -1;
      for (unsigned i = 0; i < 4; ++i)
        if (sizes[i] && (oldest < 0 || cache_ages[i] < cache_ages[oldest]))
          oldest = int(i);
      if (oldest < 0)
        break;
      (void)co_await ctx->fs().remove("cache-" + std::to_string(oldest) +
                                      ".bin");
      total -= sizes[oldest];
      sizes[oldest] = 0;
      cache_ages[oldest] = 0;
    }
    co_return Result<void>{};
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
    if (!save_running && bytes->size() + key.size() + 1 <= cache_budget()) {
      unsigned slot = found >= 0 ? unsigned(found)
                      : empty >= 0
                          ? unsigned(empty)
                          : unsigned(std::min_element(cache_ages.begin(),
                                                      cache_ages.end()) -
                                     cache_ages.begin());
      // Cache files are disposable; release the old slot before staging its
      // replacement so an update never temporarily doubles its disk usage.
      (void)co_await ctx->fs().remove("cache-" + std::to_string(slot) + ".bin");
      (void)co_await trim_cache(bytes->size() + key.size() + 1, int(slot));
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
    if (job == Job::vocab_catalog)
      address = std::string(vocab_base) + "/catalog.json";
    else if (job == Job::school_catalog)
      address = std::string(school_base) + "/index.json";
    else if (job == Job::science_catalog)
      address = std::string(science_base) + "/catalog.json";
    else if (job == Job::vocab_unit || job == Job::school_outline ||
             job == Job::school_item || job == Job::science_item)
      address = selected.path;
    else if (job == Job::stories)
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
    if (job == Job::vocab_catalog || job == Job::school_catalog ||
        job == Job::science_catalog) {
      auto rows = job == Job::vocab_catalog    ? vocab_packs(*bytes)
                  : job == Job::school_catalog ? school_books(*bytes, query)
                                               : science_books(*bytes);
      if (rows.empty())
        failed(tag);
      else if (job == Job::school_catalog)
        push_list("同步教材", std::move(rows));
      else
        root_list(job == Job::vocab_catalog ? "英语单词岛" : "科学发现馆",
                  std::move(rows), Screen::home);
    } else if (job == Job::vocab_unit || job == Job::school_outline ||
               job == Job::science_item) {
      auto rows = job == Job::vocab_unit ? vocab_words(*bytes)
                  : job == Job::school_outline
                      ? school_lessons(*bytes, selected)
                      : science_pages(*bytes, selected);
      if (rows.empty())
        failed(tag);
      else {
        if (job == Job::vocab_unit)
          word_bank = rows;
        push_list(selected.title, std::move(rows));
      }
    } else if (job == Job::school_item) {
      if (!school_content(*bytes, selected))
        failed(tag);
      else
        opened(std::move(selected), loading_from);
    } else if (job == Job::stories) {
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
  std::string personal_snapshot() const {
    std::string bytes =
        "{\"read\":" + std::to_string(read_count) +
        ",\"answered\":" + std::to_string(answered_count) +
        ",\"correct\":" + std::to_string(correct_count) +
        ",\"page\":" + std::to_string(bookmark_page) +
        (bookmark_offset ? ",\"offset\":"+std::to_string(*bookmark_offset) : "") + ",\"bookmark\":" +
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
    return bytes;
  }
  Task<void> save_loop() {
    player.cache.defer_persist();
    (void)co_await ctx->fs().remove("sibo-audio.tmp");
    while (saved_revision != revision) {
      auto rev = revision;
      auto bytes = personal_snapshot();
      auto saved = co_await write_file("personal.json", bytes, "personal.tmp");
      if (!saved) {
        // A failed cache write must never starve user records. Retry once
        // after releasing only disposable online files.
        for (unsigned i = 0; i < 4; ++i)
          (void)co_await ctx->fs().remove("cache-" + std::to_string(i) +
                                          ".bin");
        (void)co_await ctx->fs().remove("cache.tmp");
        cache_ages.fill(0);
        (void)co_await player.cache.evict_disk();
        saved = co_await write_file("personal.json", std::move(bytes),
                                    "personal.tmp");
      }
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
    player.suspend();
    player.cache.ctx=ctx;
    (void)co_await player.cache.clear();
    for (unsigned i = 0; i < 4; ++i)
      (void)co_await ctx->fs().remove("cache-" + std::to_string(i) + ".bin");
    (void)co_await ctx->fs().remove("cache.tmp");
    (void)co_await ctx->fs().remove("cache-order.json");
    (void)co_await ctx->fs().remove("cache-order.tmp");
    cache_ages.fill(0);
    cache_clock = 0;
    course_assets.clear();
    message("音频与在线缓存清理好了，收藏与阅读位置都保留着。");
    co_return Result<void>{};
  }
  Task<void> initialize() {
    for (const auto *temp : {"personal.tmp", "cache.tmp", "cache-order.tmp", "sibo-audio.tmp"})
      (void)co_await ctx->fs().remove(temp);
    auto bytes = co_await read_file("personal.json", 256 * 1024);
    if (bytes && study::complete_json(*bytes)) {
      auto root = study::json(*bytes);
      read_count = std::min<uint64_t>(root.get("read").number(), 1000000);
      answered_count =
          std::min<uint64_t>(root.get("answered").number(), 1000000);
      correct_count =
          std::min<uint64_t>(root.get("correct").number(), answered_count);
      bookmark_page = std::min<uint64_t>(root.get("page").number(), 4096);
      auto saved_offset=root.get("offset");
      if(saved_offset)bookmark_offset=std::min<uint64_t>(saved_offset.number(),body_limit);
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
    (void)co_await player.cache.inspect();
    (void)co_await player.cache.trim_disk();
    (void)co_await trim_cache();
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
    const int x = int(layout.hero_width) / 2;
    drawing.rect(0, 0, layout.hero_width, 72, 0xf6e5bfff, 18)
        .rect(x - 51, 18, 48, 42, white, 5)
        .rect(x + 3, 18, 48, 42, 0xe0f1e5ff, 5)
        .line(x, 20, x, 61, green, 3)
        .line(x - 40, 30, x - 14, 30, 0xe3b85cff, 3)
        .line(x - 40, 39, x - 20, 39, 0xe3b85cff, 3)
        .line(x + 14, 31, x + 39, 31, 0x75b49bff, 3)
        .line(x + 14, 40, x + 34, 40, 0x75b49bff, 3)
        .rect(x - 8, 5, 16, 16, 0xf3ba49ff, 8)
        .rect(15, 18, 5, 5, green, 2)
        .rect(layout.hero_width - 24, 48, 5, 5, 0xd98b65ff, 2);
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
  int picture_view_width(int level) {
    if (!layout.sidebar || level || !cover)
      return int(layout.content_width);
    const auto &d = cover->descriptor();
    auto fit = int(uint64_t(layout.cover_height) * d.width /
                   std::max<unsigned>(1, d.height));
    return std::clamp(fit, int(layout.content_width / 4),
                      int(layout.content_width * 3 / 5));
  }
  void clear_picture() {
    picture_drag = false;
    picture_last_tap = 0;
    // The hidden Canvas also owns its last image. Drop that display lease
    // before decoding another original under the same bounded Host quota.
    if (cover && cover_canvas.mounted()) {
      drawing.reset();
      (void)cover_canvas.present(drawing);
    }
    cover.reset();
  }
  void draw_picture() {
    if (!cover || !cover_canvas.mounted())
      return;
    const auto r = picture_view.rectangle();
    picture_width.set(picture_view.width);
    picture_height.set(picture_view.height);
    picture_description_width.set(
        std::max(1, int(layout.content_width) - picture_view.width - 12));
    picture_description_visible.set(layout.sidebar && picture_view.level == 0);
    picture_pan_vertical.set(r.height > picture_view.height);
    picture_pan_horizontal.set(r.width > picture_view.width);
    picture_plus.set(picture_view.level + 1 == int(PictureView::scales.size())
                         ? "最大"
                         : "放大");
    picture_minus.set(picture_view.level ? "缩小" : "最小");
    drawing.reset();
    drawing.rect(0, 0, picture_view.width, picture_view.height, 0xf1efe6ff, 10)
        .image({r.x, r.y, r.width, r.height}, *cover, ImageFit::contain)
        .rect(6, 6, 56, 27, paper, 7)
        .text(12, 9, 44, green,
              std::to_string(PictureView::scales[picture_view.level]) + "×", 0);
    (void)cover_canvas.present(drawing);
  }
  void zoom_picture(int delta, int x = -1, int y = -1) {
    if (!cover)
      return;
    auto next = std::clamp(picture_view.level + delta, 0,
                           int(PictureView::scales.size() - 1));
    if (next == picture_view.level)
      return;
    picture_view.zoom(next, picture_view_width(next), int(layout.cover_height),
                      x, y);
    picture_last_tap = 0;
    draw_picture();
  }
  void reset_picture() {
    if (!cover)
      return;
    picture_view.zoom(0, picture_view_width(0), int(layout.cover_height));
    picture_last_tap = 0;
    draw_picture();
  }
  void pan_picture(int x, int y) {
    if (!cover || !picture_view.level)
      return;
    picture_view.move(-x * std::max(24, picture_view.width / 4),
                      -y * std::max(24, picture_view.height / 4));
    draw_picture();
  }
  void picture_pointer(const CanvasPointer &event) {
    if (screen != Screen::picture || !cover)
      return;
    if (event.action() == PointerPhase::down) {
      if (picture_drag)
        return;
      picture_drag = true;
      picture_moved = false;
      picture_pointer_id = event.pointer_id;
      picture_last_x = picture_down_x = event.x;
      picture_last_y = picture_down_y = event.y;
      return;
    }
    if (!picture_drag || event.pointer_id != picture_pointer_id)
      return;
    if (event.action() == PointerPhase::move) {
      const int x = event.x - picture_last_x, y = event.y - picture_last_y;
      if (std::abs(event.x - picture_down_x) +
              std::abs(event.y - picture_down_y) >
          6)
        picture_moved = true;
      picture_last_x = event.x;
      picture_last_y = event.y;
      if (picture_moved && picture_view.level) {
        picture_view.move(x, y);
        draw_picture();
      }
      return;
    }
    picture_drag = false;
    auto time = event.timestamp_us ? event.timestamp_us : now_ms() * 1000;
    if (event.action() == PointerPhase::up && !picture_moved) {
      if (picture_last_tap && time >= picture_last_tap &&
          time - picture_last_tap < 400000 &&
          std::abs(event.x - picture_tap_x) +
                  std::abs(event.y - picture_tap_y) <
              24) {
        if (picture_view.level)
          reset_picture();
        else
          zoom_picture(1, event.x, event.y);
        picture_last_tap = 0;
      } else {
        picture_last_tap = time;
        picture_tap_x = event.x;
        picture_tap_y = event.y;
      }
    } else
      picture_last_tap = 0;
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
    // Retain only the successfully decoded image. Every attempt keeps the same
    // charged Host allocator and reopens the source after a rejected request.
    constexpr uint32_t budgets[] = {1048576u, 262144u, 65536u};
    clear_picture();
    for (auto pixels : budgets) {
      if (tag != generation)
        co_return Result<void>{};
      auto request = co_await ctx->http().open(
          {.url = url,
           .wanted_headers = wanted,
           .options = {.buffer_bytes = 4096, .header_bytes = 512,
                       .max_body_bytes = 128 * 1024, .timeout_ms = 20000}},
          *grant);
      if (tag != generation)
        co_return Result<void>{};
      if (!request) {
        failed(tag);
        co_return Result<void>{};
      }
      auto response = co_await std::move(*request).response();
      if (tag != generation)
        co_return Result<void>{};
      if (!response || response->status_code != 200) {
        failed(tag);
        co_return Result<void>{};
      }
      auto decoded = co_await ctx->assets().decode_image(
          std::move(response->body),
          {.max_width = uint16_t(pixels > 65536 ? 2048 : 1024),
           .max_height = uint16_t(pixels > 65536 ? 2048 : 1024),
           .max_pixels = pixels,
           .max_encoded_bytes = 128 * 1024,
           .formats = ImageFormats::jpeg | ImageFormats::png});
      if (tag != generation)
        co_return Result<void>{};
      if (decoded) {
        cover.emplace(std::move(*decoded));
        break;
      }
      auto error = decoded.error();
      if (error != Error::invalid_argument && error != Error::unsupported &&
          error != Error::resource_limit && error != Error::quota_exceeded &&
          error != Error::limit_exceeded)
        break;
    }
    if (!cover) {
      failed(tag);
      co_return Result<void>{};
    }
    const auto &info = cover->descriptor();
    (void)ctx->log().write(LogLevel::info, "sibo image " +
        std::to_string(info.source_width) + "x" + std::to_string(info.source_height) +
        " -> " + std::to_string(info.width) + "x" + std::to_string(info.height));
    picture_view.initialize(info.width, info.height, picture_view_width(0),
                            int(layout.cover_height));
    picture_drag = false;
    picture_last_tap = 0;
    auto excerpt = study::reading_pages(selected.body, 48, 12);
    picture_description.set(excerpt.front() +
                            (excerpt.size() > 1 ? "\n…" : ""));
    picture_caption.set(selected.title);
    show(Screen::picture);
    draw_picture();
    co_return Result<void>{};
  }
  Result<void> on_start(Context &context, std::span<const std::byte> config) {
    ctx = &context;
    player.cache.ctx = &context;
    auto result = environment.initialize(config);
    if (!result)
      return result;
    auto display = environment.display().get();
    layout = Layout(width(), height(), display.width, display.height);
    canvas_width = layout.cover_width;
    picture_width.set(int(layout.content_width));
    picture_height.set(int(layout.cover_height));
    picture_pointer_handler = [this](const CanvasPointer &event) {
      picture_pointer(event);
    };
    pinyin_columns = layout.pinyin_columns;
    pinyin_rows = layout.pinyin_rows;
    player.changed = [this] { audio_changed(); };
    player.cache.disk_budget=[this] { return cache_budget(); };
    player.cache.reserve_disk=[this](size_t bytes) { return trim_cache(bytes); };
    player.cache_save_allowed=[this] { return !save_running; };
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
    player.suspend();
    if (screen == Screen::loading)
      back();
    else
      cancel();
  }
  auto view() {
    return SafeArea(environment.display(), AppView(this))
        .background(paper)
        .enabled(ready);
  }
};

template <class Page> bool AppView::render(Page &page, uint32_t parent) {
  auto &a = *app;
  Painter p(page, a.callbacks);
  p.roomy = a.layout.roomy;
  p.header_size = a.layout.header;
  auto root = p.box(parent);
  p.fill(root, true);
  p.padding(root, a.layout.margin, a.layout.margin, a.layout.margin,
            a.layout.margin);
  auto stage = [&](Screen s) {
    return p.frame(root, a.screens[unsigned(s)].value);
  };
  auto header = [&](uint32_t f, auto &title) {
    p.header(f, title, [&a] { a.back(); });
  };
  auto notice = [&](uint32_t b) {
    auto n = p.text(b, a.notice, Font::caption, muted);
    p.visible(n, a.notice_visible);
  };
  {
    auto f = stage(Screen::home), b = p.scroll(p.body(f));
    // Callbacks must outlive this render call; capture the App directly.
    auto action = [&a](unsigned index) {
      switch (index) {
      case 0:
        a.show(Screen::poetry);
        a.message("");
        break;
      case 1:
        a.begin(Job::stories);
        break;
      case 2:
        a.show(Screen::recite);
        a.message("");
        break;
      case 3:
        a.begin(Job::vocab_catalog);
        break;
      case 4:
        a.subjects();
        break;
      case 5:
        a.begin(Job::science_catalog);
        break;
      case 6:
        a.root_list("我的收藏", a.favorites, Screen::home);
        break;
      case 7:
        a.refresh_summary();
        a.show(Screen::about);
        break;
      }
    };
    uint32_t tiles = b;
    if (a.layout.roomy) {
      auto shell = p.box(b, a.layout.sidebar, 12);
      auto intro = p.box(shell, !a.layout.sidebar, 10);
      p.ok &= p.tx.logical_px(intro, protocol::width,
                              a.layout.sidebar ? a.layout.sidebar_width
                                               : a.layout.content_width);
      p.canvas(intro, a.hero_canvas, a.layout.hero_width, 72);
      auto description = a.layout.sidebar ? intro : p.box(intro);
      if (!a.layout.sidebar)
        p.ok &=
            p.tx.logical_px(description, protocol::width,
                            a.layout.content_width - a.layout.hero_width - 10);
      p.text(description, a.subtitle, Font::title, green);
      p.text(description, a.summary, Font::body, muted);
      auto resume =
          p.button(description, a.resume_title, [&a] { a.resume(); });
      p.visible(resume, a.resume_visible);
      tiles = p.box(shell);
      p.ok &= p.tx.logical_px(tiles, protocol::width, a.layout.home_width);
    } else {
      p.canvas(b, a.hero_canvas, a.layout.hero_width, 72);
      p.text(b, a.subtitle, Font::title, green);
      p.text(b, a.summary, Font::caption, muted);
      auto resume = p.button(b, a.resume_title, [&a] { a.resume(); });
      p.visible(resume, a.resume_visible);
    }
    constexpr const char *labels[] = {"诗词小天地", "故事小书架",  "读出声来",
                                      "英语单词岛", "同步小课堂",  "科学发现馆",
                                      "我的收藏",   "小书包与说明"};
    constexpr const char *captions[] = {
        "75首课本古诗",   "寓言与童话",     "蒙学与小学听题", "翻词卡 · 听发音",
        "按学科年级选课", "一步一步看原理", "留住喜欢的内容", "学习记录与设置"};
    constexpr uint32_t colors[] = {0xd7eee7ff, 0xffedcaff, 0xe0f1e5ff,
                                   0xe3efffff, 0xeee7ffff, 0xffe8ddff,
                                   0xe8f2dfff, 0xefede5ff};
    uint32_t row = tiles;
    const auto columns = a.layout.home_columns;
    const auto tile_width = (a.layout.home_width - (columns - 1) * 8) / columns;
    for (unsigned i = 0; i < 8; ++i) {
      if (a.layout.roomy) {
        if (i % columns == 0)
          row = p.box(tiles, true, 8);
        auto tile = p.button_root(row, [action, i] { action(i); });
        p.ok &= p.tx.logical_px(tile, protocol::width, tile_width) &&
                p.tx.logical_px(tile, protocol::height,
                                a.layout.home_tile_height) &&
                p.tx.rgba(tile, protocol::background, colors[i]);
        p.text(tile, labels[i],
               tile_width < 140             ? Font::body
               : a.layout.reader_unit >= 12 ? Font::display
                                            : Font::title,
               ink);
        p.text(tile, captions[i],
               tile_width >= 200 ? Font::body : Font::caption, muted);
      } else {
        auto label = std::string_view(labels[i]);
        p.button(tiles, label, [action, i] { action(i); });
      }
    }
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
    uint32_t row = b;
    const auto columns = a.layout.list_columns;
    for (unsigned i = 0; i < a.layout.page_size; ++i) {
      if (columns > 1 && i % columns == 0)
        row = p.box(b, true, 8);
      auto n = p.button_root(row, [&a, i] { a.list_pick(i); });
      if (columns > 1)
        p.ok &= p.tx.logical_px(n, protocol::width,
                                (a.layout.content_width - (columns - 1) * 8) /
                                    columns) &&
                p.tx.template logical_px<protocol::min_height>(n, 72);
      p.text(n, v.slots[i].title,
             a.layout.roomy && a.layout.physical_width >= 420 ? Font::headline
                                                              : Font::title,
             ink);
      p.text(n, v.slots[i].detail,
             a.layout.roomy && a.layout.physical_width >= 420 ? Font::body
                                                              : Font::caption,
             muted);
      p.visible(n, v.slots[i].shown);
    }
    auto navigation = columns > 1 ? p.box(b, true, 8) : b;
    auto next = p.button(navigation, "下一页", [&a] { a.list_turn(1); });
    p.visible(next, v.next);
    auto prev = p.button(navigation, "上一页", [&a] { a.list_turn(-1); });
    p.visible(prev, v.previous);
    if (columns > 1) {
      p.ok &= p.tx.logical_px(next, protocol::width,
                              (a.layout.content_width - 8) / 2) &&
              p.tx.logical_px(prev, protocol::width,
                              (a.layout.content_width - 8) / 2);
    }
    auto categories = p.button(b, "换个故事分类", [&a] { a.categories(); });
    p.visible(categories, v.categories);
    notice(b);
    header(f, v.title);
  }
  for (unsigned panel = 0; panel < 2; ++panel) {
    auto f = stage(panel ? Screen::card1 : Screen::card0),
         b = p.scroll(p.body(f));
    p.text(b, a.card_counter, Font::caption, muted);
    auto card = p.box(b);
    p.padding(card, 12, 14, 12, 14);
    p.ok &= p.tx.rgba(card, protocol::background, mint) &&
            p.tx.dp(card, protocol::radius, 16);
    if (a.layout.roomy)
      p.ok &= p.tx.template logical_px<protocol::min_height>(
                  card, std::min(280u, a.layout.height / 3)) &&
              p.tx.u8(card, protocol::justify, 1);
    auto word = p.text(card, a.item_title, Font::display, green);
    auto phonetic = p.text(card, a.item_detail, Font::title, muted);
    if (a.layout.roomy)
      p.ok &= p.tx.u8(word, protocol::text_align, 1) &&
              p.tx.u8(phonetic, protocol::text_align, 1);
    auto meaning = p.text(card, a.card_meaning, Font::body, ink);
    p.visible(meaning, a.card_revealed);
    auto actions = a.layout.roomy ? p.box(b, true, 8) : b;
    auto flip = p.button(actions, a.card_action, [&a] { a.flip_card(); }, true);
    auto listen = p.button(actions, "听这个单词", [&a] { a.listen(); });
    if (a.layout.roomy)
      p.ok &= p.tx.logical_px(flip, protocol::width,
                              (a.layout.content_width - 8) / 2) &&
              p.tx.logical_px(listen, protocol::width,
                              (a.layout.content_width - 8) / 2);
    p.text(b, a.audio_status, Font::caption, muted);
    auto other = a.layout.roomy ? p.box(b, true, 8) : b;
    auto quiz = p.button(other, "单词小挑战", [&a] { a.quiz(); });
    auto previous = p.button(other, "上一个单词", [&a] { a.word_turn(-1); });
    if (a.layout.roomy)
      p.ok &= p.tx.logical_px(quiz, protocol::width,
                              (a.layout.content_width - 8) / 2) &&
              p.tx.logical_px(previous, protocol::width,
                              (a.layout.content_width - 8) / 2);
    p.visible(previous, a.card_previous);
    auto full=p.button(b,"完整词卡",[&a] {
      a.read(a.selected.title+"\n"+a.selected.detail+"\n\n"+a.selected.body,
             "完整词卡",a.item_screen);
    });
    p.visible(full,a.full_word);
    notice(b);
    auto title = std::string_view("英语词卡");
    header(f, title);
  }
  {
    auto f = stage(Screen::detail), b = p.scroll(p.body(f));
    p.text(b, a.item_title, Font::headline, green);
    p.text(b, a.item_detail, Font::caption, muted);
    p.text(b, a.preview, Font::body);
    p.button(b, "开始阅读", [&a] { a.read_selected(); }, true);
    auto listen = p.button(b, "听一听朗读", [&a] { a.listen(); });
    p.visible(listen, a.has_audio);
    auto audio_status = p.text(b, a.audio_status, Font::caption, muted);
    p.visible(audio_status, a.has_audio);
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
    auto note=p.text(b,a.reader_notice,Font::caption,muted);
    p.visible(note,a.notice_visible);
    auto text =
        p.text(b, a.reader_text,
               a.layout.reader_unit >= 12 ? Font::display : Font::title);
    p.padding(text, 8, 5, 8, 5);
    auto row = p.box(wrap, true);
    auto next = p.button(row, a.reader_next, [&a] { a.next_page(); }, true);
    p.ok &=
        pixel_width(p.tx, next, a.reader_button_width.get()) &&
        page.template bind<int32_t, pixel_width>(a.reader_button_width, next);
    auto prev = p.button(row, "上一页", [&a] {
      if (a.reader_index) {
        --a.reader_index;
        a.reader_update();
        if (a.reader_title.get() == a.selected.title)
          a.remember();
      }
    });
    p.visible(prev, a.reader_previous);
    p.ok &= p.tx.logical_px(prev, protocol::width,
                            (a.layout.content_width - 8) / 2);
    header(f, a.reader_title);
  }
  {
    auto f = stage(Screen::pinyin), wrap = p.body(f), b = p.box(wrap);
    p.ok &= p.tx.u16(b, protocol::grow, 1);
    auto counter = p.text(b, a.pinyin_counter, Font::caption, green);
    p.ok &= p.tx.logical_px(counter, protocol::height, 22);
    for (unsigned row = 0; row < a.pinyin_rows; ++row) {
      auto r = p.box(b, true, 4);
      p.ok &= p.tx.logical_px(r, protocol::height, a.layout.pinyin_row);
      for (unsigned col = 0; col < a.pinyin_columns; ++col) {
        unsigned i = row * a.pinyin_columns + col;
        auto cell = p.box(r, false, 3);
        p.ok &= p.tx.logical_px(cell, protocol::width, a.layout.pinyin_cell);
        const bool compact = a.layout.pinyin_row < 64;
        auto py = p.text(cell, a.py[i].value, Font::body, green),
             han = p.text(cell, a.han[i].value,
                          compact ? Font::body : Font::title);
        p.ok &= p.tx.u8(py, protocol::text_align, 1) &&
                p.tx.u8(han, protocol::text_align, 1) &&
                p.tx.logical_px(py, protocol::height, 26) &&
                p.tx.logical_px(han, protocol::height, compact ? 28 : 32);
      }
    }
    auto controls = p.box(wrap, true, 8);
    auto next = p.button(
        controls, "下一页",
        [&a] {
          if (a.pinyin_index + 1 < a.pinyin_pages.size()) {
            ++a.pinyin_index;
            a.pinyin_update();
          }
        },
        true);
    p.enabled(next, a.pinyin_next_visible);
    auto prev = p.button(controls, "上一页", [&a] {
      if (a.pinyin_index) {
        --a.pinyin_index;
        a.pinyin_update();
      }
    });
    p.enabled(prev, a.pinyin_previous);
    for (auto control : {next, prev}) {
      p.padding(control, 4, 2, 4, 2);
      p.ok &= p.tx.logical_px(control, protocol::width,
                              (a.layout.content_width - 8) / 2) &&
              p.tx.logical_px(control, protocol::height, a.layout.pinyin_toolbar);
    }
    auto title = std::string_view("拼音伴读");
    header(f, title);
  }
  {
    auto f = stage(Screen::quiz), b = p.scroll(p.body(f));
    p.text(b, a.question_counter, Font::caption, green);
    p.text(b, a.question_text, Font::title);
    uint32_t row = b;
    for (unsigned i = 0; i < 4; ++i) {
      if (a.layout.sidebar && i % 2 == 0)
        row = p.box(b, true, 8);
      auto n = p.button(row, a.answers[i].value, [&a, i] { a.answer(i); });
      if (a.layout.sidebar)
        p.ok &= p.tx.logical_px(n, protocol::width,
                                (a.layout.content_width - 8) / 2);
      p.visible(n, a.answer_visible[i].value);
    }
    auto full=p.button(b,"完整题目",[&a] { a.read_question(); });
    p.visible(full,a.long_question);
    auto title = std::string_view("来个小挑战");
    header(f, title);
  }
  {
    auto f = stage(Screen::selfcheck), wrap = p.body(f), b = p.scroll(wrap);
    p.text(b, a.feedback_preview, Font::title);
    p.button(
        wrap, "我答对了",
        [&a] {
          if (a.self_rating.get())
            a.record_answer(true);
        },
        true);
    p.button(wrap, "还要再练练", [&a] {
      if (a.self_rating.get())
        a.record_answer(false);
    });
    p.button(wrap, "分页看讲解", [&a] {
      a.read(a.feedback_text.get(), "慢慢想一想", Screen::selfcheck);
    });
    auto title = std::string_view("对照答案");
    header(f, title);
  }
  {
    auto f = stage(Screen::feedback), wrap = p.body(f), b = p.scroll(wrap);
    p.text(b, a.feedback_preview, Font::title);
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
    auto f = stage(Screen::picture), b = p.body(f);
    auto viewport = p.box(b, true, 12);
    // A gesture viewport is a scroll container with native scrolling disabled:
    // the app owns panning, while keypad navigation visits the toolbar buttons.
    auto gesture = page.create(viewport, 3); // PXA_UI_NODE_SCROLL
    p.ok &= p.tx.logical_px(gesture, protocol::width, a.picture_width.get()) &&
            p.tx.logical_px(gesture, protocol::height, a.picture_height.get()) &&
            p.tx.u8(gesture, protocol::scroll_axis, 0) &&
            p.tx.u8(gesture, protocol::scrollbar, 0);
    auto picture_canvas = p.canvas(gesture, a.cover_canvas, a.picture_width.get(),
                                   a.picture_height.get());
    p.ok &= page.template bind<int32_t, pixel_width>(a.picture_width,
                                                     gesture) &&
            page.template bind<int32_t, pixel_height>(a.picture_height,
                                                      gesture) &&
            page.template bind<int32_t, pixel_width>(a.picture_width,
                                                     picture_canvas) &&
            p.tx.u64(gesture, protocol::event_mask,
                     protocol::event_mask_pointer) &&
            page.on_pointer(gesture, a.picture_pointer_handler);
    auto description = p.box(viewport);
    p.ok &=
        pixel_width(p.tx, description, a.picture_description_width.get()) &&
        page.template bind<int32_t, pixel_width>(a.picture_description_width,
                                                 description) &&
        p.tx.logical_px(description, protocol::height, a.layout.cover_height);
    p.visible(description, a.picture_description_visible);
    p.text(description, "边看边读", Font::title, green);
    auto text = p.scroll(description);
    p.text(text, a.picture_description, Font::title);
    auto tool = [&](uint32_t row, auto &label, std::function<void()> fn,
                    unsigned columns) {
      auto n = p.button_root(row, std::move(fn));
      p.padding(n, 4, 4, 4, 4);
      p.ok &= p.tx.logical_px(n, protocol::width,
                              (a.layout.content_width - (columns - 1) * 8) /
                                  columns) &&
              p.tx.logical_px(n, protocol::height,
                              a.layout.picture_toolbar_height) &&
              p.tx.template logical_px<protocol::min_height>(
                  n, a.layout.picture_toolbar_height);
      auto t = p.text(n, label,
                      a.layout.roomy                         ? Font::title
                      : a.layout.picture_toolbar_height < 32 ? Font::caption
                                                             : Font::body,
                      ink);
      p.ok &= p.tx.u8(t, protocol::text_align, 1);
      return n;
    };
    auto zoom = p.box(b, true, 8);
    tool(zoom, a.picture_plus, [&a] { a.zoom_picture(1); }, 3);
    tool(zoom, a.picture_minus, [&a] { a.zoom_picture(-1); }, 3);
    auto reset = std::string_view("全图");
    tool(zoom, reset, [&a] { a.reset_picture(); }, 3);
    auto pan = p.box(b, true, 8);
    constexpr const char *directions[] = {"↑", "↓", "←", "→"};
    for (unsigned i = 0; i < 4; ++i) {
      auto label = std::string_view(directions[i]);
      auto n = tool(
          pan, label,
          [&a, i] {
            a.pan_picture(i == 2   ? -1
                          : i == 3 ? 1
                                   : 0,
                          i == 0   ? -1
                          : i == 1 ? 1
                                   : 0);
          },
          4);
      p.enabled(n, i < 2 ? a.picture_pan_vertical : a.picture_pan_horizontal);
    }
    auto title = std::string_view("图片查看");
    if (a.layout.roomy)
      header(f, a.picture_caption);
    else
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
                 "英语词卡、同步教材和科学发现，都在这个小书包里。\n\n上下键选"
                 "择，确认键打开。每页右上角都可"
                 "以返回，也支持触摸。\n\n75首课本古诗原文离线可读。故事、拼音"
                 "、讲解、插图和朗读需联网，应用不会自动开启设备WiFi。\n\n最近"
                 "内容按需联网读取，缓存按个人记录体积自动缩减，合计至多62KB；"
                 "空间不足时优先保存个人记录。收藏最多12篇。内部存储即可使用，"
                 "不依赖SD卡。常用音频会自动缓存，容量不足时淘汰旧缓存；小书包里可以清理。\n\n长文本进入分页阅读后，上下键选择上一页或下一页，确认翻页。长题目和词卡也有完整阅读入口。\n\n阅读时可翻页，"
                 "首页“继续读”会保留上次正文与阅读位置。\n\n英语词卡先想词义，"
                 "再按确认翻开答案。单词小挑战有自动判分。同步教材的选择、判断"
                 "题可直接作答，开放题先思考再对照答案、自评。教材暂以文字学习"
                 "为主。\n\n在线诗词和故事由内"
                 "容服务提供，家长可以先陪孩子挑选适合的内容。",
                 "使用说明", Screen::about);
        },
        true);
    p.button(b, "来源与许可", [&a] {
      a.read("四博学习宝 PXA "
             "版\n\n学习内容与接口参考原四博学习宝。公开服务：wtoy.doit."
             "am，同步教材：pindou.doit.am/study/"
             "。\n\n原项目、chinese-poetry、ECDICT、vistep "
             "数据和本应用代码遵循 MIT，minimp3 "
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
