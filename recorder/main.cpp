// SPDX-License-Identifier: MIT
#include "ogg.hpp"
#include "ui.hpp"
#include <opus.h>
#include <pxa/events.hpp>
#include <pxa/ui_geometry.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <optional>
#include <vector>

namespace recorder {
using namespace pxa;
using namespace pxa::ui;
using Clock = std::chrono::steady_clock;
enum class Screen { home, library, detail, confirm };
enum class Mode { idle, preparing, recording, paused, saving, playing };
struct Entry {
    std::string name;
    uint64_t bytes = 0;
};
struct App;
struct View : ViewModifiers {
    static constexpr Capacity capacity{100, 48, 24, 0, 1};
    App* app;
    std::array<std::function<void()>, 16> callbacks{};
    explicit View(App* value) : app(value) {}
    template<class Page> bool render(Page& page, uint32_t parent);
};
struct Route {
    App* app;
    explicit Route(App* value) : app(value) {}
    auto view();
};
inline std::string duration(uint64_t samples) {
    const auto seconds = samples / 16000;
    char text[32];
    std::snprintf(text, sizeof text, "%02llu:%02llu:%02llu",
                  seconds / 3600, (seconds / 60) % 60, seconds % 60);
    return text;
}
inline std::string size_label(uint64_t bytes) {
    char text[40];
    if (bytes < 1024 * 1024)
        std::snprintf(text, sizeof text, "%.1f KiB", double(bytes) / 1024);
    else
        std::snprintf(text, sizeof text, "%.1f MiB", double(bytes) / (1024 * 1024));
    return text;
}

struct App {
    Context* ctx = nullptr;
    Environment<DisplayMetrics, UiCapabilities> environment;
    Navigator<1, 8192, 32> navigation_;
    TaskScope jobs;
    Permission mic_permission, playback_permission;
    AudioCapture microphone;
    File file;
    std::optional<AudioSession> speaker;
    OpusDecoder* decoder = nullptr;
    CanvasRef meter;
    CanvasCommands<1024> drawing;
    State<std::string> status{"正在读取录音…"}, timer{"00:00:00"};
    State<std::string> primary{"开始录音"}, pause_label{"暂停录音"};
    State<std::string> play_label{"播放录音"}, library_label{"我的录音"};
    State<bool> idle{false}, can_pause{false};
    State<bool> operation_error{false};
    std::vector<Entry> entries;
    Entry selected;
    std::string pending_name;
    Screen screen = Screen::home;
    Mode mode = Mode::idle;
    bool busy = false, want_record = false, want_pause = false;
    bool want_play = false, foreground = true, exit_after_save = false;
    bool dirty = true, eof = false;
    uint32_t next_id = 1, serial = 0, sequence = 0;
    uint32_t read_serial = 0, read_sequence = 0;
    uint64_t samples = 0, played = 0, total_samples = 0;
    size_t library_page = 0, pcm_count = 0, pcm_position = 0;
    std::array<std::byte, ogg::max_packet> packet{};
    std::array<std::byte, ogg::max_page> page_buffer{};
    std::array<opus_int16, 960> pcm{};
    int meter_width = 160, meter_height = 28;
    std::array<unsigned, 25> levels{};
    unsigned level_cursor = 0;

