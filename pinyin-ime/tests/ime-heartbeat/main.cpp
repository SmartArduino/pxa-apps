#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <charconv>
#include <ctime>

using namespace pxa::ui;
using namespace pxa::ui::literals;

struct SystemInput {
    State<std::string> text{""};
    State<std::string> status{"点输入框，或按打开"};
    TextInputRef editor;
    pxa::Context* context = nullptr;
    std::uint64_t previous = 0;
    void report(pxa::Result<void> result, const char* message) {
        status.set(result ? message : "Host 不支持或输入框不可见");
        (void)context->log().write(pxa::LogLevel::info,
            result ? message : "IME control failed");
    }
    auto view() {
        return Column(
            Text("系统输入法").font(Font::title),
            TextInput(text, editor).max_bytes(2048).single_line().on_submit([this] {
                report(editor.hide_keyboard(), "IME submitted");
                (void)context->log().write(pxa::LogLevel::info, text.get());
            }),
            Row(Button("打开").on_click([this] {
                    report(editor.show_keyboard(), "IME opened");
                }),
                Button("关闭").on_click([this] {
                    report(editor.hide_keyboard(), "IME closed");
                })).gap(8_dp),
            Text(status)
        ).gap(4_dp).padding(12_dp);
    }
    pxa::Result<void> on_start(pxa::Context& ctx, std::span<const std::byte>) {
        context = &ctx;
        return ctx.clock().set_period(16);
    }
    pxa::Result<bool> on_event(pxa::Context& ctx, const pxa::Event& event) {
        if (!event.is<pxa::ClockTick>()) return false;
        timespec time{};
        if (clock_gettime(CLOCK_MONOTONIC, &time)) return std::unexpected(pxa::Error::io_error);
        const auto now = std::uint64_t(time.tv_sec)*1000000 + time.tv_nsec/1000;
        char line[96]; char* cursor = line;
        auto text = [&](std::string_view value) { for (char byte:value) *cursor++=byte; };
        auto number = [&](std::uint64_t value) { cursor=std::to_chars(cursor,line+sizeof(line),value).ptr; };
        text("IME_HEARTBEAT now_us="); number(now);
        text(" gap_us="); number(previous ? now-previous : 0); previous=now;
        auto logged=ctx.log().write(pxa::LogLevel::info,{line,std::size_t(cursor-line)});
        if (!logged) return std::unexpected(logged.error());
        return true;
    }
    void on_background(pxa::Context&) { (void)editor.hide_keyboard(); }
};

PXA_APPLICATION(SystemInput)
