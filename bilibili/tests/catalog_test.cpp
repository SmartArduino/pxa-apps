// SPDX-License-Identifier: MIT
#include "../catalog.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstdio>
int main(int argc,char **argv) {
    bili::SearchHistory searches;
    for(unsigned i=0;i<10;++i)searches.put("搜索 "+std::to_string(i));
    searches.put("搜索 8");assert(searches.terms.size()==8&&searches.terms.front()=="搜索 8");
    auto search_bytes=searches.encode();assert(search_bytes.size()<=2048);bili::SearchHistory restored_searches;
    assert(restored_searches.decode(search_bytes.data(),search_bytes.size())&&restored_searches.terms==searches.terms);
    search_bytes[8]=0;assert(!restored_searches.decode(search_bytes.data(),search_bytes.size())&&restored_searches.terms==searches.terms);
    searches.terms.clear();search_bytes=searches.encode();assert(restored_searches.decode(search_bytes.data(),search_bytes.size())&&restored_searches.terms.empty());
    char id[13]{};assert(bili::extract_id("https://www.bilibili.com/video/BV1J4411v7g6?p=1",id));
    assert(!std::strcmp(id,"BV1J4411v7g6"));assert(!bili::extract_id("BV1J4411v7g6x",id));
    assert(!bili::video_id("BV1J4411v7g!"));
    assert(bili::plain_title("<em class=\"keyword\">ESP32</em> &amp; PXA") == "ESP32 & PXA");
    assert(bili::seconds(" 829:29")==49769 && bili::seconds("1:02:03")==3723);
    assert(!bili::seconds("1:2:3:4") && !bili::seconds("999999999999999999"));
    char url[320];assert(bili::picture_url("//i0.hdslb.com/bfs/archive/test.jpg",url,sizeof(url)));
    assert(!std::strcmp(url,"https://i0.hdslb.com/bfs/archive/test.jpg@320w_180h_1c.jpg"));
    assert(bili::picture_url("http://i0.hdslb.com/bfs/a.jpg",url,sizeof(url)));
    assert(!bili::picture_url("https://i0.hdslb.com.evil/a.jpg",url,sizeof(url)));
    assert(!bili::picture_url("https://a@i0.hdslb.com/a.jpg",url,sizeof(url)));
    assert(!bili::picture_url("file:///tmp/a",url,sizeof(url)));
    assert(!bili::picture_url("https://i0.hdslb.com/a\r\nb",url,sizeof(url)));
    const char json[]=R"({"code":0,"data":{"result":[{"bvid":"BV1J4411v7g6","title":"<em>测试</em> &amp; 视频","author":"UP","pic":"//i0.hdslb.com/a.jpg","duration":"1:02","play":12345}]}})";
    auto list=bili::parse_list({reinterpret_cast<const uint8_t*>(json),sizeof(json)-1},true);
    assert(list.size()==1 && list[0].duration==62 && list[0].views==12345 && !std::strcmp(list[0].title,"测试 & 视频"));
    bili::Library library;for(unsigned i=0;i<10;++i){auto video=list[0];video.bvid[11]=char('0'+i);video.cid=1000+i;library.put(video,i*10);}
    assert(library.records.size()==6 && library.records.front().position==90);
    auto bytes=library.encode();assert(bytes.size()<=2048);
    bili::Library restored;assert(restored.decode(bytes.data(),bytes.size()) && restored.records.size()==6);
    assert(restored.records.front().video.cid==1009 && restored.records.front().position==90 && restored.find("BV1J4411v7g9"));
    auto legacy=bytes;std::memcpy(legacy.data(),"BLY1",4);legacy.resize(8+restored.records.size()*293);
    for(unsigned i=0;i<restored.records.size();++i)std::memcpy(legacy.data()+8+i*293,bytes.data()+8+i*301,293);
    bili::Library migrated;assert(migrated.decode(legacy.data(),legacy.size())&&migrated.records.front().video.cid==0);
    std::memset(bytes.data()+8+21,'x',96);assert(!restored.decode(bytes.data(),bytes.size()));
    assert(restored.records.size()==6);assert(!restored.decode(bytes.data(),7));
    assert(restored.erase("BV1J4411v7g9") && !restored.find("BV1J4411v7g9"));
    for(int i=1;i<argc;++i){std::ifstream f(argv[i],std::ios::binary);assert(f);std::string data((std::istreambuf_iterator<char>(f)),{});
        auto videos=bili::parse_list({reinterpret_cast<const uint8_t*>(data.data()),data.size()},i==2);
        assert(!videos.empty() && videos.size()<=24);for(auto& v:videos)assert(bili::video_id(v.bvid) && *v.title);
        std::printf("API fixture %d: %zu videos\n",i,videos.size());}
    std::puts("Catalog, title sanitation, URL bounds, durations and persistent library passed");
}
