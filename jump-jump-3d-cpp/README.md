# Jump Jump 3D C++

使用独立 PXA C++ SDK 的跳一跳。按住蓄力、松开跳跃，保留 C 版的平台、翻转动作、中心落点连击、特殊平台奖励、最高分、音效和背景音乐。应用 ID 为 `pxa-jump-jump-3d-cpp`，与 C 版独立保存最高分。

渲染仍为原来的等距视角和 painter 几何，不新增离屏 UI 层。背景改为 RGB565 量化前的 8×8 Bayer 抖动，用一个 8×256 的小纹理批量平铺；减少原来渐变条带。文字使用真实背景上的 alpha 抗锯齿，按 DPI 和可用安全区域选择三档字体，由 Host 按需加载当前档位的三个字体，Guest 只保留字形度量。圆屏、圆角屏和非对称安全边距用于 HUD，世界背景仍铺满面板。

按 C 原版对每次时钟回调的间隔四舍五入到 20 ms，最多补两步，保留原来的蓄力和飞行节奏；不累计回调间隔的零头。切后台暂停音频和时钟，前台恢复时重置计时。所有绘制与上传缓冲固定容量，调色板、资源上传与命令编码按使用时段共用 32,788 B 临时内存；音频由 Host 解码和调度。三帧缓冲、不分配深度缓冲。可选分辨率控制器使用 Host 光栅耗时，保留 C 版的预热、降档和升档迟滞；固定画质基准可编译 `J3_FORCE_SCALE_SHIFT=0`。构建保持 O3。

从 PXA workspace 根目录构建：

```sh
bash tools/app.sh build jump-jump-3d-cpp --target esp32s3 --aot-only --source-root local/pxa-apps --output local/cpp-games/jump-device
bash tools/app.sh build jump-jump-3d-cpp --target simulator,esp32s3,esp32s31 --source-root local/pxa-apps --output local/cpp-games/jump-all
python3 tools/pxadb/pxadb.py package install local/cpp-games/jump-device/pxa-jump-jump-3d-cpp.pxa --port /dev/ttyACM0
```

原生检查需要支持 C++26 的编译器：

```sh
PXA_CPP_GAME_TEST_BUILD="$PWD/local/cpp-games/native-build" bash local/pxa-apps/common/cpp/tests/run.sh
local/cpp-games/native-build/jump_preview --frames 30 --auto 8 --width 296 --height 240 --out /tmp/jump-cpp
```

`tools/selftest.cpp` 验证原版跳跃、评分、奖励与失败状态；`tools/preview.cpp` 使用真实 Host 光栅器输出 PPM。音频文件来自同仓库 C 版，保持逐字节相同。共享的绘制、输入和音频适配器位于 `../common/cpp/`，生产构建不依赖 C SDK。

验证结果、截图和真机性能数据见 [本轮报告](docs/2026-10-10-validation.zh-CN.md)。

蓄力计时修复、真实设备回调对照及临时内存复用的验证见 [蓄力修复报告](docs/2026-10-10-charge.zh-CN.md)。生产包不启用 `J3_TRACE_TIMING`；该宏仅用于计时诊断，不能用于 FPS 基准。
