// SPDX-License-Identifier: MIT
#include "../model.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace sibo;
static std::string load(const std::string &path) {
  std::ifstream in(path);
  assert(in.good());
  return {std::istreambuf_iterator<char>(in), {}};
}
int main(int argc, char **argv) {
  auto poems = course();
  assert(poems.size() == 75);
  assert(poems.front().title == "江南");
  unsigned offline_count = 0;
  for (auto poem : poems) {
    if (!load_course_pinyin(poem))
      continue;
    ++offline_count;
    std::string text;
    for (const auto &cell : poem.pinyin)
      text += cell.han;
    assert(text == poem.body + "\n");
    assert(!poem.audio.empty() && !poem.follow.empty());
    Entry restored;
    assert(deserialize(serialize(poem), restored));
    assert(restored.follow == poem.follow && load_course_pinyin(restored));
  }
  assert(offline_count == 22);
  auto changed = poems.front();
  changed.lines.front() += "改";
  assert(!load_course_pinyin(changed) && changed.pinyin.empty());
  auto untrusted_follow = poems.front();
  untrusted_follow.follow = "https://evil.test/track.mp3";
  Entry invalid;
  assert(!deserialize(serialize(untrusted_follow), invalid));
  for (const auto &poem : poems) {
    assert(poem.kind == Kind::poem && !poem.body.empty() && poem.course);
    Entry restored;
    assert(deserialize(serialize(poem), restored));
    assert(restored.body == poem.body && restored.lines == poem.lines);
    auto questions = make_quiz(poem);
    assert(questions.size() >= 2);
    for (auto &q : questions) {
      assert(q.answer < 4);
      for (unsigned i = 0; i < 4; ++i) {
        assert(!q.options[i].empty());
        for (unsigned j = 0; j < i; ++j)
          assert(q.options[i] != q.options[j]);
      }
    }
  }
  assert(encode("江南 &+") == "%E6%B1%9F%E5%8D%97%20%26%2B");
  assert(asset(bases[0], "audio/one.mp3") ==
         std::string(bases[0]) + "/audio/one.mp3");
  for (auto path : {"../secret", "//evil.test/x", "https://evil.test/a",
                    "/audio/a.mp3", "audio/%2e%2e/x", "x\\a", "x\na"})
    assert(asset(bases[0], path).empty());
  for (auto url : {"http://wtoy.doit.am/a", "https://wtoy.doit.am.evil/a",
                   "https://wtoy.doit.am@evil/a", "https://evil.test/",
                   "https://wtoy.doit.am/../a", "https://wtoy.doit.am/a\n"})
    assert(!trusted(url));
  assert(trusted(std::string(api) + "/api/poem/search?q=%E6%9D%8E%E7%99%BD"));
  Entry e;
  e.kind = Kind::story;
  e.id = "story-0001";
  e.title = "测试故事";
  assert(story_cover(e, "art/story-0001/01.jpg") ==
         std::string(api) + "/api/story/cover/story-0001/01.jpg");
  assert(story_cover(e, "art/story-0002/01.jpg").empty());
  assert(!parse_story("{\"text\":", e));
  assert(!deserialize("{}", e));
  auto fixture =
      R"({"text":"一步一步走。","audio":"audio/test.mp3","questions":[{"type":"choice","prompt":"谁赢了？","options":["乌龟","兔子","小鸟"],"answer":0,"explanation":"坚持就好。"}]})";
  assert(parse_story(fixture, e));
  assert(e.questions.size() == 1 && e.questions[0].count == 3);
  assert(make_quiz(e).size() == 1);
  Entry saved;
  assert(deserialize(serialize(e), saved));
  assert(saved.questions.size() == 1 && saved.questions[0].count == 3);
  assert(parse_catalog("[]", Kind::story).empty());
  assert(parse_tracks("{}", Kind::mathear).empty());
  assert(
      parse_poems(
          R"({"data":[{"id":3,"title":"静夜思","author":"李白","dynasty":"唐","lines":["床前明月光，","疑是地上霜。"]}]})")
          .size() == 1);
  assert(parse_poems(R"([{"title":"无正文"}])").empty());
  std::string body;
  for (unsigned i = 0; i < 500; ++i)
    body += "好";
  std::string lines;
  for (unsigned i = 0; i < 40; ++i) {
    if (i)
      lines += ",";
    lines += quote(body);
  }
  auto track = parse_tracks(
      "{\"id\":\"book\",\"tracks\":[{\"title\":\"长文\",\"lines\":[" + lines +
          "],\"speech\":" + quote(body) + "}]}",
      Kind::mengxue);
  assert(track.size() == 1 && track[0].body.size() <= body_limit);
  assert(study::complete_json(quote(track[0].body)));
  for (auto invalid :
       {"", "{", "{\"a\":1}trailing", "[1,]", "{\"a\":\"\\ud800\"}"}) {
    assert(parse_catalog(invalid, Kind::story).empty());
    assert(parse_tracks(invalid, Kind::mengxue).empty());
    assert(parse_poems(invalid).empty());
  }
  auto compacted =
      compact(" { \"a\" : \"two words \\\" quoted\",\n\"b\": 1 } ");
  assert(study::complete_json(compacted));
  assert(compacted.find("two words") != std::string::npos);
  if (argc > 1) {
    std::string dir = argv[1];
    auto stories = parse_catalog(load(dir + "/stories.json"), Kind::story);
    assert(stories.size() >= 200);
    auto story = stories[0];
    assert(parse_story(load(dir + "/story.json"), story));
    assert(!story.audio.empty() && !story.body.empty() &&
           !story.image.empty() && !story.questions.empty());
    auto books = parse_catalog(load(dir + "/recite.json"), Kind::mengxue);
    auto grades = parse_catalog(load(dir + "/recite.json"), Kind::mathear);
    assert(books.size() == 6 && grades.size() == 6);
    auto chapters = parse_tracks(load(dir + "/mengxue.json"), Kind::mengxue);
    assert(chapters.size() == 30 && !chapters[0].audio.empty());
    auto math = parse_tracks(load(dir + "/mathear.json"), Kind::mathear);
    assert(math.size() == 10 &&
           math[0].body.find("答案：9") != std::string::npos);
    assert(enrich(load(dir + "/poem-meta.json"), poems[0]));
    assert(!poems[0].pinyin.empty() && !poems[0].meta.empty());
    assert(parse_poems(load(dir + "/course.json"), true).size() >= 22);
  }
  std::cout << "Sibo models: 75 poems, quizzes, API schemas, UTF-8, URL "
               "boundaries passed\n";
}
