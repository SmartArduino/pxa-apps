// SPDX-License-Identifier: MIT
#include <pxa/app.hpp>
using namespace pxa;
using namespace pxa::ui;
using namespace pxa::ui::literals;

struct FontProbe {
    Context* context=nullptr;
    Result<void> on_start(Context& ctx) {context=&ctx;return {};}
    void on_foreground(Context& ctx) {
        (void)ctx.window().fullscreen(WindowBarMode::hidden,WindowBarMode::hidden);
    }
    auto view() {
        return Column(
            Text("拼音与英语音标").font(Font::title).rgba(0x263b4aff),
            Text("rén  nǚ  lǜ  xué").font(Font::headline).rgba(0x263b4aff),
            Text("Ā ÁǍ À Ǖ Ǘ Ǚ Ǜ").font(Font::title).rgba(0x263b4aff),
            Text("ə ɚ ɪ ʊ ʌ ɜː").font(Font::title).rgba(0x263b4aff),
            Text("æ ɑː ɔː ʃ ʒ ŋ ˈ ˌ Ω").font(Font::title).rgba(0x263b4aff),
            Button("退出检查").on_click([this]{(void)context->window().request_back();}).height(40_dp).fill()
        ).fill().fill_height().padding(12_dp).gap(8_dp).background(0xf3f7edff).rgba(0x263b4aff);
    }
};
PXA_APPLICATION(FontProbe)
