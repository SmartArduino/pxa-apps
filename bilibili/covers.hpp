// SPDX-License-Identifier: MIT
#pragma once
#include "catalog.hpp"
#include "cover_policy.hpp"
#include "pxa_http.h"
#include "pxa_image_decode.h"
#include "pxa_fs.h"
#include "pxa_log.h"
namespace bili {
// Private direct-mapped disk cache: 32 slots, <= 4 MiB. The committed header
// binds each slot to its URL; interrupted writes never become cache hits.
class Covers {
    struct Entry {char url[320]{};uint64_t image=0,touched=0,retry=0;bool wanted=false,tried_disk=false;};
    std::array<Entry,16> entries{};
    enum class Phase {idle,cache_open,cache_header,headers,write_open,write_header,copy,seek_commit,commit,decode};
    Phase phase=Phase::idle;
    uint64_t sequence=0xb1200000,open_token=0,decode_token=0,file_token=0,source=0,file=0,deadline=0,key=0;
    uint64_t now=0,used_clock=0;CoverPolicy policy;
    int active=-1;bool from_disk=false,cache_disabled=false,fetched=false;
    uint8_t packet[2048]{},headers[2048]{},chunk[4096]{},stamp[16]{};
    size_t pending=0,offset=0,total=0;
    char path[48]{};
    static constexpr pxa_image_decode_options_t options{320,180,57600,131072,3};
    static uint64_t hash(const char *s){uint64_t h=14695981039346656037ull;while(*s)h=(h^uint8_t(*s++))*1099511628211ull;return h;}
    void cancel_active() {
        for(auto token:{open_token,decode_token,file_token})if(token)pxa_cancel(token);
        if(source)pxa_close_handle(source);if(file)pxa_close_handle(file);
        open_token=decode_token=file_token=source=file=0;active=-1;phase=Phase::idle;pending=offset=total=0;fetched=false;
    }
    void fail(bool network=true) {
        char line[96];std::snprintf(line,sizeof(line),"BILLY_COVER failed phase=%u network=%u",unsigned(phase),unsigned(network));pxa_log_write(PXA_LOG_LEVEL_INFO,line);
        uint64_t retry=policy.fail(now,network);
        if(active>=0)entries[active].retry=retry;
        cancel_active();
    }
    void decode(bool cached) {
        from_disk=cached;phase=Phase::decode;decode_token=++sequence;
        uint64_t input=cached?file:source;
        if(pxa_image_decode(decode_token,input,cached?PXA_IMAGE_SOURCE_FILE:PXA_IMAGE_SOURCE_STREAM,options,nullptr)){decode_token=0;fail(!cached);}
    }
    void try_disk() {
        file_token=++sequence;phase=Phase::cache_open;
        if(pxa_fs_request_open(file_token,path,std::strlen(path),PXA_FS_OPEN_READ)){file_token=0;phase=Phase::idle;}
    }
    void mark_header(uint32_t bytes) {std::memcpy(stamp,"BIC1",4);pxa_store_u64(stamp+4,key);pxa_store_u32(stamp+12,bytes);}
    void network(uint64_t permission) {
        if(!policy.admit(now))return;
        pxa_net_header_t h[]={{"Referer",7,reinterpret_cast<const uint8_t*>("https://www.bilibili.com/"),25},
            {"User-Agent",10,reinterpret_cast<const uint8_t*>("Mozilla/5.0"),11}};
        const char *wanted[]={"Content-Type","Retry-After"};const uint16_t sizes[]={12,11};
        auto r=pxa_http_request_defaults();r.url=entries[active].url;r.url_size=std::strlen(r.url);r.permission_handle=permission;
        r.headers=h;r.header_count=2;r.wanted_headers=wanted;r.wanted_header_sizes=sizes;r.wanted_header_count=2;
        r.buffer_bytes=4096;r.header_bytes=2048;r.max_body_bytes=options.max_encoded_bytes;r.timeout_ms=15000;
        fetched=true;open_token=++sequence;phase=Phase::headers;deadline=now+25000;
        pxa_log_write(PXA_LOG_LEVEL_DEBUG,"BILLY_COVER network fetch");
        if(pxa_http_open(open_token,&r,packet,sizeof(packet))){open_token=0;fail();}
    }
public:
    bool changed=false;
    void begin(){for(auto& e:entries)e.wanted=false;}
    uint64_t want(const char *picture) {
        char url[320]{};if(!picture_url(picture,url,sizeof(url)))return 0;
        for(auto& e:entries)if(!std::strcmp(e.url,url)){e.wanted=true;e.touched=++used_clock;return e.image;}
        int chosen=-1;
        for(unsigned i=0;i<entries.size();++i){auto& e=entries[i];if(!*e.url){chosen=i;break;}
            if(!e.wanted&&int(i)!=active&&(chosen<0||e.touched<entries[chosen].touched))chosen=i;}
        if(chosen>=0){auto& e=entries[chosen];if(e.image)pxa_close_handle(e.image);e={};std::memcpy(e.url,url,std::strlen(url)+1);e.wanted=true;e.touched=++used_clock;}
        return 0;
    }
    void prune(){if(active>=0&&!entries[active].wanted)cancel_active();}
    void pause(){cancel_active();}
    void stop(){cancel_active();for(auto& e:entries){if(e.image)pxa_close_handle(e.image);e={};}changed=true;}
    bool event(const pxa_event_t& event) {
        const bool own=event.token>0xb1200000&&event.token<=sequence;
        if(event.service==PXA_FS_SERVICE&&event.opcode==PXA_FS_OPEN&&own){
            pxa_fs_open_result_t r{};bool valid=pxa_fs_parse_open(&event,event.token,&r);
            if(event.token!=file_token){if(r.handle)pxa_close_handle(r.handle);return true;}
            file_token=0;
            if(!valid||r.status){if(r.handle)pxa_close_handle(r.handle);
                if(phase==Phase::write_open){cache_disabled=true;decode(false);}else phase=Phase::idle;}
            else{file=r.handle;phase=phase==Phase::write_open?Phase::write_header:Phase::cache_header;}return true;
        }
        if(event.service==PXA_FS_SERVICE&&event.opcode==PXA_FS_SEEK&&event.token==file_token&&file_token){
            pxa_fs_seek_result_t r{};bool ok=pxa_fs_parse_seek(&event,file_token,&r);file_token=0;
            if(!ok||r.status){fail(false);return true;}mark_header(total);phase=Phase::commit;return true;
        }
        if(open_token&&event.token==open_token&&event.service==PXA_NET_SERVICE&&event.opcode==PXA_HTTP_OPEN){
            pxa_http_open_result_t r{};bool ok=pxa_http_parse_open(&event,open_token,&r);open_token=0;
            if(!ok||r.status){if(r.handle)pxa_close_handle(r.handle);fail();}else source=r.handle;return true;
        }
        if(event.service==PXA_ASSETS_SERVICE&&event.opcode==PXA_ASSETS_DECODE_IMAGE&&own){
            pxa_image_decode_result_t r{};bool ok=pxa_image_decode_parse(&event,event.token,PXA_ASSETS_DECODE_IMAGE,0,options,&r);
            if(event.token!=decode_token){if(r.handle)pxa_close_handle(r.handle);return true;}decode_token=0;
            if(!ok||r.status){if(r.handle)pxa_close_handle(r.handle);
                if(from_disk&&!fetched){if(file)pxa_close_handle(file);file=0;phase=Phase::idle;deadline=now+25000;}else fail();}
            else{entries[active].image=r.handle;policy.failures=0;cancel_active();changed=true;}return true;
        }
        return false;
    }
    void pump(uint64_t time,uint64_t permission,bool enabled) {
        now=time;if(!enabled){cancel_active();return;}
        if(active>=0&&phase!=Phase::idle&&now>deadline){fail();return;}
        if(active<0){
            if(!permission)return;
            for(unsigned i=0;i<entries.size();++i){auto& e=entries[i];if(!e.wanted||e.image||e.retry>now||!*e.url)continue;
                active=i;key=hash(e.url);std::snprintf(path,sizeof(path),"billy-cover-%02u.bin",unsigned(key%32));deadline=now+25000;
                if(!cache_disabled&&!e.tried_disk){e.tried_disk=true;try_disk();}break;}
            if(active<0)return;
        }
        if(phase==Phase::idle){network(permission);return;}
        if(phase==Phase::cache_header){
            int n=pxa_io(file,PXA_FS_IO_READ,stamp,sizeof(stamp));if(n==PXA_STATUS_WOULD_BLOCK)return;
            bool valid=n==16&&!std::memcmp(stamp,"BIC1",4)&&pxa_load_u64(stamp+4)==key&&pxa_load_u32(stamp+12)>0&&pxa_load_u32(stamp+12)<=options.max_encoded_bytes;
            if(valid){if(!fetched)pxa_log_write(PXA_LOG_LEVEL_DEBUG,"BILLY_COVER disk hit");decode(true);return;}pxa_close_handle(file);file=0;phase=Phase::idle;return;
        }
        if(phase==Phase::headers&&source){
            int n=pxa_http_read_response(source,headers,sizeof(headers));if(n==PXA_STATUS_WOULD_BLOCK)return;
            pxa_http_response_t r{};if(n<0||!pxa_http_parse_response(headers,n,&r)){fail();return;}
            if(r.status_code!=200){uint64_t retry=r.status_code==429||r.status_code==403?60000:15000;
                size_t at=0;pxa_net_header_t h{};while(pxa_http_header_next(&r,&at,&h))if(pxa_http_header_equal(h.name,h.name_size,"Retry-After",11)){
                    retry=CoverPolicy::retry_after({reinterpret_cast<const char*>(h.value),h.value_size},retry);}

                policy.defer(now+retry);fail();return;}
            if(r.body_length_known&&r.body_length>options.max_encoded_bytes){fail();return;}
            if(cache_disabled){decode(false);return;}
            file_token=++sequence;phase=Phase::write_open;mark_header(0);
            if(pxa_fs_request_open(file_token,path,std::strlen(path),PXA_FS_OPEN_WRITE|PXA_FS_OPEN_CREATE|PXA_FS_OPEN_TRUNCATE)){file_token=0;cache_disabled=true;decode(false);}return;
        }
        if(phase==Phase::write_header||phase==Phase::commit){
            int n=pxa_io(file,PXA_FS_IO_WRITE,stamp,sizeof(stamp));if(n==PXA_STATUS_WOULD_BLOCK)return;
            if(n!=16){cache_disabled=true;fail(false);return;}
            if(phase==Phase::commit){pxa_close_handle(file);file=0;try_disk();}else phase=Phase::copy;return;
        }
        if(phase==Phase::copy){
            if(pending){int n=pxa_io(file,PXA_FS_IO_WRITE,chunk+offset,pending-offset);if(n==PXA_STATUS_WOULD_BLOCK)return;
                if(n<=0||size_t(n)>pending-offset){cache_disabled=true;fail(false);return;}offset+=n;if(offset==pending)pending=offset=0;return;}
            int n=pxa_http_read(source,chunk,sizeof(chunk));if(n==PXA_STATUS_WOULD_BLOCK)return;
            if(n<0||total+std::max(n,0)>options.max_encoded_bytes){fail();return;}
            if(n){total+=n;pending=n;return;}
            pxa_close_handle(source);source=0;if(!total){fail();return;}
            file_token=++sequence;phase=Phase::seek_commit;
            if(pxa_fs_request_seek(file_token,file,0,PXA_FS_SEEK_START)){file_token=0;fail(false);}return;
        }
    }
};
}
