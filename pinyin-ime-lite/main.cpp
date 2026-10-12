#include <pxa/app.hpp>
using namespace pxa::ui;
using namespace pxa::ui::literals;
struct Help {
    auto view() {
        return Column(Text("轻量拼音输入法").font(Font::title),
            Text("在系统设置的「输入法」中选择本输入法。"),
            Text("支持九键、全键盘、数字与符号，与完整版共用键盘。"),
            Text("拼音逐字选字；连续拼音选完一字后可继续选下一字。"),
            Text("遇到读音歧义请点「分词」，例如 xi'an。"),
            Text("候选左右滑动翻页，箭头展开；九键上排选拼音，下排选汉字。"),
            Text("不含整词、整句候选和学习词库。字表约 19 KB，按需运行。"),
            Text("字表与九键来自 lvgl_pinyin_ime（Apache-2.0）。")
        ).gap(12_dp).padding(16_dp);
    }
};
PXA_APPLICATION(Help)
