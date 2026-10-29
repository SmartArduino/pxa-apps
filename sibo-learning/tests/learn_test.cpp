// SPDX-License-Identifier: MIT
#include "../model.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace sibo;
static std::string load(const std::string &path) {
  std::ifstream f(path);
  assert(f);
  return {std::istreambuf_iterator<char>(f), {}};
}
int main(int argc, char **argv) {
  assert(trusted("https://pindou.doit.am/study/data/index.json"));
  for (auto u : {"http://pindou.doit.am/study/data/index.json",
                 "https://pindou.doit.am.evil/study/data/index.json",
                 "https://pindou.doit.am/study/../../secret",
                 "https://pindou.doit.am/secret"})
    assert(!trusted(u));
  for (auto bad : {"{", "null", "[]",
                   "{\"packs\":[{\"id\":\"../"
                   "escape\",\"name\":\"x\",\"unitCount\":9000}]}"}) {
    assert(vocab_packs(bad).empty());
    assert(vocab_words(bad).empty());
    assert(school_books(bad, "math").empty());
    assert(science_books(bad).empty());
  }
  Entry lesson;
  lesson.title = "测试课";
  lesson.id = "test";
  lesson.kind = Kind::school_lesson;
  assert(school_content(
      R"({"id":"test","knowledge":{"core_concept":"概念"},"questions":[
      {"type":"choice","question":"谁？","options":["甲","乙"],"answer":"乙"},
      {"type":"true_false","question":"对吗？","answer":"错"},
      {"type":"word_order","question":"排句子","options":["家","我","爱"],"answer":"我,爱,家"},
      {"type":"fill_blank","question":"几？","answer":3},
      {"type":"choice","question":"无效？","options":["a","b"],"answer":"c"},
      {"type":"choice","question":"不应默认正确","options":["a","b"]}]})",
      lesson));
  assert(lesson.questions.size() == 5);
  assert(lesson.questions[0].answer == 1 && !lesson.questions[0].self_check);
  assert(lesson.questions[1].answer == 1 && !lesson.questions[1].self_check);
  for (unsigned i = 2; i < 5; ++i)
    assert(lesson.questions[i].self_check && lesson.questions[i].count == 1);
  Entry restored;
  assert(deserialize(serialize(lesson), restored));
  assert(restored.questions.size() == 5 &&
         restored.questions[2].solution == "我,爱,家");
  assert(plain_math("$2 \\times 3 \\div 2$") == "2 × 3 ÷ 2");
  assert(plain_math("$\\frac{1}{\\frac{2}{3}}$") == "(1)/((2)/(3))");
  assert(!plain_math("\\frac{broken").empty());
  auto words = vocab_words(R"({"words":[
    {"id":"a","word":"cat","translation":"猫","phonetic":"kæt","audio":{"path":"packs/zk/001.mp3","startMs":100,"endMs":500}},
    {"id":"b","word":"kitten","translation":"猫"},
    {"id":"c","word":"dog","translation":"狗","audio":{"path":"https://evil/test.mp3","startMs":100,"endMs":500}},
    {"id":"d","word":"fish","translation":"鱼","audio":{"path":"packs/zk/001.mp3","startMs":800,"endMs":500}}]})");
  assert(words.size() == 4 && words[0].audio_start == 100 &&
         words[0].audio_end == 500);
  assert(words[2].audio.empty() && words[3].audio.empty());
  assert(deserialize(serialize(words[0]), restored) &&
         restored.audio_start == 100 && restored.audio_end == 500);
  auto qs = vocab_quiz(words, 2);
  assert(qs.size() == 4);
  for (const auto &q : qs) {
    assert(q.count == 3 && q.answer < 3);
    for (unsigned i = 0; i < q.count; ++i)
      for (unsigned j = i + 1; j < q.count; ++j)
        assert(q.options[i] != q.options[j]);
  }
  Entry book;
  book.id = "test";
  book.title = "小发现";
  assert(science_pages(
             R"({"id":"wrong","sections":[{"title":"a","text":"b"}]})", book)
             .empty());
  auto pages = science_pages(
      R"({"id":"test","sections":[{"title":"光","text":"看看光","image":"images/test/01.jpg"}]})",
      book);
  assert(pages.size() == 1 &&
         pages[0].image == std::string(science_base) + "/images/test/01.jpg");
  assert(deserialize(serialize(pages[0]), restored) &&
         restored.kind == Kind::science_page);
  if (argc > 1) {
    std::string dir = argv[1];
    auto packs = vocab_packs(load(dir + "/vocab.json"));
    assert(packs.size() == 9);
    unsigned units = 0;
    for (auto &pack : packs)
      units += vocab_units(pack).size();
    assert(units == 794);
    auto actual = vocab_words(load(dir + "/vocab-unit.json"));
    assert(actual.size() == 50 && !actual.back().audio.empty());
    for (unsigned i = 0; i < 50; ++i) {
      auto quiz = vocab_quiz(actual, i);
      assert(quiz.size() == 12);
      assert(quiz.front().options[quiz.front().answer] == actual[i].body);
    }
    unsigned books = 0;
    for (auto s : {"chinese", "math", "english", "science"})
      books += school_books(load(dir + "/study.json"), s).size();
    assert(books == 44);
    auto maths = school_books(load(dir + "/study.json"), "math");
    auto lessons = school_lessons(load(dir + "/outline.json"), maths[0]);
    assert(lessons.size() == 32);
    auto l = lessons[0];
    assert(school_content(load(dir + "/g1up-u1-kp1.json"), l));
    assert(l.questions.size() == 12 && !l.body.empty());
    assert(l.questions[0].count == 2 && !l.questions[0].self_check &&
           l.questions.back().self_check);
    auto science = science_books(load(dir + "/vistep.json"));
    assert(science.size() == 62);
    auto aperture =
        *std::find_if(science.begin(), science.end(),
                      [](const auto &e) { return e.id == "aperture"; });
    assert(science_pages(load(dir + "/science.json"), aperture).size() == 8);
  }
  std::cout << "English, textbook and science model checks passed\n";
}
