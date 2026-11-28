// SPDX-License-Identifier: MIT
#include "../common/media/canvas.hpp"
#include "../common/media/json.hpp"
#include "../common/media/pcm_packet.hpp"
#include "http.hpp"
#include "dash.hpp"
#include "danmaku.hpp"
#include "playback.hpp"
#include "client.hpp"
#include "pxa_video.h"
#include "pxa.h"
#include "pxa_surface.h"
#include "pxa_clock.h"
#include "pxa_permission.h"
#include "pxa_log.h"
#include <ctime>
extern "C" int billy_scan_nal4_zbb(const uint8_t *,size_t,uint8_t *);
namespace {
bili::ScanNal4 scan_nal4=bili::scan_nal4;
enum class Job { none,view,playurl,box,moov,prebuffer,media,dash_video_init,dash_audio_init,dash_video };
struct DashPrefetch {
    bili::Http response;
    size_t first=0,end=0;
    bili::DashRange paged{};
    bool continuation=false;
    uint64_t retry_at=0;
    void discard() { response.cancel();response.release_body();retry_at=0; }
    bool pending() const { return response.active || response.done; }
};
constexpr size_t progressive_prebuffer_limit=768*1024;
constexpr size_t dash_audio_group=160*1024,dash_video_group=256*1024;
constexpr size_t dash_video_demand_group=320*1024;
constexpr uint64_t dash_media_idle_ms=8000;
constexpr uint64_t dash_video_first_byte_ms=12000;
constexpr uint64_t dash_prefetch_idle_ms=6500;
// The HTTP deadline covers the body too. The ESP Host caps connection setup
// for long transfers, while buffered ranges can outlive a slow decoder read.
constexpr uint32_t dash_transfer_timeout_ms=90000;
constexpr size_t dash_video_budget=1152*1024,dash_cache_budget=dash_video_budget+dash_audio_group;
constexpr uint64_t history_checkpoint_interval_ms=60000;
constexpr unsigned player_hud_ms=4000;
constexpr int player_header_dp=52, player_controls_dp=96;
media::Canvas scene{0}; // Billy handles pointer input on the Canvas node.
bili::Client client;
bili::Http http,audio_http,dm_http;
DashPrefetch video_prefetch,audio_prefetch;
bili::DanmakuSegment dm_segment;
std::array<const bili::Danmaku*,bili::danmaku_lanes> dm_lanes{};
uint64_t video_cid=0,displayed_pts=0,dm_retry_at=0,dm_next_paint=0;
uint32_t dm_loaded=0,dm_requested=0,dm_painted_ms=UINT32_MAX,dm_ui_generation=0;
size_t dm_cursor=0;
bool dm_enabled=false,dm_canvas_visible=false,dm_layer_error=false,dm_paint_logged=false;
int dm_x=0,dm_y=0,dm_width=0;
bili::Track video,audio_track;
bili::DashStream dash_video,dash_audio;
bili::DashRangeParser video_range_parser, audio_range_parser;
bool dash_playback=false,dash_opening=false,range_started_early=false;
unsigned audio_range_retries=0;
unsigned video_range_retries=0;

bili::DashRange opening_video_segment{}, paged_video{};

bool playurl_retried=false;
uint64_t playurl_retry_at=0;
media::PcmPacket pcm_packet;
pxa_video_capability_t capability{};
uint8_t control[4096]{},info[32]{};
// Native encoded audio needs no guest PCM buffers. Allocate the decoded
// output on the PCM path and the resampler input only at non-unit speed.
constexpr size_t max_pcm_values=8192;
std::unique_ptr<int16_t[]> pcm,pcm_source;
bili::PcmSpeed pcm_speed;unsigned speed_quarters=4;
std::vector<uint8_t> compressed,frame,scaled;
std::vector<uint8_t> encoded_batch;
unsigned encoded_batch_count=0;
uint64_t encoded_retry_at=0;
unsigned encoded_batches_built=0,encoded_batches_blocked=0,encoded_batches_written=0;
std::unique_ptr<uint8_t,decltype(&std::free)> mapped_storage{nullptr,&std::free};
uint8_t *mapped_pixels=nullptr;
uint8_t mapped_buffer_count=2;
int mapped_index=-1;
uint64_t surface_retry_at=0;
std::vector<uint32_t> scale_x;
uint64_t serial=100,net_permission=0,audio_permission=0,decoder=0,surface=0,audio=0;
uint64_t decoder_token=0,surface_token=0,audio_token=0,gain_token=0,query_token=0,next_query=0;
uint64_t audio_cache_offset=UINT64_MAX,cap_token=2,audio_pts_base=0,pause_pts=0;
uint64_t file_bytes=0;
uint64_t resume_pending=0,next_history=0,play_offset=0;
bool network_prompt=false,audio_prompt=false;
media::PcmOpen pcm_open;
bool audio_output_unavailable=false;
bool encoded_allowed=true,encoded_audio=false,encoded_finishing=false,encoded_drained=false;
unsigned ui_retries=0;
uint64_t stats_at=0,decoded_us=0,decoded_frames=0,skipped_frames=0;
uint64_t video_inputs=0,video_disposable_inputs=0,video_disposable_skips=0;
uint64_t async_packets=0,async_overlap=0,async_submit_us=0,surface_blocks=0;
uint64_t async_retry_at=0, fused_frames=0;
bool fuse_surface=true, ordered_surface=true;
uint64_t scaled_us=0,display_us=0,aac_us=0,video_read_us=0,net_us=0;
uint64_t box_offset=0,box_bytes=0,cache_offset=0,play_started=0,pause_started=0,frame_id=0,video_pts=0;
size_t vi=0,ai=0,pcm_used=0,pcm_at=0;
unsigned qn=16,boxes_seen=0,surface_w=0,surface_h=0,surface_x=0,surface_y=0;
unsigned output_w=0,output_h=0;
uint64_t duration_us=0,hud_until=0,hud_tick=0,drag_pts=0,discard_before_pts=0;
uint64_t audio_discard_before_pts=0;
bool hud=true,overlay_dirty=true,dragging=false,drag_paused=false,consume_up=false,seeking=false,ended=false,video_eof=false,frame_scaled=false;
int player_pointer=-1;
bool scaled_read=true,audio_drained=false,want_surface=false;
bool host_surface=false;
bool mapped_surface=false,mapped_filled=false,mapped_strided=false;
bool async_decode=true,async_pending=false,async_drain=false;
bool poll_frame_supported=true;
bool buffering=false;
bool compact_video=false;
bool limit_video_pages=false;
uint32_t audio_rate=0,audio_channels=0;
bool background=false,dirty=true,playing=false,paused=false,audio_ready=false,frame_pending=false,draining=false;
Job job=Job::none;
char draft[192]="BV1J4411v7g6",bvid[13]{},title[192]="billy",message[192]="输入 BV 号或视频链接，默认 360p";
char media_url[2049]{};
uint64_t now_us() { timespec t{}; clock_gettime(CLOCK_MONOTONIC,&t); return uint64_t(t.tv_sec)*1000000+t.tv_nsec/1000; }
uint64_t now_ms() { timespec t{}; clock_gettime(CLOCK_MONOTONIC,&t); return uint64_t(t.tv_sec)*1000+t.tv_nsec/1000000; }
void close(uint64_t& h) { if (h) pxa_close_handle(h); h=0; }
void cancel(uint64_t& t) { if (t) pxa_cancel(t); t=0; }
void reset_encoded_batch() {
    encoded_batch.clear();
    encoded_batch_count=0;
    encoded_retry_at=0;
}
void close_audio_output() {
    close(audio);cancel(audio_token);cancel(gain_token);cancel(query_token);
    next_query=0;
    audio_ready=audio_drained=encoded_audio=encoded_finishing=encoded_drained=false;
    reset_encoded_batch();
    pcm_used=pcm_at=0;pcm_packet.reset();pcm_speed.reset();
    pcm.reset();pcm_source.reset();
}
void discard_prefetch() {
    video_prefetch.discard();audio_prefetch.discard();
}
void status(const char *s) { std::snprintf(message,sizeof(message),"%s",s); dirty=true; if(!client.in_player)client.notice(s); pxa_log_write(PXA_LOG_LEVEL_INFO,s); }
void browser_window() {
    pxa_window_config_t window{};window.status_bar_icons=window.navigation_bar_icons=bili::ui::colors.dark?1:2;
    window.status_bar_color=window.navigation_bar_color=pxa_canvas_rgba(bili::ui::colors.background);pxa_window_configure(&window);
}
void stop() {
    if(client.in_player){if(playing||surface)client.record(video_pts,ended);client.leave_player();}
    pxa_window_keep_screen_on(0);
    browser_window(); pxa_window_set_rotation(0);
    http.cancel(); audio_http.cancel(); dm_http.cancel(); close(decoder); close(surface); close(audio);
    discard_prefetch();
    dm_http.release_body();
    dm_lanes.fill(nullptr); dm_segment={}; dm_cursor=0;
    dm_loaded=dm_requested=0;
    video_cid=displayed_pts=dm_retry_at=dm_next_paint=0;
    dm_painted_ms=UINT32_MAX;
    dm_layer_error=dm_paint_logged=false;
    mapped_pixels=nullptr; mapped_index=-1; surface_retry_at=0;
    host_surface=mapped_surface=mapped_filled=mapped_strided=false;
    mapped_buffer_count=2;
    mapped_storage.reset();
    cancel(decoder_token); cancel(surface_token); cancel(audio_token); cancel(gain_token); cancel(query_token);
    video={}; audio_track={};
    decltype(compressed){}.swap(compressed);
    decltype(frame){}.swap(frame);
    decltype(scaled){}.swap(scaled);
    decltype(scale_x){}.swap(scale_x);
    dash_video={}; dash_audio={}; dash_playback=dash_opening=false;
    paged_video={};opening_video_segment={};playurl_retried=false;playurl_retry_at=0;
    limit_video_pages=false;
    video_range_parser={}; range_started_early=false;
    audio_range_retries=0;
    http.release_body(); audio_http.release_body();
    playing=paused=audio_ready=frame_pending=draining=buffering=false;file_bytes=0;
    pcm_open={};audio_output_unavailable=false;
    encoded_allowed=true;encoded_audio=encoded_finishing=encoded_drained=false;
    reset_encoded_batch();
    decltype(encoded_batch){}.swap(encoded_batch);
    pcm.reset();pcm_source.reset();
    encoded_batches_built=encoded_batches_blocked=encoded_batches_written=0;
    player_pointer=-1; consume_up=false;
    hud=overlay_dirty=true; dragging=seeking=ended=video_eof=audio_drained=want_surface=false; duration_us=discard_before_pts=audio_discard_before_pts=0; job=Job::none; vi=ai=pcm_used=pcm_at=0;
    pcm_packet.reset();pcm_speed.reset(); audio_cache_offset=UINT64_MAX; play_started=video_pts=pause_pts=pause_started=drag_pts=0;
    stats_at=decoded_us=decoded_frames=skipped_frames=frame_id=audio_pts_base=0;
    video_inputs=video_disposable_inputs=video_disposable_skips=0;
    async_decode=fuse_surface=ordered_surface=true;async_pending=async_drain=false;fused_frames=0;
    poll_frame_supported=true;
    async_packets=async_overlap=async_submit_us=surface_blocks=0;
    async_retry_at=0;
    scaled_us=display_us=aac_us=video_read_us=net_us=0;
    resume_pending=next_history=play_offset=0;
}
void fail(const char *s) { stop(); status(s); }
void defer_danmaku() {
    if (!dm_http.active && !dm_http.done) return;
    dm_http.cancel();
    dm_http.release_body();
    dm_requested=0;
    dm_retry_at=now_ms()+2500;
}
bool request(const char *url,Job next,size_t bytes=65536,bool range=false,uint64_t offset=0) {
    defer_danmaku();
    job=next;
    auto cookies=client.cookie();
    if (!http.begin(++serial,url,net_permission,now_ms(),bytes,range,offset,cookies.c_str())) { fail("无法创建网络请求"); return false; }
    return true;
}
void permission(uint64_t token,const char *name,const char *scope) {
    uint32_t bytes=0; if (pxa_permission_build(control,sizeof(control),PXA_PERMISSION_ACQUIRE,token,
        name,std::strlen(name),reinterpret_cast<const uint8_t*>(scope),std::strlen(scope),&bytes)) pxa_control_packet(control,bytes);
}
int32_t open_audio() {
    audio_token=++serial;
    return pcm_open.open(audio_token,audio_permission,audio_rate,audio_channels);
}
bool audio_enabled() {
    return audio_permission && !audio_output_unavailable && (capability.audio_codecs&PXA_VIDEO_AUDIO_AAC_ADTS);
}
void audio_open_failed(int32_t result) {
    if (pcm_open.use_default(result)) {
        pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_AUDIO using PCM without buffered mode");
        result=open_audio();
        if (!result) return;
    }
    if (result!=PXA_STATUS_UNSUPPORTED) {
        char line[96];std::snprintf(line,sizeof(line),"BILI_AUDIO pcm_open_result=%d rate=%u channels=%u",result,audio_rate,audio_channels);
        pxa_log_write(PXA_LOG_LEVEL_WARN,line);
        fail("无法打开音频输出");
        return;
    }
    cancel(audio_token);
    audio_output_unavailable=true;
    reset_encoded_batch();
    pcm_used=pcm_at=0;
    pcm_packet.reset();
    pcm_speed.reset();
    audio_http.cancel();audio_http.release_body();
    audio_prefetch.discard();
    status("设备音频输出不可用，已切换无声播放");
}
void request_playurl(uint64_t cid) {
    video_cid=cid;
    char url[256];
    std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/player/playurl?bvid=%s&cid=%llu&qn=%u&fnval=%u&fnver=0&fourk=0&platform=%s",
        bvid,(unsigned long long)cid,qn,qn<=32?16:1,qn<=32?"pc":"html5");
    status("正在获取播放地址…");request(url,Job::playurl);
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
        request_playurl(client.selected.cid);
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
    const auto& display=scene.viewport.display;
    const bool landscape=video.width>=video.height;
    if (display.width==display.height) pxa_window_set_rotation(landscape ? 1 : 0);
    else pxa_window_set_orientation(landscape ? PXA_ORIENTATION_LANDSCAPE : PXA_ORIENTATION_PORTRAIT);
    pxa_window_request_snapshot(3);
    decoder_token=++serial;
    if (pxa_video_request_open(decoder_token,capability.index,video.width,video.height)) { fail("无法打开解码器"); return; }
    status("正在准备播放…");
}
void reset_video_range() {
    paged_video={};
    video_range_parser={}; range_started_early=false;
    video_range_retries=0;
    if (http.active) http.deadline=std::min(http.deadline,now_ms()+dash_video_first_byte_ms);
    video.samples.clear(); video.sync_samples.clear(); vi=0;
}
void reset_audio_range() {
    audio_range_parser={};
    audio_track.samples.clear();
    ai=0;
    if (audio_http.active && !audio_range_retries) audio_http.deadline=now_ms()+dash_media_idle_ms;
}
bool index_audio_range() {
    const auto previous=audio_track.samples.size();
    if (!audio_range_parser.append({audio_http.data(),audio_http.used},audio_http.limit,
                                   audio_http.start,dash_audio,audio_track)) return false;
    if (audio_discard_before_pts && ai==previous && ai<audio_track.samples.size()) {
        ai=size_t(std::lower_bound(audio_track.samples.begin()+ai,audio_track.samples.end(),
            audio_discard_before_pts,[](const bili::Sample& sample,uint64_t pts){return sample.pts<pts;})
            -audio_track.samples.begin());
    }
    return true;
}
size_t extra_display_bytes() {
    return !host_surface && mapped_buffer_count==3 ? size_t(output_w)*output_h*2 : 0;
}
size_t video_page_target() { return compact_video || limit_video_pages ? 128u*1024u : dash_video_demand_group; }
size_t video_page_limit() {
    if (limit_video_pages) return video_page_target();
    return compact_video ? 256u*1024u : bili::dash_max_range;
}
bool fetch_dash(bool audio_stream) {
    auto& stream=audio_stream ? dash_audio : dash_video;
    auto& destination=audio_stream ? audio_http : http;
    auto& prefetched=audio_stream ? audio_prefetch : video_prefetch;
    if (stream.next==stream.segments.size() || destination.active || destination.done) return false;
    if (prefetched.pending()) {
        auto& response=prefetched.response;
        if (!prefetched.continuation && prefetched.first==stream.next && !response.failed &&
            (response.active || (response.done && response.used==response.limit))) {
            const bool in_flight=response.active;
            destination.accept_body(response);
            if (!audio_stream) { job=Job::dash_video;reset_video_range();paged_video=prefetched.paged; }
            else { audio_range_retries=0;reset_audio_range(); }
            stream.next=prefetched.end;
            pxa_log_write(PXA_LOG_LEVEL_INFO,in_flight?(audio_stream?"BILI_PREFETCH audio continued":"BILI_PREFETCH video continued"):
                audio_stream?"BILI_PREFETCH audio hit":"BILI_PREFETCH video hit");
            return true;
        }
        prefetched.discard();
    }
    if (http.active || audio_http.active || (!audio_stream && audio_prefetch.response.active)) return false;
    if (audio_stream && video_prefetch.response.active) video_prefetch.discard();
    if (dash_opening && !audio_stream) opening_video_segment=bili::dash_range(stream,0);
    auto range=bili::dash_range(stream,audio_stream ? dash_audio_group : video_page_target());
    const size_t cache_budget=dash_cache_budget-extra_display_bytes();
    if (audio_stream && range.bytes+http.body_capacity()+video_prefetch.response.body_capacity()>cache_budget)
        video_prefetch.discard();
    else if (!audio_stream && range.bytes+audio_http.body_capacity()+audio_prefetch.response.body_capacity()>cache_budget)
        audio_prefetch.discard();
    defer_danmaku();
    if (audio_stream) {
        audio_range_retries=0;
        if (!audio_http.begin(++serial,stream.url.c_str(),net_permission,now_ms(),range.bytes,true,range.offset,
                              nullptr,nullptr,dash_transfer_timeout_ms)) {
            fail("无法读取 DASH 音轨");return false;
        }
        audio_cache_offset=range.offset;
        reset_audio_range();
    } else {
        int32_t status=0;
        auto begin_range=[&] {
            const auto bytes=range.bytes>video_page_limit() ? video_page_target() : range.bytes;
            return http.begin(++serial,stream.url.c_str(),net_permission,now_ms(),bytes,true,range.offset,
                              nullptr,nullptr,dash_transfer_timeout_ms,&status);
        };
        if (!begin_range()) {
            // Under memory pressure, keep only the media needed immediately.
            // The old video window is being replaced by this request.
            discard_prefetch();
            http.release_body();
            range=bili::dash_range(stream,0);
            // An individual GOP may itself exceed the remaining linear heap.
            // Retain its index and read sample-aligned pages from the same GOP.
            if (status==PXA_STATUS_RESOURCE_LIMIT) limit_video_pages=true;
            if (!begin_range()) { fail("无法创建视频分段请求");return false; }
            pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_REFILL video demand retried with one segment");
        }
        job=Job::dash_video;
        cache_offset=range.offset;
        reset_video_range();
        if (range.bytes>video_page_limit()) paged_video=range;
    }
    if (playing) pxa_log_write(PXA_LOG_LEVEL_INFO,audio_stream?"BILI_REFILL audio demand":"BILI_REFILL video demand");
    stream.next=range.next;
    return true;
}
bool index_video_range() {
    if (!paged_video.bytes)
        return video_range_parser.append({http.data(),http.used},http.limit,http.start,dash_video,video);
    return video_range_parser.append({http.data(),http.used},paged_video.bytes,paged_video.offset,
                                     dash_video,video,size_t(http.start-paged_video.offset));
}

