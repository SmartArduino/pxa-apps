// SPDX-License-Identifier: Apache-2.0
#include "matroska.hpp"
#include "preview_geometry.hpp"
#include "camera_profiles.hpp"
#include <opus.h>
#include <pxa-ext/peripherals.hpp>
#include <pxa/canvas.hpp>
#include <pxa/ui_environment.hpp>
#include <pxa/ui_layout.hpp>
#include <pxa/ui_refresh.hpp>
#include <chrono>
#include <vector>
#include <cstdio>
#include <optional>
#include <charconv>
#include <variant>

using namespace pxa::ui;
using namespace pxa::ui::literals;
namespace ports = pxa::ext::peripherals;
using Clock = std::chrono::steady_clock;

// Only the active control layout lives in each Refresh page. The viewfinder
// is mounted independently and remains alive through all page transitions.
template<class... Views> class CameraControls {
public:
    static constexpr Capacity capacity = [] {
        Capacity result{};
        for (const auto c : std::array{capacity_of<Views>...}) {
            result.nodes=std::max(result.nodes,c.nodes);
            result.bindings=std::max(result.bindings,c.bindings);
            result.handlers=std::max(result.handlers,c.handlers);
            result.dynamic=std::max(result.dynamic,c.dynamic);
            result.refs=std::max(result.refs,c.refs);
            result.gestures=std::max(result.gestures,c.gestures);
        }
        return result;
    }();
    template<size_t Index, class View>
    CameraControls(std::in_place_index_t<Index> index, View view) : view_(index,std::move(view)) {}
    template<class Owner> bool render(Owner& owner, uint32_t parent) {
        return std::visit([&](auto& view) { return view.render(owner,parent); },view_);
    }
private:
    std::variant<Views...> view_;
};

struct CameraApp {
    Environment<> environment;
    State<std::string> status{"正在读取摄像头档位"}, profile_label{"摄像头"};
    State<uint32_t> layout_revision{0};
    State<bool> chrome{true};
    State<std::string> shutter_label{""}, page_label{"1 / 1"};
    State<std::string> album_title{"相册"}, audio_label{"录音 · 开"};
    State<std::string> orientation_label{"横屏"};
    bool portrait_capture = false;
    std::optional<bool> window_portrait;
    std::optional<pxa::WindowMetrics> window_metrics;
    enum class BackTarget { none, camera, gallery };
    BackTarget pending_back = BackTarget::none;
    bool gallery = false, gallery_reload = false;
    struct AlbumItem {
        std::string name;
        uint32_t sequence=0, duration=0;
        uint64_t size=0;
        std::optional<pxa::Asset> thumbnail;
    };
    std::array<AlbumItem,6> items{};
    size_t item_count=0, page_size=6;
    uint32_t grid_anchor=0, grid_page=0;
    bool grid_newer=false, grid_has_older=false, grid_has_newer=false;
    int grid_columns=3, grid_rows=2, tile_width=70, tile_height=50, grid_x=6, grid_y=60;
    int safe_x=0, safe_y=0, safe_width=240, safe_height=200;
    std::optional<pxa::Asset> album_thumbnail;
    pxa::input::PointerCapture pointer;
    int pointer_x=0, pointer_y=0;
    bool album = false, video_mode = false, album_video = false, loading = false, playing = false;
    uint32_t media_sequence = 0, media_epoch = 0, media_total = 0, older_media = 0, newer_media = 0;
    uint32_t playback_offset = 224, playback_index = 0;
    uint64_t media_size = 0, read_cookie = 0;
    std::string media_name;
    pxa::File playback_file;
    std::array<std::byte, 24+pxa::AudioCapture::max_packet_bytes> read_packet{};
    std::array<std::byte, pxa::AudioCapture::max_packet_bytes> read_body{};
    camera_media::MovieInfo movie;
    Clock::time_point playback_due{};
    CanvasRef canvas;
    CanvasCommands<2048> commands;
    std::optional<pxa::Asset> image;
    std::array<ports::PortInfo, PXA_PERIPHERALS_MAX_PORTS> available{};
    std::array<ports::CameraProfile, PXA_CAMERA_MAX_PROFILES> profiles{};
    // Reserve space before each JPEG for its Matroska block header.
    std::vector<std::byte> bytes;
    std::span<std::byte> frame_data() { return std::span(bytes).subspan(32,bytes.size()-40); }
    ports::PortInfo camera_info{};
    ports::Port port;
    pxa::Permission permission;
    pxa::File video_file;
    std::unique_ptr<std::byte[]> recording_buffer;
    camera_media::Video video;
    static constexpr size_t recording_buffer_capacity = 65536;
    size_t recording_buffer_size = 0;
    bool recording_write_failed = false;
    pxa::AudioCapture microphone;
    pxa::Permission microphone_permission, playback_permission;
    std::optional<pxa::AudioSession> speaker;
    OpusDecoder* playback_decoder = nullptr;
    uint64_t playback_timestamp_ms=0, playback_anchor_ms=0;
    std::array<std::byte,1920> playback_pcm{};
    bool record_sound = true, audio_running = false;
    std::optional<pxa::Error> recording_audio_error;
    std::array<std::byte,pxa::AudioCapture::max_packet_bytes> microphone_packet{};
    std::array<opus_int16,960> microphone_pcm{};
    std::array<std::byte,65536> audio_queue{};
    size_t audio_queue_head = 0, audio_queue_size = 0;
    std::array<std::byte,3208> audio_chunk{};
    std::string video_name;
    pxa::Context* context = nullptr;
    size_t profile_count = 0, selected = 0, photo_profile = 0, video_profile = 0;
    size_t active_frame_limit = PXA_VISION_MAX_JPEG;
    uint32_t next_photo = 1, next_video = 1, measured_frames = 0;
    uint32_t captured_audio_samples=0, recorded_frame_offset=0;
    bool foreground = true, discovering = false, running = false;
    bool preview = false, photograph = false, record = false, restart = false, dirty = true;
    bool native_preview = true;
    uint64_t capture_us=0,storage_us=0,preview_us=0;
    uint64_t paint_us=0;
    uint32_t paint_calls=0;
    int width = 240, height = 120;
    uint32_t density_q16=65536;
    Clock::time_point measured_at{}, video_started{};

