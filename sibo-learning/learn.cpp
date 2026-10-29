// SPDX-License-Identifier: MIT
// Public JSON contracts for the Sibo static learning catalogs. No miniapp
// implementation is linked into the PXA client.
#include "model.hpp"
#include <cstdlib>
namespace sibo {
static Value document(std::string_view bytes) {
  return study::complete_json(bytes) ? study::json(bytes) : Value{};
}
static std::vector<Value> array(Value value, unsigned limit) {
  std::vector<Value> out;
  media::json::Array a(value);
  Value v;
  while (out.size() < limit && a.next(v))
    out.push_back(v);
  return out;
}
static std::string name(Value v) { return str(v, 180); }
static bool identifier(std::string_view s) {
  return !s.empty() && s.size() <= 70 &&
         std::all_of(s.begin(), s.end(), [](unsigned char c) {
           return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
         });
}
std::string plain_math(std::string text) {
  // Show elementary fractions using readable inline notation on a small
  // screen. Balanced groups preserve nested fractions without a TeX engine.
  auto group = [&](size_t &at, std::string &content) {
    while (at < text.size() && text[at] == ' ')
      ++at;
    if (at == text.size() || text[at] != '{')
      return false;
    size_t begin = ++at;
    unsigned depth = 1;
    while (at < text.size()) {
      if (text[at] == '{' && ++depth > 16)
        return false;
      if (text[at] == '}' && --depth == 0) {
        content = text.substr(begin, at++ - begin);
        return true;
      }
      ++at;
    }
    return false;
  };
  size_t search = 0;
  for (unsigned work = 0; work < 64; ++work) {
    auto at = text.find("\\frac", search);
    if (at == text.npos)
      break;
    auto end = at + 5;
    std::string top, bottom;
    if (group(end, top) && group(end, bottom))
      text.replace(at, end - at, "(" + top + ")/(" + bottom + ")");
    else
      search = at + 5;
  }
  for (auto [from, to] : {std::pair{"\\times", "×"},
                          {"\\div", "÷"},
                          {"\\leq", "≤"},
                          {"\\geq", "≥"},
                          {"\\neq", "≠"},
                          {"\\circ", "°"},
                          {"\\pi", "π"},
                          {"\\cdot", "·"},
                          {"\\pm", "±"},
                          {"\\left", ""},
                          {"\\right", ""},
                          {"\\text", ""},
                          {"\\mathrm", ""},
                          {"\\quad", " "},
                          {"\\,", " "},
                          {"\\sqrt", "√"},
                          {"\\(", ""},
                          {"\\)", ""},
                          {"\\[", ""},
                          {"\\]", ""},
                          {"$", ""}}) {
    size_t at = 0;
    while ((at = text.find(from, at)) != text.npos) {
      text.replace(at, std::char_traits<char>::length(from), to);
      at += std::char_traits<char>::length(to);
    }
  }
  return text;
}
std::vector<Entry> vocab_packs(std::string_view bytes) {
  std::vector<Entry> out;
  for (auto v : array(document(bytes).get("packs"), 32)) {
    Entry e;
    e.kind = Kind::vocab_pack;
    e.id = str(v.get("id"), 32);
    e.title = name(v.get("name"));
    auto count = v.get("count").number(), units = v.get("unitCount").number();
    if (!identifier(e.id) || e.title.empty() || !units || units > 200 ||
        count > 10000)
      continue;
    e.units = unsigned(units);
    e.detail =
        std::to_string(count) + " 词 · " + std::to_string(units) + " 单元";
    out.push_back(std::move(e));
  }
  return out;
}
std::vector<Entry> vocab_units(const Entry &pack) {
  std::vector<Entry> out;
  if (!identifier(pack.id) || pack.units > 200)
    return out;
  for (unsigned i = 1; i <= pack.units; ++i) {
    Entry e;
    e.kind = Kind::vocab_unit;
    auto number = std::to_string(i + 1000).substr(1);
    e.id = pack.id + "/" + number;
    e.title = "第 " + std::to_string(i) + " 单元";
    e.detail = pack.title + " · 最多 50 词";
    e.path = asset(vocab_base, "packs/" + e.id + ".json");
    out.push_back(std::move(e));
  }
  return out;
}
std::vector<Entry> vocab_words(std::string_view bytes) {
  auto root = document(bytes);
  std::vector<Entry> out;
  for (auto v : array(root.get("words"), 50)) {
    Entry e;
    e.kind = Kind::word;
    e.id = str(v.get("id"), 70);
    e.title = str(v.get("word"), 100);
    e.detail = str(v.get("phonetic"), 140);
    e.body = str(v.get("translation"), 1600);
    if (e.body.empty())
      e.body = str(v.get("definition"), 1600);
    auto audio = v.get("audio");
    e.audio = asset(vocab_base, str(audio.get("path"), 240));
    auto start = audio.get("startMs").number(),
         end = audio.get("endMs").number();
    if (start < end && end <= 300000) {
      e.audio_start = unsigned(start);
      e.audio_end = unsigned(end);
    } else
      e.audio.clear();
    if (!e.id.empty() && !e.title.empty() && !e.body.empty())
      out.push_back(std::move(e));
  }
  return out;
}
std::vector<Question> vocab_quiz(const std::vector<Entry> &words,
                                 unsigned start) {
  std::vector<Question> out;
  if (words.empty())
    return out;
  for (unsigned i = 0; i < std::min<size_t>(12, words.size()); ++i) {
    auto index = (start + i) % words.size();
    const auto &word = words[index];
    Question q;
    q.text = word.title + "\n" + word.detail + "\n选出这个单词的词义。";
    std::vector<std::string> choices{word.body};
    for (unsigned j = 1; j < words.size() && choices.size() < 4; ++j) {
      const auto &candidate = words[(index + j) % words.size()].body;
      if (std::find(choices.begin(), choices.end(), candidate) == choices.end())
        choices.push_back(candidate);
    }
    if (choices.size() < 2)
      continue;
    q.count = unsigned(choices.size());
    q.answer = i % q.count;
    for (unsigned j = 0; j < q.count; ++j)
      q.options[(j + q.answer) % q.count] = choices[j];
    q.explanation = word.title + "：" + word.body;
    out.push_back(std::move(q));
  }
  return out;
}
std::vector<Entry> school_books(std::string_view bytes,
                                std::string_view subject) {
  std::vector<Entry> out;
  for (auto v : array(document(bytes).get("books"), 80)) {
    if (str(v.get("subject"), 32) != subject)
      continue;
    Entry e;
    e.kind = Kind::textbook;
    e.id = str(v.get("id"), 70);
    e.title = name(v.get("gradeName")) + name(v.get("semesterName"));
    e.detail = name(v.get("subjectName")) + " · " +
               std::to_string(v.get("lessonsCount").number()) + " 课";
    if (!identifier(e.id) || e.title.empty())
      continue;
    e.path = asset(school_base, "books/" + e.id + "/outline.json");
    out.push_back(std::move(e));
  }
  return out;
}
std::vector<Entry> school_lessons(std::string_view bytes, const Entry &book) {
  std::vector<Entry> out;
  if (!identifier(book.id))
    return out;
  for (auto v : array(document(bytes).get("lessons"), 200)) {
    Entry e;
    e.kind = Kind::school_lesson;
    e.id = str(v.get("id"), 70);
    e.title = name(v.get("title"));
    e.detail = name(v.get("unitTitle")) + " · " +
               std::to_string(v.get("questionCount").number()) + " 题";
    if (!identifier(e.id) || e.title.empty())
      continue;
    e.path =
        asset(school_base, "books/" + book.id + "/lessons/" + e.id + ".json");
    out.push_back(std::move(e));
  }
  return out;
}
static std::string answer_text(Value v) {
  auto s = str(v, 1600);
  if (!s.empty())
    return plain_math(std::move(s));
  auto items = array(v, 12);
  if (!items.empty()) {
    for (auto item : items) {
      if (!s.empty())
        s += "、";
      s += str(item, 180);
    }
    return plain_math(std::move(s));
  }
  if (v.p && v.n && (v.p[0] == '-' || (v.p[0] >= '0' && v.p[0] <= '9')))
    return std::string(reinterpret_cast<const char *>(v.p),
                       std::min<size_t>(v.n, 80));
  if (v.p && v.n && (v.p[0] == 't' || v.p[0] == 'f'))
    return v.boolean() ? "true" : "false";
  return {};
}
bool school_content(std::string_view bytes, Entry &lesson) {
  auto root = document(bytes);
  if (!root || str(root.get("id"), 70) != lesson.id)
    return false;
  Entry e = lesson;
  e.questions.clear();
  auto knowledge = root.get("knowledge");
  e.body = plain_math(str(knowledge.get("core_concept"), 6000));
  auto append = [&](std::string title, std::string body) {
    if (!body.empty()) {
      if (!e.body.empty())
        e.body += "\n\n";
      e.body += title + plain_math(std::move(body));
    }
  };
  append("记一记\n", str(knowledge.get("key_formula"), 1800));
  for (auto v : array(knowledge.get("common_mistakes"), 6))
    append("容易弄错\n", str(v, 800));
  append("小窍门\n", str(knowledge.get("tips"), 1800));
  if (e.body.empty())
    e.body = "先试着做一做，再看答案和讲解。";
  for (auto v : array(root.get("questions"), 40)) {
    Question q;
    q.text = plain_math(str(v.get("question"), 1600));
    q.solution = answer_text(v.get("answer"));
    q.explanation = plain_math(str(v.get("explanation"), 2400));
    auto type = str(v.get("type"), 48);
    if (q.text.empty() || q.solution.empty())
      continue;
    auto opts = array(v.get("options"), 12);
    q.count = 0;
    q.answer = 99;
    if (type == "true_false") {
      q.options[0] = "对";
      q.options[1] = "错";
      q.count = 2;
      if (q.solution == "对" || q.solution == "正确" || q.solution == "true")
        q.answer = 0;
      if (q.solution == "错" || q.solution == "错误" || q.solution == "false")
        q.answer = 1;
    } else if (type == "choice" && opts.size() >= 2 && opts.size() <= 4) {
      for (auto opt : opts) {
        auto text = plain_math(str(opt, 1600));
        if (text.empty())
          break;
        if (text == q.solution)
          q.answer = q.count;
        q.options[q.count++] = std::move(text);
      }
    }
    if (q.answer >= q.count) {
      q.self_check = true;
      q.count = 1;
      q.answer = 0;
      q.options[0] = "想好啦 · 看答案";
      if (!opts.empty()) {
        q.text += "\n";
        for (auto opt : opts)
          q.text += "\n" + plain_math(str(opt, 300));
      }
    }
    e.questions.push_back(std::move(q));
  }
  lesson = std::move(e);
  return true;
}
std::vector<Entry> science_books(std::string_view bytes) {
  std::vector<Entry> out;
  for (auto v : array(document(bytes).get("items"), 150)) {
    Entry e;
    e.kind = Kind::science_book;
    e.id = str(v.get("id"), 70);
    e.title = name(v.get("title"));
    e.detail = name(v.get("categoryName")) + " · " +
               std::to_string(v.get("minAge").number()) + " 岁起";
    e.path = asset(science_base, str(v.get("item"), 240));
    if (!identifier(e.id) || e.title.empty() || e.path.empty())
      continue;
    out.push_back(std::move(e));
  }
  return out;
}
std::vector<Entry> science_pages(std::string_view bytes, const Entry &book) {
  auto root = document(bytes);
  std::vector<Entry> out;
  if (str(root.get("id"), 70) != book.id)
    return out;
  for (auto v : array(root.get("sections"), 32)) {
    Entry e;
    e.kind = Kind::science_page;
    e.id = book.id + "-" + std::to_string(out.size() + 1);
    e.title = name(v.get("title"));
    e.detail = "第 " + std::to_string(out.size() + 1) + " 步 · " + book.title;
    e.body = str(v.get("text"), 6000);
    e.image = asset(science_base, str(v.get("image"), 240));
    if (e.title.empty() || e.body.empty())
      continue;
    auto sources = root.get("sources");
    std::string credit = "\n\n图文来源：vistep（MIT）";
    for (auto source : array(sources, 6))
      credit += "\n" + str(source.get("title"), 180);
    e.body += credit;
    out.push_back(std::move(e));
  }
  return out;
}
} // namespace sibo