// Only replace the window after its indexed samples have been consumed.
// Re-read a straddling frame or moof from its start in the next HTTP range.
bool continue_video_page(const bili::Sample *sample=nullptr) {
    if (!paged_video.bytes) return false;
    const auto page=bili::next_video_page(paged_video,video_range_parser,sample,video_page_target(),
                                          http.start,http.used);
    if (!page.bytes) return false;
    if (http.active || http.done || audio_http.active) return true;
    if (page.offset==http.start && page.bytes<=http.used) {
        fail("DASH 分段头超过读取窗口");return true;
    }
    defer_danmaku();
    auto& prefetched=video_prefetch.response;
    if (video_prefetch.continuation && video_prefetch.pending() && !prefetched.failed &&
        prefetched.start==page.offset && prefetched.limit==page.bytes) {
        http.accept_body(prefetched);
        pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_PREFETCH video page continued");
    } else {
        if (video_prefetch.pending()) video_prefetch.discard();
        if (audio_prefetch.response.active) audio_prefetch.discard();
        if (!http.begin(++serial,dash_video.url.c_str(),net_permission,now_ms(),page.bytes,true,page.offset,
                        nullptr,nullptr,dash_transfer_timeout_ms)) {
            fail("无法读取 DASH 视频分段窗口");return true;
        }
    }
    job=Job::dash_video;
    video_range_retries=0;
    http.deadline=now_ms()+dash_video_first_byte_ms;
    return true;
}
bool retry_dash_audio() {
    ++audio_range_retries;
    const bool mirror=(audio_range_retries%2) && !dash_audio.backup_url.empty();
    const auto& url=mirror ? dash_audio.backup_url : dash_audio.url;
    if (!audio_http.resume_range(++serial,url.c_str(),net_permission,now_ms(),dash_transfer_timeout_ms))
        return false;
    audio_http.deadline=now_ms()+dash_video_first_byte_ms;
    if (audio_http.active) pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_REFILL audio resumed range");
    return true;
}
bool dash_audio_end() {
    return !dash_playback || !audio_enabled() ||
        (dash_audio.next==dash_audio.segments.size() && !audio_http.active && !audio_http.done);
}
bool video_cache_fits(size_t extra_video,size_t extra_audio) {
    const size_t video_bytes=http.body_capacity()+video_prefetch.response.body_capacity()+extra_video;
    const size_t audio_bytes=audio_http.body_capacity()+audio_prefetch.response.body_capacity()+extra_audio;
    const size_t display_bytes=extra_display_bytes();
    return video_bytes<=dash_video_budget-display_bytes && video_bytes+audio_bytes<=dash_cache_budget-display_bytes;
}
void trim_video_cache() {
    if (paged_video.bytes) return;
    if (vi>=video.samples.size()) {
        http.release_body();
        return;
    }
    uint64_t first_unread=video.samples[vi].offset;
    for (size_t i=vi+1;i<video.samples.size();++i)
        first_unread=std::min(first_unread,video.samples[i].offset);
    if (first_unread<http.start || first_unread-http.start<64*1024) return;
    const auto capacity=http.body_capacity();
    http.discard_cached_prefix(size_t(first_unread-http.start));
    if (capacity!=http.body_capacity()) pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_PREFETCH released consumed video bytes");
}
bool start_dash_prefetch(bool audio_stream,uint64_t now,const bili::VideoPage *page=nullptr) {
    auto& stream=audio_stream ? dash_audio : dash_video;
    auto& prefetched=audio_stream ? audio_prefetch : video_prefetch;
    const size_t limit=audio_stream ? dash_audio_group : dash_video_group;
    const auto range=page ? bili::DashRange{page->offset,page->bytes,stream.next} : bili::dash_range(stream,limit);
    if (audio_stream && range.bytes>limit) return false;
    const bool paged=!audio_stream && !page && range.bytes>video_page_limit();
    const size_t bytes=paged ? video_page_target() : range.bytes;
    const size_t extra_video=audio_stream ? 0 : bytes;
    const size_t extra_audio=audio_stream ? bytes : 0;
    if (!video_cache_fits(extra_video,extra_audio)) {
        trim_video_cache();
        if (!video_cache_fits(extra_video,extra_audio)) {
            prefetched.retry_at=now+250;
            return false;
        }
    }
    defer_danmaku();
    if (!prefetched.response.begin(++serial,stream.url.c_str(),net_permission,now,bytes,true,range.offset,
                                   nullptr,nullptr,dash_transfer_timeout_ms)) {
        prefetched.discard();prefetched.retry_at=now+4000;return false;
    }
    prefetched.first=stream.next;
    prefetched.end=range.next;
    prefetched.continuation=page!=nullptr;
    prefetched.paged=paged ? range : bili::DashRange{};
    prefetched.response.deadline=now+dash_prefetch_idle_ms;
    return true;
}
void prefetch_dash(uint64_t media_time) {
    if (!dash_playback || paused || seeking || !play_started || http.active || audio_http.active ||
        http.done || audio_http.done || audio_prefetch.response.active || video_prefetch.response.active) return;
    const auto now=now_ms();
    if (dm_enabled && video_cid && frame_id &&
        (dm_http.active || (displayed_pts/1000/bili::danmaku_segment_ms+1!=dm_requested && now>=dm_retry_at))) {
        // Give a requested comment segment a turn before speculative ranges.
        // Keep a second of playable media in reserve if the comment CDN stalls.
        const uint64_t until=media_time+(dm_http.active ? 1000000ULL : 2000000ULL)*speed_quarters/4;
        const bool audio_buffered=dash_audio_end() ||
            bili::buffered_through(audio_track,ai,audio_http.start,audio_http.used,until);
        if (audio_buffered && bili::buffered_through(video,vi,http.start,http.used,until)) return;
    }
    const bool video_remaining=dash_video.next<dash_video.segments.size();
    const bool large_next_video=video_remaining && dash_video.segments[dash_video.next].bytes>512*1024;
    // A large range can hold the one HTTPS slot for several seconds.
    const uint64_t audio_lead_us=(large_next_video ? 15000000ULL : 5000000ULL)*speed_quarters/4;
    uint64_t audio_end=UINT64_MAX,video_end=UINT64_MAX;
    if (!audio_prefetch.pending() && now>=audio_prefetch.retry_at && !dash_audio_end() &&
        ai<audio_track.samples.size() && audio_track.samples.back().pts<=audio_track.samples[ai].pts+audio_lead_us)
        audio_end=audio_track.samples.back().pts;
    if (paged_video.bytes && !video_prefetch.pending() && now>=video_prefetch.retry_at) {
        auto page=bili::prefetch_video_page(paged_video,video_range_parser,video,vi,
                                           video_page_target(),http.start,http.used);
        if (page.bytes) {
            if (audio_end!=UINT64_MAX && start_dash_prefetch(true,now)) return;
            start_dash_prefetch(false,now,&page);
            return;
        }
    }
    if (!video_prefetch.pending() && now>=video_prefetch.retry_at && dash_video.next &&
        video_remaining && vi<video.samples.size()) {
        const bool large=dash_video.segments[dash_video.next].bytes>dash_video_group;
        // A small 360p window often lasts only one GOP. Start its successor
        // while there is still time for DNS, TLS and an alternate CDN.
        const uint64_t lead_us=(qn==16 ? 8000000ULL : large ? 5000000ULL : 2500000ULL)*speed_quarters/4;
        const auto end=dash_video.segments[dash_video.next-1].end;
        if (end<=std::max(media_time,video.samples[vi].pts)+lead_us) video_end=end;
    }
    // Once audio runs dry, demand cancels an in-flight video prefetch.
    // Leave time to refill audio even when the video decoder is behind.
    const bool audio_due=audio_end!=UINT64_MAX && audio_end<=media_time+3000000ULL*speed_quarters/4;
    const bool video_first=!audio_due && video_end<audio_end && (!large_next_video || audio_end==UINT64_MAX);
    if (video_first && start_dash_prefetch(false,now)) return;
    if (audio_end!=UINT64_MAX && start_dash_prefetch(true,now)) return;
    if (!video_first && video_end!=UINT64_MAX) start_dash_prefetch(false,now);
}
bool catch_up_dash(uint64_t media_time) {
    if (!dash_playback || paused || seeking || frame_pending || http.active || http.done ||
        !dash_video.next || dash_video.next==dash_video.segments.size() ||
        media_time<=dash_video.segments[dash_video.next-1].end+500000)
        return false;
    const auto target=bili::dash_segment_at(dash_video,media_time);
    const auto& segment=dash_video.segments[target];
    if (target<dash_video.next || !segment.random_access) return false;
    const auto& pending=video_prefetch.response;
    // Let the old video keep advancing while the next GOP is still opening.
    // After the first burst arrives, the in-flight request can be promoted.
    const bool matching=!video_prefetch.continuation && video_prefetch.first==target;
    const bool ready=matching &&
        (pending.done || (pending.active && pending.used>=32*1024));
    if (!ready && (audio_http.active || audio_prefetch.response.active ||
        (video_prefetch.response.active && matching))) return false;
    const auto previous=dash_video.next;
    const auto skipped=video.samples.size()-vi;
    dash_video.next=target;
    if (!fetch_dash(false)) {
        if (playing) dash_video.next=previous;
        return false;
    }
    skipped_frames+=skipped;
    discard_before_pts=segment.pts;
    buffering=true;
    char line[128];
    std::snprintf(line,sizeof(line),"BILI_DASH_CATCHUP target_ms=%llu skipped_segments=%zu",
        (unsigned long long)(segment.pts/1000),target-previous);
    pxa_log_write(PXA_LOG_LEVEL_INFO,line);
    return true;
}
void start_dash() {
    if (resume_pending) {
        dash_video.next=bili::dash_segment_at(dash_video,resume_pending);
        discard_before_pts=audio_discard_before_pts=dash_video.segments[dash_video.next].pts;
        if (audio_track.audio) dash_audio.next=bili::dash_segment_at(dash_audio,audio_discard_before_pts);
        resume_pending=0;
    }
    // Buffer the first audio window before starting the video transfer. The
    // decoder and Surface can then open while that transfer is still running.
    fetch_dash(audio_track.audio && !dash_audio.segments.empty());
}
bool try_open_dash_video() {
    if (!dash_opening || vi>=video.samples.size() ||
        !bili::window(video.samples[vi],http.start,http.data(),http.used)) return false;
    dash_opening=false;
    open_video();
    return true;
}
void prepare() {
    if (!bili::parse_moov({http.data(),http.used},video,audio_track)) { fail("不支持此 MP4 的编码或索引格式"); return; }
    if (video.width>capability.max_width || video.height>capability.max_height ||
        uint32_t(video.width)*video.height>capability.max_pixels) { fail("视频超过设备解码上限，请选择更低画质"); return; }
    if (!video.width || !video.height) { fail("视频尺寸无效"); return; }
    for(const auto *track:{&video,&audio_track})for(const auto& sample:track->samples)
        if(sample.offset>file_bytes || sample.size>file_bytes-sample.offset)file_bytes=0;
    // Finish bounded small-file HTTPS before decoder/display allocations.
    // This preserves internal DMA headroom for TLS and avoids startup retries.
    if(file_bytes && file_bytes<=progressive_prebuffer_limit) {
        status("正在缓冲视频…");cache_offset=0;
        request(media_url,Job::prebuffer,size_t(file_bytes),true,0);return;
    }
    http.release_body();
    http.used=0;
    open_video();
}
void create_surface(bool allow_triple=true) {
    const auto& display=scene.viewport.display;
    output_w=surface_w; output_h=surface_h;
    mapped_buffer_count=2;
    const bool near_full = surface_w*100u >= display.width*97u &&
        surface_h*100u >= display.height*97u;
    const bool region_surface = qn==16 && host_surface &&
        bili::letterboxed_host_surface(surface_w,surface_h,display.width,display.height);
    if ((host_surface && ((display.width<=320 && display.height<=320) || near_full)) ||
        (mapped_surface && !region_surface && !compact_video && display.width==display.height && display.width<=480)) {
        output_w=display.width; output_h=display.height;
    } else if (compact_video && host_surface && surface_w<=display.width && display.width-surface_w<=2) {
        output_w=display.width;
    } else if (mapped_surface && surface_h==display.height && display.width>surface_w &&
               display.width-surface_w<=2) {
        // Include narrow side bars so landscape scanout can rotate the mapped
        // frame directly. The existing strided read preserves both black edges.
        output_w=display.width;
    }
    const bool fullscreen = output_w==display.width && output_h==display.height;
    // Host output is allocated against the native Surface budget. One extra
    // slot allows the next picture to be prepared during panel presentation.
    if (allow_triple && dash_playback && qn==16 && (fullscreen || region_surface)) {
        if (host_surface && size_t(output_w)*output_h<=854u*480u) {
            mapped_buffer_count=3;
        } else if (!host_surface && mapped_surface && output_w==output_h && output_w<=480) {
            auto max_range=[](const bili::DashStream& stream,size_t bytes) {
                if (stream.segments.empty()) return size_t(0);
                for (const auto& segment:stream.segments) bytes=std::max(bytes,size_t(segment.bytes));
                return bytes;
            };
            const size_t display_bytes=size_t(output_w)*output_h*2;
            const size_t video_bytes=max_range(dash_video,dash_video_demand_group);
            const size_t audio_bytes=max_range(dash_audio,dash_audio_group);
            if (video_bytes+display_bytes<=dash_video_budget &&
                video_bytes+audio_bytes+display_bytes<=dash_cache_budget) mapped_buffer_count=3;
        }
    }
    if (mapped_surface && !host_surface) {
        constexpr size_t alignment=std::max<size_t>(PXA_SURFACE_BUFFER_ALIGNMENT,128);
        const size_t total=size_t(output_w)*output_h*2*mapped_buffer_count+alignment-1;
        mapped_storage.reset(static_cast<uint8_t*>(std::malloc(total)));
        if (!mapped_storage) {
            if (mapped_buffer_count==3) {
                pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_SURFACE using two buffers after allocation failure");
                create_surface(false); return;
            }
            mapped_surface=false;
            output_w=surface_w; output_h=surface_h;
        } else {
            mapped_pixels=reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(mapped_storage.get())+
                alignment-1)&~uintptr_t(alignment-1));
        }
    }
    unsigned flags=host_surface ? PXA_SURFACE_HOST_PRODUCED :
        (mapped_surface ? PXA_SURFACE_GUEST_MAPPED : 0);
    mapped_strided=!host_surface && mapped_surface && output_w>surface_w &&
        pxa_video_configure_scaled_stride(decoder,surface_w,surface_h,output_w*2)==0;
    if (output_w==display.width && output_h==display.height) flags|=PXA_SURFACE_PREFER_DIRECT_SCANOUT;
    const pxa_surface_desc_t desc{uint16_t(output_w),uint16_t(output_h),PXA_SURFACE_RGB565,mapped_buffer_count,uint8_t(flags)};
    uint32_t bytes=0; surface_token=++serial;
    if (!pxa_surface_build_create(control,sizeof(control),surface_token,desc,&bytes) || pxa_control_packet(control,bytes)) {
        if (mapped_buffer_count==3) {
            cancel(surface_token); mapped_pixels=nullptr; mapped_storage.reset();
            create_surface(false); return;
        }
        fail("无法创建视频画面");
    }
}
void make_surface() {
    want_surface=true;
    const auto& display=scene.viewport.display;
    if (!display.width || !display.height) return;
    if (display.width!=display.height &&
        (display.width>display.height)!=(video.width>=video.height)) {
        const auto& window=scene.viewport.window;
        if (!window.revision || window.pixel_width!=display.width || window.pixel_height!=display.height) return;
        const unsigned quarter_turns=(window.rotation & 1u) ? 0x5u : 0xau;
        if (window.supported_rotations & quarter_turns) return;
    }
    want_surface=false;
    auto fit=bili::fit(video.width,video.height,scene.viewport.display.width,scene.viewport.display.height);
    uint16_t native_w=fit.width, native_h=fit.height;
    if (!pxa_video_preferred_size(decoder,&native_w,&native_h) &&
        native_w*100u >= fit.width*97u && native_h*100u >= fit.height*97u) {
        fit={unsigned(display.width-native_w)/2,unsigned(display.height-native_h)/2,native_w,native_h};
    }
    surface_w=fit.width;surface_h=fit.height;surface_x=fit.x;surface_y=fit.y;
    compact_video=bili::compact_video_window(video.width,video.height,display.width,display.height);
    scale_x.resize(surface_w);
    for (unsigned x=0;x<surface_w;++x) scale_x[x]=(uint64_t(x)*video.width/surface_w)*2;
    // Configure before creating a mapped Surface so older Hosts can use copying.
    mapped_surface=pxa_video_configure_scaled(decoder,surface_w,surface_h)==0;
    host_surface=true;
    create_surface();
}
void complete_http() {
    if (job==Job::playurl && http.failed && !playurl_retried &&
        (!http.status || http.status==200 || http.status>=500)) {
        playurl_retried=true;
        playurl_retry_at=now_ms()+1500;
        status("网络连接中断，正在重试播放地址…");
        return;
    }
    // Keep the startup buffer on healthy connections. On a stalled transfer
    // only the first indexed segment is needed to hand off to the decoder.
    if (job==Job::dash_video && !paged_video.bytes && dash_opening && http.used<http.limit &&
        opening_video_segment.offset==http.start && http.shorten_received_range(opening_video_segment.bytes)) {
        dash_video.next=opening_video_segment.next;
        pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_REFILL reduced stalled opening to one video segment");
    }
    if (job==Job::dash_video && http.can_resume_range() && video_range_retries<bili::Http::max_range_retries) {
        ++video_range_retries;
        const bool mirror=(video_range_retries%2) && !dash_video.backup_url.empty();
        const auto& url=mirror ? dash_video.backup_url : dash_video.url;
        if (http.resume_range(++serial,url.c_str(),net_permission,now_ms(),dash_transfer_timeout_ms)) {
            http.deadline=now_ms()+dash_video_first_byte_ms;
            if (http.active) pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_REFILL video resumed range");
            return;
        }
    }
    if (http.failed) { char text[192]; std::snprintf(text,sizeof(text),"网络请求失败（HTTP %u），可重试或重新获取链接",http.status); fail(text); return; }
    auto root=media::json::Value{http.data(),http.used};
    if (job==Job::view) {
        if (!json_ok(root)) return;
        client.apply_view(root);
        auto data=root.get("data"); uint64_t cid=data.get("cid").number();
        if (!cid || !data.get("title").string(title,sizeof(title))) { fail("视频不存在或没有可播放分P"); return; }
        request_playurl(cid);
    } else if (job==Job::playurl) {
        if (!json_ok(root)) return;
        auto data=root.get("data"); media::json::Array urls(data.get("durl")); media::json::Value first;
        if (data.get("dash")) {
            if (!bili::dash_tracks(data.get("dash"),qn,dash_video,dash_audio)) {
                fail("当前视频没有所选画质的 AVC 码流");return;
            }
            dash_playback=dash_opening=true;
            status("正在读取 DASH 索引…");
            request(dash_video.url.c_str(),Job::dash_video_init,dash_video.init_bytes,true,0);
            return;
        }
        bool truncated=false;
        if (!urls.next(first) || !first.get("url").string(media_url,sizeof(media_url),&truncated) || truncated ||
            std::strncmp(media_url,"https://",8)) { fail("没有可播放的 HTTPS MP4 地址"); return; }
        if (data.get("video_codecid").number(7)!=7) { fail("此视频未提供设备支持的 AVC 编码"); return; }
        if (data.get("quality").number(qn)!=qn) {fail("站点未提供所选画质，请选择其他画质");return;}
        file_bytes=first.get("size").number();
        box_offset=0; boxes_seen=0; status("正在读取 MP4 索引…"); read_box();
    } else if (job==Job::box) {
        if (http.used<16) { fail("MP4 文件头不完整"); return; }
        box_bytes=bili::be32(http.data()); unsigned header=8;
        if (box_bytes==1) { box_bytes=bili::be64(http.data()+8); header=16; }
        if (box_bytes<header || box_bytes>UINT64_MAX-box_offset) { fail("MP4 文件头无效"); return; }
        if (bili::be32(http.data()+4)==bili::tag("moov")) {
            if (box_bytes-header>768*1024) { fail("视频索引超过内存上限，请选择较短视频"); return; }
            request(media_url,Job::moov,size_t(box_bytes-header),true,box_offset+header);
        } else { box_offset+=box_bytes; read_box(); }
    } else if (job==Job::moov) { job=Job::none; prepare(); }
    else if (job==Job::prebuffer) {cache_offset=0;job=Job::none;open_video();}
    else if (job==Job::media) { cache_offset=http.start; job=Job::none;buffering=false;dirty=true; }
    else if (job==Job::dash_video_init) {
        if (!bili::parse_dash_init({http.data(),http.used},dash_video,video,true)) {
            fail("DASH 视频初始化数据或分段索引无效");return;
        }
        compact_video=bili::compact_video_window(video.width,video.height,
            scene.viewport.display.width,scene.viewport.display.height);
        if (!video.width || !video.height || video.width>capability.max_width ||
            video.height>capability.max_height || uint32_t(video.width)*video.height>capability.max_pixels) {
            char reason[128];
            std::snprintf(reason,sizeof(reason),"视频尺寸 %u×%u 超过设备解码范围，请选择更低画质",video.width,video.height);
            fail(reason);return;
        }
        if (dash_audio.url.empty()) start_dash();
        else request(dash_audio.url.c_str(),Job::dash_audio_init,dash_audio.init_bytes,true,0);
    } else if (job==Job::dash_audio_init) {
        if (!bili::parse_dash_init({http.data(),http.used},dash_audio,audio_track,false)) {
            fail("DASH 音轨索引格式无效");return;
        }
        start_dash();
    } else if (job==Job::dash_video) {
        if (http.used!=http.limit ||
            !index_video_range()) {
            fail("DASH 视频分段不完整或格式无效");return;
        }
        if ((video_range_retries%2) && !dash_video.backup_url.empty())
            dash_video.url.swap(dash_video.backup_url);
        job=Job::none;buffering=false;
        // A range handoff changes the buffering label only while controls are visible.
        if (hud || !surface) dirty=true;
        if (dash_opening) {
            if (try_open_dash_video()) return;
            const auto *first=vi<video.samples.size() ? &video.samples[vi] : nullptr;
            if (!continue_video_page(first)) fail("DASH 首个视频包不完整");
        }
    }
}
void show_hud(bool visible);
const uint8_t *cached(const bili::Sample& sample,bool audio_sample=false) {
    if (dash_playback) {
        const auto& part=audio_sample ? audio_http : http;
        const auto *data=bili::window(sample,part.start,part.data(),part.used);
        if (!data && !audio_sample) continue_video_page(&sample);
        return data;
    }
    if(file_bytes && file_bytes<=progressive_prebuffer_limit) {
        if(http.active)return nullptr;
        if(auto *p=bili::window(sample,cache_offset,http.data(),http.used))return p;
        if(request(media_url,Job::media,size_t(file_bytes),true,0)) {
            cache_offset=0;buffering=true;dirty=true;show_hud(true);
        }
        return nullptr;
    }
    if (auto *p=bili::window(sample,cache_offset,http.data(),http.used)) return p;
    if (auto *p=bili::window(sample,audio_cache_offset,audio_http.data(),audio_http.used)) return p;
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
            defer_danmaku();
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
    hud_until=now_ms()+player_hud_ms;
}
void reset_danmaku_timeline(uint32_t time_ms) {
    dm_cursor=0;
    if (dm_segment.size) {
        const auto first=dm_segment.entries.get();
        dm_cursor=size_t(std::lower_bound(first,first+dm_segment.size,time_ms,[](const auto& item,uint32_t time) {
            return item.time_ms<time;
        })-first);
    }
    dm_lanes.fill(nullptr);
    dm_painted_ms=UINT32_MAX;
}
void pause_playback(bool value,bool save_position=true) {
    if (paused==value) return;
    if (value) {
        if (save_position) client.record(video_pts);
        next_history=0;
        pause_pts=position(); pause_started=now_ms();
        close_audio_output();
        ai=size_t(std::lower_bound(audio_track.samples.begin(),audio_track.samples.end(),pause_pts,
            [](const bili::Sample& sample,uint64_t pts){return sample.pts<pts;})-audio_track.samples.begin());
    } else if (play_started) play_started+=now_ms()-pause_started;
    paused=value;show_hud(true);dirty=true;
}
void seek(uint64_t requested,bool stay_paused=false,bool user_scrub=false) {
    if (!decoder || (!dash_playback && video.samples.empty())) return;
    pxa_window_keep_screen_on(1);
    const uint64_t limit=duration_us>1000 ? duration_us-1000 : 0;
    requested=std::min(requested,limit);
    const size_t target_sample=dash_playback ? 0 : bili::keyframe(video,requested);
    const size_t segment=dash_playback ? bili::dash_segment_at(dash_video,requested) : 0;
    const uint64_t actual=dash_playback ? dash_video.segments[segment].pts : video.samples[target_sample].pts;
    // A repeated tap can resolve to the same in-flight keyframe. Restarting
    // its HTTP range cancels useful data and creates a second TLS handshake.
    if (user_scrub && seeking && discard_before_pts==actual && paused==stay_paused) return;
    vi=target_sample;
    reset_danmaku_timeline(uint32_t(actual/1000));
    discard_before_pts=audio_discard_before_pts=actual;
    close_audio_output();
    frame_pending=video_eof=ended=false;
    async_pending=async_drain=mapped_filled=false;
    surface_retry_at=0;
    async_retry_at=0;
    if (pxa_io(decoder,PXA_VIDEO_IO_RESET,nullptr,0)) {fail("无法定位视频");return;}
    compressed.assign(16,0);compressed.insert(compressed.end(),video.parameters.begin(),video.parameters.end());
    if (pxa_video_decode(decoder,compressed.data(),compressed.size(),0,0)) {fail("无法恢复视频参数");return;}
    if (dash_playback) {
        auto cached_index = [actual](const bili::Track& track, const bili::DashStream& stream,
                                     size_t target, const bili::Http& response) {
            if (target >= stream.next || response.active || response.done || response.failed ||
                response.used != response.limit) return track.samples.size();
            return bili::dash_cached_seek_sample(track,stream.segments[target],actual,response.start,response.used);
        };
        const bool has_audio = audio_track.audio && !audio_output_unavailable;
        const size_t audio_segment = has_audio ? bili::dash_segment_at(dash_audio,actual) : 0;
        vi = cached_index(video,dash_video,segment,http);
        ai = has_audio ? cached_index(audio_track,dash_audio,audio_segment,audio_http) : audio_track.samples.size();
        const bool keep_video = vi < video.samples.size();
        const bool keep_audio = ai < audio_track.samples.size();
        discard_prefetch();
        http.cancel();audio_http.cancel();job=Job::none;
        if (!keep_video) {
            reset_video_range();
            dash_video.next=segment;
        } else buffering=false;
        if (!keep_audio) {
            reset_audio_range();
            dash_audio.next=audio_segment;
        }
        if (keep_video || keep_audio) {
            char cached[64];
            std::snprintf(cached,sizeof(cached),"BILI_SEEK_CACHE video=%d audio=%d",keep_video,keep_audio);
            pxa_log_write(PXA_LOG_LEVEL_INFO,cached);
        }
        const bool need_audio = !stay_paused && has_audio && audio_enabled() && !keep_audio;
        if ((need_audio || !keep_video) && !fetch_dash(need_audio)) return;
    } else {
        ai=size_t(std::lower_bound(audio_track.samples.begin(),audio_track.samples.end(),actual,
            [](const bili::Sample& sample,uint64_t pts){return sample.pts<pts;})-audio_track.samples.begin());
        if (http.active && !bili::requested(video.samples[vi],cache_offset,http.limit)) {http.cancel();job=Job::none;http.used=0;cache_offset=UINT64_MAX;}
        if (audio_http.active && ai<audio_track.samples.size() && !bili::requested(audio_track.samples[ai],audio_cache_offset,audio_http.limit)) {
            audio_http.cancel();audio_http.used=0;audio_cache_offset=UINT64_MAX;
        }
    }
    playing=true;paused=stay_paused;seeking=true;video_pts=pause_pts=actual;pause_started=now_ms();
    play_started=dash_playback ? 0 : now_ms();play_offset=actual;show_hud(true);
    char line[120];std::snprintf(line,sizeof(line),"BILI_SEEK requested_ms=%llu actual_ms=%llu sample=%zu",
        (unsigned long long)(requested/1000),(unsigned long long)(actual/1000),vi);pxa_log_write(PXA_LOG_LEVEL_INFO,line);
}
void overlay() {
    if (!surface || !overlay_dirty) return;
    pxa_surface_rect_t regions[2]{};unsigned count=0;
    if (hud) {
        const auto& content=scene.viewport.content;
        const int top=int((uint64_t(content.y+player_header_dp)*scene.density+65535)/65536);
        const int bottom=int(uint64_t(std::max(0,content.y+content.h-player_controls_dp))*scene.density/65536);
        const int display_h=int(scene.viewport.display.height);
        for(auto [y,h]:{std::pair{0,top},std::pair{bottom,display_h-bottom}}) {
            const int origin=output_h==surface_h ? int(surface_y) : 0;
            int first=std::max(y,origin),last=std::min(y+h,origin+int(output_h));
            if (last>first)regions[count++]={0,uint16_t(first-origin),uint16_t(output_w),uint16_t(last-first)};
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
        if (pcm_at<pcm_used) { unsigned frames=pcm_packet.append(pcm.get()+pcm_at/2,
            (pcm_used-pcm_at)/(audio_channels*2)); pcm_at+=frames*audio_channels*2; }
    }
    pcm_used=pcm_at=0; return true;
}
void write_adts(uint8_t *p,const uint8_t *sample,size_t size) {
    const unsigned len=unsigned(size)+7,channels=audio_track.channels;
    p[0]=0xff;p[1]=0xf1;p[2]=(1<<6)|(audio_track.aac_rate_index<<2)|(channels>>2);
    p[3]=((channels&3)<<6)|(len>>11);p[4]=len>>3;p[5]=(len<<5)|31;p[6]=0xfc;
    std::memcpy(p+7,sample,size);
}
bool decode_audio_sample() {
    auto& s=audio_track.samples[ai]; const uint8_t *sample=cached(s,true); if (!sample) return false;
    if (s.size+15>8192) { fail("AAC 帧超过输入上限"); return false; }
    compressed.resize(s.size+15); pxa_store_u64(compressed.data(),s.pts);
    write_adts(compressed.data()+8,sample,s.size);
    uint64_t audio_started=now_us();
    if (pxa_io(decoder,PXA_VIDEO_IO_AUDIO_DECODE,compressed.data(),compressed.size()) ||
        pxa_io(decoder,PXA_VIDEO_IO_AUDIO_INFO,info,24)) { fail("AAC 音频解码失败"); return false; }
    pcm_used=pxa_load_u32(info+8); pcm_at=0;
    const unsigned channels=pxa_load_u16(info+4);
    if ((channels!=1&&channels!=2)||pcm_used>max_pcm_values*sizeof(int16_t)||pcm_used%(channels*2)) {
        fail("音频帧无效");return false;
    }
    if (!pcm) pcm.reset(new(std::nothrow) int16_t[2*max_pcm_values]);
    if (speed_quarters!=4 && !pcm_source) pcm_source.reset(new(std::nothrow) int16_t[max_pcm_values]);
    if (!pcm || (speed_quarters!=4 && !pcm_source)) { fail("音频缓冲区内存不足");return false; }
    auto *destination=speed_quarters==4 ? pcm.get() : pcm_source.get();
    if (pxa_io(decoder,PXA_VIDEO_IO_AUDIO_READ,reinterpret_cast<uint8_t*>(destination),pcm_used)!=int(pcm_used)) {
        fail("音频帧无效");return false;
    }
    if (speed_quarters!=4) {
        size_t frames=pcm_used/(channels*2);
        size_t converted=pcm_speed.convert(pcm_source.get(),frames,channels,speed_quarters,
            pcm.get(),2*max_pcm_values/channels);
        if(frames&&!converted){fail("音频变速失败");return false;}
        pcm_used=converted*channels*2;
    }
    aac_us+=now_us()-audio_started;
    if (!audio && !audio_token) {
        audio_rate=pxa_load_u32(info);audio_channels=pxa_load_u16(info+4); audio_pts_base=s.pts;
        pcm_packet.configure(audio_rate,audio_channels);
        int result=open_audio();
        if (result) { audio_open_failed(result); if (!playing || audio_output_unavailable) return false; }
    }
    ++ai;audio_drained=false;
    return true;
}
void resume_with_pcm(int32_t error) {
    // Closing the encoded stream drops any accepted but still queued packets.
    // Resume the audio cursor at the playback clock, keeping reordered pictures.
    const uint64_t target=play_started ? position() : discard_before_pts;
    encoded_allowed=false;
    close_audio_output();
    audio_discard_before_pts=target;
    ai=size_t(std::lower_bound(audio_track.samples.begin(),audio_track.samples.end(),target,
        [](const bili::Sample& sample,uint64_t pts){return sample.pts<pts;})-audio_track.samples.begin());
    if (dash_playback && !dash_audio.segments.empty()) {
        const size_t segment=bili::dash_segment_at(dash_audio,target);
        // An in-flight range can keep indexing samples as they arrive.
        if (audio_http.failed || !bili::dash_contains_segment(dash_audio.segments[segment],
                                                               audio_http.start,audio_http.limit)) {
            audio_http.cancel();
            audio_prefetch.discard();
            audio_range_retries=0;
            reset_audio_range();
            dash_audio.next=segment;
        }
    }
    char line[104];
    std::snprintf(line,sizeof(line),"BILI_AUDIO async error=%d, switching to PCM at %llu ms",
        int(error),(unsigned long long)(target/1000));
    pxa_log_write(PXA_LOG_LEVEL_WARN,line);
}
bool pump_encoded_audio(uint64_t media_time) {
    if (!encoded_allowed || speed_quarters!=4 || audio_track.rate<8000 || audio_track.rate>48000)
        return false;
    if (!audio && !audio_token && ai<audio_track.samples.size()) {
        audio_pts_base=audio_track.samples[ai].pts;
        audio_rate=audio_track.rate;
        audio_channels=audio_track.channels;
        pcm_packet.configure(audio_rate,audio_channels);
        int result=open_audio();
        if (result) audio_open_failed(result);
    }
    if (!audio_ready) return true;
    if (!encoded_audio) {
        const auto result=pxa_audio_configure_encoded(audio,PXA_AUDIO_ENCODED_AAC_ADTS);
        if (result) {
            encoded_allowed=false;
            char line[88];
            std::snprintf(line,sizeof(line),"BILI_AUDIO using synchronous PCM, configure_result=%d",int(result));
            pxa_log_write(PXA_LOG_LEVEL_INFO,line);
            return false;
        }
        encoded_audio=true;
        pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_AUDIO using Host AAC queue");
    }
    // Open the mixer first, then hold the first AAC batch until the reordered
    // video picture is ready. Network refill after a seek can take seconds.
    if (dash_playback && !play_started && !frame_pending && !video_eof) return true;
    if (ai==audio_track.samples.size() && dash_audio_end()) {
        if (!encoded_finishing) {
            const auto result=pxa_audio_finish_encoded(audio);
            if (result) { resume_with_pcm(result);return true; }
            encoded_finishing=true;
        }
        if (!encoded_drained) {
            uint32_t flags=0;
            const auto result=pxa_audio_encoded_state(audio,&flags);
            if (result) { resume_with_pcm(result);return true; }
            if (flags&PXA_AUDIO_ENCODED_DRAINED) {
                encoded_drained=true;
                // A query queued before the worker's last PCM write may
                // report zero samples. Wait for a fresh mixer snapshot.
                cancel(query_token);
                next_query=0;
            }
        }
        return true;
    }
    // Refill from 600 to 750 ms ahead in batches. A previously blocked batch
    // already owns its input bytes and can be retried as soon as the Host drains.
    if (ai==audio_track.samples.size() ||
        (!encoded_batch_count && audio_track.samples[ai].pts>media_time+600000)) return true;
    const auto now=now_ms();
    if (now<encoded_retry_at) return true;
    if (!encoded_batch_count) {
        encoded_batch.assign(4,0);
        unsigned count=0;
        for (; count<PXA_AUDIO_ENCODED_BATCH_MAX && ai+count<audio_track.samples.size(); ++count) {
            const auto &s=audio_track.samples[ai+count];
            if (s.pts>media_time+750000) break;
            if (s.size+7>PXA_AUDIO_ENCODED_PACKET_BYTES) {
                if (count) break;
                resume_with_pcm(PXA_STATUS_LIMIT_EXCEEDED);return true;
            }
            if (encoded_batch.size()+s.size+17>PXA_AUDIO_ENCODED_BATCH_BYTES) break;
            const auto *sample=cached(s,true);
            if (!sample) break;
            const auto at=encoded_batch.size();
            encoded_batch.resize(at+17+s.size);
            auto *p=encoded_batch.data()+at;
            pxa_store_u64(p,s.pts);
            pxa_store_u16(p+8,uint16_t(s.size+7));
            write_adts(p+10,sample,s.size);
        }
        if (!playing || !count) return true;
        pxa_store_u16(encoded_batch.data(),uint16_t(count));
        encoded_batch_count=count;
        ++encoded_batches_built;
    }
    const auto result=pxa_audio_write_encoded(audio,encoded_batch.data(),encoded_batch.size());
    if (result==int(encoded_batch.size())) {
        ai+=encoded_batch_count;
        ++encoded_batches_written;
        reset_encoded_batch();
        audio_drained=false;
    } else if (result==PXA_STATUS_WOULD_BLOCK) {
        // A full Host queue still has playable audio. Let the worker drain it
        // for one clock tick and retry the same ADTS batch across video events.
        encoded_retry_at=now+PXA_CLOCK_MIN_PERIOD_MS;
        ++encoded_batches_blocked;
    } else resume_with_pcm(result);
    return true;
}
void accept_frame(const pxa_video_frame_t& f) {
    if (f.width!=video.width || f.height!=video.height) { fail("视频尺寸发生变化，请重新播放"); return; }
    decoded_us+=f.decode_us;++decoded_frames;
    if(f.pts_us<discard_before_pts)return;
    if (host_surface || mapped_surface) { video_pts=f.pts_us; frame_pending=true; mapped_filled=false; return; }
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
void present_frame() {
    pxa_video_frame_t f{};
    const int result=pxa_video_frame_info(decoder,&f);
    if (!result) accept_frame(f);
    else if (result!=PXA_STATUS_WOULD_BLOCK) fail("无法读取解码画面");
}
bool prepare_frame() {
    if (host_surface) {
        if (mapped_filled) return true;
        if (surface_retry_at && now_ms()<surface_retry_at) return false;
        const uint64_t start=now_us();
        const int result=pxa_video_prepare_surface(decoder);
        video_read_us+=now_us()-start;
        if (result==PXA_STATUS_WOULD_BLOCK) {
            ++surface_blocks;
            // A Host-owned slot can become writable as soon as the display
            // acquires its replacement. Coalesce retries from the same event.
            surface_retry_at=now_ms()+2;
            return false;
        }
        if (result) { fail("无法准备视频画面"); return false; }
        surface_retry_at=0;
        mapped_filled=true;
        return true;
    }
    if (!mapped_filled) {
        if (mapped_index<0) {
            if (surface_retry_at && now_ms()<surface_retry_at) return false;
            uint8_t index=0;
            int n=pxa_surface_acquire_buffer(surface,&index);
            if (n==PXA_STATUS_WOULD_BLOCK) {
                ++surface_blocks;
                // RELEASED normally wakes us first. Keep a bounded retry if
                // this Host delivers an older release behind other events.
                surface_retry_at=now_ms()+16;
                return false;
            }
            if (n!=4 || index>=mapped_buffer_count) { fail("无法取得视频显示缓冲区"); return false; }
            surface_retry_at=0;
            mapped_index=index;
        }
        uint64_t reading=now_us();
        uint8_t *pixels=mapped_pixels+size_t(mapped_index)*output_w*output_h*2;
        const unsigned top=output_h==surface_h ? 0 : surface_y;
        const unsigned left=mapped_strided ? surface_x : 0;
        const size_t stride=(mapped_strided ? output_w : surface_w)*2;
        const size_t bytes=size_t(surface_h-1)*stride+surface_w*2;
        int result=pxa_video_read_scaled_pixels(decoder,pixels+size_t(top)*output_w*2+left*2,bytes);
        video_read_us+=now_us()-reading;
        if (result) { fail("画面缩放失败"); return false; }
        if (!mapped_strided && output_w>surface_w) {
            const uint64_t started=now_us();
            bili::pad_rgb565_rows(pixels,output_w,surface_w,surface_h,surface_x);
            scaled_us+=now_us()-started;
        }
        mapped_filled=true;
    }
    return true;
}
bool show_frame() {
    if (!frame_pending) return true;
    // Once the picture is in a Surface buffer, the next decode may reuse
    // the Host's packed output while this picture waits for its PTS.
    if (async_decode && async_packets && (host_surface || mapped_surface) && !prepare_frame()) return false;
    if (!play_started) {
        if (!paused && audio_enabled() && !audio_track.samples.empty()) return false;
        play_started=now_ms();play_offset=video_pts;
    }
    if (!paused) {
        const uint64_t clock=bili::media_position(play_started,play_offset,now_ms(),speed_quarters);
        // Mapped presents reach the panel on a subsequent refresh. Submit up
        // to one refresh early so the decoder can start the next access unit.
        const uint64_t lead=(host_surface || mapped_surface) && frame_id && !seeking ? 15000u*speed_quarters/4 : 0;
        if (clock<video_pts && video_pts-clock>lead) return false;
    }
    if (host_surface || mapped_surface) {
        if (surface_retry_at && host_surface && ordered_surface && now_ms()<surface_retry_at) return false;
        if (!prepare_frame()) return false;
        uint64_t displaying=now_us();
        int n;
        if (host_surface) {
            if (ordered_surface) {
                n=pxa_video_present_surface_ordered(decoder,frame_id+1);
                if (n==PXA_STATUS_UNSUPPORTED) ordered_surface=false;
            }
            if (!ordered_surface) n=pxa_video_present_surface(decoder,frame_id+1);
        } else n=pxa_surface_present_buffer(surface,uint8_t(mapped_index),frame_id+1);
        display_us+=now_us()-displaying;
        if (n==PXA_STATUS_WOULD_BLOCK) {
            ++surface_blocks;
            // HTTP and decoder events can arrive repeatedly within one display refresh.
            if (host_surface && ordered_surface) surface_retry_at=now_ms()+2;
            return false;
        }
        surface_retry_at=0;
        if (n!=(host_surface ? 0 : 16)) { fail("视频提交失败"); return false; }
        ++frame_id; displayed_pts=video_pts; mapped_index=-1; frame_pending=false; seeking=false;
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
    displayed_pts=video_pts;frame_pending=false;seeking=false; return true;
}
bool submit_video(uint64_t pts, bool drain,const bili::CachedAvcPacket& packet={}) {
    auto submit=[&](auto import) {
        return packet.submit([&](uint8_t *bytes,size_t count) {
            return import(decoder,bytes,uint32_t(count),pts,drain);
        },compressed);
    };
    if (async_decode && (!async_retry_at || now_ms()>=async_retry_at)) {
        int status=PXA_STATUS_UNSUPPORTED;
        const bool unprepared=host_surface && frame_pending && !mapped_filled;
        if (fuse_surface && unprepared) {
            const auto started=now_us();
            status=submit(pxa_video_prepare_and_queue);
            video_read_us+=now_us()-started;
            if (!status) {
                mapped_filled=true;
                surface_retry_at=0;
                ++fused_frames;
            } else if (status==PXA_STATUS_UNSUPPORTED) {
                fuse_surface=false;
            } else if (status!=PXA_STATUS_WOULD_BLOCK && status!=PXA_STATUS_RESOURCE_LIMIT) {
                fail("无法准备视频画面");return false;
            }
        }
        if (status) {
            if (unprepared && !prepare_frame()) return false;
            const auto started=now_us();
            status=submit(pxa_video_queue_decode);
            async_submit_us+=now_us()-started;
        }
        if (!status) {
            if (!async_packets++ || async_retry_at) pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_VIDEO using Host decode queue");
            async_retry_at=0;
            async_overlap+=frame_pending;
            async_pending=true;async_drain=drain;
            return true;
        }
        if (status==PXA_STATUS_UNSUPPORTED) {
            async_decode=false;
            pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_VIDEO using synchronous decode");
        } else if (status==PXA_STATUS_RESOURCE_LIMIT) {
            // Large access units can exceed the queue's memory budget while
            // DASH caches are full. Retry once the current ranges can drain.
            async_retry_at=now_ms()+1000;
            pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_VIDEO queue memory limit; using synchronous decode temporarily");
        } else {
            fail("无法排入 H.264 解码队列");return false;
        }
    }
    if (frame_pending) return false;
    if (submit(pxa_video_decode)) {
        fail(drain ? "视频尾帧解码失败" : "H.264 解码失败，请降低画质");return false;
    }
    present_frame();
    if (!playing) return false;
    if (drain && !frame_pending) video_eof=true;
    show_frame();
    return playing;
}
void pump() {
    uint64_t net_started=now_us();
    const auto now=now_ms();
    if (playurl_retry_at && now>=playurl_retry_at) {
        playurl_retry_at=0;
        request_playurl(video_cid);
    }
    http.pump_with_idle(now,job==Job::dash_video ? dash_media_idle_ms : 0);
    audio_http.pump_with_idle(now,dash_playback ? dash_media_idle_ms : 0);
    for (auto* prefetched:{&audio_prefetch,&video_prefetch}) {
        auto& response=prefetched->response;
        response.pump_with_idle(now,dash_prefetch_idle_ms);
        if (response.done && (response.failed || response.used!=response.limit)) {
            prefetched->discard();prefetched->retry_at=now+4000;
        }
    }
    net_us+=now_us()-net_started;
    if (audio_http.done && !(dash_playback && audio_http.defer_retry(now,audio_range_retries))) { audio_http.done=false;
        if (audio_http.failed || (dash_playback && audio_http.used!=audio_http.limit)) {
            if (!dash_playback || audio_range_retries>=bili::Http::max_range_retries || !retry_dash_audio()) { fail("音轨网络请求失败，请重新播放"); return; }
        } else {
            audio_cache_offset=audio_http.start;
            if (dash_playback) {
                if (!index_audio_range()) {
                    fail("DASH 音轨分段格式无效");return;
                }
                if ((audio_range_retries%2) && !dash_audio.backup_url.empty()) dash_audio.url.swap(dash_audio.backup_url);
                audio_range_retries=0;
                if (dash_opening) fetch_dash(false);
            }
        }
    }
    if (dash_playback && audio_http.active && !index_audio_range()) {
        fail("DASH 音轨分段格式无效");return;
    }
    if (http.done && !(job==Job::dash_video && http.defer_retry(now,video_range_retries))) {
        http.done=false; complete_http();
    }
    if ((playing || dash_opening) && http.active && job==Job::dash_video) {
        if (!index_video_range()) {fail("DASH 视频分段格式无效");return;}
        try_open_dash_video();
    }
    if (surface && hud && player_pointer<0 && !paused && !seeking && !buffering && playing && now_ms()>hud_until) show_hud(false);
    if (surface && hud && now_ms()>hud_tick) {dirty=true;hud_tick=now_ms()+500;}
    if (!playing || (paused && !seeking) || dragging || !decoder || !surface ||
        (audio_prompt && audio_track.audio)) return;
    if(resume_pending) {
        if (!dash_playback) {
            size_t index=bili::keyframe(video,resume_pending);
            if(index>=video.samples.size() || !cached(video.samples[index],false))return;
        }
        uint64_t target=resume_pending;resume_pending=0;seek(target);return;
    }
    if (!paused && play_started && video_pts>1000000) {
        const uint64_t now=now_ms();
        // The internal flash write can stall the CPU while it is fetching
        // decoder code. Avoid writes at startup and after advancing the clock
        // by seeking. Pause, stop and completion still save the current point.
        if (!next_history) next_history=now+history_checkpoint_interval_ms;
        else if (now>=next_history) {
            client.record(video_pts);
            next_history=now+history_checkpoint_interval_ms;
        }
    }
    if (now_ms()-stats_at>=5000) {
        char line[180]; std::snprintf(line,sizeof(line),"BILI shown=%llu decoded=%llu skipped=%llu decode_mean_us=%llu video_pts_ms=%llu audio_next=%zu",
            (unsigned long long)frame_id,(unsigned long long)decoded_frames,(unsigned long long)skipped_frames,
            (unsigned long long)(decoded_frames ? decoded_us/decoded_frames : 0),(unsigned long long)(video_pts/1000),ai);
        pxa_log_write(PXA_LOG_LEVEL_INFO,line); stats_at=now_ms();
        std::snprintf(line,sizeof(line),"BILI_TIME scale_ms=%llu display_ms=%llu read_ms=%llu aac_ms=%llu net_ms=%llu",
            (unsigned long long)(scaled_us/1000),(unsigned long long)(display_us/1000),(unsigned long long)(video_read_us/1000),
            (unsigned long long)(aac_us/1000),(unsigned long long)(net_us/1000));
        pxa_log_write(PXA_LOG_LEVEL_INFO,line);
        if (encoded_batches_built) {
            std::snprintf(line,sizeof(line),"BILI_AAC_BATCH built=%u written=%u blocked=%u",
                encoded_batches_built,encoded_batches_written,encoded_batches_blocked);
            pxa_log_write(PXA_LOG_LEVEL_INFO,line);
        }
        if (video_inputs) {
            std::snprintf(line,sizeof(line),"BILI_H264_INPUT total=%llu non_reference=%llu dropped_non_reference=%llu",
                (unsigned long long)video_inputs,(unsigned long long)video_disposable_inputs,
                (unsigned long long)video_disposable_skips);
            pxa_log_write(PXA_LOG_LEVEL_INFO,line);
        }
        if (async_packets || surface_blocks) {
            std::snprintf(line,sizeof(line),
                "BILI_VIDEO_QUEUE queued=%llu overlap=%llu submit_us=%llu surface_blocks=%llu fused=%llu",
                (unsigned long long)async_packets,(unsigned long long)async_overlap,
                (unsigned long long)async_submit_us,(unsigned long long)surface_blocks,(unsigned long long)fused_frames);
            pxa_log_write(PXA_LOG_LEVEL_INFO,line);
        }
        std::snprintf(line,sizeof(line),"BILI_CLOCK media_ms=%llu video_ms=%llu lag_ms=%lld",
            (unsigned long long)(position()/1000),(unsigned long long)(video_pts/1000),
            (long long)(int64_t(position()/1000)-int64_t(video_pts/1000)));pxa_log_write(PXA_LOG_LEVEL_INFO,line);
    }
    if (audio_ready && now_ms()>=next_query && !query_token) {
        query_token=++serial; next_query=now_ms()+250; pxa_audio_query(query_token,audio);
    }
    bool pcm_free=pump_pcm(); if (!playing) return;
    show_frame(); if (!playing) return;
    if (!paused && ai>=audio_track.samples.size() && !dash_audio_end()) {
        fetch_dash(true);
        // Give the first audio range time to arrive before allocating video
        // reference frames alongside the Host's PCM session.
        if (!playing || (!play_started && audio_track.audio)) return;
    }
    const uint64_t media_time=play_started
        ? bili::media_position(play_started,play_offset,now_ms(),speed_quarters) : discard_before_pts;
    prefetch_dash(media_time);
    if (!paused && audio_enabled() && pump_encoded_audio(media_time)) {
        if (!playing) return;
        // Once the DASH audio session exists, decode through the H.264 reorder
        // delay while its clock is still stopped. show_frame waits for audio.
        if (!play_started && audio_enabled() && (audio_token || audio) &&
            !(dash_playback && encoded_audio)) return;
    } else {
        // Older Hosts and speed changes use synchronous PCM.
        const auto audio_started=now_us();
        for (unsigned packets=0; packets<3 && pcm_free && !paused && audio_enabled() &&
            ai<audio_track.samples.size() &&
            (audio_track.samples[ai].pts<=media_time+450000 || vi>=video.samples.size()); ++packets) {
            if (!decode_audio_sample()) return;
            pcm_free=pump_pcm();
            if (!playing) return;
            if (now_us()-audio_started>=8000) break;
        }
        const uint64_t audio_guard_us=(pcm_open.buffered ? 360000ULL : 160000ULL)*speed_quarters/4;
        if (audio_ready && pcm_free && ai<audio_track.samples.size() &&
            audio_track.samples[ai].pts<media_time+audio_guard_us) return;
    }
    bool prepare_with_next=false;
    if (async_pending) {
        if (frame_pending) return;
        pxa_video_frame_t completed{};
        int status=PXA_STATUS_UNSUPPORTED;
        if (poll_frame_supported) {
            status=pxa_video_poll_frame(decoder,&completed);
            if (status==PXA_STATUS_UNSUPPORTED) poll_frame_supported=false;
        }
        if (!poll_frame_supported) status=pxa_video_poll_decode(decoder);
        if (status==PXA_STATUS_WOULD_BLOCK) return;
        if (status) { fail("H.264 解码失败，请降低画质");return; }
        async_pending=false;
        if (!poll_frame_supported) present_frame();
        else if (completed.frame_bytes) accept_frame(completed);
        if (!playing) return;
        if (async_drain && !frame_pending) video_eof=true;
        prepare_with_next=fuse_surface && host_surface && frame_pending && !seeking &&
            !async_drain && vi<video.samples.size() && !async_retry_at;
        if (!prepare_with_next) show_frame();
        if (!playing) return;
    }
    if (frame_pending && !prepare_with_next && !(async_decode && (host_surface || mapped_surface) && mapped_filled)) return;
    if ((!prepare_with_next && catch_up_dash(media_time)) || !playing) return;
    if (vi<video.samples.size()) {
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
        // Discard several cheap, late non-reference samples in one turn.
        // Retain H.264 decode order while the Host overlaps the queued call.
        for (unsigned scanned=0; scanned<6 && vi<video.samples.size(); ++scanned) {
            const auto& sample=video.samples[vi];
            const uint8_t* data=cached(sample);
            if (!data) {
                if (prepare_with_next) show_frame();
                return;
            }
            if (dash_playback && http.active && !range_started_early) {
                range_started_early=true;
                buffering=false;
                if (hud) dirty=true;
                char line[120];
                std::snprintf(line,sizeof(line),"BILI_VIDEO_STREAMING received=%zu total=%zu pts_ms=%llu",
                    http.used,http.limit,(unsigned long long)(sample.pts/1000));
                pxa_log_write(PXA_LOG_LEVEL_INFO,line);
            }
            bool non_reference=false;
            const bool late=!seeking && media_time>sample.pts+100000;
            bili::CachedAvcPacket packet;
            static_assert(bili::Http::headroom>=bili::CachedAvcPacket::header_bytes);
            // cached() has verified that this complete DASH sample is in http.
            const bool borrowed=dash_playback && video.nal_bytes==4 &&
                packet.bind({http.data()+(sample.offset-http.start)-packet.header_bytes,
                             packet.header_bytes+sample.size},capability.max_packet_bytes,non_reference);
            if (!borrowed && !bili::annexb({data,sample.size},video.nal_bytes,compressed,
                                          capability.max_packet_bytes,&non_reference,late,scan_nal4)) {
                fail("AVC 帧格式无效或过大");return;
            }
            if (late && non_reference) {
                ++vi;
                ++skipped_frames;++video_disposable_skips;
                continue;
            }
            if (submit_video(sample.pts,false,packet)) {
                ++vi;++video_inputs;
                video_disposable_inputs+=non_reference;
            }
            if (prepare_with_next && playing) show_frame();
            return;
        }
        if (prepare_with_next) show_frame();
    } else {
        if (dash_playback && continue_video_page()) return;
        if (dash_playback && (http.active || dash_video.next<dash_video.segments.size())) {
            if (!http.active) {
                buffering=true;
                if (fetch_dash(false) && hud) dirty=true;
            }
            return;
        }
        if (frame_pending) return;
        if (!video_eof) {
            compressed.resize(16);
            submit_video(0,true);
            if (!playing || !video_eof) return;
        }
        if (pcm_used || !dash_audio_end()) return;
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
void pump_danmaku() {
    if (!dm_enabled || !playing || !frame_id || seeking || !video_cid) return;
    const uint32_t media_ms=uint32_t(displayed_pts/1000);
    const uint32_t segment=media_ms/bili::danmaku_segment_ms+1;
    dm_http.pump(now_ms());
    if (dm_http.done) {
        dm_http.done=false;
        if (!dm_http.failed && dm_requested==segment &&
            dm_segment.parse({dm_http.data(),dm_http.used},segment)) {
            dm_loaded=segment;
            reset_danmaku_timeline(media_ms>450 ? media_ms-450 : 0);
            char line[96];std::snprintf(line,sizeof(line),"BILI_DANMAKU segment=%u kept=%zu",segment,dm_segment.size);
            pxa_log_write(PXA_LOG_LEVEL_INFO,line);
        } else {
            dm_loaded=0;
            dm_lanes.fill(nullptr);
            if (dm_http.failed && (!dm_http.status || dm_http.status==429 || dm_http.status>=500)) {
                dm_requested=0;
                dm_retry_at=now_ms()+6000;
            }
            pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_DANMAKU segment unavailable");
        }
        dm_http.release_body();
    }
    if (segment!=dm_requested && !paused && !http.active && !audio_http.active &&
        !video_prefetch.response.active && !audio_prefetch.response.active && now_ms()>=dm_retry_at) {
        char url[160];
        std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/v2/dm/web/seg.so?type=1&oid=%llu&segment_index=%u",
            (unsigned long long)video_cid,segment);
        dm_requested=segment;
        if (dm_http.begin(++serial,url,net_permission,now_ms(),512*1024,false,0,nullptr,nullptr,4500))
            dm_http.deadline=now_ms()+4500;
        else {
            dm_http.cancel(); dm_http.release_body();
            dm_requested=0; dm_retry_at=now_ms()+6000;
        }
    }
    if (dm_loaded!=segment) {
        dm_lanes.fill(nullptr);
        return;
    }
    for (auto& lane:dm_lanes) {
        if (lane && (lane->time_ms>media_ms || media_ms-lane->time_ms>=bili::danmaku_lifetime_ms))
            lane=nullptr;
    }
    while (dm_cursor<dm_segment.size && dm_segment.entries[dm_cursor].time_ms<=media_ms) {
        const auto& item=dm_segment.entries[dm_cursor++];
        if (media_ms-item.time_ms>800) continue;
        auto slot=std::find(dm_lanes.begin(),dm_lanes.end(),nullptr);
        if (slot!=dm_lanes.end()) *slot=&item;
    }
}
void present_danmaku() {
    constexpr uint32_t node=31;
    const bool showing=dm_enabled && surface && playing && !seeking && !dragging &&
        std::any_of(dm_lanes.begin(),dm_lanes.end(),[](auto* lane){return lane!=nullptr;});
    if (dm_ui_generation!=scene.ui_generation) dm_canvas_visible=false;
    if (!scene.initialized || (!showing && !dm_canvas_visible) || now_ms()<dm_next_paint) return;
    const auto to_dp=[](unsigned pixels){return int(uint64_t(pixels)*65536/scene.density);};
    const auto& viewport=scene.viewport.content;
    const int left_edge=int((uint64_t(surface_x)*65536+scene.density-1)/scene.density);
    const int x=std::max(left_edge,viewport.x);
    const int y=std::max(to_dp(surface_y)+4,viewport.y+(hud?player_header_dp+2:4));
    const int width=std::min(to_dp(surface_x+surface_w),viewport.x+viewport.w)-x;
    const int height=bili::danmaku_lanes*bili::danmaku_line_dp+1;
    const bool fits=showing && width>20 && y+height<=to_dp(surface_y+surface_h);
    if (!fits || !dm_canvas_visible || x!=dm_x || y!=dm_y || width!=dm_width) {
        if (!fits && !dm_canvas_visible) return;
        pxa_ui_transaction_t tx{};
        bool ok=pxa_ui_transaction_begin(&tx,++scene.generation,PXA_UI_TRANSACTION_PATCH,control,sizeof(control));
        const char* error="transaction begin";
        if (ok && !fits) ok=pxa_ui_remove(&tx,node);
        if (ok && fits) {
            error="canvas geometry";
            if (!dm_canvas_visible) ok=pxa_ui_create(&tx,node,1,0,PXA_UI_NODE_CANVAS) &&
                pxa_ui_set_u8(&tx,node,PXA_UI_PROPERTY_COMPOSITION,PXA_UI_COMPOSITION_ALPHA_OVERLAY) &&
                pxa_ui_set_u8(&tx,node,PXA_UI_PROPERTY_POSITION,1) &&
                pxa_ui_set_rgba(&tx,node,PXA_UI_PROPERTY_BACKGROUND,0);
            ok=ok && pxa_ui_set_length(&tx,node,PXA_UI_PROPERTY_X,PXA_UI_LENGTH_PX,x) &&
                pxa_ui_set_length(&tx,node,PXA_UI_PROPERTY_Y,PXA_UI_LENGTH_PX,y) &&
                pxa_ui_set_length(&tx,node,PXA_UI_PROPERTY_WIDTH,PXA_UI_LENGTH_PX,width) &&
                pxa_ui_set_length(&tx,node,PXA_UI_PROPERTY_HEIGHT,PXA_UI_LENGTH_PX,height);
        }
        if (ok) { error="canvas commit"; ok=pxa_ui_transaction_commit(&tx); }
        if (!ok) {
            if (tx.active) pxa_ui_transaction_cancel(&tx);
            if (!dm_layer_error) {
                char line[90];std::snprintf(line,sizeof(line),"BILI_DANMAKU overlay failed at %s",error);
                pxa_log_write(PXA_LOG_LEVEL_WARN,line);
            }
            dm_layer_error=true;
            dm_next_paint=now_ms()+500;
            return;
        }
        dm_ui_generation=scene.ui_generation=scene.generation;
        dm_canvas_visible=fits;
        dm_x=x;dm_y=y;dm_width=width;
        dm_painted_ms=UINT32_MAX;
        if (!fits) return;
    }
    const uint32_t media_ms=uint32_t(displayed_pts/1000);
    if (media_ms==dm_painted_ms || now_ms()<dm_next_paint) return;
    uint8_t commands[1024];
    pxa_canvas_frame_t canvas{};
    pxa_canvas_begin(&canvas,commands,sizeof(commands));
    for (size_t i=0;i<dm_lanes.size();++i) {
        const auto* item=dm_lanes[i];
        if (!item || item->time_ms>media_ms) continue;
        const int estimated_width=int(item->chars)*16+6;
        const int text_width=item->mode==5 ? std::min(width,estimated_width) : estimated_width;
        const int left=bili::danmaku_left(item->mode,width,text_width,media_ms-item->time_ms);
        const int row=int(i*bili::danmaku_line_dp);
        const auto length=std::strlen(item->text);
        const uint8_t alignment=item->mode==5 ? PXA_CANVAS_ALIGN_CENTER : PXA_CANVAS_ALIGN_LEFT;
        pxa_canvas_clip_push(&canvas,0,row,width,bili::danmaku_line_dp);
        pxa_canvas_text_role(&canvas,left+1,row+1,text_width,pxa_canvas_rgba(0x121212),0,alignment,item->text,length);
        pxa_canvas_text_role(&canvas,left,row,text_width,pxa_canvas_rgba(item->color),0,alignment,item->text,length);
        pxa_canvas_clip_pop(&canvas);
    }
    uint8_t initialized=1;
    if (pxa_canvas_present_regions(node,&canvas,&scene.generation,&initialized,0,control,sizeof(control),
        control,sizeof(control),nullptr,0)) {
        dm_painted_ms=media_ms;
        if (!dm_paint_logged) {
            pxa_log_write(PXA_LOG_LEVEL_INFO,"BILI_DANMAKU overlay submitted");
            dm_paint_logged=true;
        }
    } else if (!dm_layer_error) {
        dm_layer_error=true;
        pxa_log_write(PXA_LOG_LEVEL_WARN,"BILI_DANMAKU overlay submit failed");
    }
    dm_next_paint=now_ms()+90;
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
        const auto& viewport=scene.viewport;
        const int bottom=std::max(0,viewport.content.y+h-player_controls_dp);
        if (scene.content_clip) scene.unclip();
        pxa_canvas_rect(&scene.frame,0,0,viewport.width,viewport.content.y+player_header_dp,0x101219,0);
        pxa_canvas_rect(&scene.frame,0,bottom,viewport.width,viewport.height-bottom,0x101219,0);
        if (scene.content_clip) scene.clip(0,0,w,h);
        bili::ui::icon(scene,"back",23,player_header_dp/2,0xffffff);scene.hit(21,2,1,44,player_header_dp-2);
        char hint[96];
        if(buffering && file_bytes)std::snprintf(hint,sizeof(hint),"缓冲 %u%%",unsigned(std::min<uint64_t>(100,http.used*100/file_bytes)));
        else std::snprintf(hint,sizeof(hint),"%s",seeking?"正在定位…":buffering?"正在缓冲…":paused?"已暂停":ended?"播放结束":audio_output_unavailable?"音频不可用 · 无声播放":"");
        scene.text(48,*hint?10:18,w-114,title,0xffffff);
        if (*hint) scene.text(48,28,w-114,hint,0xb3bac9);
        char speed[24];std::snprintf(speed,sizeof(speed),"%g×",double(speed_quarters)/4);
        bili::ui::label(scene,w-63,0,56,player_header_dp,speed,0xffffff,1);scene.hit(24,w-66,0,66,player_header_dp);
        char current[24],total[24];time_text(position(),current,sizeof(current));time_text(duration_us,total,sizeof(total));
        int center=w/2;
        bili::ui::label(scene,16,h-96,66,26,current,0xffffff);bili::ui::label(scene,w-82,h-96,66,26,total,0xb3bac9,2);
        scene.rect(center-33,h-94,66,22,dm_enabled?bili::ui::colors.primary:0x262d38,11);
        bili::ui::label(scene,center-33,h-94,66,22,dm_enabled?"弹幕开":"弹幕关",dm_enabled?bili::ui::colors.on_primary:0xe4e9f0,1);
        scene.hit(25,center-36,h-96,72,26);
        int x=20+int(duration_us ? uint64_t(std::max(1,w-40))*position()/duration_us : 0);
        scene.line(20,h-63,w-20,h-63,0x414655,3);scene.line(20,h-63,x,h-63,bili::ui::colors.primary,3);
        scene.circle(x,h-63,5,bili::ui::colors.primary);scene.hit(40,12,h-79,w-24,32);
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
    else if (id==25) {
        dm_enabled=!dm_enabled;
        if (!dm_enabled) {
            defer_danmaku();
            dm_lanes.fill(nullptr);
        } else {
            dm_requested=dm_loaded;
            dm_retry_at=0;
            reset_danmaku_timeline(uint32_t(displayed_pts/1000));
        }
        show_hud(true);dirty=true;
        pxa_log_write(PXA_LOG_LEVEL_INFO,dm_enabled?"BILI_DANMAKU enabled":"BILI_DANMAKU disabled");
    }
    else if(id==24){static constexpr unsigned rates[]={4,5,6,8,3,2};unsigned next=4;for(unsigned i=0;i<6;++i)if(rates[i]==speed_quarters)next=rates[(i+1)%6];
        bool resume=playing&&!paused&&!ended;if(resume)pause_playback(true);speed_quarters=next;pcm_speed.reset();
        play_offset=pause_pts;play_started=pause_started=now_ms();if(resume)pause_playback(false);show_hud(true);dirty=true;}
    else if (id==22 || id==23) {
        uint64_t current=position();seek(id==22 ? (current>10000000?current-10000000:0) : std::min(duration_us,current+10000000),paused,true);
    }
}
void pointer(const pxa_ui_pointer_data_t& p) {
    int x=p.x-scene.viewport.content.x,y=p.y-scene.viewport.content.y;
    if (!surface){
        if(client.in_player && p.phase==PXA_POINTER_UP && scene.find(x,y)==103){stop();return;}
        client.pointer(scene,p);return;
    }
    if (p.phase==PXA_POINTER_DOWN) {
        if (player_pointer>=0) return;
        player_pointer=p.pointer_id;
        if (!hud){show_hud(true);consume_up=true;return;}
    } else {
        if (p.pointer_id!=player_pointer) return;
        if (p.phase==PXA_POINTER_UP || p.phase==PXA_POINTER_CANCEL) {
            player_pointer=-1;
            hud_until=now_ms()+player_hud_ms;
        }
    }
    if (consume_up){if(p.phase==PXA_POINTER_UP || p.phase==PXA_POINTER_CANCEL)consume_up=false;return;}
    if (p.phase==PXA_POINTER_DOWN && hud && scene.find(x,y)==40) {
        drag_paused=paused;
        drag_pts=position();
        // While locating a frame the pump already waits for its media.
        // Keep that audio session alive across repeated touches on the bar.
        if (!seeking) pause_playback(true,false);
        dragging=true;
    }
    if (dragging) {
        int width=std::max(1,scene.w-40);drag_pts=duration_us*unsigned(std::clamp(x-20,0,width))/unsigned(width);dirty=true;
        if (p.phase==PXA_POINTER_UP){uint64_t target=drag_pts;dragging=false;seek(target,drag_paused,true);}
        else if (p.phase==PXA_POINTER_CANCEL){dragging=false;pause_playback(drag_paused);}
    } else if (p.phase==PXA_POINTER_UP) {
        unsigned id=scene.find(x,y);if(id)click(id);else show_hud(false);
    }
}

}
extern "C" PXA_CPU_FEATURE_CALLBACK void pxa_app_set_cpu_features(uint32_t mask) {
    scan_nal4=(mask&PXA_CPU_FEATURE_RV32_ZBB) ? billy_scan_nal4_zbb : bili::scan_nal4;
}
extern "C" int32_t pxa_app_start(const uint8_t *data,uint32_t size) {
    pxa_ui_environment_t e{}; if (pxa_ui_parse_start_environment(data,size,&e)) scene.environment(e);
    pxa_window_request_snapshot(3); pxa_clock_set_period(PXA_CLOCK_MIN_PERIOD_MS); pxa_video_request_capability(2,0);
    browser_window();pxa_ui_theme_get(5);client.start();render();return 0;
}
extern "C" int32_t pxa_app_on_event(const uint8_t *data,uint32_t size) {
    pxa_event_t e{}; if (!pxa_parse_event(data,size,&e)) return PXA_EVENT_UNHANDLED;
    if (surface_retry_at && e.service==PXA_SURFACE_SERVICE && e.opcode==PXA_SURFACE_RELEASED) {
        pxa_surface_released_t released{};
        if (pxa_surface_parse_released(&e,&released) && released.handle==surface) surface_retry_at=0;
    }
    pxa_ui_theme_t theme{};
    if(pxa_ui_parse_theme_event(&e,&theme)){bili::ui::theme(theme);scene.invalidate();client.dirty=dirty=true;if(!surface)browser_window();}
    bool client_event=client.event(e);
    if (!client_event && e.service==PXA_NET_SERVICE && e.opcode==PXA_HTTP_OPEN) {
        if (e.token==audio_http.token) audio_http.event(e);
        else if (e.token==video_prefetch.response.token) video_prefetch.response.event(e);
        else if (e.token==audio_prefetch.response.token) audio_prefetch.response.event(e);
        else if (e.token==dm_http.token) dm_http.event(e);
        else http.event(e);
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
            char line[180];std::snprintf(line,sizeof(line),"BILI_STREAM bvid=%s size=%ux%u profile=%u level=%u headers_status=%d qn=%u format=%s audio=%d",
                bvid,video.width,video.height,profile,level,int(result),qn,dash_playback?"dash":"mp4",audio_track.audio);
            pxa_log_write(PXA_LOG_LEVEL_INFO,line);
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
        if (sr.status && host_surface) {
            if (mapped_buffer_count==3 && sr.status==PXA_STATUS_RESOURCE_LIMIT) create_surface(false);
            else { host_surface=false; create_surface(); }
        } else if (sr.status && mapped_surface) {
            if (mapped_buffer_count==2) mapped_surface=false;
            mapped_pixels=nullptr;
            mapped_storage.reset();
            create_surface(false);
        }
        else if (sr.status) fail("无法分配视频画面");
        else { surface=sr.handle; uint32_t bytes=0;
            if (host_surface) {
                const uint16_t x=output_w==surface_w ? 0 : surface_x;
                const uint16_t y=output_h==surface_h ? 0 : surface_y;
                if (pxa_video_bind_surface(decoder,surface,x,y,surface_w,surface_h)) {
                    close(surface); host_surface=false; create_surface();
                    return PXA_EVENT_HANDLED;
                }
                char line[128];
                std::snprintf(line,sizeof(line),"BILI_SURFACE host=1 video=%ux%u output=%ux%u origin=%u,%u buffers=%u",
                    surface_w,surface_h,output_w,output_h,unsigned(x),unsigned(y),unsigned(mapped_buffer_count));
                pxa_log_write(PXA_LOG_LEVEL_INFO,line);
            } else if (mapped_surface) {
                const size_t frame_bytes=size_t(output_w)*output_h*2;
                const size_t total=frame_bytes*mapped_buffer_count;
                std::memset(mapped_pixels,0,total);
                if (pxa_surface_register_buffers(surface,mapped_pixels,frame_bytes,mapped_buffer_count)!=int(total)) {
                    if (mapped_buffer_count==3) {
                        close(surface); mapped_pixels=nullptr; mapped_storage.reset();
                        create_surface(false); return PXA_EVENT_HANDLED;
                    }
                    close(surface); mapped_surface=false;
                    mapped_pixels=nullptr; mapped_storage.reset();
                    create_surface(false); return PXA_EVENT_HANDLED;
                }
                char line[120];std::snprintf(line,sizeof(line),"BILI_SURFACE mapped=1 video=%ux%u output=%ux%u strided=%d buffers=%u",
                    surface_w,surface_h,output_w,output_h,int(mapped_strided),unsigned(mapped_buffer_count));
                pxa_log_write(PXA_LOG_LEVEL_INFO,line);
            }
            const int x=output_w==surface_w ? surface_x : 0;
            const int y=output_h==surface_h ? surface_y : 0;
            if (pxa_surface_build_configure_layer(control,sizeof(control),++serial,surface,x,y,output_w,output_h,0,1,&bytes)) pxa_control_packet(control,bytes);
            duration_us=dash_playback?dash_video.segments.back().end:bili::duration(video);
            hud=overlay_dirty=true;hud_until=now_ms()+player_hud_ms;
            playing=true; play_started=0;
            if(!dash_playback && (!file_bytes || http.used!=file_bytes || cache_offset!=0)){http.used=0;cache_offset=UINT64_MAX;}
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
            if (e.token==4) {audio_prompt=false;if (!r.status) audio_permission=r.handle;}
        }
    }
    if (audio_token && e.token==audio_token && e.service==PXA_AUDIO_SERVICE) {
        pxa_audio_pcm_result_t r{};
        if (pxa_audio_parse_pcm_buffered(&e,audio_token,&r)) { audio_token=0;
            if (r.source.status) audio_open_failed(r.source.status);
            else { audio=r.source.handle; gain_token=++serial; pxa_audio_commit_gain(gain_token,audio,0); } }
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
                if (!encoded_audio || state.accepted_samples) {
                    play_offset=audio_pts_base+state.accepted_samples*250000*speed_quarters/audio_rate;
                    play_started=now_ms();
                }
                audio_drained=!state.queued_samples && ai==audio_track.samples.size() && dash_audio_end() &&
                    !pcm_used && !pcm_packet.frames && (!encoded_audio || encoded_drained);
                if(audio_drained){close(audio);audio_ready=false;}
            }
        }
    }
    pxa_window_snapshot_t ws{};
    if (pxa_window_parse_snapshot(&e,3,&ws) || pxa_window_parse_metrics_changed(&e,&ws)) { scene.window(ws); dirty=true; }
    pxa_ui_environment_t env{}; if (pxa_ui_parse_environment_event(&e,&env)) { scene.environment(env); dirty=true; }
    pxa_ui_event_data_t ui{}; if (pxa_ui_parse_event(&e,&ui) && ui.node==30){pxa_ui_event_text(&ui,client.query,sizeof(client.query));client.dirty=true;}
    pxa_ui_pointer_data_t p{};
    if (pxa_ui_parse_pointer(&e,&p) && p.node==2 &&
        (p.generation==scene.ui_generation ||
         (surface && p.phase!=PXA_POINTER_DOWN && player_pointer==p.pointer_id))) pointer(p);
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
        pump_danmaku();
        if(dirty)render();
        present_danmaku();
    }
    return PXA_EVENT_HANDLED;
}
extern "C" void pxa_app_stop(uint32_t) { decoder=surface=audio=0; }