    void log(std::string_view message) { (void)ctx->log().write(LogLevel::info, message); }
    void clock() { (void)ctx->clock().set_period(mode == Mode::idle ? 250 : 16); }
    void sync() {
        const bool recording = mode == Mode::recording || mode == Mode::paused;
        idle.set(mode == Mode::idle && !busy);
        can_pause.set(recording);
        primary.set(recording ? "停止并保存" : mode == Mode::preparing ? "正在准备…" :
                    mode == Mode::saving ? "正在保存…" : "开始录音");
        pause_label.set(want_pause ? "继续录音" : "暂停录音");
        play_label.set(mode == Mode::playing ? "停止播放" : "播放录音");
        library_label.set("我的录音 · " + std::to_string(entries.size()));
        dirty = true;
        clock();
    }
    void show(Screen value) {
        screen = value;
        auto result = navigation_.replace<Route>(this);
        if (!result && result.error() != Error::busy) log("Recorder navigation failed");
        dirty = true;
    }
    void release_audio() {
        (void)microphone.close();
        speaker.reset();
        if (decoder) { opus_decoder_destroy(decoder); decoder = nullptr; }
    }
    void fail(Error error) {
        release_audio();
        (void)file.close();
        want_record = want_play = want_pause = false;
        mode = Mode::idle;
        busy = false;
        exit_after_save = false;
        if (error == Error::denied) status.set("未获得音频权限，可重试授权");
        else if (error == Error::unsupported || error == Error::unavailable)
            status.set("音频设备暂不可用，请稍后重试");
        else if (error == Error::quota_exceeded) status.set("存储空间不足，请删除旧录音后重试");
        else status.set("操作失败，可重试 · " + std::to_string(int(error)));
        if (!pending_name.empty()) status.set(status.get() + "\n未完成录音保留为 .part 文件");
        log("Recorder error " + std::to_string(int(error)));
        operation_error.set(true);
        (void)ctx->window().keep_screen_on(false);
        sync();
    }
    Task<void> execute(Task<void> task) {
        auto result = co_await std::move(task);
        if (result && file && !want_record &&
            (mode == Mode::recording || mode == Mode::paused || mode == Mode::saving))
            result = co_await finish_recording();
        if (result && mode == Mode::playing && !want_play) stop_playback();
        busy = false;
        if (!result) fail(result.error());
        else sync();
        co_return Result<void>{};
    }
    void run(Task<void> task) {
        if (busy) return;
        busy = true;
        operation_error.set(false);
        idle.set(false);
        if (auto result = jobs.start(execute(std::move(task))); !result) fail(result.error());
    }
    Task<void> write_all(std::span<const std::byte> bytes) {
        const auto deadline = Clock::now() + std::chrono::seconds(10);
        while (!bytes.empty()) {
            auto result = file.write(bytes);
            if (result) {
                if (!*result) co_return std::unexpected(Error::io_error);
                bytes = bytes.subspan(*result);
            } else if (result.error() != Error::would_block) co_return std::unexpected(result.error());
            if (bytes.empty()) break;
            if (Clock::now() > deadline) co_return std::unexpected(Error::timed_out);
            if (auto yielded = co_await ctx->clock().yield(); !yielded) co_return yielded;
        }
        co_return Result<void>{};
    }
    Task<void> read_exact(std::span<std::byte> bytes) {
        const auto deadline = Clock::now() + std::chrono::seconds(10);
        while (!bytes.empty()) {
            auto result = file.read(bytes);
            if (result) {
                if (!*result) co_return std::unexpected(Error::io_error);
                bytes = bytes.subspan(*result);
            } else if (result.error() != Error::would_block) co_return std::unexpected(result.error());
            if (bytes.empty()) break;
            if (Clock::now() > deadline) co_return std::unexpected(Error::timed_out);
            if (auto yielded = co_await ctx->clock().yield(); !yielded) co_return yielded;
        }
        co_return Result<void>{};
    }
    Task<void> write_page(std::span<const std::byte> bytes, uint8_t flags = 0) {
        const auto count = ogg::page(page_buffer, bytes, serial, sequence++, samples * 3, flags);
        if (!count) co_return std::unexpected(Error::limit_exceeded);
        co_return co_await write_all(std::span(page_buffer).first(count));
    }
    Task<size_t> read_page() {
        if (auto result = co_await read_exact(std::span(page_buffer).first(27)); !result)
            co_return std::unexpected(result.error());
        if (!ogg::header(page_buffer) || ogg::get32(page_buffer.data() + 18) != read_sequence)
            co_return std::unexpected(Error::protocol_error);
        if (!read_sequence) read_serial = ogg::get32(page_buffer.data() + 14);
        if (ogg::get32(page_buffer.data() + 14) != read_serial)
            co_return std::unexpected(Error::protocol_error);
        const auto segments = unsigned(page_buffer[26]);
        if (auto result = co_await read_exact(std::span(page_buffer).subspan(27, segments)); !result)
            co_return std::unexpected(result.error());
        size_t count = 0;
        for (unsigned i = 0; i < segments; ++i) {
            const auto part = unsigned(page_buffer[27 + i]);
            if ((i + 1 < segments && part != 255) || (i + 1 == segments && part == 255))
                co_return std::unexpected(Error::protocol_error);
            count += part;
        }
        if (count > packet.size()) co_return std::unexpected(Error::limit_exceeded);
        if (auto result = co_await read_exact(std::span(page_buffer).subspan(27 + segments, count)); !result)
            co_return std::unexpected(result.error());
        if (ogg::crc(std::span(page_buffer).first(27 + segments + count)) != ogg::get32(page_buffer.data() + 22))
            co_return std::unexpected(Error::protocol_error);
        std::copy_n(page_buffer.begin() + 27 + segments, count, packet.begin());
        eof = (unsigned(page_buffer[5]) & 4) != 0;
        ++read_sequence;
        co_return count;
    }
    Task<void> scan() {
        entries.clear();
        auto directory = co_await ctx->fs().directory("recordings");
        if (!directory) co_return std::unexpected(directory.error());
        unsigned visited = 0;
        for (;;) {
            auto item = co_await directory->next();
            if (!item) co_return std::unexpected(item.error());
            if (!*item) break;
            const auto& entry = **item;
            unsigned id = 0;
            if (entry.name().starts_with("rec-") && entry.bytes.size() >= 15 &&
                std::sscanf(entry.bytes.c_str(), "rec-%u", &id) == 1 && id < 999999)
                next_id = std::max(next_id, id + 1);
            if (entry.info.kind == FileKind::regular && entry.name().starts_with("rec-") && entry.name().ends_with(".opus"))
                entries.push_back({entry.bytes, entry.info.size});
            if (++visited % 16 == 0) {
                if (auto yielded = co_await ctx->clock().yield(); !yielded) co_return yielded;
            }
        }
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.name > b.name; });
        library_page = std::min(library_page, entries.empty() ? size_t(0) : (entries.size() - 1) / 6);
        co_return Result<void>{};
    }
    Task<void> initialize() {
        auto directory = co_await ctx->fs().make_directory("recordings");
        // Existing directories are reported differently by older Hosts.
        if (!directory) {
            auto info = co_await ctx->fs().stat("recordings");
            if (!info || info->kind != FileKind::directory) co_return std::unexpected(directory.error());
        }
        if (auto result = co_await scan(); !result) co_return result;
        status.set("随时记录，留住灵感");
        co_return Result<void>{};
    }
    Task<void> open_microphone() {
        if (!mic_permission) {
            auto grant = co_await ctx->permissions().acquire("audio.capture", "microphone");
            if (!grant) co_return std::unexpected(grant.error());
            mic_permission = std::move(*grant);
        }
        auto opened = co_await ctx->capture().open(mic_permission);
        if (!opened) co_return std::unexpected(opened.error());
        microphone = std::move(*opened);
        co_return Result<void>{};
    }
    Task<void> begin_recording() {
        if (auto result = co_await open_microphone(); !result) co_return result;
        if (!want_record || !foreground) { (void)microphone.close(); mode = Mode::idle; co_return Result<void>{}; }
        pending_name.clear();
        for (;;) {
            if (next_id > 999999) co_return std::unexpected(Error::limit_exceeded);
            char name[40];
            std::snprintf(name, sizeof name, "rec-%06u", next_id++);
            pending_name = "recordings/" + std::string(name);
            auto existing = co_await ctx->fs().stat(pending_name + ".opus");
            if (existing) continue;
            if (existing.error() != Error::not_found) co_return std::unexpected(existing.error());
            auto opened = co_await ctx->fs().open(pending_name + ".part", OpenMode::write | OpenMode::create | OpenMode::exclusive);
            if (opened) { file = std::move(*opened); break; }
            if (opened.error() != Error::busy) co_return std::unexpected(opened.error());
        }
        serial = next_id;
        sequence = 0;
        samples = 0;
        int decoder_error = 0;
        decoder = opus_decoder_create(16000, 1, &decoder_error);
        if (!decoder) co_return std::unexpected(Error::resource_limit);
        timer.set(duration(0));
        levels.fill(0);
        if (auto result = co_await write_page(ogg::head, 2); !result) co_return result;
        if (auto result = co_await write_page(ogg::tags); !result) co_return result;
        mode = Mode::recording;
        status.set("正在录音 · 16 kHz 单声道");
        log("Recorder recording");
        co_return Result<void>{};
    }
    Task<void> finish_recording() {
        mode = Mode::saving;
        sync();
        release_audio();
        if (auto result = co_await write_page({}, 4); !result) co_return result;
        if (auto result = file.close(); !result) co_return result;
        if (!samples) {
            if (auto result = co_await ctx->fs().remove(pending_name + ".part"); !result) co_return result;
            status.set("录音时间太短，尚未生成文件");
        } else {
            if (auto result = co_await ctx->fs().rename(pending_name + ".part", pending_name + ".opus"); !result) co_return result;
            log("Recorder saved " + pending_name + ".opus samples=" + std::to_string(samples));
            status.set("已保存 · " + duration(samples));
        }
        pending_name.clear();
        mode = Mode::idle;
        want_pause = false;
        (void)ctx->window().keep_screen_on(false);
        if (auto result = co_await scan(); !result) co_return result;
        if (exit_after_save) { exit_after_save = false; (void)ctx->window().request_back(); }
        co_return Result<void>{};
    }
    Task<void> record_step() {
        if (!want_record) co_return co_await finish_recording();
        if (want_pause) {
            (void)microphone.close();
            mode = Mode::paused;
            status.set("录音已暂停");
            co_return Result<void>{};
        }
        if (!microphone) {
            if (auto result = co_await open_microphone(); !result) co_return result;
            opus_decoder_ctl(decoder, OPUS_RESET_STATE);
            mode = Mode::recording;
            status.set("正在录音 · 16 kHz 单声道");
        }
        // Bound each dispatch, including when the Host has queued many packets.
        for (unsigned i = 0; i < 4 && want_record && !want_pause; ++i) {
            auto count = microphone.read(packet);
            if (!count) {
                if (count.error() == Error::would_block) break;
                co_return std::unexpected(count.error());
            }
            if (!*count || *count > packet.size()) co_return std::unexpected(Error::protocol_error);
            const int frames = opus_packet_get_nb_samples(reinterpret_cast<const unsigned char*>(packet.data()), int(*count), 16000);
            if (frames <= 0 || frames > 960) co_return std::unexpected(Error::protocol_error);
            // Granule positions use the packet duration, including 40/60 ms Hosts.
            samples += unsigned(frames);
            if (auto result = co_await write_page(std::span(packet).first(*count)); !result) co_return result;
            const auto decoded = opus_decode(decoder, reinterpret_cast<const unsigned char*>(packet.data()),
                                             int(*count), pcm.data(), int(pcm.size()), 0);
            if (decoded <= 0) co_return std::unexpected(Error::protocol_error);
            unsigned peak = 0;
            for (int sample = 0; sample < decoded; ++sample)
                peak = std::max(peak, unsigned(std::abs(int(pcm[size_t(sample)]))));
            levels[level_cursor++ % levels.size()] = std::min(24u, peak / 1000 + 2);
        }
        timer.set(duration(samples));
        dirty = true;
        co_return Result<void>{};
    }
    void toggle_recording() {
        if (mode == Mode::recording || mode == Mode::paused) {
            want_record = false;
            mode = Mode::saving;
            sync();
            if (!busy) run(finish_recording());
        } else if (mode == Mode::idle && !busy) {
            want_record = true;
            want_pause = false;
            mode = Mode::preparing;
            status.set("正在准备麦克风…");
            (void)ctx->window().keep_screen_on(true);
            sync();
            run(begin_recording());
        }
    }
    void toggle_pause() {
        if (mode != Mode::recording && mode != Mode::paused) return;
        want_pause = !want_pause;
        sync();
    }
    Task<void> select_entry(size_t index) {
        selected = entries[index];
        total_samples = 0;
        auto opened = co_await ctx->fs().open("recordings/" + selected.name);
        if (!opened) co_return std::unexpected(opened.error());
        file = std::move(*opened);
        if (selected.bytes < 27) co_return std::unexpected(Error::protocol_error);
        auto position = co_await file.seek(int64_t(selected.bytes - 27));
        if (!position) co_return std::unexpected(position.error());
        if (auto result = co_await read_exact(std::span(page_buffer).first(27)); !result) co_return result;
        if (!ogg::header(page_buffer) || page_buffer[5] != std::byte{4} ||
            page_buffer[26] != std::byte{} || ogg::crc(std::span(page_buffer).first(27)) != ogg::get32(page_buffer.data() + 22))
            co_return std::unexpected(Error::protocol_error);
        total_samples = ogg::granule(page_buffer.data()) / 3;
        (void)file.close();
        timer.set(duration(0) + " / " + duration(total_samples));
        status.set(size_label(selected.bytes) + " · Opus · 单声道");
        show(Screen::detail);
        co_return Result<void>{};
    }
    Task<void> begin_playback() {
        auto opened = co_await ctx->fs().open("recordings/" + selected.name);
        if (!opened) co_return std::unexpected(opened.error());
        file = std::move(*opened);
        read_sequence = 0;
        eof = false;
        auto head_size = co_await read_page();
        if (!head_size || *head_size != ogg::head.size() || !std::equal(ogg::head.begin(), ogg::head.end(), packet.begin()))
            co_return std::unexpected(Error::protocol_error);
        auto tag_size = co_await read_page();
        if (!tag_size || *tag_size < 16 || std::memcmp(packet.data(), "OpusTags", 8))
            co_return std::unexpected(Error::protocol_error);
        if (!playback_permission) {
            auto grant = co_await ctx->permissions().acquire("audio.playback", "media");
            if (!grant) co_return std::unexpected(grant.error());
            playback_permission = std::move(*grant);
        }
        auto audio = co_await ctx->audio().open(playback_permission);
        if (!audio) co_return std::unexpected(audio.error());
        speaker.emplace(std::move(*audio));
        if (speaker->format().sample_rate != 16000 || speaker->format().channels != 1)
            co_return std::unexpected(Error::unsupported);
        if (auto result = co_await speaker->graph(0); !result) co_return result;
        int error = 0;
        decoder = opus_decoder_create(16000, 1, &error);
        if (!decoder) co_return std::unexpected(Error::resource_limit);
        pcm_count = pcm_position = 0;
        played = 0;
        if (!want_play || !foreground) { stop_playback(); co_return Result<void>{}; }
        mode = Mode::playing;
        status.set("正在播放 · " + size_label(selected.bytes));
        log("Recorder playing " + selected.name);
        co_return Result<void>{};
    }
    void stop_playback(bool ended = false) {
        release_audio();
        (void)file.close();
        want_play = false;
        mode = Mode::idle;
        status.set(ended ? "播放结束" : "播放已停止");
        log(ended ? "Recorder playback ended" : "Recorder playback stopped");
        sync();
    }
    Task<void> playback_step() {
        if (!want_play) { stop_playback(); co_return Result<void>{}; }
        for (unsigned i = 0; i < 4; ++i) {
            if (pcm_position == pcm_count) {
                if (eof) {
                    auto state = co_await speaker->query();
                    if (!state) co_return std::unexpected(state.error());
                    if (!state->queued_samples) stop_playback(true);
                    break;
                }
                auto count = co_await read_page();
                if (!count) co_return std::unexpected(count.error());
                if (!*count) { if (eof) continue; co_return std::unexpected(Error::protocol_error); }
                int decoded = opus_decode(decoder, reinterpret_cast<const unsigned char*>(packet.data()),
                                          int(*count), pcm.data(), int(pcm.size()), 0);
                if (decoded <= 0) co_return std::unexpected(Error::protocol_error);
                pcm_count = size_t(decoded);
                pcm_position = 0;
            }
            const auto count = std::min(size_t(320), pcm_count - pcm_position);
            auto result = speaker->write_pcm(std::as_bytes(std::span(pcm).subspan(pcm_position, count)));
            if (!result) {
                if (result.error() == Error::would_block) break;
                co_return std::unexpected(result.error());
            }
            if (!*result) co_return std::unexpected(Error::io_error);
            pcm_position += *result / 2;
            played += *result / 2;
        }
        timer.set(duration(std::min(played, total_samples)) + " / " + duration(total_samples));
        co_return Result<void>{};
    }
    void toggle_playback() {
        if (mode == Mode::playing) {
            want_play = false;
            if (!busy) stop_playback();
        } else if (mode == Mode::idle && !busy) {
            want_play = true;
            mode = Mode::preparing;
            status.set("正在准备播放…");
            sync();
            run(begin_playback());
        }
    }
    Task<void> erase() {
        auto result = co_await ctx->fs().remove("recordings/" + selected.name);
        if (!result) co_return result;
        log("Recorder deleted " + selected.name);
        if (auto refreshed = co_await scan(); !refreshed) co_return refreshed;
        status.set("录音已删除");
        show(Screen::library);
        co_return Result<void>{};
    }
    void back() {
        if (screen == Screen::home) { (void)ctx->window().request_back(); return; }
        if (mode == Mode::playing) {
            want_play = false;
            if (busy) return;
            stop_playback();
        }
        if (busy) return;
        if (screen == Screen::confirm) show(Screen::detail);
        else if (screen == Screen::detail) { status.set(""); show(Screen::library); }
        else if (screen == Screen::library) { timer.set(duration(samples)); status.set("随时记录，留住灵感"); operation_error.set(false); show(Screen::home); }
        else (void)ctx->window().request_back();
    }
    BackAction on_back() {
        if (mode == Mode::recording || mode == Mode::paused || mode == Mode::saving) {
            exit_after_save = true;
            want_record = false;
            if (!busy && mode != Mode::saving) run(finish_recording());
            return BackAction::stay;
        }
        if (busy) return BackAction::stay;
        if (screen == Screen::home) return BackAction::close;
        back();
        return BackAction::stay;
    }
    void paint() {
        if (!dirty || !meter.mounted()) return;
        drawing.reset();
        drawing.rect(0, 0, meter_width, meter_height, theme::paper);
        const int center = meter_height / 2;
        for (unsigned i = 0; i < levels.size(); ++i) {
            const bool live = mode == Mode::recording;
            const auto level = live ? levels[(level_cursor + i) % levels.size()] : 3u;
            const int bar = std::min(std::max(3, int(level)), meter_height - 4);
            const int x = (int(i) * (meter_width - 8) / int(levels.size())) + 4;
            drawing.rect(x, center - bar / 2, 3, bar, live ? theme::green : 0xb3cfc2ff, 1);
        }
        if (meter.present(drawing)) dirty = false;
    }
    Result<void> on_start(Context& context, std::span<const std::byte> config) {
        ctx = &context;
        if (auto result = environment.initialize(config); !result) return result;
        jobs.on_error(this, [](void* owner, Error error) noexcept { static_cast<App*>(owner)->fail(error); });
        (void)ctx->window().fullscreen(WindowBarMode::hidden, WindowBarMode::hidden);
        show(Screen::home);
        sync();
        run(initialize());
        return {};
    }
    Result<bool> on_event(Context&, const Event& event) {
        jobs.reap();
        auto result = environment.update(event);
        if (result && *result) show(screen);
        paint();
        return result;
    }
    void on_frame(Context&, game::FrameTick) {
        jobs.reap();
        if (!busy) {
            if (mode == Mode::recording || mode == Mode::paused || mode == Mode::saving) {
                if (mode != Mode::paused || !want_pause || !want_record) run(record_step());
            } else if (mode == Mode::playing) run(playback_step());
        }
        paint();
    }
    void on_foreground(Context&) {
        foreground = true;
        (void)ctx->window().fullscreen(WindowBarMode::hidden, WindowBarMode::hidden);
        dirty = true;
        clock();
    }
    void on_background(Context&) {
        foreground = false;
        want_record = want_play = false;
        // A file transaction already in flight finishes before finalization.
        (void)microphone.close();
        if (!busy) {
            if (mode == Mode::recording || mode == Mode::paused) run(finish_recording());
            else if (mode == Mode::playing) stop_playback();
        }
    }
    void on_stop(StopReason) { jobs.cancel(); release_audio(); (void)file.close(); }
    auto& navigation() { return navigation_; }
};

