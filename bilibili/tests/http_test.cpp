// SPDX-License-Identifier: MIT
#include "../http.hpp"
#include <cassert>
#include <string>
std::string request;
int32_t submit_status=0;
extern "C" int32_t pxa_call(uint32_t route,uint64_t,const uint8_t *data,uint32_t size){
    if ((route&65535)!=PXA_NET_SERVICE) return 0;
    request.assign(reinterpret_cast<const char*>(data),size);
    return submit_status;
}
std::string reply_range,reply_body;
bool would_block=false;
bool ordinary_response=false;
uint64_t content_length=UINT64_MAX;
extern "C" int32_t pxa_io(uint64_t,uint32_t op,uint8_t *out,uint32_t size){
    if (ordinary_response) {
        if (op==PXA_HTTP_IO_RESPONSE) {
            assert(size>=16);
            pxa_store_u16(out,200); pxa_store_u16(out+2,content_length!=UINT64_MAX);
            pxa_store_u32(out+4,0); pxa_store_u64(out+8,content_length);
            return 16;
        }
        if (op==1) {
            const auto count=std::min<size_t>(size,reply_body.size());
            std::memcpy(out,reply_body.data(),count);
            reply_body.erase(0,count);
            return int32_t(count);
        }
    }
    if (op==PXA_HTTP_IO_RESPONSE && !reply_range.empty()) {
        const unsigned headers=4+13+reply_range.size();
        assert(size>=16+headers);
        pxa_store_u16(out,206);pxa_store_u16(out+2,1);
        pxa_store_u32(out+4,headers);pxa_store_u64(out+8,reply_body.size());
        pxa_store_u16(out+16,13);pxa_store_u16(out+18,reply_range.size());
        std::memcpy(out+20,"Content-Range",13);
        std::memcpy(out+33,reply_range.data(),reply_range.size());
        return 16+headers;
    }
    if (op==1 && !reply_body.empty()) {
        assert(size>=reply_body.size());
        int n=reply_body.size();std::memcpy(out,reply_body.data(),n);reply_body.clear();return n;
    }
    return would_block ? PXA_STATUS_WOULD_BLOCK : -1;
}
int main(){bili::Http http;uint64_t permission=uint64_t(1)<<32;
    assert(http.begin(9,"https://video.example/track",permission,100,4096,true,0,nullptr,nullptr,12000));
    uint8_t field[12]{}, timeout[4]{};
    size_t field_size=0;
    pxa_store_u32(timeout,12000);
    assert(pxa_net_append(field,sizeof(field),&field_size,8,timeout,sizeof(timeout)));
    assert(request.find(reinterpret_cast<const char*>(field),0,field_size)!=request.npos);
    assert(http.deadline==17100);
    http.cancel();
    assert(http.begin(1,"https://api.bilibili.com/x/web-interface/nav",permission,0,4096,false,0,"SESSDATA=fake"));assert(request.find("SESSDATA=fake")!=request.npos);http.cancel();
    for(auto url:{"https://i0.hdslb.com/a.jpg","https://video.example/a.mp4","https://passport.bilibili.com/poll","http://api.bilibili.com/x","https://api.bilibili.com.evil.test/x","https://api.bilibili.com@evil.test/x"}){
        assert(http.begin(2,url,permission,0,4096,false,0,"SESSDATA=fake"));assert(request.find("SESSDATA=fake")==request.npos);http.cancel();}
    assert(!http.begin(3,"https://api.bilibili.com/x",permission,0,4096,false,0,"SESSDATA=fake\r\nX: bad"));
    assert(http.begin(4,"https://api.bilibili.com/x",permission,0,4096,true,100,"SESSDATA=fake","https://space.bilibili.com/1"));assert(request.find("SESSDATA=fake")!=request.npos&&request.find("bytes=100-4195")!=request.npos&&request.find("Origin")!=request.npos);
    assert(http.begin(5,"https://video.example/a.mp4",permission,0,657245,true,500000));
    assert(http.data() && http.used==0 && http.limit==657245 && http.start==500000);
    assert(http.body_capacity()==657245);
    http.cancel();
    http.used=http.limit;
    http.finish();
    std::memset(const_cast<uint8_t*>(http.data()),0x19,http.used);
    const_cast<uint8_t*>(http.data())[128*1024]=0xa7;
    const auto* previous_body=http.data();
    bili::Http playback;
    playback.accept_body(http);
    assert(playback.done && playback.data()==previous_body && playback.used==657245 && playback.start==500000);
    assert(playback.body_capacity()==657245);
    assert(!http.data() && !http.used && !http.done && !http.body_capacity());
    playback.discard_cached_prefix(128*1024);
    assert(playback.start==500000 && playback.used==657245);
    playback.done=false;
    playback.discard_cached_prefix(128*1024);
    assert(playback.start==631072 && playback.used==526173 && playback.limit==526173);
    assert(playback.data()[0]==0xa7 && playback.data()[1]==0x19);
    playback.discard_cached_prefix(playback.used);
    assert(playback.start==631072 && playback.used==526173);
    assert(http.begin(6,"https://video.example/a.mp4",permission,100,128*1024,true,800000));
    http.used=40000;
    http.handle=uint64_t(7)<<32;
    http.token=0;
    http.have_headers=true;
    const auto* live_body=http.data();
    playback.accept_body(http);
    assert(playback.active && !playback.done && playback.have_headers);
    assert(playback.data()==live_body && playback.used==40000 && playback.start==800000);
    assert(playback.handle==(uint64_t(7)<<32) && playback.deadline==65100);
    assert(!http.active && !http.handle && !http.token && !http.data());
    playback.cancel();
    assert(http.begin(7,"https://video.example/track",permission,100,8,true,0));
    http.deadline=150;http.handle=uint64_t(1)<<32;http.token=0;
    http.have_headers=true;would_block=true;reply_body="abc";
    http.pump_with_idle(140,50);
    assert(http.active && http.used==3 && http.deadline==190);
    playback.accept_body(http);
    playback.pump_with_idle(189,50);
    assert(playback.active && playback.deadline==190 && playback.used==3);
    playback.pump_with_idle(191,50);
    assert(!playback.active && playback.failed && playback.used==3);
    would_block=false;
    for (size_t received:{2u,4u,5u}) {
        assert(http.begin(7,"https://video.example/track",permission,100,8,true,900));
        assert(!http.shorten_received_range(4));
        http.used=received;http.fail();
        assert(!http.shorten_received_range(4));
        http.status=206;http.have_headers=true;http.range_total=1200;
        assert(http.shorten_received_range(4) && http.start==900 && http.limit==4);
        assert(http.failed==(received<4) && http.used==std::min<size_t>(received,4));
        if (http.failed) {
            assert(http.resume_range(8,"https://mirror.example/track",permission,200,1000));
            assert(request.find("bytes=902-903")!=request.npos);
        }
    }
    // Losing Wi-Fi after a partial response can be followed by several DNS
    // errors before the new association gets an IP. Retain the exact prefix.
    assert(http.begin(7,"https://video.example/track",permission,100,6,true,100));
    std::memcpy(const_cast<uint8_t*>(http.data()),"abc",3);
    http.used=3;http.status=206;http.have_headers=true;http.range_total=200;http.fail();
    auto *prefix=http.data();
    unsigned now=200;
    for (int status:{PXA_STATUS_BUSY,PXA_STATUS_WOULD_BLOCK,PXA_STATUS_RESOURCE_LIMIT,PXA_STATUS_DENIED}) {
        submit_status=status;
        assert(http.resume_range(8,"https://mirror.example/track",permission,now,1000)==(status!=PXA_STATUS_DENIED));
        assert(!http.active && http.failed && http.done && http.can_resume_range());
        assert(http.data()==prefix && !std::memcmp(prefix,"abc",3));
        assert(http.start==100 && http.used==3 && http.limit==6 && http.range_total==200);
        assert(request.find("bytes=103-105")!=request.npos);
    }
    submit_status=0;
    for (unsigned attempt=0;attempt<bili::Http::max_range_retries;++attempt) {
        if (attempt) {
            assert(http.defer_retry(now,attempt));
            now+=1500u<<(attempt-1);
            assert(http.defer_retry(now-1,attempt));
        }
        assert(!http.defer_retry(now,attempt));
        assert(http.resume_range(8,"https://mirror.example/track",permission,now,1000));
        assert(http.data()==prefix && http.used==3 && http.range_total==200);
        assert(request.find("bytes=103-105")!=request.npos);
        http.fail();
    }
    assert(!http.defer_retry(now,bili::Http::max_range_retries));
    http.status=403;
    assert(!http.can_resume_range() && !http.defer_retry(now,1));
    for (const char *range:{"bytes 103-105/200","bytes 100-102/200","bytes 103-105/201"}) {
        assert(http.begin(7,"https://video.example/track",permission,100,6,true,100));
        std::memcpy(const_cast<uint8_t*>(http.data()),"abc",3);
        http.used=3;http.status=206;http.have_headers=true;http.range_total=200;http.fail();
        const auto *prefix=http.data();
        assert(http.resume_range(8,"https://mirror.example/track",permission,200,1000));
        assert(request.find("bytes=103-105")!=request.npos && http.data()==prefix && http.used==3);
        reply_range=range;reply_body="def";http.handle=uint64_t(1)<<32;http.token=0;http.pump(202);
        if (reply_range=="bytes 103-105/200") {
            assert(http.done && !http.failed && http.used==6 && http.start==100);
            assert(!std::memcmp(http.data(),"abcdef",6));
        } else {
            assert(http.failed && http.used==3 && !std::memcmp(http.data(),"abc",3));
        }
    }
    ordinary_response=true;
    constexpr size_t maximum=512*1024;
    for (bool known:{false,true}) {
        for (size_t length:{0u,4096u,83818u,512u*1024,512u*1024+1}) {
            bili::Http next;
            assert(next.begin(12,"https://example.org/comments",permission,0,maximum));
            assert(next.body_capacity()==0);
            next.handle=uint64_t(1)<<32; next.token=0;
            reply_body=std::string(length,'d');
            content_length=known ? length : UINT64_MAX;
            while (next.active) next.pump(1);
            assert(next.done && next.failed==(length>maximum));
            if (!next.failed) {
                assert(next.used==length && (!known || next.body_capacity()==length));
                assert(!length || std::all_of(next.data(),next.data()+length,[](auto b){return b=='d';}));
            }
            assert(next.body_capacity()<=maximum);
        }
    }
}
