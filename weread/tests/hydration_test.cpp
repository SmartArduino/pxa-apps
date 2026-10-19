#include "../reading.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
 using namespace weread;
 Hydration h;
 auto r=h.decode("window.__NUXT__=(function(a,b){a[0]={title:b};a[2]=1;a[2]=2;return {list:a}}(Array(1),\"safe\"));");assert(r);
 auto list=weread::Json(*r).get("list");unsigned i=0;assert(list.each([&](auto v){if(i==0)assert(v.get("title").string()=="safe");if(i==1)assert(v.raw()=="null");if(i==2)assert(v.number()==2);++i;return true;}));assert(i==3);
 r=h.decode("window.__NUXT__=(function(a){a.x=1;a.y=2;a.x=3;return a}({}));");assert(r&&weread::Json(*r).get("x").number()==3);
 for(auto bad:{"window.__NUXT__=(function(a){a[0]=a;return a}(Array(1)));",
  "window.__NUXT__=(function(a){a[8192]=1;return a}(Array(1)));",
  "window.__NUXT__=(function(a){a[01]=1;return a}(Array(1)));",
  "window.__NUXT__=(function(a){a[x]=1;return a}(Array(1)));",
  "window.__NUXT__=(function(a){a[0]=alert(1);return a}(Array(1)));",
  "window.__NUXT__=(function(a){global[0]=1;return a}(Array(1)));"})assert(!h.decode(bad));

 // Borrowed boundaries must ignore punctuation inside escaped/Unicode strings.
 // The validating first pass still rejects invalid data in unused branches.
 for(std::string_view text:{"ordinary run",R"([ ] { } \ " ' end)","中文𠳐", "A\nB\tC"}){
  const auto html="window.__NUXT__=(function(a){return {state:{unused:{nested:[a,{value:a},Array(3)],empty:void 0},reader:{bookInfo:{bookId:'x',title:a},curChapter:{chapterUid:1,title:a},contentText:a,chapterIndexes:[],readerUrlParams:[]}}}}("+quote(text)+"));";
  auto full=h.decode(html);auto projected=h.decode_reader(html);assert(full&&projected);
  assert(Json(*projected).get("contentText").string()==text);
  assert(Json(*projected).get("bookInfo").get("title").string()==text);
  assert(Json(*full).get("state").get("reader").get("contentText").string()==text);
 }
 for(std::string_view bad:{"alert(1)","{x:1]","'unclosed","[1,]"}){
  const auto html="window.__NUXT__=(function(a){return {state:{unused:"+std::string(bad)+",reader:{bookInfo:{},curChapter:{}}}}}(0));";
  assert(!h.decode_reader(html));
 }
 // Resumable root/state/reader lookup works for literal objects, aliases,
 // initialized fields, escaped keys, and last-write-wins field assignments.
 for(auto html:{
  "window.__NUXT__=(function(a,b,c,d){a.state=b;b.reader=c;c.bookInfo=d;c.curChapter={chapterUid:1};c.contentText='old';c.contentText='new';c.chapterIndexes=[];c.readerUrlParams=[];return a}({},{},{},{title:'data'}));",
  "window.__NUXT__=(function(a){return {'sta\\u0074e':{'rea\\u0064er':{bookInfo:{title:a},curChapter:{chapterUid:1},contentText:a,chapterIndexes:[],readerUrlParams:[]}}}}('new'));"}){
  Hydration incremental;assert(incremental.prepare(html));auto job=incremental.reader_projection(std::string_view(html).size());
  unsigned steps=0;while(!job.done()){assert(job.step(1));assert(++steps<1000);}
  const auto value=std::move(job).result();assert(value);
  assert(Json(*value).get("contentText").string()=="new");
 }

 std::string raw="window.__NUXT__=(function(";
 for(unsigned i=0;i<5117;++i){if(i)raw+=',';raw+="a"+number(i);}
 raw+="){";
 for(unsigned i=0;i<900;++i){
  raw+="a0["+number(i)+"]={chapterUid:"+number(i+1)+",chapterIdx:"+number(i)+",title:a5116};";
  raw+="a1["+number(i)+"]={cUid:"+number(i+1)+",param:a5116};";
 }
 raw+="return {state:{reader:{bookInfo:{title:a5116,bookId:'synthetic'},curChapter:{title:a5116,chapterUid:701,chapterIdx:700},chapterIndexes:a0,readerUrlParams:a1,chapterInfoCount:910,curCatalogRange:{start:10},curChapterUrlParam:{param:a5116}}}}}(Array(900),Array(900)";
 for(unsigned i=2;i<5117;++i)raw+=",'synthetic'";
 raw+="));";
 auto check=[](const std::string& html,unsigned total,unsigned begin){
  Hydration h;auto output=h.decode_reader(html);
  if(!output)std::cerr<<"stage="<<h.stage<<'\n';
  assert(output);const auto json=Json(*output);
  for(auto [bytes,operations]:{std::pair{1u,1u},std::pair{7u,3u},std::pair{4096u,32u}}){
   Hydration incremental;auto preparation=incremental.preparation(html);unsigned steps=0;
   while(!preparation.done()){
    assert(preparation.step(bytes,operations));assert(++steps<2000000);
   }
   assert(preparation.result());auto projection=incremental.reader_projection(html.size());
   while(!projection.done())assert(projection.step(17));
   assert(std::move(projection).result()==output);
  }
  for(unsigned rows:{1u,7u,256u}){
   Hydration incremental;assert(incremental.prepare(html));
   auto job=incremental.reader_projection(html.size());unsigned steps=0;
   while(!job.done()){assert(job.step(rows));assert(++steps<20000);}
   assert(std::move(job).result()==output);
  }
  { // Cancellation owns no source bytes or global parser state.
   Hydration interrupted;assert(interrupted.prepare(html));
   {auto job=interrupted.reader_projection(html.size());for(int i=0;i<15&&!job.done();++i)assert(job.step(1));}
   assert(interrupted.decode_reader(html)==output);
  }
  unsigned chapters=0,params=0;
  assert(json.get("chapterIndexes").each([&](Json){++chapters;return true;}));
  assert(json.get("readerUrlParams").each([&](Json){++params;return true;}));
  assert(chapters==300&&params>=300);
  assert(json.get("chapterInfoCount").number()==total);
  assert(json.get("catalogWindow").get("offset").number()==begin);
  Reading reader;assert(reader.parse(json));
  assert(reader.catalog.items.size()==300&&reader.catalog.begin==begin&&reader.catalog.total==total);
  assert(std::ranges::all_of(reader.catalog.items,[](const auto& c){return !c.param.empty();}));
  std::cout<<"projected "<<html.size()<<" -> "<<output->size()<<" B; directory="<<chapters<<'\n';
 };
 check(raw,910,550);
 { // Cancellation at any preparation phase leaves no retained source/cache.
  Hydration interrupted;
  for(unsigned pause:{1u,50u,500u,2000u}){
   {auto job=interrupted.preparation(raw);for(unsigned i=0;i<pause&&!job.done();++i)assert(job.step(97,5));}
   assert(interrupted.decode_reader(raw));
  }
 }
 { // Even one very long literal is sliced, including split escape sequences.
  const auto text=std::string(250000,'x')+"\n中文𠳐";
  const auto html="window.__NUXT__=(function(a){return {body:a}}("+quote(text)+"));";
  Hydration incremental;auto job=incremental.preparation(html);unsigned steps=0;
  while(!job.done()){
   const auto begin=incremental.position();assert(job.step(31,7));
   assert(incremental.position()-begin<128);assert(++steps<20000);
  }
  assert(job.result());assert(incremental.decode(html)==h.decode(html));
 }
 // Truncation must finish with an error rather than leave a no-progress job.
 for(std::size_t size=0;size<raw.size();size+=997){
  Hydration truncated;auto job=truncated.preparation(std::string_view(raw).substr(0,size));unsigned steps=0;
  while(!job.done()){job.step(4096,256);assert(++steps<20000);}
  assert(!job.result());
 }
 assert(!h.decode_reader(raw,2097152)&&h.capacity_limited);
 assert(h.decode_reader(raw)&&!h.capacity_limited);

 { // The index is governed by retained bytes, not an arbitrary 8192-alias cap.
  std::string many="window.__NUXT__=(function(";
  for(unsigned i=0;i<9000;++i){if(i)many+=',';many+="a"+number(i);}
  many+="){return {state:{reader:{bookInfo:{bookId:'1'},curChapter:{chapterUid:1},contentText:a8999,chapterIndexes:[],readerUrlParams:[]}}}}(";
  for(unsigned i=0;i<9000;++i){if(i)many+=',';many+=i==8999?"'kept'":"0";}
  many+="));";
  auto result=h.decode_reader(many);assert(result&&Json(*result).get("contentText").string()=="kept");
  {auto job=h.preparation(many,1310720);while(!job.done())job.step();assert(!job.result()&&h.capacity_limited);}
  assert(h.decode_reader(many)&&!h.capacity_limited);
  // Previously retained index storage cannot make the next smaller page fail.
  assert(h.decode_reader("window.__NUXT__=(function(a){return {state:{reader:{bookInfo:{},curChapter:{},contentText:a,chapterIndexes:[],readerUrlParams:[]}}}}('small'));"));
 }

 { // A bounded window must preserve the complete directory, including its tail.
  std::string page="window.__NUXT__=(function(a){return {state:{reader:{bookInfo:{bookId:'1',lastChapterIdx:2538},curChapter:{chapterUid:1},chapterInfoCount:2538,chapterIndexes:[";
  for(unsigned i=1;i<=2538;++i){if(i>1)page+=',';page+="{chapterUid:"+number(i)+",chapterIdx:"+number(i)+",title:'Chapter'}";}
  page+="],readerUrlParams:[";
  for(unsigned i=1;i<=2538;++i){if(i>1)page+=',';page+="{cUid:"+number(i)+",param:'fixture-"+number(i)+"'}";}
  page+="]}}}}(0));";
  assert(h.prepare(page));unsigned visited=0;
  for(unsigned offset=0;offset<2538;offset+=300){
   auto job=h.reader_projection(page.size(),offset);while(!job.done())assert(job.step(7));
   auto output=std::move(job).result();assert(output);Reading reader;assert(reader.parse(Json(*output)));
   const auto& c=reader.catalog;
   assert(c.begin==offset&&c.total==2538&&c.source_rows==2538&&c.end==std::min(offset+300,2538u));
   for(const auto& row:c.items){assert(row.uid==++visited&&row.param=="fixture-"+number(visited));}
  }
  assert(visited==2538);
 }
 if(argc>1){std::ifstream f(argv[1]);std::string actual((std::istreambuf_iterator<char>(f)),{});check(actual,1645,0);}
}
