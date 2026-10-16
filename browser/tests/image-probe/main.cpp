#include <pxa/app.hpp>
#include <pxa/canvas.hpp>
#include <pxa/image_frames.hpp>
#include <array>
#include <cstdio>
#include <ctime>

using namespace pxa::ui;
using namespace pxa::ui::literals;

namespace {
std::uint64_t micros() {
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return std::uint64_t(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
}
}

// Offline diagnostic only: copying the packaged source is a separate setup
// phase. Actual decode borrows a File on the Host, exactly like HTTP decoding;
// encoded input never traverses Guest memory during the measured operation.
struct ImageProbe {
    pxa::Context* context = nullptr;
    State<std::string> status{"Preparing"};
    CanvasRef canvas;
    CanvasCommands<192> draw;
    std::array<std::byte, 4064> scratch{};
    std::optional<pxa::DecodedImage> image;
    bool busy = true, foreground = true;
    std::uint64_t rendered_at = 0;
    unsigned frames = 0;

    void log(std::string_view text) { (void)context->log().write(pxa::LogLevel::info, text); }
    void fail(pxa::Error error) {
        char text[96]; std::snprintf(text, sizeof(text), "IMAGE PROBE ERROR code=%d", int(error));
        log(text); status.set("Error"); busy = false;
    }
    pxa::Task<void> prepare(bool large) {
        busy = true; image.reset(); status.set("Preparing");
        auto file = co_await context->fs().open("image-probe.bin", pxa::OpenMode::write |
            pxa::OpenMode::create | pxa::OpenMode::truncate);
        if (!file) co_return std::unexpected(file.error());
        std::uint32_t offset = 0;
        for (;;) {
            auto part = co_await context->assets().read(large ? "assets/large.bin" : "assets/common.bin", offset, scratch);
            if (!part) co_return std::unexpected(part.error());
            auto bytes = std::span<const std::byte>(scratch).first(part->bytes);
            if (bytes.empty() && !part->eof()) co_return std::unexpected(pxa::Error::io_error);
            while (!bytes.empty()) {
                auto written = file->write(bytes);
                if (!written || !*written) co_return std::unexpected(written ? pxa::Error::io_error : written.error());
                bytes = bytes.subspan(*written);
            }
            offset += part->bytes;
            if (part->eof()) break;
        }
        auto closed = file->close();
        if (!closed) co_return closed;
        char text[96]; std::snprintf(text, sizeof(text), "IMAGE PROBE READY large=%u bytes=%u", unsigned(large), offset);
        log(text); status.set(large ? "Large ready" : "Ready"); busy = false;
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> decode() {
        busy = true; image.reset(); status.set("Decoding");
        auto file = co_await context->fs().open("image-probe.bin", pxa::OpenMode::read);
        if (!file) co_return std::unexpected(file.error());
        const auto start = micros(); log("IMAGE PROBE BEGIN");
        auto value = co_await context->assets().images().decode(std::move(*file),
            {.max_width=236,.max_height=1024,.max_pixels=32768,.max_encoded_bytes=524288,.formats=pxa::ImageFormats::jpeg});
        const auto elapsed = micros() - start;
        char text[192];
        if (!value) {
            std::snprintf(text,sizeof(text),"IMAGE PROBE DECODE_ERROR code=%d elapsed_us=%llu",int(value.error()),static_cast<unsigned long long>(elapsed));
            log(text); busy=false;status.set("Decode failed");
            co_return pxa::Result<void>{};
        }
        image.emplace(std::move(*value));
        const auto& descriptor=image->image().descriptor();
        std::snprintf(text,sizeof(text),"IMAGE PROBE DONE elapsed_us=%llu bytes=%u width=%u height=%u resident=%u",
            static_cast<unsigned long long>(elapsed),descriptor.source_bytes,descriptor.width,descriptor.height,descriptor.resident_bytes);
        log(text);status.set("Decoded");busy=false;frames=0;
        co_return pxa::Result<void>{};
    }
    auto view() {
        return Column(Text(status),
            Row(Button("Decode").on_click([this] {if(!busy) (void)context->tasks().start(decode());}),
                Button("Release").on_click([this] {if(!busy) {image.reset();status.set("Released");log("IMAGE PROBE RELEASED");}}),
                Button("Large").on_click([this] {if(!busy) (void)context->tasks().start(prepare(true));})),
            Canvas(canvas).fill().fill_height()).fill().fill_height().padding(20_dp);
    }
    pxa::Result<void> on_start(pxa::Context& ctx) {
        context=&ctx;
        ctx.tasks().on_error(this,[](void* self,pxa::Error error) noexcept {static_cast<ImageProbe*>(self)->fail(error);});
        return ctx.tasks().start(prepare(false));
    }
    void on_background(pxa::Context&) { foreground=false; }
    void on_foreground(pxa::Context&) { foreground=true; }
    void on_frame(pxa::Context&,pxa::game::FrameTick tick) {
        if (!foreground || !canvas.mounted() || tick.timestamp_us-rendered_at<33333) return;
        rendered_at=tick.timestamp_us;
        draw.reset();draw.rect(0,0,296,240,0x142434ff);
        if (image) draw.image({8,-int(frames%160),236,472},image->image(),ImageFit::stretch);
        draw.rect(250,int(frames%120),12,12,0xe4c444ff);
        auto shown=canvas.present(draw);
        if (!shown) {fail(shown.error());return;}
        ++frames;
    }
};
PXA_APPLICATION(ImageProbe)
