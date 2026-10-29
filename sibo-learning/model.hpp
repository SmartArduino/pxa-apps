// SPDX-License-Identifier: MIT
#pragma once
#include "../common/study/core.hpp"
#include "../common/study/reader.hpp"
#include <array>
#include <string>
#include <vector>
namespace sibo {
using Value = media::json::Value;
inline constexpr const char *origin = "https://wtoy.doit.am";
inline constexpr const char *api = "https://wtoy.doit.am/shici";
inline constexpr const char *bases[] = {
    "https://wtoy.doit.am/oss/stories", "https://wtoy.doit.am/oss/recite",
    "https://wtoy.doit.am/oss/recite", "https://wtoy.doit.am/oss/shici"};
inline constexpr size_t body_limit = 24 * 1024, json_limit = 512 * 1024;
enum class Kind {
  story,
  mengxue,
  mathear,
  poem,
  category,
  dynasty,
  verse_type,
  favorite,
  vocab_pack,
  vocab_unit,
  word,
  subject,
  textbook,
  school_lesson,
  science_book,
  science_page
};
struct Cell {
  std::string han, py;
};
struct Question {
  std::string text, explanation;
  std::array<std::string, 4> options;
  unsigned answer = 0, count = 4;
  bool self_check = false;
  std::string solution;
};
struct Entry {
  Kind kind = Kind::story;
  std::string id, title, detail, path, audio, follow, image, category, author, dynasty,
      type, body, meta;
  std::vector<std::string> lines;
  std::vector<Cell> pinyin;
  std::vector<Question> questions;
  bool course = false;
  unsigned audio_start = 0, audio_end = 0, units = 0;
};
struct Category {
  std::string id, name;
};
std::string str(Value v, size_t max = body_limit);
std::string encode(std::string_view value);
std::string quote(std::string_view value);
std::string asset(std::string_view base, std::string_view path);
bool trusted(std::string_view url);
std::string story_cover(const Entry &entry, std::string_view path);
std::string form(const Entry &poem);
std::string joined(const std::vector<std::string> &lines,
                   std::string_view separator = "\n");
bool parse_poem(Value root, Entry &result);
std::vector<Entry> course();
bool load_course_pinyin(Entry &entry);
std::vector<Entry> parse_catalog(std::string_view bytes, Kind kind,
                                 std::vector<Category> *categories = nullptr);
std::vector<Entry> parse_poems(std::string_view bytes, bool assets = false);
bool parse_story(std::string_view bytes, Entry &result);
std::vector<Entry> parse_tracks(std::string_view bytes, Kind kind);
bool enrich(std::string_view bytes, Entry &result);
std::vector<Cell> parse_pinyin(Value value);
std::vector<Question> make_quiz(const Entry &poem);
std::string compact(std::string_view json);
std::string serialize(const Entry &value);
bool deserialize(std::string_view bytes, Entry &value);
inline constexpr const char *vocab_base = "https://wtoy.doit.am/oss/vocab";
inline constexpr const char *science_base = "https://wtoy.doit.am/oss/vistep";
inline constexpr const char *school_base = "https://pindou.doit.am/study/data";
std::vector<Entry> vocab_packs(std::string_view bytes);
std::vector<Entry> vocab_units(const Entry &pack);
std::vector<Entry> vocab_words(std::string_view bytes);
std::vector<Question> vocab_quiz(const std::vector<Entry> &words,
                                 unsigned start);
std::vector<Entry> school_books(std::string_view bytes,
                                std::string_view subject);
std::vector<Entry> school_lessons(std::string_view bytes, const Entry &book);
bool school_content(std::string_view bytes, Entry &lesson);
std::vector<Entry> science_books(std::string_view bytes);
std::vector<Entry> science_pages(std::string_view bytes, const Entry &book);
std::string plain_math(std::string text);
} // namespace sibo
