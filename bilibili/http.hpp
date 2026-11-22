// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_http.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <string>
#include <vector>
namespace bili {
struct Http {
    uint64_t token=0,handle=0,deadline=0;
    uint8_t packet[4096]{},headers[4096]{};
    std::vector<uint8_t> data;
    std::string response_cookies;bool capture_cookies=false;
    size_t used=0,limit=0;
    uint64_t start=0;
    unsigned status=0;
    bool active=false,done=false,failed=false,have_headers=false,ranged=false;
    void cancel() {
        if (token) pxa_cancel(token); if (handle) pxa_close_handle(handle);
        token=handle=0; active=done=failed=false;
    }
    bool begin(uint64_t id,const char *url,uint64_t permission,uint64_t now,
        size_t bytes=65536,bool range=false,uint64_t offset=0,const char *cookie=nullptr,const char *referer=nullptr) {
        if (!url || !*url || std::strlen(url)>2048 || bytes>4*1024*1024 || !bytes || bytes-1>UINT64_MAX-offset) return false;
        cancel(); data.resize(bytes); used=0; limit=bytes; start=offset; ranged=range;
        have_headers=false; status=0;response_cookies.clear();
        capture_cookies=std::string_view(url).starts_with("https://passport.bilibili.com/x/passport-login/web/qrcode/poll?");
        char range_value[80]; std::snprintf(range_value,sizeof(range_value),"bytes=%llu-%llu",
            (unsigned long long)offset,(unsigned long long)(offset+bytes-1));
        auto field=[](const char *k,const char *v){return pxa_net_header_t{k,uint16_t(std::strlen(k)),
            reinterpret_cast<const uint8_t*>(v),uint16_t(std::strlen(v))};};
        // Account cookies belong only on the API origin, never media/cover URLs.
        bool authenticated=cookie && *cookie && std::string_view(url).starts_with("https://api.bilibili.com/");
        if(authenticated && (std::strlen(cookie)>1536 || std::strpbrk(cookie,"\r\n")))return false;
        pxa_net_header_t fields[]={field("User-Agent","Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/130.0.0.0 Safari/537.36"),
            field("Referer",referer?referer:"https://www.bilibili.com/"),field("Accept","*/*"),field("Range",range_value),field("Cookie",authenticated?cookie:""),field("Origin","https://space.bilibili.com")};
        const char *wanted[]={"Content-Range","Set-Cookie"}; const uint16_t wanted_sizes[]={13,10};
        auto r=pxa_http_request_defaults(); r.url=url; r.url_size=std::strlen(url); r.permission_handle=permission;
        if(authenticated && !range)fields[3]=fields[4];
        unsigned count=(range?4:3)+(authenticated?1:0);if(referer)fields[count++]=fields[5];
        r.headers=fields; r.header_count=count; r.wanted_headers=wanted; r.wanted_header_sizes=wanted_sizes;
        r.wanted_header_count=capture_cookies?2:1; r.buffer_bytes=32768; r.max_body_bytes=bytes; r.header_bytes=2048; r.timeout_ms=60000;
        token=id; deadline=now+65000;
        if (pxa_http_open(token,&r,packet,sizeof(packet))) { token=0; failed=done=true; return false; }
        active=true; return true;
    }
    bool event(const pxa_event_t& e) {
        if (e.service!=PXA_NET_SERVICE || e.opcode!=PXA_HTTP_OPEN) return false;
        pxa_http_open_result_t r{}; if (!pxa_http_parse_open(&e,e.token,&r)) return false;
        if (e.token!=token) { if (r.handle) pxa_close_handle(r.handle); return true; }
        token=0; if (r.status) fail(); else handle=r.handle; return true;
    }
    void fail() { cancel(); failed=done=true; }
    void finish() { if (handle) pxa_close_handle(handle); handle=0; active=false; done=true; }
    void pump(uint64_t now) {
        if (!active) return;
        if (now>deadline) { fail(); return; } if (!handle) return;
        if (!have_headers) {
            int n=pxa_http_read_response(handle,headers,sizeof(headers)); if (n==PXA_STATUS_WOULD_BLOCK) return;
            pxa_http_response_t r{}; if (n<0 || !pxa_http_parse_response(headers,n,&r)) { fail(); return; }
            status=r.status_code;
            if ((!ranged && status!=200) || (ranged && status!=206)) { fail(); return; }
            if (r.body_length_known && r.body_length>limit) { fail(); return; }
            if (ranged) {
                bool valid=false; size_t at=0; pxa_net_header_t h{};
                while (pxa_http_header_next(&r,&at,&h)) {
                    if (!pxa_http_header_equal(h.name,h.name_size,"Content-Range",13)) continue;
                    const uint8_t *p=h.value,*end=p+h.value_size;
                    if (end-p<6 || std::memcmp(p,"bytes ",6)) break;
                    p+=6;
                    auto number=[&](uint64_t& value) {
                        value=0; const uint8_t *begin=p;
                        while (p<end && *p>='0' && *p<='9') {
                            unsigned digit=*p++-'0'; if (value>(UINT64_MAX-digit)/10) return false;
                            value=value*10+digit;
                        } return p!=begin;
                    };
                    uint64_t first=0,last=0,total=0;
                    if (!number(first) || p==end || *p++!='-' || !number(last) || p==end || *p++!='/' ||
                        !number(total) || p!=end || first!=start || last<first || last-first>=limit || total<=last ||
                        (r.body_length_known && r.body_length!=last-first+1)) break;
                    valid=true; break;
                }
                if (!valid) { fail(); return; }
            }
            if(capture_cookies){size_t at=0;pxa_net_header_t h{};while(pxa_http_header_next(&r,&at,&h)){
                if(!pxa_http_header_equal(h.name,h.name_size,"Set-Cookie",10))continue;
                std::string_view value(reinterpret_cast<const char*>(h.value),h.value_size);value=value.substr(0,value.find(';'));
                if(!(value.starts_with("SESSDATA=")||value.starts_with("bili_jct=")||value.starts_with("DedeUserID=")))continue;
                if(response_cookies.size()+value.size()+2>1024){fail();return;}response_cookies+=value;response_cookies+=';';}}
            have_headers=true;
        }
        for (unsigned work=0;work<8;++work) {
            if (ranged && used==limit) { finish(); return; }
            uint8_t overflow=0; size_t count=std::min<size_t>(16384,limit-used);
            int n=pxa_http_read(handle,count ? data.data()+used : &overflow,count ? count : 1);
            if (n==PXA_STATUS_WOULD_BLOCK) return;
            if (n<0 || (n>0 && !count)) { fail(); return; }
            if (!n) { finish(); return; } used+=size_t(n);
        }
    }
};
}
