#include <pxa/app.hpp>
using namespace pxa::ui;
using namespace pxa::ui::literals;
struct Settings {
    auto view() {
        return Column(Text("拼音输入法").font(Font::title),
            Text("在系统设置的「输入法」中启用和设置。"),
            Text("支持九键、全键盘、词句候选、数字与符号。"),
            Text("九键上排选拼音，下排选汉字；左右滑动翻页，箭头展开候选。"),
            Text("底栏切换中英、数字与符号；齿轮切换九键和全键。"),
            Text("长按九键输入数字，长按空格拖动光标；收起不会提交表单。"),
            Text("输入期间按需运行；关闭键盘后释放词库。"),
            Text("引擎来自 lvgl_pinyin_ime / libgooglepinyin（Apache-2.0）。")
        ).gap(12_dp).padding(16_dp);
    }
};
PXA_APPLICATION(Settings)
