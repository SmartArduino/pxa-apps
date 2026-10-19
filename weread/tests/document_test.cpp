#include "../reading.hpp"
#include "../state.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace weread;
 assert(book_image_url("//res.weread.qq.com/a.png?a=1&amp;b=2")=="https://res.weread.qq.com/a.png?a=1&b=2");
 assert(image_redirect("https://res.weread.qq.com/a/b","https://wrco-40036.sh.gfp.tencent-cloud.com/api?v=1&amp;key=2")==
     "https://wrco-40036.sh.gfp.tencent-cloud.com/api?v=1&amp;key=2");
 assert(image_redirect("https://res.weread.qq.com/a/b","/img?a=1")=="https://res.weread.qq.com/img?a=1");
 assert(image_redirect("https://res.weread.qq.com/a/b","?v=2")=="https://res.weread.qq.com/a/b?v=2");
 for(auto bad:{"http://res.weread.qq.com/a","https://evil.gfp.tencent-cloud.com.evil/a","https://user@wrco.sh.gfp.tencent-cloud.com/a","file:///etc/passwd","https://127.0.0.1/a"})
     assert(!image_redirect("https://res.weread.qq.com/a",bad));
 for(auto bad:{"https://res.weread.qq.com.evil/a","http://res.weread.qq.com/a","https://user@res.weread.qq.com/a","https://res.weread.qq.com:443/a","data:image/png;base64,AA==","https://res.weread.qq.com/a&#10;x"})assert(!book_image_url(bad));
 assert(decimal_fixed("1.389",65536,64)==91030);
 for(auto bad:{"", "0", "-1", "nan", "1e9", "9999999999999999", "1.2.3"})assert(!decimal_fixed(bad,65536,64));
 std::vector<BookImage> images;bool limit=false;
 auto text=reader_text("<p>A<img src='//res.weread.qq.com/a' alt='a&amp;b' data-ratio='1.389' style='color:red; width:3.5em'/>B</p>",{},&images,&limit);
 assert(!limit&&text=="A\xef\xbf\xbc" "B"&&images.size()==1);
 assert(images[0].offset==1&&images[0].width_em_q8==896&&images[0].aspect_q16==47182&&images[0].alt=="a&b");
 assert(migrate_text_offset(0,images)==0&&migrate_text_offset(1,images)==4&&migrate_text_offset(2,images)==5);
 assert(reader_text("<p>A<img src='x'>B</p>")=="AB");
 for(auto ratio:{"0.00001","64"}){
  reader_text("<img src='//res.weread.qq.com/a' data-ratio='"+std::string(ratio)+"'>",{},&images);
  assert(images.size()==1&&images[0].aspect_q16>=1024&&images[0].aspect_q16<=4194304);
 }
 SavedState saved;saved.remember({"1","opaque",1,0,123,1});saved.preferences.images=false;
 SavedState copy;assert(copy.decode(saved.encode())&&copy.find("1")->text_version==1&&!copy.preferences.images);
 auto legacy=saved.encode();
 legacy.erase(legacy.find(",\"images\":false"),15);legacy.erase(legacy.find(",\"text\":1"),9);
 assert(copy.decode(legacy)&&copy.find("1")->text_version==0&&copy.preferences.images);
 std::string many;for(unsigned i=0;i<1000;++i)many+="<img src='https://res.weread.qq.com/"+std::string(100,'x')+"'>";
 assert(reader_text(many,{},&images,&limit).empty()&&limit&&images.empty());
 std::cout<<"Book image URL, entity, dimensions, text positions and metadata budget passed\n";
}
