// SPDX-License-Identifier: MIT
#pragma once
#include "catalog.hpp"
#include "vendor/qrcodegen/qrcodegen.h"
#include <ctime>
#include <cctype>
namespace bili {
inline std::string url_encode(std::string_view value) {
    static constexpr char hex[]="0123456789ABCDEF";std::string out;
    for(unsigned char c:value){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=char(c);
        else{out+='%';out+=hex[c>>4];out+=hex[c&15];}}return out;
}
inline std::string url_decode(std::string_view value) {
    std::string out;auto hex=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
    for(size_t i=0;i<value.size();++i){char c=value[i];if(c=='+')c=' ';else if(c=='%'){if(i+2>=value.size()||hex(value[i+1])<0||hex(value[i+2])<0)return {};c=char(hex(value[i+1])*16+hex(value[i+2]));i+=2;}if(!c||c=='\r'||c=='\n')return {};out+=c;}return out;
}
inline std::string md5(std::string_view input) {
    static constexpr uint32_t k[]={0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
    static constexpr unsigned shifts[]={7,12,17,22,5,9,14,20,4,11,16,23,6,10,15,21};
    std::vector<uint8_t> bytes(input.begin(),input.end());uint64_t bits=uint64_t(bytes.size())*8;bytes.push_back(128);
    while(bytes.size()%64!=56)bytes.push_back(0);for(unsigned i=0;i<8;++i)bytes.push_back(bits>>(8*i));
    uint32_t h[]={0x67452301,0xefcdab89,0x98badcfe,0x10325476};
    for(size_t p=0;p<bytes.size();p+=64){uint32_t words[16];for(unsigned i=0;i<16;++i){words[i]=0;for(unsigned j=0;j<4;++j)words[i]|=uint32_t(bytes[p+4*i+j])<<(8*j);}
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3];for(unsigned i=0;i<64;++i){uint32_t f;unsigned g;
            if(i<16){f=(b&c)|(~b&d);g=i;}else if(i<32){f=(d&b)|(~d&c);g=(5*i+1)%16;}
            else if(i<48){f=b^c^d;g=(3*i+5)%16;}else{f=c^(b|~d);g=(7*i)%16;}
            uint32_t v=a+f+k[i]+words[g];unsigned s=shifts[(i/16)*4+i%4];a=d;d=c;c=b;b+=(v<<s)|(v>>(32-s));}
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;}
    char out[33];for(unsigned i=0;i<16;++i)std::snprintf(out+i*2,3,"%02x",unsigned(uint8_t(h[i/4]>>(8*(i%4)))));return out;
}
inline std::string mixin_key(std::string_view img,std::string_view sub) {
    auto stem=[](std::string_view u){u.remove_prefix(u.find_last_of('/')==u.npos?0:u.find_last_of('/')+1);return std::string(u.substr(0,u.find('.')));};
    std::string key=stem(img)+stem(sub),out;static constexpr unsigned order[]={46,47,18,2,53,8,23,32,15,50,10,31,58,3,45,35,27,43,5,49,33,9,42,19,29,28,14,39,12,38,41,13};
    if(key.size()!=64)return {};for(auto i:order)out+=key[i];return out;
}
inline std::string wbi_query(std::vector<std::pair<std::string,std::string>> params,const std::string& key,uint64_t epoch) {
    if(key.size()!=32||epoch<1600000000)return {};
    params.emplace_back("wts",std::to_string(epoch));std::sort(params.begin(),params.end());std::string q;
    for(auto& [name,value]:params){value.erase(std::remove_if(value.begin(),value.end(),[](char c){return std::strchr("!'()*",c)!=nullptr;}),value.end());
        if(!q.empty())q+='&';q+=url_encode(name)+"="+url_encode(value);}
    return q+"&w_rid="+md5(q+key);
}
struct Account {
    char sess[512]{},csrf[65]{},uid[33]{},buvid[129]{},name[96]{};
    bool logged_in() const{return *sess&&*csrf&&*uid;}
    static bool token(std::string_view s,size_t max){if(s.empty()||s.size()>=max)return false;for(unsigned char c:s)if(c<=32||c>=127||c==';'||c=='"'||c=='\\')return false;return true;}
    bool accept_url(std::string_view url) {
        auto at=url.find('?');if(at==url.npos)return false;auto slash=url.find('/',8);auto origin=url.substr(0,slash);
        if(origin!="https://passport.biligame.com"&&origin!="https://passport.bilibili.com"&&origin!="https://account.bilibili.com")return false;
        Account next;std::snprintf(next.buvid,sizeof(next.buvid),"%s",buvid);url.remove_prefix(at+1);
        while(!url.empty()){auto end=url.find('&');auto field=url.substr(0,end);auto eq=field.find('=');if(eq!=field.npos){auto k=field.substr(0,eq);auto v=url_decode(field.substr(eq+1));
            if(k=="SESSDATA"&&token(v,sizeof(next.sess)))std::snprintf(next.sess,sizeof(next.sess),"%s",v.c_str());
            if(k=="bili_jct"&&token(v,sizeof(next.csrf)))std::snprintf(next.csrf,sizeof(next.csrf),"%s",v.c_str());
            if(k=="DedeUserID"&&token(v,sizeof(next.uid))&&std::all_of(v.begin(),v.end(),[](char c){return c>='0'&&c<='9';}))std::snprintf(next.uid,sizeof(next.uid),"%s",v.c_str());}
            if(end==url.npos)break;url.remove_prefix(end+1);}
        if(!next.logged_in())return false;*this=next;return true;
    }
    bool accept_cookie_headers(std::string_view cookies) {
        std::string query="https://passport.biligame.com/crossDomain?";
        while(!cookies.empty()){auto end=cookies.find(';');auto field=cookies.substr(0,end);while(!field.empty()&&field.front()==' ')field.remove_prefix(1);
            auto equal=field.find('=');if(equal!=field.npos){auto key=field.substr(0,equal);if(key=="SESSDATA"||key=="bili_jct"||key=="DedeUserID")query+=std::string(key)+"="+url_encode(field.substr(equal+1))+"&";}
            if(end==cookies.npos)break;cookies.remove_prefix(end+1);}
        return accept_url(query);
    }
    std::string cookie() const {
        std::string out;if(logged_in())out="SESSDATA="+std::string(sess)+"; bili_jct="+csrf+"; DedeUserID="+uid;
        if(*buvid){if(!out.empty())out+="; ";out+="buvid3="+std::string(buvid);}return out;
    }
    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> out(4+sizeof(sess)+sizeof(csrf)+sizeof(uid));std::memcpy(out.data(),"BSA1",4);size_t at=4;
        for(auto field:{std::string_view(sess),std::string_view(csrf),std::string_view(uid)}){std::memcpy(out.data()+at,field.data(),field.size());at+=field.data()==sess?sizeof(sess):field.data()==csrf?sizeof(csrf):sizeof(uid);}return out;
    }
    bool decode(const uint8_t *p,size_t n) {
        if(n!=4+sizeof(sess)+sizeof(csrf)+sizeof(uid)||std::memcmp(p,"BSA1",4))return false;Account next;
        const size_t sizes[]={sizeof(sess),sizeof(csrf),sizeof(uid)};char *fields[]={next.sess,next.csrf,next.uid};size_t at=4;
        for(unsigned i=0;i<3;++i){if(!std::memchr(p+at,0,sizes[i]))return false;std::memcpy(fields[i],p+at,sizes[i]);at+=sizes[i];}
        if(*next.sess||*next.csrf||*next.uid){if(!next.logged_in()||!token(next.sess,sizeof(sess))||!token(next.csrf,sizeof(csrf))||!token(next.uid,sizeof(uid)))return false;
            for(char c:std::string_view(next.uid))if(c<'0'||c>'9')return false;}
        std::snprintf(next.buvid,sizeof(next.buvid),"%s",buvid);*this=next;return true;
    }
};
struct LoginCode {
    uint8_t pixels[qrcodegen_BUFFER_LEN_FOR_VERSION(8)]{};char key[33]{};uint64_t expires=0,next_poll=0;bool scanned=false,valid=false;
    bool generate(media::json::Value data,uint64_t now) {
        char url[320]{};bool cut=false;uint8_t temp[sizeof(pixels)];
        valid=false;scanned=false;
        if(!data.get("url").string(url,sizeof(url),&cut)||cut||!(std::string_view(url).starts_with("https://passport.bilibili.com/")||std::string_view(url).starts_with("https://account.bilibili.com/"))||
            !data.get("qrcode_key").string(key,sizeof(key),&cut)||cut||std::strlen(key)!=32)return false;
        for(char c:std::string_view(key))if(!std::isxdigit(static_cast<unsigned char>(c)))return false;
        valid=qrcodegen_encodeText(url,temp,pixels,qrcodegen_Ecc_LOW,1,8,qrcodegen_Mask_AUTO,true);expires=now+180000;next_poll=now+2500;return valid;
    }
};
}
