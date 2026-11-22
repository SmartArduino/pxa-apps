// SPDX-License-Identifier: MIT
#include "../common/media/canvas.hpp"
#include "../common/media/json.hpp"
#include "../common/media/pcm_packet.hpp"
#include "http.hpp"
#include "playback.hpp"
#include "client.hpp"
#include "pxa_video.h"
#include "pxa.h"
#include "pxa_surface.h"
#include "pxa_clock.h"
#include "pxa_permission.h"
#include "pxa_log.h"
#include <ctime>
namespace {
enum class Job { none,view,playurl,box,moov,prebuffer,media };
media::Canvas scene;
bili::Client client;
bili::Http http,audio_http;
bili::Track video,audio_track;
media::PcmPacket pcm_packet;
pxa_video_capability_t capability{};
uint8_t control[4096]{},info[32]{},pcm[32768]{};
int16_t pcm_source[8192]{};bili::PcmSpeed pcm_speed;unsigned speed_quarters=4;
std::vector<uint8_t> compressed,frame,scaled,mapped_storage;
uint8_t *mapped_pixels=nullptr;
int mapped_index=-1;
std::vector<uint32_t> scale_x;
uint64_t serial=100,net_permission=0,audio_permission=0,decoder=0,surface=0,audio=0;
uint64_t decoder_token=0,surface_token=0,audio_token=0,gain_token=0,query_token=0,next_query=0;
uint64_t audio_cache_offset=UINT64_MAX,cap_token=2,audio_pts_base=0,pause_pts=0;
uint64_t file_bytes=0;
uint64_t resume_pending=0,next_history=0,play_offset=0;
bool network_prompt=false,audio_prompt=false;
unsigned ui_retries=0;
uint64_t stats_at=0,decoded_us=0,decoded_frames=0,skipped_frames=0;
uint64_t scaled_us=0,display_us=0,aac_us=0,video_read_us=0,net_us=0;
uint64_t box_offset=0,box_bytes=0,cache_offset=0,play_started=0,pause_started=0,frame_id=0,video_pts=0;
size_t vi=0,ai=0,pcm_used=0,pcm_at=0;
unsigned qn=16,boxes_seen=0,surface_w=0,surface_h=0,surface_x=0,surface_y=0;
uint64_t duration_us=0,hud_until=0,hud_tick=0,drag_pts=0,discard_before_pts=0;
bool hud=true,overlay_dirty=true,dragging=false,drag_paused=false,consume_up=false,seeking=false,ended=false,video_eof=false,frame_scaled=false;
bool scaled_read=true,audio_drained=false,want_surface=false;
bool mapped_surface=false,mapped_filled=false;
bool buffering=false;
uint32_t audio_rate=0,audio_channels=0;
bool background=false,dirty=true,playing=false,paused=false,audio_ready=false,frame_pending=false,draining=false;
Job job=Job::none;
char draft[192]="BV1J4411v7g6",bvid[13]{},title[192]="billy",message[192]="输入 BV 号或视频链接，默认 360p";
char media_url[2049]{};
uint64_t now_us() { timespec t{}; clock_gettime(CLOCK_MONOTONIC,&t); return uint64_t(t.tv_sec)*1000000+t.tv_nsec/1000; }
uint64_t now_ms() { timespec t{}; clock_gettime(CLOCK_MONOTONIC,&t); return uint64_t(t.tv_sec)*1000+t.tv_nsec/1000000; }
void close(uint64_t& h) { if (h) pxa_close_handle(h); h=0; }
void cancel(uint64_t& t) { if (t) pxa_cancel(t); t=0; }
void status(const char *s) { std::snprintf(message,sizeof(message),"%s",s); dirty=true; if(!client.in_player)client.notice(s); pxa_log_write(PXA_LOG_LEVEL_INFO,s); }
void browser_window() {
    pxa_window_config_t window{};window.status_bar_icons=window.navigation_bar_icons=bili::ui::colors.dark?1:2;
    window.status_bar_color=window.navigation_bar_color=pxa_canvas_rgba(bili::ui::colors.background);pxa_window_configure(&window);
}
void stop() {
    if(client.in_player){if(playing||surface)client.record(video_pts,ended);client.leave_player();}
    pxa_window_keep_screen_on(0);
    browser_window(); pxa_window_set_rotation(0);
    http.cancel(); audio_http.cancel(); close(decoder); close(surface); close(audio);
    mapped_pixels=nullptr; mapped_index=-1; mapped_surface=mapped_filled=false;
    std::vector<uint8_t>().swap(mapped_storage);
    cancel(decoder_token); cancel(surface_token); cancel(audio_token); cancel(gain_token); cancel(query_token);
    video={}; audio_track={}; compressed={}; frame={}; scaled={}; scale_x={};
    std::vector<uint8_t>().swap(http.data); std::vector<uint8_t>().swap(audio_http.data);
    playing=paused=audio_ready=frame_pending=draining=buffering=false;file_bytes=0;
    hud=overlay_dirty=true; dragging=seeking=ended=video_eof=audio_drained=want_surface=false; duration_us=discard_before_pts=0; job=Job::none; vi=ai=pcm_used=pcm_at=0;
    pcm_packet.reset();pcm_speed.reset(); audio_cache_offset=UINT64_MAX; play_started=video_pts=pause_pts=pause_started=drag_pts=0;
    stats_at=decoded_us=decoded_frames=skipped_frames=frame_id=audio_pts_base=0;
    scaled_us=display_us=aac_us=video_read_us=net_us=0;
    resume_pending=next_history=play_offset=0;
}
void fail(const char *s) { stop(); status(s); }
bool request(const char *url,Job next,size_t bytes=65536,bool range=false,uint64_t offset=0) {
    job=next;
    auto cookies=client.cookie();
    if (!http.begin(++serial,url,net_permission,now_ms(),bytes,range,offset,cookies.c_str())) { fail("无法创建网络请求"); return false; }
    return true;
}
void permission(uint64_t token,const char *name,const char *scope) {
    uint32_t bytes=0; if (pxa_permission_build(control,sizeof(control),PXA_PERMISSION_ACQUIRE,token,
        name,std::strlen(name),reinterpret_cast<const uint8_t*>(scope),std::strlen(scope),&bytes)) pxa_control_packet(control,bytes);
}
void begin_video() {
    if (!capability.count || capability.codec!=PXA_VIDEO_H264) { status("当前 Host 不支持 H.264，请更新固件"); return; }
    const char *p=std::strstr(draft,"BV");
    if (!p || std::strlen(p)<12) { status("请输入完整的 BV 号"); return; }
    for (unsigned i=0;i<12;++i) if (!((p[i]>='0'&&p[i]<='9')||(p[i]>='A'&&p[i]<='Z')||(p[i]>='a'&&p[i]<='z'))) {
        status("BV 号格式有误"); return;
    }
    std::memcpy(bvid,p,12); bvid[12]=0;
    if (!net_permission) { client.want_network=true; return; }
    stop();client.begin_player();resume_pending=client.resume_us;qn=client.quality;
    if(!audio_permission && !audio_prompt){audio_prompt=true;permission(4,"audio.playback","media");}
    pxa_window_keep_screen_on(1);char url[256];
    if(client.selected.cid && !std::strcmp(client.selected.bvid,bvid)) {
        std::snprintf(title,sizeof(title),"%s",client.selected.title);
        std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/player/playurl?bvid=%s&cid=%llu&qn=%u&fnval=1&fnver=0&fourk=0&platform=html5",
            bvid,(unsigned long long)client.selected.cid,qn);
        status("正在获取播放地址…");request(url,Job::playurl);
    } else {
        std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/web-interface/view?bvid=%s",bvid);
        status("正在获取视频信息…");request(url,Job::view);
    }
}
bool json_ok(media::json::Value root) {
    if (root.get("code").number(999999)==0) return true;
    char text[150]{}; root.get("message").string(text,sizeof(text));
    char result[192]; std::snprintf(result,sizeof(result),"B站接口拒绝请求：%s",*text?text:"请稍后重试"); fail(result); return false;
}
void read_box() {
    if (++boxes_seen>64 || box_offset>UINT64_C(8)*1024*1024*1024) { fail("无法找到 MP4 索引"); return; }
    request(media_url,Job::box,32,true,box_offset);
}
void open_video() {
    pxa_window_fullscreen();pxa_window_keep_screen_on(1);
    pxa_window_set_rotation(video.width>=video.height ? 1 : 0);
    pxa_window_request_snapshot(3);
    decoder_token=++serial;
    if (pxa_video_request_open(decoder_token,capability.index,video.width,video.height)) { fail("无法打开解码器"); return; }
    status("正在准备播放…");
}
void prepare() {
    if (!bili::parse_moov({http.data.data(),http.used},video,audio_track)) { fail("不支持此 MP4 的编码或索引格式"); return; }
    if (video.width>capability.max_width || video.height>capability.max_height ||
        uint32_t(video.width)*video.height>capability.max_pixels) { fail("视频超过设备解码上限，请选择更低画质"); return; }
    if (!video.width || !video.height) { fail("视频尺寸无效"); return; }
    for(const auto *track:{&video,&audio_track})for(const auto& sample:track->samples)
        if(sample.offset>file_bytes || sample.size>file_bytes-sample.offset)file_bytes=0;
    // Finish bounded small-file HTTPS before decoder/display allocations.
    // This preserves internal DMA headroom for TLS and avoids startup retries.
    if(file_bytes && file_bytes<=3*1024*1024) {
        status("正在缓冲视频…");cache_offset=0;
        request(media_url,Job::prebuffer,size_t(file_bytes),true,0);return;
    }
    open_video();
}
void make_surface() {
    want_surface=true;
    if ((scene.viewport.display.width>=scene.viewport.display.height)!=(video.width>=video.height)) return;
    want_surface=false;
    auto fit=bili::fit(video.width,video.height,scene.viewport.display.width,scene.viewport.display.height);
    surface_w=fit.width;surface_h=fit.height;surface_x=fit.x;surface_y=fit.y;
    scale_x.resize(surface_w);
    for (unsigned x=0;x<surface_w;++x) scale_x[x]=(uint64_t(x)*video.width/surface_w)*2;
    // Configure before creating a mapped Surface so older Hosts can use copying.
    mapped_surface=pxa_video_configure_scaled(decoder,surface_w,surface_h)==0;
    pxa_surface_desc_t desc{uint16_t(surface_w),uint16_t(surface_h),PXA_SURFACE_RGB565,2,
        uint8_t(mapped_surface ? PXA_SURFACE_GUEST_MAPPED : 0)};
    uint32_t bytes=0; surface_token=++serial;
    if (!pxa_surface_build_create(control,sizeof(control),surface_token,desc,&bytes) || pxa_control_packet(control,bytes))
        fail("无法创建视频画面");
}
void complete_http() {
    if (http.failed) { char text[192]; std::snprintf(text,sizeof(text),"网络请求失败（HTTP %u），可重试或重新获取链接",http.status); fail(text); return; }
    auto root=media::json::Value{http.data.data(),http.used};
    if (job==Job::view) {
        if (!json_ok(root)) return;
        client.apply_view(root);
        auto data=root.get("data"); uint64_t cid=data.get("cid").number();
        if (!cid || !data.get("title").string(title,sizeof(title))) { fail("视频不存在或没有可播放分P"); return; }
        char url[256]; std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/player/playurl?bvid=%s&cid=%llu&qn=%u&fnval=1&fnver=0&fourk=0&platform=html5",
            bvid,(unsigned long long)cid,qn);
        status("正在获取播放地址…"); request(url,Job::playurl);
    } else if (job==Job::playurl) {
        if (!json_ok(root)) return;
        auto data=root.get("data"); media::json::Array urls(data.get("durl")); media::json::Value first;
        bool truncated=false;
        if (!urls.next(first) || !first.get("url").string(media_url,sizeof(media_url),&truncated) || truncated ||
            std::strncmp(media_url,"https://",8)) { fail("没有可播放的 HTTPS MP4 地址"); return; }
        if (data.get("video_codecid").number(7)!=7) { fail("此视频未提供设备支持的 AVC 编码"); return; }
        file_bytes=first.get("size").number();
        box_offset=0; boxes_seen=0; status("正在读取 MP4 索引…"); read_box();
    } else if (job==Job::box) {
        if (http.used<16) { fail("MP4 文件头不完整"); return; }
        box_bytes=bili::be32(http.data.data()); unsigned header=8;
        if (box_bytes==1) { box_bytes=bili::be64(http.data.data()+8); header=16; }
        if (box_bytes<header || box_bytes>UINT64_MAX-box_offset) { fail("MP4 文件头无效"); return; }
        if (bili::be32(http.data.data()+4)==bili::tag("moov")) {
            if (box_bytes-header>4*1024*1024) { fail("视频索引超过内存上限，请选择较短视频"); return; }
            request(media_url,Job::moov,size_t(box_bytes-header),true,box_offset+header);
        } else { box_offset+=box_bytes; read_box(); }
    } else if (job==Job::moov) { job=Job::none; prepare(); }
    else if (job==Job::prebuffer) {cache_offset=0;job=Job::none;open_video();}
    else if (job==Job::media) { cache_offset=http.start; job=Job::none;buffering=false;dirty=true; }
}
void show_hud(bool visible);
const uint8_t *cached(const bili::Sample& sample,bool audio_sample=false) {
    if(file_bytes && file_bytes<=3*1024*1024) {
        if(http.active)return nullptr;
        if(auto *p=bili::window(sample,cache_offset,http.data.data(),http.used))return p;
        if(request(media_url,Job::media,size_t(file_bytes),true,0)) {
            cache_offset=0;buffering=true;dirty=true;show_hud(true);
        }
        return nullptr;
    }
    if (auto *p=bili::window(sample,cache_offset,http.data.data(),http.used)) return p;
    if (auto *p=bili::window(sample,audio_cache_offset,audio_http.data.data(),audio_http.used)) return p;
    if ((http.active && bili::requested(sample,cache_offset,http.limit)) ||
        (audio_http.active && bili::requested(sample,audio_cache_offset,audio_http.limit))) return nullptr;
    // One HTTPS transfer at a time keeps TLS/control memory bounded. Either
    // completed window can continue serving both tracks during the transfer.
    if(http.active || audio_http.active)return nullptr;
    auto& cache=audio_sample ? audio_http : http;
    if (!cache.active) {
        uint64_t offset=sample.offset&~UINT64_C(16383);
        size_t bytes=std::max<size_t>(512*1024,sample.size+size_t(sample.offset-offset));
        if (audio_sample) {
            if (!audio_http.begin(++serial,media_url,net_permission,now_ms(),bytes,true,offset)) fail("无法读取音轨");
            else audio_cache_offset=offset;
        } else if (request(media_url,Job::media,bytes,true,offset)) cache_offset=offset;
    }
    return nullptr;
}
uint64_t position() {
    if (ended) return duration_us;
    if (dragging) return drag_pts;
    if (paused) return std::min(pause_pts,duration_us);
    return play_started && now_ms()>play_started ? std::min(bili::media_position(play_started,play_offset,now_ms(),speed_quarters),duration_us) : video_pts;
}
void show_hud(bool visible) {
    if (hud!=visible) {hud=visible;overlay_dirty=dirty=true;}
    hud_until=now_ms()+4000;
}
void pause_playback(bool value) {
    if (paused==value) return;
    if (value) {
        client.record(video_pts);
        pause_pts=position(); pause_started=now_ms();
        close(audio);cancel(audio_token);cancel(gain_token);cancel(query_token);audio_ready=false;
        pcm_used=pcm_at=0;pcm_packet.reset();pcm_speed.reset();
        ai=size_t(std::lower_bound(audio_track.samples.begin(),audio_track.samples.end(),pause_pts,
            [](const bili::Sample& sample,uint64_t pts){return sample.pts<pts;})-audio_track.samples.begin());
    } else play_started+=now_ms()-pause_started;
    paused=value;show_hud(true);dirty=true;
}
void seek(uint64_t requested,bool stay_paused=false) {
    if (!decoder || video.samples.empty()) return;
    pxa_window_keep_screen_on(1);
    const uint64_t limit=duration_us>1000 ? duration_us-1000 : 0;
    requested=std::min(requested,limit);vi=bili::keyframe(video,requested);
    const uint64_t actual=video.samples[vi].pts;discard_before_pts=actual;
    close(audio);cancel(audio_token);cancel(gain_token);cancel(query_token);audio_ready=false;
    pcm_used=pcm_at=0;pcm_packet.reset();pcm_speed.reset();frame_pending=video_eof=ended=audio_drained=false;
    ai=size_t(std::lower_bound(audio_track.samples.begin(),audio_track.samples.end(),actual,
        [](const bili::Sample& sample,uint64_t pts){return sample.pts<pts;})-audio_track.samples.begin());
    if (pxa_io(decoder,PXA_VIDEO_IO_RESET,nullptr,0)) {fail("无法定位视频");return;}
    compressed.assign(16,0);compressed.insert(compressed.end(),video.parameters.begin(),video.parameters.end());
    if (pxa_video_decode(decoder,compressed.data(),compressed.size(),0,0)) {fail("无法恢复视频参数");return;}
    if (http.active && !bili::requested(video.samples[vi],cache_offset,http.limit)) {http.cancel();job=Job::none;http.used=0;cache_offset=UINT64_MAX;}
    if (audio_http.active && ai<audio_track.samples.size() && !bili::requested(audio_track.samples[ai],audio_cache_offset,audio_http.limit)) {
        audio_http.cancel();audio_http.used=0;audio_cache_offset=UINT64_MAX;
    }
    playing=true;paused=stay_paused;seeking=true;video_pts=pause_pts=actual;pause_started=now_ms();
    play_started=now_ms();play_offset=actual;show_hud(true);
    char line[120];std::snprintf(line,sizeof(line),"BILI_SEEK requested_ms=%llu actual_ms=%llu sample=%zu",
        (unsigned long long)(requested/1000),(unsigned long long)(actual/1000),vi);pxa_log_write(PXA_LOG_LEVEL_INFO,line);
}
void overlay() {
    if (!surface || !overlay_dirty) return;
    pxa_surface_rect_t regions[2]{};unsigned count=0;
    if (hud) {
        int scale=scene.density;
        int top=int((uint64_t(44)*scale+65535)/65536),bottom=int((uint64_t(92)*scale+65535)/65536);
        for(auto [y,h]:{std::pair{0,top},std::pair{int(scene.viewport.display.height)-bottom,bottom}}) {
            int first=std::max(y,int(surface_y)),last=std::min(y+h,int(surface_y+surface_h));
            if (last>first)regions[count++]={0,uint16_t(first-int(surface_y)),uint16_t(surface_w),uint16_t(last-first)};
        }
    }
    uint32_t bytes=0;
    if (pxa_surface_build_opaque_ui_regions(control,sizeof(control),++serial,surface,regions,count,&bytes)) pxa_control_packet(control,bytes);
    overlay_dirty=false;
}
bool pump_pcm() {
    if (!pcm_used) return true;
    if (!audio_ready) return false;
    while (pcm_at<pcm_used || pcm_packet.full()) {
        if (pcm_packet.full()) {
            int n=pcm_packet.write(audio); if (n==PXA_STATUS_WOULD_BLOCK) return false;
            if (n<0) { fail("音频输出失败"); return false; }
            audio_drained=false;
        }
        if (pcm_at<pcm_used) { unsigned frames=pcm_packet.append(reinterpret_cast<int16_t*>(pcm)+pcm_at/2,
            (pcm_used-pcm_at)/(audio_channels*2)); pcm_at+=frames*audio_channels*2; }
    }
    pcm_used=pcm_at=0; return true;
}
void present_frame() {
    pxa_video_frame_t f{};
    int n=pxa_video_frame_info(decoder,&f);
    if (n==PXA_STATUS_WOULD_BLOCK) return;
    if (n<0) { fail("无法读取解码画面"); return; }
    if (f.width!=video.width || f.height!=video.height) { fail("视频尺寸发生变化，请重新播放"); return; }
    decoded_us+=f.decode_us;++decoded_frames;
    if(f.pts_us<discard_before_pts)return;
    if (mapped_surface) { video_pts=f.pts_us; frame_pending=true; mapped_filled=false; return; }
    uint64_t reading=now_us();
    frame_scaled=false;
    if (scaled_read && (surface_w!=f.width || surface_h!=f.height)) {
        frame.resize(size_t(surface_w)*surface_h*2+8);
        int result=pxa_video_read_scaled(decoder,frame.data(),frame.size(),surface_w,surface_h);
        if (result==PXA_STATUS_UNSUPPORTED) scaled_read=false;
        else if (result) {fail("画面缩放失败");return;}
        else frame_scaled=true;
    }
    if (!frame_scaled) {
        frame.resize(f.frame_bytes);
        if (pxa_io(decoder,1,frame.data(),frame.size())!=int(frame.size())) {fail("解码画面不完整");return;}
    }
    video_read_us+=now_us()-reading;
    video_pts=f.pts_us; frame_pending=true;
}
bool show_frame() {
    if (!frame_pending) return true;
    if (!play_started) {
        if (!paused && audio_permission && (capability.audio_codecs&PXA_VIDEO_AUDIO_AAC_ADTS) && !audio_track.samples.empty()) return false;
        play_started=now_ms();play_offset=video_pts;
    }
    if (!paused && bili::media_position(play_started,play_offset,now_ms(),speed_quarters)<video_pts) return false;
    if (mapped_surface) {
        if (mapped_index<0) {
            uint8_t index=0;
            int n=pxa_surface_acquire_buffer(surface,&index);
            if (n==PXA_STATUS_WOULD_BLOCK) return false;
            if (n!=4 || index>=2) { fail("无法取得视频显示缓冲区"); return false; }
            mapped_index=index;
        }
        if (!mapped_filled) {
            uint64_t reading=now_us();
            int result=pxa_video_read_scaled_pixels(decoder,mapped_pixels+size_t(mapped_index)*surface_w*surface_h*2,
                surface_w*surface_h*2);
            video_read_us+=now_us()-reading;
            if (result) { fail("画面缩放失败"); return false; }
            mapped_filled=true;
        }
        uint64_t displaying=now_us();
        int n=pxa_surface_present_buffer(surface,uint8_t(mapped_index),frame_id+1);
        display_us+=now_us()-displaying;
        if (n==PXA_STATUS_WOULD_BLOCK) return false;
        if (n!=16) { fail("视频提交失败"); return false; }
        ++frame_id; mapped_index=-1; frame_pending=false; seeking=false;
        return true;
    }
    uint64_t scale_started=now_us();
    uint8_t *pixels=frame.data()+(frame_scaled?8:0);
    if (!frame_scaled && (surface_w!=video.width || surface_h!=video.height)) {
        scaled.resize(size_t(surface_w)*surface_h*2);
        for (unsigned y=0;y<surface_h;++y) {
            const uint8_t *source=frame.data()+size_t(uint64_t(y)*video.height/surface_h)*video.width*2;
            uint8_t *destination=scaled.data()+size_t(y)*surface_w*2;
            for (unsigned x=0;x<surface_w;++x) {
                uint16_t pixel; std::memcpy(&pixel,source+scale_x[x],2); std::memcpy(destination+x*2,&pixel,2);
            }
        } pixels=scaled.data();
    }
    scaled_us+=now_us()-scale_started;
    uint64_t display_started=now_us();
    int n=pxa_io(surface,2,pixels,surface_w*surface_h*2);
    display_us+=now_us()-display_started;
    if (n==PXA_STATUS_WOULD_BLOCK) return false;
    if (n!=int(surface_w*surface_h*2)) { fail("视频显示失败"); return false; }
    uint32_t bytes=0;
    if (!pxa_surface_build_queue_frame(control,sizeof(control),surface,++frame_id,nullptr,0,&bytes) ||
        pxa_control_packet(control,bytes)) { fail("视频提交失败"); return false; }
    frame_pending=false;seeking=false; return true;
}
void pump() {
    uint64_t net_started=now_us();
    http.pump(now_ms()); audio_http.pump(now_ms());
    net_us+=now_us()-net_started;
    if (audio_http.done) { audio_http.done=false;
        if (audio_http.failed) { fail("音轨网络请求失败，请重新播放"); return; }
        audio_cache_offset=audio_http.start;
    }
    if (http.done) { http.done=false; complete_http(); }
    if (surface && hud && !paused && !dragging && !seeking && !buffering && playing && now_ms()>hud_until) show_hud(false);
    if (surface && hud && now_ms()>hud_tick) {dirty=true;hud_tick=now_ms()+500;}
    if (!playing || (paused && !seeking) || dragging || !decoder || !surface) return;
    if(resume_pending) {
        size_t index=bili::keyframe(video,resume_pending);
        if(index>=video.samples.size() || !cached(video.samples[index],false))return;
        uint64_t target=resume_pending;resume_pending=0;seek(target);
    }
    if(video_pts>=next_history && video_pts>1000000){client.record(video_pts);next_history=video_pts+15000000;}
    if (now_ms()-stats_at>=5000) {
        char line[180]; std::snprintf(line,sizeof(line),"BILI shown=%llu decoded=%llu skipped=%llu decode_mean_us=%llu video_pts_ms=%llu audio_next=%zu",
            (unsigned long long)frame_id,(unsigned long long)decoded_frames,(unsigned long long)skipped_frames,
            (unsigned long long)(decoded_frames ? decoded_us/decoded_frames : 0),(unsigned long long)(video_pts/1000),ai);
        pxa_log_write(PXA_LOG_LEVEL_INFO,line); stats_at=now_ms();
        std::snprintf(line,sizeof(line),"BILI_TIME scale_ms=%llu display_ms=%llu read_ms=%llu aac_ms=%llu net_ms=%llu",
            (unsigned long long)(scaled_us/1000),(unsigned long long)(display_us/1000),(unsigned long long)(video_read_us/1000),
            (unsigned long long)(aac_us/1000),(unsigned long long)(net_us/1000));
        pxa_log_write(PXA_LOG_LEVEL_INFO,line);
        std::snprintf(line,sizeof(line),"BILI_CLOCK media_ms=%llu video_ms=%llu lag_ms=%lld",
            (unsigned long long)(position()/1000),(unsigned long long)(video_pts/1000),
            (long long)(int64_t(position()/1000)-int64_t(video_pts/1000)));pxa_log_write(PXA_LOG_LEVEL_INFO,line);
    }
    if (audio_ready && now_ms()>=next_query && !query_token) {
        query_token=++serial; next_query=now_ms()+250; pxa_audio_query(query_token,audio);
    }
    bool pcm_free=pump_pcm(); if (!playing) return;
    show_frame(); if (!playing) return;
    // Decode at most one access unit per event to retain input responsiveness.
    uint64_t media_time=play_started ? bili::media_position(play_started,play_offset,now_ms(),speed_quarters) : 0;
    bool audio_next=!paused && pcm_free && audio_permission && (capability.audio_codecs&PXA_VIDEO_AUDIO_AAC_ADTS) && ai<audio_track.samples.size() &&
        (audio_track.samples[ai].pts<=media_time+450000 || vi>=video.samples.size());
    if (audio_next) {
        auto& s=audio_track.samples[ai]; const uint8_t *sample=cached(s,true); if (!sample) return;
        if (s.size+15>8192) { fail("AAC 帧超过输入上限"); return; }
        compressed.resize(s.size+15); pxa_store_u64(compressed.data(),s.pts);
        auto *p=compressed.data()+8; unsigned len=s.size+7,channels=audio_track.channels;
        p[0]=0xff;p[1]=0xf1;p[2]=(1<<6)|(audio_track.aac_rate_index<<2)|(channels>>2);
        p[3]=((channels&3)<<6)|(len>>11);p[4]=len>>3;p[5]=(len<<5)|31;p[6]=0xfc;
        std::memcpy(p+7,sample,s.size);
        uint64_t audio_started=now_us();
        if (pxa_io(decoder,0x110,compressed.data(),compressed.size()) || pxa_io(decoder,0x111,info,24)) { fail("AAC 音频解码失败"); return; }
        pcm_used=pxa_load_u32(info+8); pcm_at=0;
        channels=pxa_load_u16(info+4);
        if ((channels!=1&&channels!=2)||pcm_used>sizeof(pcm_source)||pcm_used%(channels*2)||pxa_io(decoder,0x112,reinterpret_cast<uint8_t*>(pcm_source),pcm_used)!=int(pcm_used)) { fail("音频帧无效"); return; }
        size_t frames=pcm_used/(channels*2);size_t converted=pcm_speed.convert(pcm_source,frames,channels,speed_quarters,reinterpret_cast<int16_t*>(pcm),sizeof(pcm)/(channels*2));
        if(frames&&!converted){fail("音频变速失败");return;}pcm_used=converted*channels*2;
        aac_us+=now_us()-audio_started;
        if (!audio && !audio_token) {
            audio_rate=pxa_load_u32(info);audio_channels=pxa_load_u16(info+4); audio_pts_base=s.pts;
            pcm_packet.configure(audio_rate,audio_channels); audio_token=++serial;
            if (pxa_audio_open_pcm_buffered(audio_token,audio_permission,audio_rate,audio_channels)) {
                fail("无法打开缓冲音频输出"); return;
            }
        }
        ++ai;audio_drained=false;
    } else if (frame_pending) return;
    else if (vi<video.samples.size()) {
        if (!seeking && media_time>video.samples[vi].pts+500000 && !video.sync_samples.empty()) {
            size_t target=vi;
            for (auto key:video.sync_samples) {
                if (key>vi && video.samples[key].pts<=media_time) target=key;
                if (video.samples[key].pts>media_time) break;
            }
            if (target>vi) {
                // An IDR retires prior references itself. Reinitializing here
                // needlessly reallocates pictures and destroys AAC history.
                skipped_frames+=target-vi;vi=target;discard_before_pts=video.samples[vi].pts;
            }
        }
        auto& s=video.samples[vi]; const uint8_t *sample=cached(s); if (!sample) return;
        bool non_reference=false;
        if (!bili::annexb({sample,s.size},video.nal_bytes,compressed,capability.max_packet_bytes,&non_reference)) { fail("AVC 帧格式无效或过大"); return; }
        if(!seeking && non_reference && media_time>s.pts+100000) {++vi;++skipped_frames;return;}
        if (pxa_video_decode(decoder,compressed.data(),compressed.size(),s.pts,0)) { fail("H.264 解码失败，请降低画质"); return; }
        ++vi; present_frame();
    } else {
        if (!video_eof) {
            compressed.resize(16);
            if (pxa_video_decode(decoder,compressed.data(),16,0,1)) {fail("视频尾帧解码失败");return;}
            present_frame();if(!playing || frame_pending)return;video_eof=true;
        }
        if (pcm_used) return;
        if (pcm_packet.frames && audio_ready) {
            int result=pcm_packet.write(audio);
            if(result==PXA_STATUS_WOULD_BLOCK)return;
            if(result<0){fail("音频尾帧输出失败");return;}
            audio_drained=false;
        }
        if (audio && !audio_drained) return;
        playing=false;ended=true;client.record(video_pts,true);close(audio);audio_ready=false;show_hud(true);
        pxa_window_keep_screen_on(0);status("播放结束");
    }
}
void time_text(uint64_t us,char *out,size_t bytes) {
    bili::duration_text(us/1000000,out,bytes);
}
void render() {
    bool editing=!surface && client.search_input();
    if (scene.input_mode!=editing) {scene.input_mode=editing;scene.invalidate();}
    bool fresh=!scene.initialized;
    int w=scene.viewport.content.w,h=scene.viewport.content.h;
    scene.begin(w,h,surface?0x000000:bili::ui::colors.background);
    if (!surface)client.render(scene,client.in_player,message);
    else if (hud) {
        scene.rect(0,0,w,44,0x101219);scene.rect(0,h-96,w,96,0x101219);
        bili::ui::icon(scene,"back",23,22,0xffffff);scene.hit(21,2,1,44,42);
        bili::ui::label(scene,48,0,w-138,44,title,0xffffff,0,1);
        char speed[24];std::snprintf(speed,sizeof(speed),"%g×",double(speed_quarters)/4);bili::ui::label(scene,w-80,0,72,44,speed,0xffffff,1);scene.hit(24,w-84,0,80,44);
        char current[24],total[24];time_text(position(),current,sizeof(current));time_text(duration_us,total,sizeof(total));
        bili::ui::label(scene,16,h-96,74,26,current,0xffffff);bili::ui::label(scene,w-82,h-96,66,26,total,0xb3bac9,2);
        char hint[96];
        if(buffering && file_bytes)std::snprintf(hint,sizeof(hint),"缓冲 %u%%",unsigned(std::min<uint64_t>(100,http.used*100/file_bytes)));
        else std::snprintf(hint,sizeof(hint),"%s",seeking?"正在定位…":paused?"已暂停":ended?"播放结束":"");
        bili::ui::label(scene,90,h-96,w-180,26,hint,0xb3bac9,1);
        int x=20+int(duration_us ? uint64_t(std::max(1,w-40))*position()/duration_us : 0);
        scene.line(20,h-63,w-20,h-63,0x414655,3);scene.line(20,h-63,x,h-63,bili::ui::colors.primary,3);
        scene.circle(x,h-63,5,bili::ui::colors.primary);scene.hit(40,12,h-79,w-24,32);
        int center=w/2;
        bili::ui::label(scene,center-136,h-48,78,48,"−10 秒",0xffffff,1);scene.hit(22,center-140,h-48,88,48);
        scene.circle(center,h-24,20,bili::ui::colors.primary);bili::ui::icon(scene,paused||ended?"play":"pause",center,h-24,bili::ui::colors.on_primary);
        scene.hit(20,center-28,h-48,56,48);
        bili::ui::label(scene,center+58,h-48,78,48,"+10 秒",0xffffff,1);scene.hit(23,center+52,h-48,88,48);
    }
    bool presented=scene.present();if(presented)overlay();
    bool input_ok=true;
    if (fresh && editing)input_ok=scene.input(30,49,12,std::max(40,w-113),client.query,sizeof(client.query)-1,bili::ui::colors.surface,bili::ui::colors.text);
    if(!input_ok)scene.invalidate();
    dirty=!presented||!input_ok;
    if(dirty){if(!ui_retries++)pxa_log_write(PXA_LOG_LEVEL_INFO,"BILLY_UI retry frame");}
    else ui_retries=0;
}
void click(unsigned id) {
    if (id==10)begin_video();
    else if (id>=11 && id<=13){qn=id==11?16:id==12?32:64;dirty=true;}
    else if (id==21){stop();status("已停止");}
    else if (id==20){if(ended)seek(0);else pause_playback(!paused);}
    else if(id==24){static constexpr unsigned rates[]={4,5,6,8,3,2};unsigned next=4;for(unsigned i=0;i<6;++i)if(rates[i]==speed_quarters)next=rates[(i+1)%6];
        bool resume=playing&&!paused&&!ended;if(resume)pause_playback(true);speed_quarters=next;pcm_speed.reset();
        play_offset=pause_pts;play_started=pause_started=now_ms();if(resume)pause_playback(false);show_hud(true);dirty=true;}
    else if (id==22 || id==23) {
        uint64_t current=position();seek(id==22 ? (current>10000000?current-10000000:0) : std::min(duration_us,current+10000000),paused);
    }
}
void pointer(const pxa_ui_pointer_data_t& p) {
    int x=p.x-scene.viewport.content.x,y=p.y-scene.viewport.content.y;
    if (!surface){
        if(client.in_player && p.phase==PXA_POINTER_UP && scene.find(x,y)==103){stop();return;}
        client.pointer(scene,p);return;
    }
    if (p.phase==PXA_POINTER_DOWN && !hud){show_hud(true);consume_up=true;return;}
    if (consume_up){if(p.phase==PXA_POINTER_UP || p.phase==PXA_POINTER_CANCEL)consume_up=false;return;}
    if (p.phase==PXA_POINTER_DOWN && hud && scene.find(x,y)==40) {
        drag_paused=paused;drag_pts=position();pause_playback(true);dragging=true;
    }
    if (dragging) {
        int width=std::max(1,scene.w-40);drag_pts=duration_us*unsigned(std::clamp(x-20,0,width))/unsigned(width);dirty=true;
        if (p.phase==PXA_POINTER_UP){uint64_t target=drag_pts;dragging=false;seek(target,drag_paused);}
        else if (p.phase==PXA_POINTER_CANCEL){dragging=false;pause_playback(drag_paused);}
    } else if (p.phase==PXA_POINTER_UP) {
        unsigned id=scene.find(x,y);if(id)click(id);else show_hud(false);
    }
}

}
extern "C" int32_t pxa_app_start(const uint8_t *data,uint32_t size) {
    pxa_ui_environment_t e{}; if (pxa_ui_parse_start_environment(data,size,&e)) scene.environment(e);
    pxa_window_request_snapshot(3); pxa_clock_set_period(PXA_CLOCK_MIN_PERIOD_MS); pxa_video_request_capability(2,0);
    browser_window();pxa_ui_theme_get(5);client.start();render();return 0;
}
extern "C" int32_t pxa_app_on_event(const uint8_t *data,uint32_t size) {
    pxa_event_t e{}; if (!pxa_parse_event(data,size,&e)) return PXA_EVENT_UNHANDLED;
    pxa_ui_theme_t theme{};
    if(pxa_ui_parse_theme_event(&e,&theme)){bili::ui::theme(theme);scene.invalidate();client.dirty=dirty=true;if(!surface)browser_window();}
    bool client_event=client.event(e);
    if (!client_event && e.service==PXA_NET_SERVICE && e.opcode==PXA_HTTP_OPEN) {
        if (e.token==audio_http.token) audio_http.event(e); else http.event(e);
    }
    pxa_video_capability_t cap{};
    if (pxa_video_parse_capability(&e,cap_token,&cap)) {
        if (!cap.status && cap.codec==PXA_VIDEO_H264 && (!capability.count || cap.implementation==PXA_VIDEO_HARDWARE)) capability=cap;
        if (!cap.status && cap.index+1<cap.count) { cap_token=++serial; pxa_video_request_capability(cap_token,cap.index+1); }
        client.max_pixels=capability.max_pixels;dirty=true;
    }
    if (e.service==24 && e.opcode==2 && e.token==decoder_token) {
        decoder_token=0;
        if (e.payload_size!=12 || pxa_load_u32(e.payload)) fail("无法分配视频解码器");
        else { decoder=pxa_load_u64(e.payload+4); compressed.assign(16,0);
            compressed.insert(compressed.end(),video.parameters.begin(),video.parameters.end());
            auto result=pxa_video_decode(decoder,compressed.data(),compressed.size(),0,0);
            unsigned profile=0,level=0;
            if(video.parameters.size()>=8){profile=video.parameters[5];level=video.parameters[7];}
            char line[180];std::snprintf(line,sizeof(line),"BILI_STREAM bvid=%s size=%ux%u profile=%u level=%u headers_status=%d",
                bvid,video.width,video.height,profile,level,int(result));pxa_log_write(PXA_LOG_LEVEL_INFO,line);
            if(result==PXA_STATUS_UNSUPPORTED)fail("设备不支持此 H.264 编码格式或 Level");
            else if(result==PXA_STATUS_LIMIT_EXCEEDED)fail("视频码流超过设备限制，请选择更低画质");
            else if(result==PXA_STATUS_RESOURCE_LIMIT)fail("解码内存不足，请选择更低画质");
            else if(result)fail("视频解码参数无效");else make_surface(); }
    }
    else if (e.service==24 && e.opcode==2 && e.payload_size==12 && !pxa_load_u32(e.payload)) {
        uint64_t late=pxa_load_u64(e.payload+4); if (late) pxa_close_handle(late);
    }
    pxa_surface_create_result_t sr{};
    if (surface_token && pxa_surface_parse_create(&e,surface_token,&sr)) {
        surface_token=0;
        if (sr.status && mapped_surface) {
            mapped_surface=false;
            pxa_surface_desc_t desc{uint16_t(surface_w),uint16_t(surface_h),PXA_SURFACE_RGB565,2,0};
            uint32_t bytes=0; surface_token=++serial;
            if (!pxa_surface_build_create(control,sizeof(control),surface_token,desc,&bytes) || pxa_control_packet(control,bytes))
                fail("无法创建视频画面");
        }
        else if (sr.status) fail("无法分配视频画面");
        else { surface=sr.handle; uint32_t bytes=0;
            if (mapped_surface) {
                size_t frame_bytes=size_t(surface_w)*surface_h*2;
                mapped_storage.resize(frame_bytes*2+PXA_SURFACE_BUFFER_ALIGNMENT-1);
                mapped_pixels=reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(mapped_storage.data())+
                    PXA_SURFACE_BUFFER_ALIGNMENT-1)&~uintptr_t(PXA_SURFACE_BUFFER_ALIGNMENT-1));
                if (pxa_surface_register_buffers(surface,mapped_pixels,frame_bytes,2)!=int(frame_bytes*2)) {
                    fail("无法注册视频显示缓冲区"); return PXA_EVENT_HANDLED;
                }
                pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI mapped display buffers enabled");
            }
            int x=surface_x,y=surface_y;
            if (pxa_surface_build_configure_layer(control,sizeof(control),++serial,surface,x,y,surface_w,surface_h,0,1,&bytes)) pxa_control_packet(control,bytes);
            duration_us=bili::duration(video);hud=overlay_dirty=true;hud_until=now_ms()+4000;
            playing=true; play_started=0;
            if(!file_bytes || http.used!=file_bytes || cache_offset!=0){http.used=0;cache_offset=UINT64_MAX;}
            status("正在播放"); }
    }
    else if (e.service==PXA_SURFACE_SERVICE && e.opcode==PXA_SURFACE_CREATE &&
        pxa_surface_parse_create(&e,e.token,&sr) && !sr.status) {
        if (sr.handle) pxa_close_handle(sr.handle);
    }
    if (e.service==PXA_PERMISSION_SERVICE && e.opcode==PXA_PERMISSION_ACQUIRE) {
        pxa_permission_acquire_result_t r{};
        if (pxa_permission_parse_acquire(&e,e.token,&r)) {
            if (e.token==1) { network_prompt=false;
                if (!r.status)net_permission=r.handle;
                else client.play_request=false;
                client.permission_result(!r.status);
            }
            if (e.token==4 && !r.status) audio_permission=r.handle;
        }
    }
    if (audio_token && e.token==audio_token && e.service==PXA_AUDIO_SERVICE) {
        pxa_audio_pcm_result_t r{};
        if (pxa_audio_parse_pcm_buffered(&e,audio_token,&r)) { audio_token=0;
            if (r.source.status) fail("无法打开音频输出"); else { audio=r.source.handle; gain_token=++serial; pxa_audio_commit_gain(gain_token,audio,0); } }
    }
    else if (e.service==PXA_AUDIO_SERVICE && e.opcode==PXA_AUDIO_OPEN_PCM) {
        pxa_audio_pcm_result_t late{}; if (pxa_audio_parse_pcm_buffered(&e,e.token,&late) && !late.source.status && late.source.handle) pxa_close_handle(late.source.handle);
    }
    if (gain_token && e.token==gain_token && e.service==PXA_AUDIO_SERVICE) { int32_t result=0;
        if (pxa_audio_parse_status(&e,gain_token,PXA_AUDIO_COMMIT_GRAPH,&result)) { gain_token=0; if (result) fail("无法配置音频"); else audio_ready=true; } }
    if (query_token && e.token==query_token && e.service==PXA_AUDIO_SERVICE) {
        pxa_audio_state_result_t state{};
        if (pxa_audio_parse_state(&e,query_token,&state)) { query_token=0;
            if (!state.status && audio_rate && !paused) {
                play_offset=audio_pts_base+state.accepted_samples*250000*speed_quarters/audio_rate;
                play_started=now_ms();
                audio_drained=!state.queued_samples && ai==audio_track.samples.size() && !pcm_used && !pcm_packet.frames;
                if(audio_drained){close(audio);audio_ready=false;}
            }
        }
    }
    pxa_window_snapshot_t ws{};
    if (pxa_window_parse_snapshot(&e,3,&ws) || pxa_window_parse_metrics_changed(&e,&ws)) { scene.window(ws); dirty=true; }
    pxa_ui_environment_t env{}; if (pxa_ui_parse_environment_event(&e,&env)) { scene.environment(env); dirty=true; }
    pxa_ui_event_data_t ui{}; if (pxa_ui_parse_event(&e,&ui) && ui.node==30){pxa_ui_event_text(&ui,client.query,sizeof(client.query));client.dirty=true;}
    pxa_ui_pointer_data_t p{}; if (pxa_ui_parse_pointer(&e,&p) && p.node==2 && p.generation==scene.ui_generation)pointer(p);
    uint8_t lifecycle=0;
    if (pxa_lifecycle_parse(&e,&lifecycle)) { background=lifecycle==PXA_SYSTEM_LIFECYCLE_BACKGROUND;
        if (background) { if(client.in_player)stop();client.suspend();pxa_clock_set_period(0); } else { pxa_clock_set_period(PXA_CLOCK_MIN_PERIOD_MS); dirty=true; } }
    if (pxa_window_is_back_requested(&e)) {
        if (client.in_player || surface || job!=Job::none)stop();
        else if(!client.back())return PXA_EVENT_UNHANDLED;
    }
    if (!background) {
        if(want_surface && decoder && !surface_token)make_surface();
        uint64_t started=now_us();
        for(unsigned n=0;n<12;++n) {
            auto previous=ai+vi+frame_id;pump();
            if(previous==ai+vi+frame_id || now_us()-started>=8000)break;
        }
        client.pump(now_ms(),net_permission,true);
        if(client.want_network && !network_prompt && !net_permission){client.want_network=false;network_prompt=true;permission(1,"net.client","web");}
        if(client.play_request && net_permission){client.play_request=false;std::snprintf(draft,sizeof(draft),"%s",client.selected.bvid);begin_video();}
        if(client.input_changed){client.input_changed=false;scene.invalidate();dirty=true;}
        if(client.dirty){dirty=true;client.dirty=false;}
        if(dirty)render();
    }
    return PXA_EVENT_HANDLED;
}
extern "C" void pxa_app_stop(uint32_t) { decoder=surface=audio=0; }
