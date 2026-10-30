# Pocket UI

Pocket Game、Pocket Play 以及 Anarch 共享的 C++17 UTF-8 字体与中英文基础设施；Pocket 两个运行时另共享显示几何、surface 重建与独立语言偏好处理。游戏继续在 288×224 的逻辑画布上绘制，翻译使用明确的两份字面量，不进行英文字符串自动匹配。

## 语言与文字

```cpp
#include "../common/pocket-ui/i18n.hpp"
const char *caption = pocket::tr("CONTINUE", "继续");
int width = pocket_ui::text_width(caption, 1);
pocket_ui::draw_text(10, 20, caption, 1, [&](int x, int y) {
    // 在应用自己的 framebuffer 绘制一个像素；调用者负责裁剪。
});
```

`pocket::locale` 为 C++17 inline 存储，默认 `Locale::ZH`，可由运行时和原生 stub 共同包含；`set_english(bool)` 只修改内存状态。`tr(en, zh)` 返回当前语言的原始指针，不分配内存。运行时标题和动态提示应在绘制时调用 `tr`，避免缓存旧语言的结果。

ASCII 使用原创 5×7 字形，前进 6 像素；中文使用 10×10 单色字形，前进 10 像素。换行前进 10 像素，测宽取最长一行。支持 scale 1..4，前进与行距同倍放大；空指针或无效 scale 不绘制且宽度为零。ASCII 小写沿用大写字形。错误 UTF-8 每次消费一个字节并显示替代方框，未收录字符也显示方框。

Pocket 菜单前三项保持逻辑按钮 x56..231、y72..99 / 102..129 / 132..159；语言项使用 y162..189。触摸或控制器激活第四行会在中文和 English 间切换并保持暂停。

## 字体来源与更新

[ascii.hpp](ascii.hpp) 的原始 5×7 字体按 CC0-1.0 分发。[glyphs.hpp](glyphs.hpp) 的字体名为 **Pocket CJK Bitmap 10**，衍生自 Adobe 的 Noto Sans CJK SC Regular，Copyright 2014–2021 Adobe，使用 [SIL Open Font License 1.1](OFL.txt)。它仍为 OFL 字体；运行时代码的 CC0 许可不改变字体许可。

[generate_font.py](generate_font.py) 读取十款游戏的 C/C++ 界面字面量，包括 Anarch 的 view.hpp，生成按 Unicode 排序的子集表。每项为 32 位码点加 10 行 uint16_t，共 24 字节；当前约 16 KiB。直接以 10px rasterize，保持一致的 em 基线，阈值 96，将行左端存为 bit9。增加文案后在工作区根目录执行：

```sh
python3 local/pxa-apps/common/pocket-ui/generate_font.py --sync-assets
python3 local/pxa-apps/common/pocket-ui/generate_font.py --check --sync-assets
```

需要 Pillow；默认字体为 `/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc` 的 face 2（简体中文），可用 `--font` 和 `--index` 指定相同来源的字体。使用相同字体文件、Pillow 与选项即可复现。`--check` 只验证字面量覆盖；结合 `--sync-assets` 时也验证随包授权文件且不写入。正常 `--sync-assets` 会为十款游戏写入或更新 assets 下的 `OFL-NotoSansCJK.txt` 和 `FONT-NOTICE.txt`，保留其他资源声明。

构建器只打包每个应用自己的 assets，因此发布前须同步上述两份文件。字体表即使只编入 AOT，也要在签名包中保留完整 OFL、Adobe 版权、上游来源与 10×10 子集修改说明。

## 显示与偏好

`cover_pixel_span` 将非负的实际像素范围转换为完整覆盖它的 Canvas 整数 dp 范围。Host 对位置和长度分别向下取整；遮边背景使用这一转换，避免分数 DPI 下留下透明缝隙，必要时覆盖边界相邻像素。

[viewport.hpp](viewport.hpp) 的 `fit_viewport`、`resample` 和 `Viewport::map_x/map_y` 共用最近邻取整规则。UI 指针事件先按环境的 `density_q16` 从 dp 转为屏幕像素，再逆 Host 的整数倍和 guest 采样；启动和环境更新都会刷新输入密度，避免高 DPI 屏幕及大屏 2/4 倍重复像素造成触摸偏差。逻辑画布始终为 288×224，surface 的宽高同时不超过 480×320。实际尺寸如下，原点向下取整居中：

| 屏幕 | 物理原点 | 最终物理尺寸 |
| --- | --- | --- |
| 296×240 | (0,5) | 296×230 |
| 320×240 | (6,0) | 308×240 |
| 480×320 | (34,0) | 411×320 |
| 240×240 | (0,27) | 240×186 |
| 800×480 | (92,0) | 616×480（Host 2 倍） |

[surface.inc](surface.inc) 串行处理异步创建、图层配置和重建，显示变化会取消输入、丢弃旧 prepared/staged 帧，再在旧创建事务结束后释放旧 handle。队列忙时保留已绘制帧，新 redraw 在提交成功后继续处理。日志 `Pocket: viewport canvas=288x224 origin=X,Y size=WxH display=WxH` 使用最终物理坐标。

显示缓冲区固定为 288×224×2 = 129024 字节逻辑画布，加 2×480×320×2 = 614400 字节输出，共 743424 字节。输出数组基址 64 字节对齐，以实际 frame_bytes 排列两帧，无需同时保留各屏幕规格的缓冲区。图层尺寸与实际 surface 一致，Host 负责自己的 1/2/4 倍整数显示。

[preferences.hpp](preferences.hpp) 定义独立 4 字节语言编码 `P L 1 locale`：中文为 0，英文为 1。[preferences.inc](preferences.inc) 使用 `<app_id>.prefs.v1`，与原 `<app_id>.save.v1` 及 Tiny 的分块存档互不混用。首次异步读取前显示默认中文；错误每 1 秒重试，缺失或无效数据保留默认。用户在读取完成前切换语言时，旧读取结果不会覆盖新选择。写入期间的新选择会排入下一次写入，失败重试最新状态；退出等待偏好与游戏存档，受各运行时现有退出时限约束。切换日志为 `Pocket: language zh/en`。

## 原生检查

[portable_test.cpp](tests/portable_test.cpp) 验证 fit 边界、逐物理像素的采样和触摸一致性、UTF-8 错误恢复、字形、测宽与偏好编码。[runtime_test.cpp](tests/runtime_test.cpp) 直接包含实际运行时，使用 SDK wire codec 和原生 mock transport，验证异步偏好竞争、失败重试、resize、两种提交路径的背压、触摸捕获及第四行菜单。游戏规则测试仍位于各应用 tests 中。

从工作区根目录运行，并把产物放在被忽略的本地目录：

```sh
artifact_root="$PWD/local/artifacts/pocket-ui/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$artifact_root"
sdk=deps/pxa-system/sdk/guest-c/include
c++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -I"$sdk" \
  local/pxa-apps/common/pocket-ui/tests/portable_test.cpp -o "$artifact_root/ui-test"
"$artifact_root/ui-test"
for mode in GAME PLAY CHUNKED; do
  c++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -Wno-attributes \
    -fsanitize=address,undefined -fno-omit-frame-pointer -DPOCKET_TEST_$mode -I"$sdk" \
    local/pxa-apps/common/pocket-ui/tests/runtime_test.cpp -o "$artifact_root/runtime-$mode-test"
  "$artifact_root/runtime-$mode-test"
done
```

这些检查无需设备，不替代签名包在产品模拟器或实际硬件上的显示、输入与跨进程偏好恢复检查。