auto Route::view() { return SafeArea(app->environment.display(), View(app)).background(theme::paper); }

template<class Page> bool View::render(Page& page, uint32_t parent) {
    auto& a = *app;
    theme::Painter p(page, callbacks);
    const auto metrics = a.environment.display().get();
    const auto safe = safe_rectangle(metrics);
    const auto density = std::max(1u, metrics.density_q16);
    const int width = int(uint64_t(std::max(1, safe.width)) * 65536 / density);
    const int height = int(uint64_t(std::max(1, safe.height)) * 65536 / density);
    p.roomy = width >= 380;
    p.header_size = height < 200 ? 28 : 34;
    auto root = p.box(parent, false, 4);
    p.fill(root, true);
    p.padding(root, 10, 5, 10, 5);
    auto frame = p.box(root, false, 4);
    p.fill(frame, true);
    auto body = p.scroll(p.body(frame));
    if (a.screen == Screen::home) {
        const bool compact = height < 240;
        p.ok &= p.tx.u8(body, protocol::align, 1);
        auto time = p.text(body, a.timer, compact ? Font::title : Font::headline);
        p.ok &= p.tx.u8(time, protocol::text_align, 1);
        if (!compact) {
            const int canvas_width = std::clamp(width - 28, 80, 360);
            const int canvas_height = height < 230 ? 22 : 34;
            a.meter_width = int(uint64_t(canvas_width) * density / 65536);
            a.meter_height = int(uint64_t(canvas_height) * density / 65536);
            p.canvas(body, a.meter, canvas_width, canvas_height);
            auto state = p.text(body, a.status, Font::caption, theme::muted);
            p.ok &= p.tx.u8(state, protocol::text_align, 1);
        }
        if (compact) {
            auto state = p.text(body, a.status, Font::caption, theme::muted);
            p.visible(state, a.operation_error);
        }
        // Keep pending actions in the native three-key focus group. Their
        // handlers guard mode/busy while the button shows its progress label.
        p.button(body, a.primary, [&a] { a.toggle_recording(); }, true);
        auto pause = p.button(body, a.pause_label, [&a] { a.toggle_pause(); });
        p.visible(pause, a.can_pause);
        auto library = p.button(body, a.library_label, [&a] { a.status.set(""); a.show(Screen::library); });
        p.enabled(library, a.idle);
        p.visible(library, a.idle);
        auto title = std::string_view("轻录音");
        p.header(frame, title, [&a] { a.back(); }, true);
    } else if (a.screen == Screen::library) {
        p.text(body, a.status, Font::caption, theme::muted);
        if (a.entries.empty()) {
            p.text(body, "还没有录音", Font::headline);
            p.text(body, "回到首页，开始记录第一段声音。", Font::caption, theme::muted);
            p.button(body, "去录音", [&a] { a.back(); }, true);
        }
        for (size_t i = a.library_page * 6; i < std::min(a.entries.size(), (a.library_page + 1) * 6); ++i) {
            const auto& entry = a.entries[i];
            auto label = std::string("录音 ") + entry.name.substr(4, 6) + "\n" + size_label(entry.bytes);
            p.button(body, label, [&a, i] { a.run(a.select_entry(i)); });
        }
        if (a.entries.size() > 6) {
            auto label = std::to_string(a.library_page + 1) + " / " + std::to_string((a.entries.size() + 5) / 6);
            p.text(body, label, Font::caption, theme::muted);
            if ((a.library_page + 1) * 6 < a.entries.size())
                p.button(body, "下一页", [&a] {
                    if (a.busy) return;
                    ++a.library_page;
                    a.show(Screen::library);
                });
            if (a.library_page)
                p.button(body, "上一页", [&a] {
                    if (a.busy) return;
                    --a.library_page;
                    a.show(Screen::library);
                });
        }
        auto title = std::string_view("我的录音");
        p.header(frame, title, [&a] { a.back(); });
    } else if (a.screen == Screen::detail) {
        auto name = std::string("录音 ") + a.selected.name.substr(4, 6);
        p.text(body, name, Font::headline);
        p.text(body, a.timer, Font::title, theme::green);
        p.text(body, a.status, Font::caption, theme::muted);
        p.button(body, a.play_label, [&a] { a.toggle_playback(); }, true);
        auto erase = p.button(body, "删除录音", [&a] { a.show(Screen::confirm); });
        p.enabled(erase, a.idle);
        auto title = std::string_view("录音详情");
        p.header(frame, title, [&a] { a.back(); });
    } else {
        p.text(body, "删除这段录音？", Font::headline);
        auto name = std::string("录音 ") + a.selected.name.substr(4, 6);
        p.text(body, name, Font::title, theme::green);
        p.text(body, "删除后无法恢复。", Font::caption, theme::muted);
        p.text(body, a.status, Font::caption, theme::muted);
        p.button(body, "保留录音", [&a] { a.back(); }, true);
        p.button(body, "确认删除", [&a] { a.run(a.erase()); });
        auto title = std::string_view("删除确认");
        p.header(frame, title, [&a] { a.back(); });
    }
    return p.ok;
}
} // namespace recorder
PXA_APPLICATION(recorder::App)