    void log(std::string_view message) { (void)context->log().write(pxa::LogLevel::info, message); }
    void fail(pxa::Error error) {
        port.reset(); permission.reset(); video_file.close();
        recording_buffer.reset(); close_microphone(); speaker.reset();
        (void)context->window().keep_screen_on(foreground);
        running = discovering = preview = photograph = record = restart = false;
        loading = playing = false; playback_file.close();
        refresh();
        if (foreground) {
            switch (error) {
                case pxa::Error::denied: status.set("请允许摄像头权限"); break;
                case pxa::Error::unsupported: status.set("设备暂不支持此功能"); break;
                case pxa::Error::unavailable: status.set("摄像头未就绪，请换较低档位"); break;
                case pxa::Error::resource_limit:
                case pxa::Error::quota_exceeded: status.set("内存或存储空间不足"); break;
                case pxa::Error::protocol_error: status.set("照片或视频格式无效"); break;
                case pxa::Error::cancelled: status.set("操作已取消，可重新打开"); break;
                default: status.set("操作失败 " + std::to_string(int(error))); break;
            }
        }
        if (album && gallery) page_label.set("读取失败");
        shutter_label.set(album && album_video ? "播放" : "");
        log("Camera error: " + std::to_string(int(error)));
    }
    void show_profile() {
        const auto& profile=profiles[selected];
        std::string text="画质 · ";
        text += profile.width ? std::to_string(profile.width)+"×"+std::to_string(profile.height) : "原生";
        if ((profile.width == 1920 && profile.height == 1080) || (profile.width == 1080 && profile.height == 1920))
            text = profile.quality >= 80 ? "1080p · 高清" : "1080p · 流畅";
        if (profile_count>1) text += " ▾";
        profile_label.set(std::move(text));
    }
    auto profile_span() const { return std::span(profiles.data(), profile_count); }
    void orient_profiles(bool portrait) {
        if (record || video_file) return; // A Matroska track keeps one extent.
        portrait_capture = portrait;
        orientation_label.set(portrait ? "竖屏" : "横屏");
        if (!profile_count) return;
        const auto previous = selected;
        auto photo = camera_profiles::orient(profile_span(), photo_profile, portrait);
        auto video = camera_profiles::orient(profile_span(), video_profile, portrait);
        if (photo) photo_profile = *photo;
        if (video) video_profile = *video;
        selected = video_mode ? video_profile : photo_profile;
        show_profile();
        if (!(video_mode ? video : photo)) status.set("设备未提供此方向的拍摄档位");
        if (selected != previous && running) { restart=true; preview=false; photograph=false; }
    }
    void toggle_orientation() {
        if (record || video_file || photograph) { status.set("请先结束拍摄"); return; }
        const bool portrait = !portrait_capture;
        if (window_metrics && !camera_profiles::supports_window_orientation(window_metrics->native_width,
            window_metrics->native_height, window_metrics->supported_rotations, portrait)) {
            status.set("设备界面不支持此方向"); return;
        }
        auto changed = context->window().request_orientation(portrait ?
            pxa::OrientationPreference::portrait : pxa::OrientationPreference::landscape);
        if (!changed) { status.set("设备不支持界面旋转"); return; }
        window_portrait = portrait;
        orient_profiles(portrait);
        refresh();
    }
    void on_window_changed(const pxa::WindowMetrics& metrics) {
        window_metrics = metrics;
        // Square windows keep their angle and the user's capture preference.
        if (metrics.logical_width == metrics.logical_height) return;
        window_portrait = metrics.logical_height > metrics.logical_width;
        if (!record && !video_file && !photograph) orient_profiles(*window_portrait);
        dirty = true;
    }
    pxa::Task<void> read_window() {
        auto metrics = co_await context->window().snapshot(true);
        if (metrics) on_window_changed(*metrics);
        co_return pxa::Result<void>{}; // Older Window services remain usable.
    }
    pxa::Task<void> discover() {
        const auto old_id = profile_count ? profiles[selected].id : 0;
        auto folder = co_await context->fs().stat("album");
        if (!folder) {
            if (folder.error() != pxa::Error::not_found) co_return std::unexpected(folder.error());
            if (auto made = co_await context->fs().make_directory("album"); !made) co_return made;
        }
        auto timing = co_await context->clock().configure(16000);
        if (!timing) co_return std::unexpected(timing.error());
        auto client = co_await ports::open(*context);
        if (!client) co_return std::unexpected(client.error());
        if (client->version_minor() < 5) co_return std::unexpected(pxa::Error::unsupported);
        auto count = co_await client->list(available);
        if (!count) co_return std::unexpected(count.error());
        size_t index = 0;
        while (index < *count && (available[index].kind != PXA_PERIPHERALS_VISION ||
            available[index].scope() != "camera.jpeg" || !available[index].available())) ++index;
        if (index == *count) co_return std::unexpected(pxa::Error::unsupported);
        camera_info = available[index];
        auto total = co_await client->camera_profiles(camera_info, profiles);
        if (!total) co_return std::unexpected(total.error());
        profile_count = *total; selected = 0;
        for (size_t i = 0; i < profile_count; ++i) if (profiles[i].is_default()) selected = i;
        // Prefer the explicit HD photo profile; keep the board default usable
        // by older clients that open parameter zero without profile discovery.
        for (size_t i = 0; i < profile_count; ++i)
            if (profiles[i].width == 1920 && profiles[i].height == 1080 && profiles[i].quality >= 80) selected = i;
        for (size_t i = 0; i < profile_count; ++i) if (profiles[i].id == old_id) selected = i;
        photo_profile = video_profile = selected;
        for (size_t i = 0; i < profile_count; ++i)
            if (profiles[i].width == 1920 && profiles[i].height == 1080 && profiles[i].quality <= 65)
                video_profile = i;
        if (video_mode) selected = video_profile;
        orient_profiles(portrait_capture);
        show_profile(); discovering = false;
        if (foreground && !album) { preview=true; start(); }
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> write_all(pxa::File& file, std::span<const std::byte> input) {
        const auto deadline = Clock::now() + std::chrono::seconds(30);
        auto yield_at = Clock::now() + std::chrono::milliseconds(8);
        while (!input.empty()) {
            // FAT/SD benefits from multi-sector writes. Avoid an event round
            // trip after every 4 KiB of an otherwise ready JPEG stream.
            auto written = file.write(input.first(std::min(input.size(), size_t(32768))));
            if (written) {
                if (!*written || *written > input.size()) co_return std::unexpected(pxa::Error::io_error);
                input = input.subspan(*written);
            } else if (written.error() != pxa::Error::would_block) {
                co_return std::unexpected(written.error());
            }
            if (Clock::now() > deadline) co_return std::unexpected(pxa::Error::timed_out);
            if (!input.empty() && (!written || Clock::now() >= yield_at)) {
                if (auto yielded = co_await context->clock().yield(); !yielded) co_return yielded;
                yield_at = Clock::now() + std::chrono::milliseconds(8);
            }
        }
        co_return pxa::Result<void>{};
    }
    pxa::Task<std::string> filename(bool movie) {
        auto& sequence = movie ? next_video : next_photo;
        char name[48];
        for (;;) {
            if (!sequence) co_return std::unexpected(pxa::Error::limit_exceeded);
            std::snprintf(name, sizeof(name), movie ? "video-%06u.mkv" : "photo-%06u.jpg", unsigned(sequence++));
            auto info = co_await context->fs().stat("album/" + std::string(name));
            if (!info) {
                if (info.error() != pxa::Error::not_found) co_return std::unexpected(info.error());
                if (movie) {
                    auto old=co_await context->fs().stat("album/"+std::string(name).substr(0,std::strlen(name)-4)+".avi");
                    if (old) continue;
                    if (old.error()!=pxa::Error::not_found) co_return std::unexpected(old.error());
                }
                co_return "album/" + std::string(name);
            }
        }
    }
    pxa::Task<size_t> read_frame(ports::FrameStream source) {
        // Preview-only sessions do not reserve a full-resolution JPEG buffer.
        if (bytes.size() != active_frame_limit + 40) bytes.resize(active_frame_limit + 40);
        const auto deadline = Clock::now() + std::chrono::seconds(20);
        size_t received = 0;
        auto yield_at = Clock::now() + std::chrono::milliseconds(8);
        for (;;) {
            std::byte extra{};
            auto target = received < frame_data().size() ? frame_data().subspan(received) : std::span(&extra, 1);
            auto part = source.read(target);
            if (part) {
                if (!*part) break;
                if (*part > frame_data().size() - received) co_return std::unexpected(pxa::Error::limit_exceeded);
                received += *part;
            } else if (part.error() != pxa::Error::would_block) {
                co_return std::unexpected(part.error());
            }
            if (Clock::now() > deadline) co_return std::unexpected(pxa::Error::timed_out);
            if (!part || Clock::now() >= yield_at) {
                if (auto yielded = co_await context->clock().yield(); !yielded) co_return std::unexpected(yielded.error());
                yield_at = Clock::now() + std::chrono::milliseconds(8);
            }
        }
        const auto dimensions = camera_media::jpeg_size(frame_data().first(received));
        if (!dimensions.width || !dimensions.height) co_return std::unexpected(pxa::Error::protocol_error);
        co_return received;
    }
    pxa::Task<void> save_photo(size_t size) {
        // Retained preview pixels survive closing the sensor. Pause its DMA
        // before Flash programming, then automatically reopen the viewfinder.
        port.reset(); restart=foreground && preview && !album;
        auto name = co_await filename(false);
        if (!name) co_return std::unexpected(name.error());
        auto file = co_await context->fs().open("photo.tmp", pxa::OpenMode::write | pxa::OpenMode::create | pxa::OpenMode::truncate);
        if (!file) co_return std::unexpected(file.error());
        auto written = co_await write_all(*file, frame_data().first(size));
        if (!written) co_return written;
        if (auto closed = file->close(); !closed) co_return closed;
        auto committed = co_await context->fs().rename("photo.tmp", *name);
        if (!committed) co_return committed;
        status.set("照片已保存");
        const auto dimensions = camera_media::jpeg_size(frame_data().first(size));
        log("Camera photo saved: " + *name + " bytes=" + std::to_string(size) +
            " width=" + std::to_string(dimensions.width) + " height=" + std::to_string(dimensions.height));
        auto source = co_await context->fs().open(*name, pxa::OpenMode::read);
        if (!source) co_return std::unexpected(source.error());
        auto decoded = co_await context->assets().decode_image(std::move(*source), image_options());
        if (!decoded) co_return std::unexpected(decoded.error());
        image.emplace(std::move(*decoded)); dirty = true;
        auto thumbnail=co_await make_thumbnail(*name,size,false);
        if (thumbnail) album_thumbnail.emplace(std::move(*thumbnail));
        refresh();
        co_return pxa::Result<void>{};
    }
    static pxa::ImageDecodeOptions image_options() {
        return {.max_width=320, .max_height=320, .max_pixels=102400,
            .max_encoded_bytes=PXA_VISION_MAX_JPEG_HD, .formats=pxa::ImageFormats::jpeg};
    }
    void close_microphone() {
        (void)microphone.close();

    }
    pxa::Task<void> capture_sound() {
        struct Guard { bool& active; ~Guard() { active=false; } } guard{audio_running};
        while (microphone && foreground) {
            for (unsigned i=0;i<64;++i) {
                auto count=microphone.read(microphone_packet);
                if (!count && count.error()==pxa::Error::would_block) break;
                if (!count || !*count || *count>microphone_packet.size() || *count+2>audio_queue.size()-audio_queue_size) {
                    recording_audio_error=count ? pxa::Error::resource_limit : count.error(); break;
                }
                const auto append=[this](std::byte byte) {
                    audio_queue[(audio_queue_head+audio_queue_size)%audio_queue.size()]=byte; ++audio_queue_size;
                };
                append(std::byte(*count));append(std::byte(*count>>8));
                for (auto byte:std::span(microphone_packet).first(*count)) append(byte);
                captured_audio_samples+=uint32_t(microphone.frame_ms())*16;
            }
            if (recording_audio_error) { record=false; break; }
            // Drain already captured packets before closing a stopped recording.
            if (!record) break;
            if (auto yielded=co_await context->clock().yield(); !yielded) {
                recording_audio_error=yielded.error(); record=false; break;
            }
        }
        close_microphone();
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> open_recording_sound() {
        audio_queue_head=audio_queue_size=0; captured_audio_samples=0; recording_audio_error.reset();
        if (!record_sound) co_return pxa::Result<void>{};
        auto grant=co_await context->permissions().acquire("audio.capture","microphone");
        if (!grant) { log("Camera microphone unavailable: "+std::to_string(int(grant.error()))); co_return pxa::Result<void>{}; }
        microphone_permission=std::move(*grant);
        auto opened=co_await context->capture().open(microphone_permission);
        if (!opened) { log("Camera microphone unavailable: "+std::to_string(int(opened.error()))); co_return pxa::Result<void>{}; }
        microphone=std::move(*opened);
        video.audio=true;
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> flush_recording() {
        if (!recording_buffer_size) co_return pxa::Result<void>{};
        auto written=co_await write_all(video_file,{recording_buffer.get(),recording_buffer_size});
        if (!written) { recording_write_failed=true; co_return written; }
        recording_buffer_size=0;
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> write_recording(std::span<const std::byte> chunk) {
        if (chunk.size()>recording_buffer_capacity-recording_buffer_size) {
            if (auto flushed=co_await flush_recording(); !flushed) co_return flushed;
        }
        if (!recording_buffer || chunk.size()>recording_buffer_capacity) {
            auto written=co_await write_all(video_file,chunk);
            if (!written) recording_write_failed=true;
            co_return written;
        }
        std::memcpy(recording_buffer.get()+recording_buffer_size,chunk.data(),chunk.size());
        recording_buffer_size+=chunk.size();
        co_return pxa::Result<void>{};
    }
    bool recording_has_space(uint32_t chunk_bytes) {
        if (video.movie_bytes<=UINT32_MAX-camera_media::mkv_header_bytes-chunk_bytes) return true;
        record=false; status.set("本段已达文件格式上限"); return false;
    }
    pxa::Task<void> append_sound(uint64_t until_ms=UINT64_MAX) {
        while (audio_queue_size>=2 && uint64_t(video.audio_samples)/16<=until_ms) {
            const auto at=[this](size_t i) { return audio_queue[(audio_queue_head+i)%audio_queue.size()]; };
            const auto size=std::to_integer<unsigned>(at(0))+(std::to_integer<unsigned>(at(1))<<8);
            if (!size || size>microphone_packet.size() || size+2>audio_queue_size) co_return std::unexpected(pxa::Error::protocol_error);
            const auto header=camera_media::mkv_block_header(2,uint64_t(video.audio_samples)/16,size);
            if (!recording_has_space(uint32_t(header.size()+size))) co_return pxa::Result<void>{};
            std::copy(header.begin(),header.end(),audio_chunk.begin());
            for (size_t i=0;i<size;++i) audio_chunk[header.size()+i]=at(2+i);
            if (auto written=co_await write_recording(std::span(audio_chunk).first(header.size()+size)); !written) co_return written;
            audio_queue_head=(audio_queue_head+2+size)%audio_queue.size(); audio_queue_size-=2+size;
            video.movie_bytes+=uint32_t(header.size()+size); video.audio_samples+=uint32_t(microphone.frame_ms())*16;
        }
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> append_video(size_t size) {
        const auto dimensions = camera_media::jpeg_size(frame_data().first(size));
        if (!video_file) {
            auto name=co_await filename(true);
            if (!name) co_return std::unexpected(name.error());
            video_name=std::move(*name);
            auto file=co_await context->fs().open("video.tmp",pxa::OpenMode::read | pxa::OpenMode::write | pxa::OpenMode::create | pxa::OpenMode::truncate);
            if (!file) co_return std::unexpected(file.error());
            video_file=std::move(*file); video={}; video.size=dimensions;
            recording_write_failed=false; recording_buffer_size=0;
            if (native_preview) recording_buffer.reset(new(std::nothrow) std::byte[recording_buffer_capacity]);
            if (auto opened=co_await open_recording_sound(); !opened) co_return opened;
            const auto header=camera_media::mkv_header(video);
            if (auto written=co_await write_all(video_file,header); !written) co_return written;
            video_started=Clock::now();
            if (video.audio && record && foreground) {
                audio_running=true;
                if (auto task=context->tasks().start(capture_sound()); !task) { audio_running=false; co_return task; }
            }
        }
        if (video.size.width!=dimensions.width || video.size.height!=dimensions.height)
            co_return std::unexpected(pxa::Error::protocol_error);
        const auto timestamp=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-video_started).count();
        if (auto written=co_await append_sound(uint64_t(timestamp)); !written) co_return written;
        const auto header=camera_media::mkv_block_header(1,uint64_t(timestamp),uint32_t(size));
        recorded_frame_offset=camera_media::mkv_header_bytes+video.movie_bytes+camera_media::mkv_block_header_bytes;
        const uint32_t chunk_bytes=uint32_t(header.size()+size);
        if (!recording_has_space(chunk_bytes)) co_return pxa::Result<void>{};
        std::copy(header.begin(),header.end(),bytes.begin()+32-header.size());
        if (auto written=co_await write_recording(std::span(bytes).subspan(32-header.size(),chunk_bytes)); !written) co_return written;
        ++video.frames; video.movie_bytes+=chunk_bytes; video.max_frame=std::max(video.max_frame,uint32_t(size));
        const auto seconds=std::chrono::duration_cast<std::chrono::seconds>(Clock::now()-video_started).count();
        char timer[64];
        std::snprintf(timer,sizeof(timer),"录像 %02u:%02u · %s",unsigned(seconds/60),unsigned(seconds%60),video.audio ? "有声" : "无声");
        status.set(timer);
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> finish_video() {
        if (!video_file) co_return pxa::Result<void>{};
        record=false;
        while (audio_running) { if (auto yielded=co_await context->clock().yield(); !yielded) co_return yielded; }
        close_microphone();
        port.reset(); restart=foreground && preview && !album;
        status.set("正在保存视频");
        video.elapsed_us=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-video_started).count();
        if (!recording_write_failed) {
            if (auto written=co_await append_sound(); !written) recording_write_failed=true;
        }
        if (!recording_write_failed) {
            if (auto flushed=co_await flush_recording(); !flushed) recording_write_failed=true;
        }
        recording_buffer.reset(); recording_buffer_size=0;
        if (recording_write_failed) co_return std::unexpected(pxa::Error::io_error);
        const auto footer=camera_media::mkv_footer(video);
        if (auto written=co_await write_all(video_file,footer.data()); !written) co_return written;
        const uint64_t file_bytes=camera_media::mkv_header_bytes+video.movie_bytes+footer.size;
        if (auto closed=video_file.close(); !closed) co_return closed;
        if (!video.frames) { (void)co_await context->fs().remove("video.tmp"); co_return pxa::Result<void>{}; }
        if (auto committed=co_await context->fs().rename("video.tmp",video_name); !committed) co_return committed;
        auto thumbnail=co_await make_thumbnail(video_name,file_bytes,true);
        if (thumbnail) album_thumbnail.emplace(std::move(*thumbnail));
        status.set(recording_audio_error ? "录音中断，片段已保存" : "视频已保存"); refresh();
        log("Camera video saved: "+video_name+" frames="+std::to_string(video.frames)+
            " bytes="+std::to_string(file_bytes)+" duration_us="+std::to_string(video.elapsed_us)+
            " audio_samples="+std::to_string(video.audio_samples)+" captured_samples="+std::to_string(captured_audio_samples));
        log("Camera record timing: capture_us="+std::to_string(capture_us)+" storage_us="+std::to_string(storage_us)+" preview_us="+std::to_string(preview_us));
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> show_preview(ports::Client& client, uint32_t offset=0, uint32_t size=0) {
        const auto deadline=Clock::now()+std::chrono::seconds(20);
        while (native_preview) {
            auto prepared=co_await client.preview(port);
            if (prepared) { image.emplace(std::move(*prepared)); dirty=true; co_return pxa::Result<void>{}; }
            if (prepared.error()==pxa::Error::unsupported) { native_preview=false; break; }
            if (prepared.error()!=pxa::Error::would_block) co_return std::unexpected(prepared.error());
            if (!foreground) co_return std::unexpected(pxa::Error::cancelled);
            if (Clock::now()>deadline) co_return std::unexpected(pxa::Error::timed_out);
            if (auto yielded=co_await context->clock().yield(); !yielded) co_return yielded;
        }
        if (offset && recording_buffer_size) {
            if (auto flushed=co_await flush_recording(); !flushed) co_return flushed;
        }
        if (!read_cookie) ++read_cookie;
        auto frame=offset ? co_await client.file_frame(video_file,offset,size,read_cookie) : co_await client.frame(port);
        if (!frame) co_return std::unexpected(frame.error());
        auto decoded=co_await context->assets().decode_image(std::move(*frame),image_options());
        if (!decoded) co_return std::unexpected(decoded.error());
        image.emplace(std::move(*decoded)); dirty=true;
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> capture_loop(ports::Client client) {
        auto grant = co_await context->permissions().acquire(PXA_PERIPHERALS_PERMISSION, "camera.jpeg");
        if (!grant) co_return std::unexpected(grant.error());
        permission = std::move(*grant);
        const auto deadline = Clock::now() + std::chrono::seconds(20);
        const auto requested_profile = profiles[selected];
        while (foreground && (preview || photograph || record)) {
            auto opened = co_await client.open_port(camera_info, PXA_PERIPHERALS_MODE_STREAM, requested_profile.id, 0, permission);
            if (opened) { port = std::move(*opened); break; }
            if ((opened.error() != pxa::Error::busy && opened.error() != pxa::Error::would_block) || Clock::now() > deadline)
                co_return std::unexpected(opened.error());
            if (auto yielded = co_await context->clock().yield(); !yielded) co_return yielded;
        }
        active_frame_limit = requested_profile.max_frame_bytes;
        measured_at = Clock::now(); measured_frames = 0; paint_us = 0; paint_calls = 0;
        status.set("");
        while (foreground && (preview || photograph || record || video_file)) {
            if (!record && video_file) {
                auto finished = co_await finish_video();
                if (!finished) co_return finished;
                if (restart) break;
            }
            if (!preview && !photograph && !record) break;
            uint32_t saved_offset=0,saved_size=0;
            bool photo_shown=false;
            if (record || photograph) {
                const auto started=Clock::now();
                auto frame = co_await client.frame(port);
                if (!frame) co_return std::unexpected(frame.error());
                auto size = co_await read_frame(std::move(*frame));
                if (!size) co_return std::unexpected(size.error());
                capture_us+=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-started).count();
                if (record) {
                    const auto previous_frames=video_file ? video.frames : 0;
                    const auto writing=Clock::now();
                    auto appended = co_await append_video(*size);
                    if (!appended) co_return appended;
                    storage_us+=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-writing).count();
                    if (video.frames!=previous_frames) { saved_offset=recorded_frame_offset; saved_size=*size; }
                }
                if (photograph) {
                    photograph = false;
                    auto saved = co_await save_photo(*size);
                    if (!saved) co_return saved;
                    photo_shown=true;
                    if (restart) break;
                }
            }
            if (preview && foreground) {
                const auto showing=Clock::now();
                if (!photo_shown) {
                    if (auto shown=co_await show_preview(client,saved_offset,saved_size); !shown) co_return shown;
                }
                preview_us+=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-showing).count();
                ++measured_frames;
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - measured_at).count();
                if (elapsed >= 5000) {
                    log("Camera preview: frames=" + std::to_string(measured_frames) + " elapsed_ms=" + std::to_string(elapsed) +
                        " paint_us=" + std::to_string(paint_us) + " paint_calls=" + std::to_string(paint_calls));
                    measured_at = Clock::now(); measured_frames = 0; paint_us = 0; paint_calls = 0;
                }
            }
        }
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> run() {
        auto client = co_await ports::open(*context);
        pxa::Result<void> result;
        if (!client) result = std::unexpected(client.error());
        else result = co_await capture_loop(*client);
        port.reset(); permission.reset();
        (void)context->window().keep_screen_on(foreground);
        auto finalized = co_await finish_video();
        running = false; record = false; shutter_label.set(""); refresh();
        if (!finalized) { video_file.close(); co_return finalized; }
        if (!result && !restart) co_return result;
        co_return pxa::Result<void>{};
    }
    void start() {
        if (foreground && !album) preview=true;
        if (running) return;
        if (!profile_count || discovering) {
            preview = photograph = record = false;
            status.set("摄像头档位尚未就绪"); return;
        }
        running = true; status.set("正在打开摄像头");
        (void)context->window().keep_screen_on(true);
        if (auto task = context->tasks().start(run()); !task) fail(task.error());
    }
    void take_photo() {
        if (restart) { status.set("正在切换画质，请稍候"); return; }
        photograph = true; start();
    }
    void toggle_record() {
        if (!record && restart) { status.set("正在切换画质，请稍候"); return; }
        if (!record && video_file) return;
        record = !record;
        if (record) capture_us=storage_us=preview_us=0;
        shutter_label.set(record ? "停止" : ""); refresh();
        if (record) { preview = true; start(); }
    }
    void next_profile() {
        if (!profile_count || discovering) return;
        if (record || video_file) { status.set("请先结束录像"); return; }
        if (profile_count == 1) { status.set("设备提供原生 JPEG 档位"); return; }
        const bool oriented = camera_profiles::orient(profile_span(), selected, portrait_capture).has_value();
        for (size_t step = 1; step <= profile_count; ++step) {
            const auto candidate = (selected + step) % profile_count;
            if (!oriented || camera_profiles::matches(profiles[candidate], portrait_capture)) { selected=candidate; break; }
        }
        (video_mode ? video_profile : photo_profile) = selected;
        show_profile();
        if (running) { restart=true; preview=false; photograph=false; }
        else { preview=true; start(); }
    }
    void refresh() { pointer.reset(); layout_revision.set(layout_revision.get() + 1); dirty = true; }
    static uint32_t sequence(std::string_view name, bool video) {
        const std::string_view prefix = video ? "video-" : "photo-", suffix = video ? (name.ends_with(".mkv") ? ".mkv" : ".avi") : ".jpg";
        if (!name.starts_with(prefix) || !name.ends_with(suffix)) return 0;
        name = name.substr(prefix.size(), name.size()-prefix.size()-suffix.size());
        uint32_t value = 0;
        auto parsed = std::from_chars(name.data(), name.data()+name.size(), value);
        return parsed.ec == std::errc{} && parsed.ptr == name.data()+name.size() ? value : 0;
    }
    pxa::Task<void> read_exact(pxa::File& file, uint64_t offset, size_t size) {
        size_t received = 0;
        const auto deadline = Clock::now() + std::chrono::seconds(15);
        while (received < size) {
            if (!read_cookie) ++read_cookie;
            auto result = file.read_at(offset+received, read_cookie, std::span(read_packet).first(24+size-received));
            if (result) {
                if (!*result) co_return std::unexpected(pxa::Error::protocol_error);
                // Preserve partial results before preparing the next ReadAt header.
                std::memcpy(read_body.data()+received, read_packet.data()+24, *result);
                received += *result; ++read_cookie;
            } else if (result.error() != pxa::Error::would_block) {
                (void)file.cancel_read_at(read_cookie);
                log("Camera file read: offset="+std::to_string(offset+received)+" bytes="+std::to_string(size-received)+" error="+std::to_string(int(result.error())));
                co_return std::unexpected(result.error());
            }
            if (Clock::now() > deadline) {
                (void)file.cancel_read_at(read_cookie);
                co_return std::unexpected(pxa::Error::timed_out);
            }
            if (!result) { if (auto yielded = co_await context->clock().yield(); !yielded) co_return yielded; }
        }
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> play_pcm_chunk(uint32_t offset, uint32_t size) {
        if (size&1) co_return std::unexpected(pxa::Error::protocol_error);
        while (size) {
            const auto count=std::min(size,uint32_t(320));
            if (auto read=co_await read_exact(playback_file,offset,count); !read) co_return read;
            size_t accepted=0;
            const auto deadline=Clock::now()+std::chrono::seconds(5);
            while (speaker && accepted<count) {
                auto written=speaker->write_pcm(std::span(read_body).subspan(accepted,count-accepted));
                if (written) accepted+=*written;
                else if (written.error()!=pxa::Error::would_block) co_return std::unexpected(written.error());
                if (Clock::now()>deadline) co_return std::unexpected(pxa::Error::timed_out);
                if (accepted<count) { if (auto yielded=co_await context->clock().yield(); !yielded) co_return yielded; }
            }
            offset+=count; size-=count;
        }
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> play_opus_chunk(uint32_t offset,uint32_t size) {
        if (size>microphone_packet.size() || !playback_decoder) co_return std::unexpected(pxa::Error::protocol_error);
        if (auto read=co_await read_exact(playback_file,offset,size); !read) co_return read;
        const auto samples=opus_decode(playback_decoder,reinterpret_cast<const unsigned char*>(read_body.data()),size,
            microphone_pcm.data(),microphone_pcm.size(),0);
        if (samples<=0) co_return std::unexpected(pxa::Error::protocol_error);
        for (int i=0;i<samples;++i) camera_media::put16(playback_pcm.data()+i*2,uint16_t(microphone_pcm[i]));
        size_t position=0;
        while (speaker && position<size_t(samples)*2) {
            const auto count=std::min(size_t(samples)*2-position,size_t(320));
            auto written=speaker->write_pcm(std::span(playback_pcm).subspan(position,count));
            if (written) position+=*written;
            else if (written.error()!=pxa::Error::would_block) {
                log("Camera Opus playback submit: samples="+std::to_string(samples)+" bytes="+std::to_string(count)+" error="+std::to_string(int(written.error())));
                co_return std::unexpected(written.error());
            }
            if (position<size_t(samples)*2) { if (auto yielded=co_await context->clock().yield(); !yielded) co_return yielded; }
        }
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> decode_video_frame(ports::Client& client) {
        while (playback_offset<movie.end) {
            const auto header_size=movie.matroska ? camera_media::mkv_block_header_bytes : 8u;
            if (auto read=co_await read_exact(playback_file,playback_offset,header_size); !read) co_return read;
            const auto block=movie.matroska ? camera_media::mkv_block(std::span(read_body).first(header_size),playback_offset,movie.end) : camera_media::MkvBlock{};
            const auto length=movie.matroska ? block.size : camera_media::chunk_bytes(std::span(read_body).first(8),playback_offset,movie.end);
            if (!length) co_return std::unexpected(pxa::Error::protocol_error);
            const bool is_video=movie.matroska ? block.track==1 : !std::memcmp(read_body.data(),"00dc",4);
            const bool is_audio=movie.matroska ? block.track==2 : !std::memcmp(read_body.data(),"01wb",4);
            const auto offset=playback_offset+header_size;
            playback_offset+=movie.matroska ? header_size+length : 8+length+(length&1);
            if (is_audio && movie.audio) {
                if (speaker) {
                    auto played=movie.matroska ? co_await play_opus_chunk(offset,length) : co_await play_pcm_chunk(offset,length);
                    if (!played) co_return played;
                }
            } else if (is_video) {
                if (movie.matroska && playing) {
                    playback_timestamp_ms=block.ms;
                    while (foreground && playing && Clock::now()<playback_due+std::chrono::milliseconds(block.ms)) {
                        if (auto yielded=co_await context->clock().yield(); !yielded) co_return yielded;
                    }
                }
                auto frame=co_await client.file_frame(playback_file,offset,length,read_cookie);
                if (!frame) {
                    log("Camera playback JPEG stream: offset="+std::to_string(offset)+" bytes="+std::to_string(length)+" error="+std::to_string(int(frame.error())));
                    co_return std::unexpected(frame.error());
                }
                auto decoded=co_await context->assets().decode_image(std::move(*frame),image_options());
                if (!decoded) co_return std::unexpected(decoded.error());
                image.emplace(std::move(*decoded)); dirty=true; ++playback_index;
                co_return pxa::Result<void>{};
            } else co_return std::unexpected(pxa::Error::protocol_error);
        }
        co_return pxa::Result<void>{};
    }
    pxa::Task<camera_media::MovieInfo> read_movie(pxa::File& file,uint64_t size) {
        if (size<224) co_return std::unexpected(pxa::Error::protocol_error);
        if (auto read=co_await read_exact(file,0,uint32_t(std::min<uint64_t>(512,size))); !read) co_return std::unexpected(read.error());
        auto info=camera_media::movie_info(read_body,size);
        if (!info.end && camera_media::get_be(read_body.data(),4)==0x1a45dfa3 && size>=576) {
            if (auto read=co_await read_exact(file,size-64,64); !read) co_return std::unexpected(read.error());
            info=camera_media::mkv_footer_info(std::span(read_body).first(64),size);
        }
        if (!info.end) co_return std::unexpected(pxa::Error::protocol_error);
        co_return info;
    }
    pxa::Task<void> scan_album(uint32_t requested, uint32_t epoch) {
        struct LoadingGuard { bool& active; ~LoadingGuard() { active = false; } } guard{loading};
        while (running && foreground && album && epoch == media_epoch) {
            if (auto yielded = co_await context->clock().yield(); !yielded) co_return yielded;
        }
        if (!foreground || !album || gallery || epoch != media_epoch) co_return pxa::Result<void>{};
        image.reset(); dirty = true;
        auto directory = co_await context->fs().directory("album");
        if (!directory) co_return std::unexpected(directory.error());
        media_total = older_media = newer_media = 0;
        uint32_t chosen = requested;
        std::string newest_name;
        uint64_t newest_size = 0;
        for (;;) {
            auto entry = co_await directory->next();
            if (!entry) co_return std::unexpected(entry.error());
            if (!*entry) break;
            const auto id = sequence((**entry).name(), album_video);
            if (!id || (**entry).info.kind != pxa::FileKind::regular) continue;
            ++media_total;
            if (!requested && id > chosen) {
                chosen = id; newest_name = (**entry).bytes; newest_size = (**entry).info.size;
            }
            if (requested == id) { newest_name = (**entry).bytes; newest_size = (**entry).info.size; }
            if (requested && id < requested) older_media = std::max(older_media, id);
            if (requested && id > requested && (!newer_media || id < newer_media)) newer_media = id;
        }
        if (!foreground || !album || gallery || epoch != media_epoch) co_return pxa::Result<void>{};
        if (!requested && chosen) co_return co_await scan_album(chosen, epoch);
        media_sequence = chosen; media_name = std::move(newest_name); media_size = newest_size;
        album_title.set(album_video ? "视频" : "照片");
        if (media_name.empty()) {
            status.set(album_video ? "还没有视频" : "还没有照片"); loading = false;
            co_return pxa::Result<void>{};
        }
        status.set((album_video ? "视频 " : "照片 ") + std::to_string(media_sequence) + " · 共 " + std::to_string(media_total) + (album_video ? " 段" : " 张"));
        auto opened = co_await context->fs().open("album/" + media_name, pxa::OpenMode::read);
        if (!opened) co_return std::unexpected(opened.error());
        playback_file = std::move(*opened);
        if (album_video) {
            auto parsed=co_await read_movie(playback_file,media_size);
            if (!parsed) co_return std::unexpected(parsed.error());
            movie=*parsed;
            playback_offset = movie.data; playback_index = 0;
            auto client = co_await ports::open(*context);
            if (!client) co_return std::unexpected(client.error());
            if (auto decoded = co_await decode_video_frame(*client); !decoded) co_return decoded;
        } else {
            auto decoded = co_await context->assets().decode_image(std::move(playback_file), image_options());
            if (!decoded) co_return std::unexpected(decoded.error());
            image.emplace(std::move(*decoded)); dirty = true;
        }
        loading = false; refresh();
        co_return pxa::Result<void>{};
    }
    void load_album(uint32_t requested = 0) {
        if (loading || playing) return;
        loading = true; playback_file.close();
        if (playback_decoder) { opus_decoder_destroy(playback_decoder); playback_decoder=nullptr; }
        shutter_label.set(album_video ? "播放" : "");
        status.set("正在读取相册");
        if (auto task = context->tasks().start(scan_album(requested, ++media_epoch)); !task) fail(task.error());
    }
    void open_album() {
        if (loading) return;
        preview=photograph=record=restart=false;
        album=gallery=true; album_video=false; grid_page=0; chrome.set(true);
        shutter_label.set(""); refresh(); load_grid(0,false);
    }
    void close_album() {
        if (loading || playing) {
            pending_back=BackTarget::camera; playing=false; speaker.reset(); ++media_epoch; return;
        }
        ++media_epoch; gallery_reload=false; playback_file.close(); image.reset(); album=gallery=false;
        if (playback_decoder) { opus_decoder_destroy(playback_decoder); playback_decoder=nullptr; }
        for (auto& item:items) item.thumbnail.reset();
        chrome.set(true); shutter_label.set(""); refresh(); preview=true; start();
    }
    void return_to_grid() {
        if (loading || playing) {
            pending_back=BackTarget::gallery; playing=false; speaker.reset(); ++media_epoch; return;
        }
        playback_file.close(); image.reset();
        if (playback_decoder) { opus_decoder_destroy(playback_decoder); playback_decoder=nullptr; }
        gallery=true; chrome.set(true);
        refresh(); load_grid(grid_anchor,grid_newer);
    }
    void select_mode(bool video) {
        if (loading || playing || record || video_file) return;
        if (album) {
            album_video=video; grid_page=0; load_grid(0,false);
        } else if (video_mode != video) {
            (video_mode ? video_profile : photo_profile) = selected;
            video_mode = video;
            selected = video_mode ? video_profile : photo_profile;
            if (profile_count) show_profile();
            if (running) { restart=true; preview=false; photograph=false; }
            else if (profile_count) start();
        }
        refresh();
    }
    pxa::Task<void> play_video(uint32_t epoch) {
        auto client = co_await ports::open(*context);
        if (!client) co_return std::unexpected(client.error());
        if (playback_offset >= movie.end || playback_index<=1) { playback_offset = movie.data; playback_index = 0; playback_timestamp_ms=0; }
        if (movie.audio) {
            auto grant=co_await context->permissions().acquire("audio.playback","media");
            if (grant) {
                playback_permission=std::move(*grant);
                auto opened=co_await context->audio().open_pcm(playback_permission,16000,1);
                if (opened) {
                    speaker.emplace(std::move(*opened));
                    if (auto configured=co_await speaker->graph(0); !configured) co_return configured;
                    if (movie.matroska && !playback_decoder) {
                        int error=0;playback_decoder=opus_decoder_create(16000,1,&error);
                        if (!playback_decoder) speaker.reset();
                    }
                }
            }
        }
        playback_due = Clock::now()-std::chrono::milliseconds(movie.matroska ? playback_timestamp_ms : 0);
        while (foreground && album && !gallery && playing && epoch == media_epoch && playback_offset < movie.end) {
            if (auto decoded = co_await decode_video_frame(*client); !decoded) co_return decoded;
            if (!movie.matroska) playback_due += std::chrono::microseconds(movie.period_us);
            while (!movie.matroska && !speaker && foreground && playing && Clock::now() < playback_due) {
                if (auto yielded = co_await context->clock().yield(); !yielded) co_return yielded;
            }
        }
        if (speaker && playback_offset>=movie.end) {
            const auto deadline=Clock::now()+std::chrono::seconds(1);
            while (foreground && playing && Clock::now()<deadline) {
                auto state=co_await speaker->query();
                if (!state || !state->queued_samples) break;
                if (auto yielded=co_await context->clock().yield(); !yielded) co_return yielded;
            }
        }
        speaker.reset();
        if (playback_decoder && (!foreground || playback_offset>=movie.end)) { opus_decoder_destroy(playback_decoder); playback_decoder=nullptr; }
        playing = false; loading = false; shutter_label.set("播放");
        (void)context->window().keep_screen_on(foreground); refresh();
        log("Camera video playback: frames=" + std::to_string(playback_index));
        co_return pxa::Result<void>{};
    }
    void shutter() {
        if (!album) { if (video_mode) toggle_record(); else take_photo(); return; }
        if (!album_video || !playback_file || (loading && !playing)) return;
        if (playing) { playing = false; speaker.reset(); shutter_label.set("播放"); return; }
        playing = loading = true; shutter_label.set("暂停");
        (void)context->window().keep_screen_on(true);
        if (auto task = context->tasks().start(play_video(media_epoch)); !task) fail(task.error());
    }
    pxa::Task<pxa::Asset> make_thumbnail(std::string path, uint64_t size, bool video, uint32_t* duration=nullptr) {
        auto file=co_await context->fs().open(path,pxa::OpenMode::read);
        if (!file) co_return std::unexpected(file.error());
        const pxa::ImageDecodeOptions options{.max_width=64,.max_height=64,.max_pixels=4096,
            .max_encoded_bytes=PXA_VISION_MAX_JPEG_HD,.formats=pxa::ImageFormats::jpeg};
        if (!video) co_return co_await context->assets().decode_image(std::move(*file),options);
        auto parsed=co_await read_movie(*file,size);
        if (!parsed) co_return std::unexpected(parsed.error());
        const auto header=*parsed;
        if (duration) *duration=uint32_t(std::min<uint64_t>(UINT32_MAX,
            (uint64_t(header.frames)*header.period_us+999999)/1000000));
        const auto chunk_header=header.matroska ? camera_media::mkv_block_header_bytes : 8u;
        if (auto read=co_await read_exact(*file,header.data,chunk_header); !read) co_return std::unexpected(read.error());
        const auto length=header.matroska ? camera_media::mkv_block(std::span(read_body).first(chunk_header),header.data,header.end).size : camera_media::frame_bytes(std::span(read_body).first(8),header.data,header.end);
        if (!length) co_return std::unexpected(pxa::Error::protocol_error);
        auto client=co_await ports::open(*context);
        if (!client) co_return std::unexpected(client.error());
        auto frame=co_await client->file_frame(*file,header.data+chunk_header,length,read_cookie);
        if (!frame) co_return std::unexpected(frame.error());
        co_return co_await context->assets().decode_image(std::move(*frame),options);
    }
    pxa::Task<void> scan_grid(uint32_t anchor, bool newer, uint32_t epoch, size_t limit) {
        struct LoadingGuard { bool& value; ~LoadingGuard() { value=false; } } guard{loading};
        while (running && foreground && album && epoch==media_epoch) {
            if (auto yielded=co_await context->clock().yield(); !yielded) co_return yielded;
        }
        if (!foreground || !gallery || epoch!=media_epoch) co_return pxa::Result<void>{};
        image.reset();
        for (auto& item:items) item=AlbumItem{};
        item_count=0; media_total=0; dirty=true;
        auto directory=co_await context->fs().directory("album");
        if (!directory) co_return std::unexpected(directory.error());
        uint32_t oldest=UINT32_MAX,newest=0;
        for (;;) {
            auto entry=co_await directory->next();
            if (!entry) co_return std::unexpected(entry.error());
            if (!*entry) break;
            const auto id=sequence((**entry).name(),album_video);
            if (!id || (**entry).info.kind!=pxa::FileKind::regular) continue;
            ++media_total; oldest=std::min(oldest,id); newest=std::max(newest,id);
            if (anchor && (newer ? id<=anchor : id>=anchor)) continue;
            size_t at=0;
            while (at<item_count && (newer ? items[at].sequence<id : items[at].sequence>id)) ++at;
            if (at>=limit) continue;
            item_count=std::min(limit,item_count+1);
            for (size_t i=item_count-1;i>at;--i) items[i]=std::move(items[i-1]);
            items[at]=AlbumItem{(**entry).bytes,id,0,(**entry).info.size,{}};
        }
        if (!foreground || !gallery || epoch!=media_epoch) co_return pxa::Result<void>{};
        if (newer) std::reverse(items.begin(),items.begin()+item_count);
        grid_has_newer=item_count && items[0].sequence<newest;
        grid_has_older=item_count && items[item_count-1].sequence>oldest;
        page_label.set(std::to_string(grid_page+1)+" / "+std::to_string(std::max<size_t>(1,(media_total+limit-1)/limit))+
            " · "+std::to_string(media_total)+(album_video ? " 段" : " 张"));
        status.set(std::to_string(media_total)+(album_video ? " 段视频" : " 张照片")); refresh();
        for (size_t i=0;i<item_count;++i) {
            auto thumbnail=co_await make_thumbnail("album/"+items[i].name,items[i].size,album_video,&items[i].duration);
            if (!foreground || !gallery || epoch!=media_epoch) co_return pxa::Result<void>{};
            if (thumbnail) items[i].thumbnail.emplace(std::move(*thumbnail));
            dirty=true;
        }
        co_return pxa::Result<void>{};
    }
    void load_grid(uint32_t anchor, bool newer) {
        if (loading || playing) return;
        loading=true; grid_anchor=anchor; grid_newer=newer; page_size=size_t(grid_columns*grid_rows);
        status.set("正在读取相册"); page_label.set("正在读取");
        if (auto task=context->tasks().start(scan_grid(anchor,newer,++media_epoch,page_size)); !task) fail(task.error());
    }
    void grid_previous() {
        if (!loading && grid_has_newer && item_count) {
            if (grid_page) --grid_page;
            load_grid(items[0].sequence,true);
        }
    }
    void grid_next() {
        if (!loading && grid_has_older && item_count) {
            ++grid_page; load_grid(items[item_count-1].sequence,false);
        }
    }
    void open_item(size_t index) {
        if (loading || index>=item_count) return;
        const auto selected_id=items[index].sequence;
        for (auto& item:items) item.thumbnail.reset();
        gallery=false; chrome.set(true); refresh(); load_album(selected_id);
    }
    void pointer_event(const CanvasPointer& event) {
        if (!album || gallery) return;
        if (event.action()==PointerPhase::down) {
            if (pointer.active() && !pointer.owns(event)) pointer.reset();
            if (pointer.try_capture(event)) { pointer_x=event.x; pointer_y=event.y; }
            return;
        }
        if (!pointer.owns(event) || !event.ended()) return;
        const auto dx=event.x-pointer_x,dy=event.y-pointer_y;
        const bool up=event.action()==PointerPhase::up;
        (void)pointer.release(event);
        if (!up) return;
        if (!loading && !playing && std::abs(dx)>30 && std::abs(dx)>std::abs(dy)*2) {
            const auto target=dx<0 ? older_media : newer_media;
            if (target) load_album(target);
        } else if (std::abs(dx)<12 && std::abs(dy)<12) {
            chrome.set(!chrome.get()); dirty=true;
        }
    }
    void paint() {
        if (!dirty || !canvas.mounted()) return;
        const auto started = Clock::now();
        commands.reset(); commands.rect(0,0,width,height,0x080a0dff);
        if (album && gallery) {
            for (size_t i=0;i<item_count;++i) {
                const int x=grid_x+int(i%grid_columns)*(tile_width+4);
                const int y=grid_y+int(i/grid_columns)*(tile_height+4);
                commands.rect(x,y,tile_width,tile_height,0x20242bff);
                if (items[i].thumbnail) commands.image({x,y,tile_width,tile_height},*items[i].thumbnail,ImageFit::cover);
                if (album_video) {
                    char label[24];
                    std::snprintf(label,sizeof(label),"%u:%02u",items[i].duration/60,items[i].duration%60);
                    commands.rect(x,y+tile_height-14,tile_width,14,0x000000a0);
                    commands.text(x+2,y+tile_height-13,uint32_t(std::max(1,tile_width-4)),0xffffffff,label,2,2);
                }
            }
        } else {
            if (image) {
                const auto pixels=image->descriptor();
                const bool panel_preview=(pixels.width==840 && pixels.height==472) || (pixels.width==472 && pixels.height==840);
                const int native_width=camera_geometry::native_preview_extent(pixels.width,density_q16);
                const int native_height=camera_geometry::native_preview_extent(pixels.height,density_q16);
                if (!album && ((panel_preview && native_width<=width && native_height<=height) ||
                    (profile_count && ((profiles[selected].width==320 && profiles[selected].height==240) ||
                    (profiles[selected].width==240 && profiles[selected].height==320))))) {
                    commands.image({(width-native_width)/2,(height-native_height)/2,native_width,native_height},*image,ImageFit::contain);
                } else commands.image({0,0,width,height},*image,ImageFit::contain);
            }
            if (!album && album_thumbnail) {
                const int left_width=(safe_width-12-58-20)/2;
                const int x=safe_x+6+(left_width-40)/2;
                const int y=safe_y+safe_height-6-16-3-58+9;
                commands.image({x,y,40,40},*album_thumbnail,ImageFit::cover);
            }
        }
        if (auto shown=canvas.present(commands); !shown) {
            log("Camera display error: "+std::to_string(int(shown.error()))); return;
        }
        dirty=false;
        paint_us += std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-started).count();
        ++paint_calls;
    }
    pxa::Result<void> on_start(pxa::Context& ctx, std::span<const std::byte> config) {
        context = &ctx;
        ctx.tasks().on_error(this, [](void* self, pxa::Error error) noexcept { static_cast<CameraApp*>(self)->fail(error); });
        auto initialized = environment.initialize(config);
        if (initialized) {
            const auto display = environment.display().get();
            if (display.width != display.height) window_portrait = display.height > display.width;
            if (window_portrait) orient_profiles(*window_portrait);
            layout_revision.set(layout_revision.get() + 1);
        }
        if (!initialized) return initialized;
        return ctx.tasks().start(read_window());
    }
    void on_foreground(pxa::Context& ctx) {
        foreground = true; dirty = true;
        (void)ctx.window().keep_screen_on(true);
        (void)ctx.window().fullscreen(pxa::WindowBarMode::hidden, pxa::WindowBarMode::hidden);
        if (album) gallery_reload=true;
        if (!album && !running && !discovering) {
            discovering = true;
            if (auto task = ctx.tasks().start(discover()); !task) fail(task.error());
        }
    }
    pxa::Result<bool> on_event(pxa::Context&, const pxa::Event& event) {
        auto updated = environment.update(event);
        if (updated && *updated) { refresh(); if (album && gallery) gallery_reload=true; }
        if (window_portrait && *window_portrait != portrait_capture && !record && !video_file && !photograph)
            orient_profiles(*window_portrait);
        if (foreground && pending_back!=BackTarget::none && !loading) {
            const auto target=pending_back; pending_back=BackTarget::none;
            if (target==BackTarget::camera) close_album(); else return_to_grid();
        }
        if (foreground && album && gallery_reload && !loading) {
            gallery_reload=false;
            if (gallery) load_grid(grid_anchor,grid_newer); else load_album(media_sequence);
        }
        if (foreground && restart && !running) { restart = false; preview = true; start(); }
        paint(); return updated;
    }
    void on_background(pxa::Context&) {
        foreground = preview = photograph = record = restart = false;
        close_microphone();
        pointer.reset(); playing = false; speaker.reset(); ++media_epoch;
        if (!loading) playback_file.close();
        (void)context->window().keep_screen_on(foreground);
        port.reset(); permission.reset();
        if (!video_file) status.set("摄像头已释放");
    }
    pxa::BackAction on_back() {
        if (album) {
            if (gallery) close_album(); else return_to_grid();
            return pxa::BackAction::stay;
        }
        if (record || video_file) {
            record=false; shutter_label.set(""); status.set("正在保存视频"); refresh();
            return pxa::BackAction::stay;
        }
        return pxa::BackAction::close;
    }
    void on_stop(pxa::StopReason) { (void)context->window().keep_screen_on(false); port.reset(); permission.reset(); video_file.close(); recording_buffer.reset(); close_microphone(); speaker.reset(); if (playback_decoder) opus_decoder_destroy(playback_decoder); playback_decoder=nullptr; image.reset(); }
    auto camera_controls() {
        return SafeArea(environment.display(),Column(
            Row(Text("相机").font(Font::body).rgba(0xffffffffu).grow(),
                Button(orientation_label).on_click([this] { toggle_orientation(); }).font(Font::caption)
                    .height(26_dp).padding(0_dp).background(0x00000000u).rgba(0xffffffffu),
                Button(audio_label).on_click([this] {
                    if (record || video_file) { status.set("请先结束录像"); return; }
                    record_sound=!record_sound; audio_label.set(record_sound ? "录音 · 开" : "录音 · 关");
                }).height(26_dp).padding(0_dp).font(Font::caption).background(0x00000000u).rgba(0xffffffffu).visible(video_mode)
            ).gap(8_dp).fill(),
            Row(Button(profile_label).on_click([this] { next_profile(); }).font(Font::body).grow()
                    .height(26_dp).padding(0_dp).align(Align::center).justify(Justify::center)
                    .background(0x00000000u).rgba(0xffffffffu)).fill(),
            Column().fill().grow(),
            Column(
                Row(Button("照片").on_click([this] { select_mode(false); }).height(22_dp).padding(0_dp)
                        .font(Font::body).grow().align(Align::center).justify(Justify::center)
                        .background(0x00000000u).rgba(video_mode ? 0xbcc2cbffu : 0xffd87dffu),
                    Button("视频").on_click([this] { select_mode(true); }).height(22_dp).padding(0_dp)
                        .font(Font::body).grow().align(Align::center).justify(Justify::center)
                        .background(0x00000000u).rgba(video_mode ? 0xffd87dffu : 0xbcc2cbffu)).gap(8_dp).fill(),
                Row(Button(album_thumbnail ? "" : "相册").on_click([this] { open_album(); })
                        .grow().height(44_dp).padding(0_dp).align(Align::center).justify(Justify::center)
                        .font(Font::body).background(0x00000000u).rgba(0xffffffffu),
                    Button(shutter_label).on_click([this] { shutter(); }).width(58_dp).height(58_dp).padding(0_dp)
                        .align(Align::center).justify(Justify::center).font(Font::body)
                        .background(video_mode ? (record ? 0xc62935ffu : 0xf6555dffu) : 0xf4f5f7ffu)
                        .rgba(video_mode ? 0xffffffffu : 0x10151affu).border_width(3_dp)
                        .border_color(0xf4f5f7ffu).radius(29_dp),
                    Column().grow().height(44_dp)).gap(10_dp).align(Align::center).fill(),
                Text(status).font(Font::caption).text_align(TextAlign::center).rgba(0xd4d7ddffu).height(16_dp).fill()
            ).gap(3_dp).fill()
        ).gap(0_dp).padding(6_dp).fill().fill_height());
    }
    auto tile_button(size_t index, bool column=true) {
        return Button("").on_click([this,index] { open_item(index); })
            .width(Dp{tile_width}).height(Dp{tile_height}).padding(0_dp)
            .background(0x00000000u).visible(column && index<item_count);
    }
    auto grid_controls() {
        return SafeArea(environment.display(),Column(
            Row(Button("‹ 相机").on_click([this] { close_album(); }).height(24_dp).padding(0_dp)
                    .font(Font::caption).align(Align::center).background(0x00000000u).rgba(0xffffffffu),
                Text("相册").font(Font::caption).text_align(TextAlign::center).grow().rgba(0xffffffffu)
            ).fill(),
            Row(Button("照片").on_click([this] { select_mode(false); }).height(22_dp).padding(0_dp)
                    .font(Font::caption).grow().align(Align::center).justify(Justify::center)
                    .background(0x00000000u).rgba(album_video ? 0xb0b6c0ffu : 0xffd87dffu),
                Button("视频").on_click([this] { select_mode(true); }).height(22_dp).padding(0_dp)
                    .font(Font::caption).grow().align(Align::center).justify(Justify::center)
                    .background(0x00000000u).rgba(album_video ? 0xffd87dffu : 0xb0b6c0ffu)
            ).gap(8_dp).fill(),
            Column(Row(tile_button(0),tile_button(1),tile_button(2,grid_columns==3)).gap(4_dp).height(Dp{tile_height}).fill(),
                Row(tile_button(size_t(grid_columns)),tile_button(size_t(grid_columns+1)),
                    tile_button(size_t(grid_columns+2),grid_columns==3)).gap(4_dp).height(Dp{tile_height}).fill().visible(grid_rows>1)
            ).gap(4_dp).fill(),
            Row(Button("‹").on_click([this] { grid_previous(); }).width(30_dp).height(24_dp).padding(0_dp)
                    .align(Align::center).justify(Justify::center).background(0x00000000u).rgba(0xffffffffu),
                Text(page_label).font(Font::caption).text_align(TextAlign::center).grow().rgba(0xc9ced6ffu),
                Button("›").on_click([this] { grid_next(); }).width(30_dp).height(24_dp).padding(0_dp)
                    .align(Align::center).justify(Justify::center).background(0x00000000u).rgba(0xffffffffu)
            ).fill()
        ).gap(4_dp).padding(6_dp).fill().fill_height());
    }
    auto viewer_controls() {
        return SafeArea(environment.display(),Column(
            Row(Button("‹ 相册").on_click([this] { return_to_grid(); }).height(26_dp).padding(0_dp)
                    .align(Align::center).font(Font::caption).background(0x00000000u).rgba(0xffffffffu),
                Text(album_title).font(Font::caption).text_align(TextAlign::center).grow().rgba(0xffffffffu)
            ).fill(),
            Column().fill().grow(),
            Row(Button("‹").on_click([this] { if (newer_media) load_album(newer_media); }).grow().height(32_dp).padding(0_dp)
                    .align(Align::center).justify(Justify::center).background(0x00000000u).rgba(0xffffffffu),
                Button(shutter_label).on_click([this] { shutter(); }).width(52_dp).height(32_dp).padding(0_dp)
                    .align(Align::center).justify(Justify::center).font(Font::caption)
                    .background(0x00000050u).rgba(0xffffffffu).radius(16_dp).visible(album_video),
                Button("›").on_click([this] { if (older_media) load_album(older_media); }).grow().height(32_dp).padding(0_dp)
                    .align(Align::center).justify(Justify::center).background(0x00000000u).rgba(0xffffffffu)
            ).gap(12_dp).fill(),
            Text(status).font(Font::caption).text_align(TextAlign::center).rgba(0xc9ced6ffu).height(16_dp).fill()
        ).gap(4_dp).padding(6_dp).fill().fill_height()).visible(chrome);
    }
    auto content() {
        const auto metrics=environment.display().get();
        const auto density=std::max(uint32_t(1),metrics.density_q16);
        density_q16=density;
        const auto safe=safe_rectangle(metrics);
        const auto logical=[density](int pixels) { return int(uint64_t(std::max(0,pixels))*65536/density); };
        width=std::max(80,logical(metrics.width)); height=std::max(120,logical(metrics.height));
        safe_x=logical(safe.x); safe_y=logical(safe.y); safe_width=logical(safe.width); safe_height=logical(safe.height);
        grid_columns=safe_width>=220 ? 3 : 2;
        tile_width=std::max(24,(safe_width-12-(grid_columns-1)*4)/grid_columns);
        grid_rows=safe_height<190 ? 1 : 2;
        tile_height=std::max(18,(safe_height-94-(grid_rows-1)*4)/grid_rows);
        grid_x=safe_x+6; grid_y=safe_y+60; dirty=true;
        using Controls=CameraControls<decltype(camera_controls()),decltype(grid_controls()),decltype(viewer_controls())>;
        if (!album) return Controls(std::in_place_index<0>,camera_controls());
        if (gallery) return Controls(std::in_place_index<1>,grid_controls());
        return Controls(std::in_place_index<2>,viewer_controls());
    }
    auto view() {
        return Stack(Canvas(canvas).on_pointer([this](const CanvasPointer& event) { pointer_event(event); }).fill().fill_height(),
            Refresh(layout_revision,[this] { return content(); })
        ).background(0x080a0dffu).fill().fill_height();
    }
};
PXA_APPLICATION(CameraApp)
