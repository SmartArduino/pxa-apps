// SPDX-License-Identifier: MIT
#include "../http.hpp"
#include <cassert>
#include <string>
std::string request;
extern "C" int32_t pxa_call(uint32_t route,uint64_t,const uint8_t *data,uint32_t size){if((route&65535)==PXA_NET_SERVICE)request.assign(reinterpret_cast<const char*>(data),size);return 0;}
extern "C" int32_t pxa_io(uint64_t,uint32_t,uint8_t*,uint32_t){return -1;}
int main(){bili::Http http;uint64_t permission=uint64_t(1)<<32;
    assert(http.begin(1,"https://api.bilibili.com/x/web-interface/nav",permission,0,4096,false,0,"SESSDATA=fake"));assert(request.find("SESSDATA=fake")!=request.npos);http.cancel();
    for(auto url:{"https://i0.hdslb.com/a.jpg","https://video.example/a.mp4","https://passport.bilibili.com/poll","http://api.bilibili.com/x","https://api.bilibili.com.evil.test/x","https://api.bilibili.com@evil.test/x"}){
        assert(http.begin(2,url,permission,0,4096,false,0,"SESSDATA=fake"));assert(request.find("SESSDATA=fake")==request.npos);http.cancel();}
    assert(!http.begin(3,"https://api.bilibili.com/x",permission,0,4096,false,0,"SESSDATA=fake\r\nX: bad"));
    assert(http.begin(4,"https://api.bilibili.com/x",permission,0,4096,true,100,"SESSDATA=fake","https://space.bilibili.com/1"));assert(request.find("SESSDATA=fake")!=request.npos&&request.find("bytes=100-4195")!=request.npos&&request.find("Origin")!=request.npos);
}
