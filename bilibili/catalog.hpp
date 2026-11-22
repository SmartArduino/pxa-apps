// SPDX-License-Identifier: MIT
#pragma once
#include "../common/media/json.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>
namespace bili {
struct Video {
    char bvid[13]{},title[192]{},author[64]{},pic[256]{},description[512]{};
    uint64_t cid=0,aid=0,mid=0;
    uint32_t duration=0,views=0,danmaku=0;
};
inline bool video_id(std::string_view id) {
    if(id.size()!=12 || !id.starts_with("BV"))return false;
    for(char c:id)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')))return false;
    return true;
}
inline bool extract_id(const char *text,char (&out)[13]) {
    out[0]=0;if(!text)return false;
    const char *p=std::strstr(text,"BV");if(!p || std::strlen(p)<12 || !video_id({p,12}))return false;
    if(p[12] && ((p[12]>='0'&&p[12]<='9')||(p[12]>='A'&&p[12]<='Z')||(p[12]>='a'&&p[12]<='z')))return false;
    std::memcpy(out,p,12);out[12]=0;return true;
}
inline std::string plain_title(std::string_view s) {
    std::string out;bool tag=false;
    for(size_t i=0;i<s.size();++i) {
        if(s[i]=='<'){tag=true;continue;}if(s[i]=='>'){tag=false;continue;}if(tag)continue;
        bool entity=false;
        for(auto pair:{std::pair{"&amp;","&"},std::pair{"&lt;","<"},std::pair{"&gt;",">"},
                      std::pair{"&quot;","\""},std::pair{"&#39;","'"},std::pair{"&nbsp;"," "}})
            if(s.substr(i).starts_with(pair.first)){out+=pair.second;i+=std::strlen(pair.first)-1;entity=true;break;}
        if(!entity)out+=s[i]=='\n'||s[i]=='\r'?' ':s[i];
    }
    return out;
}
inline bool picture_url(std::string_view input,char *out,size_t capacity,bool thumbnail=true) {
    std::string url;
    if(input.starts_with("//"))url="https:";
    else if(input.starts_with("http://")){url="https://";input.remove_prefix(7);}
    url+=input;
    if(!url.starts_with("https://"))return false;
    auto slash=url.find('/',8);if(slash==std::string::npos)return false;
    auto host=std::string_view(url).substr(8,slash-8);
    if(host!="hdslb.com" && !host.ends_with(".hdslb.com"))return false;
    for(char c:host)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='-'))return false;
    if(url.find_first_of("\r\n\t\\?#@",8)!=std::string::npos)return false;
    if(thumbnail)url+="@320w_180h_1c.jpg";
    if(url.size()>=capacity)return false;
    std::memcpy(out,url.c_str(),url.size()+1);return true;
}
inline uint32_t seconds(std::string_view text) {
    uint64_t value=0,part=0;unsigned colons=0;
    for(char c:text) {
        if(c==' ' && !value && !part)continue;
        if(c==':'){if(++colons>2 || part>UINT32_MAX)return 0;value=value*60+part;part=0;}
        else if(c>='0'&&c<='9'){part=part*10+unsigned(c-'0');if(part>UINT32_MAX)return 0;}
        else return 0;
    }
    value=value*60+part;return value<=UINT32_MAX?uint32_t(value):0;
}
inline void duration_text(uint64_t seconds,char *out,size_t capacity) {
    if(seconds>=3600)std::snprintf(out,capacity,"%llu:%02u:%02u",(unsigned long long)(seconds/3600),unsigned(seconds/60%60),unsigned(seconds%60));
    else std::snprintf(out,capacity,"%02u:%02u",unsigned(seconds/60),unsigned(seconds%60));
}
inline bool parse_video(media::json::Value value,Video& out) {
    out={};bool truncated=false;
    if(!value.get("bvid").string(out.bvid,sizeof(out.bvid),&truncated)||truncated||!video_id(out.bvid))return false;
    char raw[512]{};value.get("title").string(raw,sizeof(raw));auto title=plain_title(raw);
    // Keep complete UTF-8 code points when bounding API text.
    size_t n=std::min(title.size(),sizeof(out.title)-1);
    while(n<title.size() && n && (uint8_t(title[n])&0xc0)==0x80)--n;
    std::memcpy(out.title,title.data(),n);out.title[n]=0;
    if(!*out.title)return false;
    if(!value.get("owner").get("name").string(out.author,sizeof(out.author)))value.get("author").string(out.author,sizeof(out.author));
    char picture[384]{};value.get("pic").string(picture,sizeof(picture));picture_url(picture,out.pic,sizeof(out.pic),false);
    value.get("desc").string(out.description,sizeof(out.description));out.cid=value.get("cid").number();
    out.aid=value.get("aid").number();out.mid=value.get("owner").get("mid").number(value.get("mid").number());
    out.duration=value.get("duration").number();
    if(!out.duration){char duration[32]{};if(value.get("duration").string(duration,sizeof(duration)))out.duration=seconds(duration);}
    out.views=value.get("stat").get("view").number(value.get("play").number());
    out.danmaku=value.get("stat").get("danmaku").number(value.get("video_review").number());return true;
}
inline std::vector<Video> parse_list(media::json::Value root,bool search) {
    std::vector<Video> result;if(root.get("code").number(1))return result;
    media::json::Array list(root.get("data").get(search?"result":"list"));media::json::Value item;
    while(result.size()<24 && list.next(item)){Video video;if(parse_video(item,video))result.push_back(video);}
    return result;
}
struct Record {Video video;uint32_t position=0;};
struct SearchHistory {
    std::vector<std::string> terms;
    void put(std::string_view term) {
        if(term.empty()||term.size()>191)return;
        terms.erase(std::remove(terms.begin(),terms.end(),term),terms.end());
        terms.insert(terms.begin(),std::string(term));if(terms.size()>8)terms.resize(8);
    }
    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> bytes(8+terms.size()*192);std::memcpy(bytes.data(),"BSH1",4);bytes[4]=terms.size();
        for(unsigned i=0;i<terms.size();++i)std::memcpy(bytes.data()+8+i*192,terms[i].data(),std::min<size_t>(191,terms[i].size()));
        return bytes;
    }
    bool decode(const uint8_t *data,size_t size) {
        if(!data||size<8||std::memcmp(data,"BSH1",4)||data[4]>8||data[5]||data[6]||data[7]||size!=8+data[4]*192u)return false;
        std::vector<std::string> next;
        for(unsigned i=0;i<data[4];++i){auto *p=reinterpret_cast<const char*>(data+8+i*192);auto *end=static_cast<const char*>(std::memchr(p,0,192));
            if(!end||end==p)return false;std::string term(p,end);
            if(std::find(next.begin(),next.end(),term)!=next.end())return false;next.push_back(std::move(term));}
        terms=std::move(next);return true;
    }
};
struct Library {
    static constexpr unsigned capacity=6,record_bytes=301;
    std::vector<Record> records;
    const Record *find(const char *id) const {for(auto& r:records)if(!std::strcmp(id,r.video.bvid))return &r;return nullptr;}
    bool erase(const char *id) {
        auto it=std::find_if(records.begin(),records.end(),[&](auto& r){return !std::strcmp(id,r.video.bvid);});
        if(it==records.end())return false;records.erase(it);return true;
    }
    void put(const Video& video,uint32_t position=0) {
        if(!video_id(video.bvid))return;erase(video.bvid);records.insert(records.begin(),Record{video,position});
        if(records.size()>capacity)records.resize(capacity);
    }
    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> bytes(8+records.size()*record_bytes);std::memcpy(bytes.data(),"BLY2",4);bytes[4]=uint8_t(records.size());
        auto put32=[](uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));};
        auto text=[](uint8_t *p,const char *s,size_t n){size_t count=std::min(std::strlen(s),n-1);while(count && (uint8_t(s[count])&0xc0)==0x80)--count;std::memcpy(p,s,count);};
        for(unsigned i=0;i<records.size();++i){auto *p=bytes.data()+8+i*record_bytes;auto& r=records[i];
            std::memcpy(p,r.video.bvid,13);put32(p+13,r.video.duration);put32(p+17,r.position);
            text(p+21,r.video.title,96);text(p+117,r.video.author,48);text(p+165,r.video.pic,128);
            for(unsigned j=0;j<8;++j)p[293+j]=uint8_t(r.video.cid>>(8*j));}
        return bytes;
    }
    bool decode(const uint8_t *data,size_t size) {
        if(!data||size<8||data[4]>capacity||data[5]||data[6]||data[7])return false;
        bool v2=!std::memcmp(data,"BLY2",4);if(!v2&&std::memcmp(data,"BLY1",4))return false;
        size_t stride=v2?301:293;if(size!=8+data[4]*stride)return false;
        auto get32=[](const uint8_t *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);};
        std::vector<Record> next;
        for(unsigned i=0;i<data[4];++i){auto *p=data+8+i*stride;
            if(p[12]||!video_id({reinterpret_cast<const char*>(p),12})||!std::memchr(p+21,0,96)||!std::memchr(p+117,0,48)||!std::memchr(p+165,0,128))return false;
            Record r{};std::memcpy(r.video.bvid,p,13);r.video.duration=get32(p+13);r.position=get32(p+17);if(v2)for(unsigned j=0;j<8;++j)r.video.cid|=uint64_t(p[293+j])<<(8*j);
            std::memcpy(r.video.title,p+21,96);std::memcpy(r.video.author,p+117,48);std::memcpy(r.video.pic,p+165,128);
            if(!*r.video.title || std::any_of(next.begin(),next.end(),[&](auto& q){return !std::strcmp(q.video.bvid,r.video.bvid);}))return false;
            next.push_back(r);}
        records=std::move(next);return true;
    }
};
}
