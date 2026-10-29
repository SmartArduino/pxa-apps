// SPDX-License-Identifier: MIT
// Models adapted from the MIT learning_content.h / poetry_service.cc contract.
// Original license and chinese-poetry attribution are retained in assets/.
#include "model.hpp"
namespace sibo {
std::string str(Value v, size_t max) { return study::string(v, max); }
std::string joined(const std::vector<std::string> &lines,
                   std::string_view sep) {
  std::string out;
  for (const auto &line : lines) {
    if (!out.empty())
      out += sep;
    out += line;
  }
  return out;
}
std::string encode(std::string_view value) {
  constexpr char hex[] = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : value)
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
      out += char(c);
    else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  return out;
}
std::string quote(std::string_view value) {
  std::string out = "\"";
  for (unsigned char c : value) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += char(c);
    } else if (c == '\n')
      out += "\\n";
    else if (c == '\r')
      out += "\\r";
    else if (c == '\t')
      out += "\\t";
    else if (c >= 32)
      out += char(c);
  }
  return out + '"';
}
bool trusted(std::string_view url) {
  const std::string prefix = std::string(origin) + "/";
  if ((!url.starts_with(prefix) &&
       !url.starts_with("https://pindou.doit.am/study/")) ||
      url.size() > 1024 || url.find('\\') != url.npos ||
      url.find("..") != url.npos || url.find('#') != url.npos)
    return false;
  for (unsigned char c : url)
    if (c <= 32 || c == 127)
      return false;
  return true;
}
std::string asset(std::string_view base, std::string_view path) {
  if (path.empty())
    return {};
  std::string prefix = std::string(base) + "/";
  if (path.starts_with(prefix))
    path.remove_prefix(prefix.size());
  if (path.empty() || path.front() == '/' || path.find(':') != path.npos ||
      path.find("..") != path.npos)
    return {};
  for (unsigned char c : path)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '/' || c == '.' || c == '_' ||
          c == '-'))
      return {};
  return prefix + std::string(path);
}
std::string story_cover(const Entry &e, std::string_view path) {
  if (e.id.size() != 10 || !e.id.starts_with("story-"))
    return {};
  for (char c : std::string_view(e.id).substr(6))
    if (c < '0' || c > '9')
      return {};
  const std::string absolute = std::string(bases[0]) + "/";
  if (path.starts_with(absolute))
    path.remove_prefix(absolute.size());
  const std::string prefix = "art/" + e.id + "/";
  if (path.size() != prefix.size() + 6 || !path.starts_with(prefix) ||
      !path.ends_with(".jpg"))
    return {};
  const auto scene = path.substr(prefix.size(), 2);
  if (scene[0] < '0' || scene[0] > '9' || scene[1] < '0' || scene[1] > '9')
    return {};
  return std::string(api) + "/api/story/cover/" + e.id + "/" +
         std::string(scene) + ".jpg";
}
std::string form(const Entry &poem) {
  return "title=" + encode(poem.title) + "&author=" + encode(poem.author) +
         "&dynasty=" + encode(poem.dynasty) +
         "&lines=" + encode(joined(poem.lines));
}
static Value json(std::string_view b) {
  return study::complete_json(b) ? study::json(b) : Value{};
}
static std::string id(Value v) {
  return !v.is_null() && v.p && v.n && v.p[0] == '"'
             ? str(v, 80)
             : std::to_string(v.number());
}
bool parse_poem(Value root, Entry &result) {
  if (root.get("poem"))
    root = root.get("poem");
  Entry e;
  e.kind = Kind::poem;
  e.id = id(root.get("id"));
  e.title = str(root.get("title"), 180);
  e.author = str(root.get("author"), 120);
  e.dynasty = str(root.get("dynasty"), 48);
  e.type = str(root.get("type"), 60);
  media::json::Array lines(root.get("lines"));
  Value line;
  size_t total = 0;
  while (lines.next(line) && e.lines.size() < 100) {
    auto s = str(line, 1024);
    if (!s.empty()) {
      total += s.size();
      if (total > body_limit)
        return false;
      e.lines.push_back(std::move(s));
    }
  }
  if (e.lines.empty()) {
    auto text = str(root.get("text"));
    if (!text.empty())
      e.lines.push_back(std::move(text));
  }
  if (e.title.empty() || e.lines.empty())
    return false;
  e.body = joined(e.lines);
  e.detail = e.dynasty + " · " + e.author;
  e.course = e.id.size() == 4 && e.id[0] == 'p';
  result = std::move(e);
  return true;
}
#include "course.inc"
#include "course_pinyin.inc"
static const OfflinePinyin *offline_poem(const Entry &entry) {
  if (!entry.course)
    return nullptr;
  const auto hash = study::hash(joined(entry.lines));
  for (const auto &row : kOfflinePinyin)
    if (entry.id == row.id && hash == row.body_hash)
      return &row;
  return nullptr;
}
bool load_course_pinyin(Entry &entry) {
  const auto *row = offline_poem(entry);
  if (!row)
    return false;
  std::string_view tokens(row->tokens);
  std::vector<Cell> cells;
  for (const auto &line : entry.lines) {
    for (size_t at = 0; at < line.size();) {
      const auto lead = static_cast<unsigned char>(line[at]);
      const size_t count = lead < 128 ? 1 : lead < 224 ? 2 : lead < 240 ? 3 : 4;
      if (at + count > line.size() || tokens.empty())
        return false;
      auto end = tokens.find(' ');
      auto token = tokens.substr(0, end);
      cells.push_back({line.substr(at, count), token == "-" ? "" : std::string(token)});
      tokens = end == tokens.npos ? std::string_view{} : tokens.substr(end + 1);
      at += count;
    }
    cells.push_back({"\n", ""});
  }
  if (!tokens.empty())
    return false;
  entry.pinyin = std::move(cells);
  return true;
}
std::vector<Entry> course() {
  std::vector<Entry> result;
  media::json::Array list(json(kPoetryCourseJson));
  Value row;
  while (list.next(row)) {
    Entry e;
    if (parse_poem(row, e)) {
      e.course = true;
      if (offline_poem(e)) {
        e.audio = asset(bases[3], "poems/" + e.id + "/listen.mp3");
        e.follow = asset(bases[3], "poems/" + e.id + "/follow.mp3");
      }
      result.push_back(std::move(e));
    }
  }
  return result;
}
std::vector<Entry> parse_catalog(std::string_view bytes, Kind kind,
                                 std::vector<Category> *categories) {
  auto root = json(bytes);
  std::vector<Entry> result;
  if (!root)
    return result;
  if (categories) {
    categories->clear();
    categories->push_back({"", "全部故事"});
    media::json::Array a(root.get("categories"));
    Value v;
    while (a.next(v) && categories->size() < 16) {
      auto k = str(v.get("id"), 48), n = str(v.get("name"), 60);
      if (!k.empty() && !n.empty())
        categories->push_back({k, n});
    }
  }
  const int lib = kind == Kind::story ? 0 : 1;
  auto values = root.get(kind == Kind::story     ? "items"
                         : kind == Kind::mengxue ? "mengxue"
                                                 : "mathear");
  media::json::Array a(values);
  Value v;
  while (a.next(v) && result.size() < 600) {
    Entry e;
    e.kind = kind;
    e.id = str(v.get("id"), 80);
    e.title = str(v.get("title"), 180);
    e.path = asset(bases[lib], str(v.get("item"), 256));
    e.audio = asset(bases[lib], str(v.get("audio"), 256));
    e.category = str(v.get("category"), 48);
    if (e.id.empty() || e.title.empty() || e.path.empty())
      continue;
    e.detail = kind == Kind::story
                   ? std::to_string(v.get("ageMin").number()) + "–" +
                         std::to_string(v.get("ageMax").number()) + "岁 · " +
                         std::to_string(v.get("minutes").number()) + "分钟"
                   : std::to_string(v.get("tracks").number()) + "段 · 约" +
                         std::to_string(v.get("minutes").number()) + "分钟";
    result.push_back(std::move(e));
  }
  return result;
}
std::vector<Entry> parse_poems(std::string_view bytes, bool assets) {
  auto root = json(bytes);
  std::vector<Entry> result;
  if (!root)
    return result;
  Value v;
  media::json::Array a(root.get("data")    ? root.get("data")
                       : root.get("items") ? root.get("items")
                                           : root);
  while (a.next(v) && result.size() < 100) {
    Entry e;
    if (assets) {
      e.kind = Kind::poem;
      e.id = str(v.get("id"), 80);
      e.title = str(v.get("title"), 180);
      e.path = asset(bases[3], str(v.get("meta"), 256));
      e.audio = asset(bases[3], str(v.get("listen"), 256));
      e.follow = asset(bases[3], str(v.get("follow"), 256));
      e.image = asset(bases[3], str(v.get("cover"), 256));
      if (e.id.empty() || e.path.empty())
        continue;
    } else if (!parse_poem(v, e))
      continue;
    result.push_back(std::move(e));
  }
  return result;
}
bool parse_story(std::string_view bytes, Entry &e) {
  auto root = json(bytes);
  auto text = str(root.get("text"));
  if (text.empty())
    return false;
  e.body = std::move(text);
  auto audio = asset(bases[0], str(root.get("audio"), 256));
  if (!audio.empty())
    e.audio = std::move(audio);
  media::json::Array a(root.get("illustrations"));
  Value first;
  if (a.next(first))
    e.image = story_cover(e, str(first.get("image"), 256));
  media::json::Array qs(root.get("questions"));
  Value q;
  e.questions.clear();
  while (qs.next(q) && e.questions.size() < 8) {
    if (str(q.get("type"), 20) != "choice")
      continue;
    Question item;
    item.text = str(q.get("prompt"), 600);
    item.explanation = str(q.get("explanation"), 1200);
    media::json::Array opts(q.get("options"));
    Value opt;
    item.count = 0;
    while (opts.next(opt) && item.count < 4)
      item.options[item.count++] = str(opt, 400);
    auto answer = q.get("answer").number(99);
    if (!item.text.empty() && item.count >= 2 && answer < item.count) {
      item.answer = unsigned(answer);
      e.questions.push_back(std::move(item));
    }
  }
  return true;
}
std::vector<Entry> parse_tracks(std::string_view bytes, Kind kind) {
  auto root = json(bytes);
  std::vector<Entry> out;
  if (!root)
    return out;
  media::json::Array tracks(root.get("tracks"));
  Value v;
  while (tracks.next(v) && out.size() < 100) {
    Entry e;
    e.kind = kind;
    e.id = std::to_string(out.size());
    e.title = str(v.get("title"), 180);
    if (e.title.empty())
      e.title = "第" + std::to_string(out.size() + 1) + "段";
    e.audio = asset(bases[1], str(v.get("audio").get("path"), 256));
    auto duration = v.get("audio").get("duration");
    uint64_t secs = 0;
    for (size_t j = 0; duration.p && j < duration.n && duration.p[j] >= '0' &&
                       duration.p[j] <= '9' && secs < 86400;
         ++j)
      secs = secs * 10 + duration.p[j] - '0';
    e.detail =
        secs ? "约" + std::to_string(std::max<uint64_t>(1, (secs + 30) / 60)) +
                   "分钟"
             : "可阅读";
    media::json::Array lines(v.get("lines")), py(v.get("pinyin"));
    Value line, pinyin;
    while (lines.next(line) && e.body.size() < body_limit) {
      if (py.next(pinyin)) {
        auto s = str(pinyin, 1024);
        if (!s.empty())
          e.body += s + "\n";
      }
      auto content = str(line, 2048);
      if (!content.empty())
        e.body += content + "\n\n";
    }
    media::json::Array questions(v.get("items"));
    Value question;
    while (questions.next(question) && e.body.size() < body_limit)
      e.body += std::to_string(question.get("index").number()) + ". " +
                str(question.get("text"), 2048) + "\n答案：" +
                str(question.get("answer"), 1024) + "\n\n";
    if (e.body.empty())
      e.body = str(v.get("speech"));
    if (e.body.size() > body_limit) {
      size_t n = body_limit;
      while (n && (static_cast<unsigned char>(e.body[n]) & 0xc0) == 0x80)
        --n;
      e.body.resize(n);
    }
    if (!e.body.empty()) {
      e.id = str(root.get("id"), 80) + "-" + e.id;
      out.push_back(std::move(e));
    }
  }
  return out;
}
std::vector<Cell> parse_pinyin(Value root) {
  std::vector<Cell> out;
  media::json::Array rows(root);
  Value row;
  while (rows.next(row) && out.size() < 1200) {
    media::json::Array a(row.get("items"));
    Value cell;
    while (a.next(cell) && out.size() < 1200) {
      auto han = str(cell.get("char"), 8), py = str(cell.get("pinyin"), 32);
      if (!han.empty())
        out.push_back({han, py});
    }
    out.push_back({"\n", ""});
  }
  return out;
}
bool enrich(std::string_view bytes, Entry &e) {
  auto root = json(bytes);
  if (!root)
    return false;
  auto py = parse_pinyin(root.get("pinyin"));
  if (!py.empty())
    e.pinyin = std::move(py);
  std::string text = str(root.get("translation")),
              explanation = str(root.get("explanation"));
  if (!explanation.empty())
    text += (text.empty() ? "" : "\n\n") + explanation;
  if (!text.empty())
    e.meta = std::move(text);
  media::json::Array qs(root.get("questions"));
  Value q;
  std::vector<Question> questions;
  while (qs.next(q) && questions.size() < 8) {
    if (str(q.get("type"), 20) != "choice")
      continue;
    auto answer = str(q.get("answer"), 8);
    Question item;
    item.text = str(q.get("question"), 600);
    item.explanation = str(q.get("explanation"), 1200);
    media::json::Array opts(q.get("options"));
    Value opt;
    size_t count = 0;
    while (opts.next(opt) && count < 4)
      item.options[count++] = str(opt, 400);
    if (!item.text.empty() && count == 4 && answer.size() == 1 &&
        answer[0] >= 'A' && answer[0] <= 'D') {
      item.answer = unsigned(answer[0] - 'A');
      questions.push_back(std::move(item));
    }
  }
  if (!questions.empty())
    e.questions = std::move(questions);
  return true;
}
std::vector<Question> make_quiz(const Entry &e) {
  if (!e.questions.empty())
    return e.questions;
  std::vector<Question> out;
  unsigned seed = study::hash(e.title);
  auto add = [&](std::string text, std::string answer,
                 const std::vector<std::string> &pool) {
    if (answer.empty())
      return;
    Question q;
    q.text = std::move(text);
    q.options[0] = answer;
    unsigned n = 1;
    for (const auto &p : pool) {
      if (p.empty() || std::find(q.options.begin(), q.options.begin() + n, p) !=
                           q.options.begin() + n)
        continue;
      q.options[n++] = p;
      if (n == 4)
        break;
    }
    if (n != 4)
      return;
    for (unsigned i = 3; i; --i)
      std::swap(q.options[i], q.options[study::random(seed) % (i + 1)]);
    q.answer = unsigned(std::find(q.options.begin(), q.options.end(), answer) -
                        q.options.begin());
    out.push_back(std::move(q));
  };
  add("《" + e.title + "》的作者是？", e.author,
      {"李白", "杜甫", "王维", "白居易", "苏轼", "汉乐府", "李清照"});
  add("《" + e.title + "》写于哪个朝代？", e.dynasty,
      {"汉", "唐", "宋", "元", "明", "清", "先秦"});
  auto poems = course();
  for (unsigned i = 0; i + 1 < e.lines.size() && out.size() < 6; ++i) {
    std::vector<std::string> pool;
    for (const auto &p : poems)
      if (p.title != e.title)
        for (const auto &line : p.lines)
          pool.push_back(line);
    add("“" + e.lines[i] + "”的下一句是？", e.lines[i + 1], pool);
  }
  return out;
}
std::string compact(std::string_view json) {
  std::string out;
  bool quoted = false, escape = false;
  out.reserve(json.size());
  for (char c : json) {
    if (quoted) {
      out += c;
      if (escape)
        escape = false;
      else if (c == '\\')
        escape = true;
      else if (c == '"')
        quoted = false;
    } else if (c == '"') {
      quoted = true;
      out += c;
    } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t')
      out += c;
  }
  return out;
}
std::string serialize(const Entry &e) {
  std::string questions = "[";
  bool first = true;
  for (const auto &q : e.questions) {
    if (!first)
      questions += ",";
    first = false;
    questions += "{\"self\":" + std::string(q.self_check ? "true" : "false") +
                 ",\"solution\":" + quote(q.solution) +
                 ",\"text\":" + quote(q.text) +
                 ",\"explanation\":" + quote(q.explanation) +
                 ",\"answer\":" + std::to_string(q.answer) + ",\"options\":[";
    for (unsigned i = 0; i < q.count; ++i) {
      if (i)
        questions += ",";
      questions += quote(q.options[i]);
    }
    questions += "]}";
  }
  questions += "]";
  return "{\"kind\":" + std::to_string(unsigned(e.kind)) +
         ",\"id\":" + quote(e.id) + ",\"title\":" + quote(e.title) +
         ",\"detail\":" + quote(e.detail) + ",\"path\":" + quote(e.path) +
         ",\"audio\":" + quote(e.audio) + ",\"follow\":" + quote(e.follow) +
         ",\"image\":" + quote(e.image) +
         ",\"author\":" + quote(e.author) + ",\"dynasty\":" + quote(e.dynasty) +
         ",\"type\":" + quote(e.type) + ",\"body\":" + quote(e.body) +
         ",\"questions\":" + questions +
         ",\"audioStart\":" + std::to_string(e.audio_start) +
         ",\"audioEnd\":" + std::to_string(e.audio_end) +
         ",\"course\":" + (e.course ? "true" : "false") + "}";
}
bool deserialize(std::string_view bytes, Entry &e) {
  auto root = json(bytes);
  if (!root)
    return false;
  Entry v;
  auto k = root.get("kind").number(100);
  if (k > unsigned(Kind::science_page) ||
      (k > 3 && k != unsigned(Kind::word) &&
       k != unsigned(Kind::school_lesson) && k != unsigned(Kind::science_page)))
    return false;
  v.kind = Kind(k);
  v.id = str(root.get("id"), 80);
  v.title = str(root.get("title"), 180);
  v.detail = str(root.get("detail"), 200);
  v.path = str(root.get("path"), 512);
  v.audio = str(root.get("audio"), 512);
  v.follow = str(root.get("follow"), 512);
  v.image = str(root.get("image"), 512);
  v.author = str(root.get("author"), 120);
  v.dynasty = str(root.get("dynasty"), 48);
  v.type = str(root.get("type"), 60);
  v.body = str(root.get("body"));
  v.course = root.get("course").boolean();
  v.audio_start =
      unsigned(std::min<uint64_t>(root.get("audioStart").number(), 300000));
  v.audio_end =
      unsigned(std::min<uint64_t>(root.get("audioEnd").number(), 300000));
  if (v.audio_end && v.audio_start >= v.audio_end)
    return false;
  if (v.id.empty() || v.title.empty() ||
      (!v.path.empty() && !trusted(v.path)) ||
      (!v.audio.empty() && !trusted(v.audio)) ||
      (!v.follow.empty() && !trusted(v.follow)) ||
      (!v.image.empty() && !trusted(v.image)))
    return false;
  if (v.kind == Kind::poem) {
    size_t start = 0;
    while (start < v.body.size()) {
      auto end = v.body.find('\n', start);
      v.lines.push_back(v.body.substr(start, end - start));
      if (end == v.body.npos)
        break;
      start = end + 1;
    }
  }
  media::json::Array qs(root.get("questions"));
  Value q;
  while (qs.next(q) && v.questions.size() < 40) {
    Question item;
    item.text = str(q.get("text"), 2400);
    item.explanation = str(q.get("explanation"), 2400);
    item.self_check = q.get("self").boolean();
    item.solution = str(q.get("solution"), 1600);
    item.answer = unsigned(q.get("answer").number(99));
    media::json::Array opts(q.get("options"));
    Value opt;
    item.count = 0;
    while (opts.next(opt) && item.count < 4)
      item.options[item.count++] = str(opt, 1600);
    if ((item.count >= 2 ||
         (item.count == 1 && item.self_check && !item.solution.empty())) &&
        item.answer < item.count && !item.text.empty())
      v.questions.push_back(std::move(item));
  }
  e = std::move(v);
  return true;
}
} // namespace sibo
