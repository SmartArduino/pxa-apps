#include "../document_layout.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace weread;
 std::vector<BookImage> images;
 const std::string mark="\xef\xbf\xbc";
 BookImage image;image.offset=1;image.width_em_q8=512;image.aspect_q16=2*65536;
 images.push_back(image);
 auto page=paginate_document("A"+mark+"B",images,0,100,100,10,15);
 assert(page.end==5&&page.count==3&&page.lines[1].is_image());
 assert(page.lines[0].x==0&&page.lines[1].x==10&&page.lines[1].width==20&&page.lines[2].x==30);
 assert(page.lines[1].height==10&&page.lines[1].y==2);
 images[0].offset=0;images[0].width_em_q8=0;images[0].aspect_q16=32768;
 images.push_back(images[0]);images[1].offset=3;
 auto text=mark+mark;
 page=paginate_document(text,images,0,100,100,10,15);
 assert(page.count==1&&page.end==3&&page.lines[0].width==50&&page.lines[0].height==100);
 page=paginate_document(text,images,page.end,100,100,10,15);
 assert(page.count==1&&page.end==6);
 page=paginate_document(text,images,0,100,100,10,15,false);assert(page.end==6&&page.count==2&&page.lines[0].height==15);
 images.resize(1);images[0].offset=3;images[0].width_em_q8=512;images[0].aspect_q16=13107;
 text="A\nB"+mark+"C";
 page=paginate_document(text,images,0,30,100,10,15);
 assert(page.end==2&&page.count==1);
 page=paginate_document(text,images,2,30,100,10,15);
 assert(page.count==2&&page.end==6);
 page=paginate_document(text,images,6,100,100,10,15);assert(page.end==7&&page.count==1);
 // Mix UTF-8, newlines and many inline/block images. Every byte must make
 // forward progress; a run cannot leave the page or split a UTF-8 sequence.
 text.clear();images.clear();
 for(unsigned i=0;i<120;++i){
  text+="中文 ABC。";
  BookImage item;item.offset=text.size();item.width_em_q8=i%3?512:0;
  item.aspect_q16=i%2?32768:131072;images.push_back(item);text+=mark;
  if(i%5==0)text+='\n';
 }
 for(int width:{30,77,480})for(int height:{30,111,640})for(bool shown:{false,true}){
  std::size_t at=0,steps=0;
  while(at<text.size()){
   const auto p=paginate_document(text,images,at,width,height,10,15,shown);
   assert(p.end>at&&p.end<=text.size()&&++steps<=text.size());
   for(std::size_t i=0;i<p.count;++i){const auto& run=p.lines[i];
    assert(run.x+run.width<=width&&run.y+run.height<=height);
    assert(run.start>=at&&run.end<=p.end&&run.end>=run.start);
    assert(valid_utf8(std::string_view(text).substr(run.start,run.end-run.start)));
   }
   at=p.end;
  }
 }
 std::cout<<"Inline/block image layout, hidden-image compression and row rollback passed\n";
}
