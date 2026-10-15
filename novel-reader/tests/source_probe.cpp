// Native driver for live website contract checks; shares the shipped parser.
#include "../reader_state.hpp"
#include <iostream>
#include <iterator>
#include <fstream>

int main(int argc,char** argv) {
  using namespace reader;
  if(argc<2 || argc>4)return 2;
  const std::string_view mode=argv[1];
  if(mode=="configs") {
    std::cout<<'[';bool comma=false;
    for(const auto& source:builtin_sources){if(comma)std::cout<<',';comma=true;std::cout<<source.rules;}
    std::cout<<']';return 0;
  }
  auto source=default_sources()[0];
  if(argc>=3) {
    std::ifstream file(argv[2]);if(!file)return 2;
    source.rules=std::string{std::istreambuf_iterator<char>(file),{}};
    Json config(source.rules);source.name=config.get("name").string().value_or("");source.url=config.get("searchUrl").string().value_or("");
  }
  if(mode=="config"){std::cout<<source_rules(source);return 0;}
  std::string input{std::istreambuf_iterator<char>(std::cin),{}};
  const auto rules=web::rules(source_rules(source));
  const std::string_view base=argc==4?std::string_view(argv[3]):std::string_view(source.url);
  if(!rules || input.size()>(mode=="content"?65536:max_json_bytes))return 3;
  if(mode=="books") {
    SavedState out;out.shelf=web::books(input,source,*rules,source.url);
    std::cout<<out.encode();return 0;
  }
  if(mode=="id") {
    if(!rules->paged_toc || rules->paged_toc->html)return 4;
    auto id=web::field({{},input},rules->paged_toc->id,96);
    if(id.empty())return 4;
    std::cout<<id;return 0;
  }
  if(mode=="pagecount") {
    if(!rules->paged_toc)return 5;
    const auto count=web::page_count(input,rules->paged_toc->pages);if(!count)return 5;
    std::cout<<"{\"current\":"<<count->current<<",\"total\":"<<count->total<<'}';return 0;
  }
  if(mode=="page") {
    if(!rules->paged_toc)return 5;
    const auto page=rules->paged_toc->html?web::html_toc_page(input,*rules,base,0):web::toc_page(input,*rules->paged_toc,base);
    if(!page)return 5;
    std::cout<<"{\"total\":"<<page->total<<",\"pages\":"<<page->pages<<",\"chapters\":[";
    bool comma=false;
    for(const auto& chapter:page->chapters){if(comma)std::cout<<',';comma=true;
      std::cout<<"{\"title\":"<<quote(chapter.title)<<",\"url\":"<<quote(chapter.url)<<'}';}
    std::cout<<"]}";return 0;
  }
  if(mode=="chapters") {
    const auto chapters=web::chapters(input,*rules,base);
    std::cout<<"[";bool comma=false;
    for(const auto& chapter:chapters){if(comma)std::cout<<',';comma=true;
      std::cout<<"{\"title\":"<<quote(chapter.title)<<",\"url\":"<<quote(chapter.url)<<'}';}
    std::cout<<"]";return chapters.empty()?7:0;
  }
  if(mode=="part") {
    // The matrix probe supplies original/current/index through a small JSON
    // envelope, so the native driver exercises the shipped URL and text rules.
    Json envelope(input);auto html=envelope.get("html").string(65536),original=envelope.get("original").string(512),current=envelope.get("current").string(512);
    auto index=envelope.get("index").number();if(!html || !original || !current || !index)return 8;
    auto part=web::chapter_part(*html,*rules,*original,*current,*index);if(!part)return 9;
    std::cout<<"{\"text\":"<<quote(part->text)<<",\"next\":"<<quote(part->next)<<",\"pages\":"<<part->pages<<'}';return 0;
  }
  if(mode=="content") {
    auto text=web::content(input,*rules);if(text.empty())return 6;
    std::cout<<text;return 0;
  }
  return 2;
}
