# Jump Jump 3D

使用 PXA C++26 SDK 的跳一跳。按住蓄力、松开跳跃，保留原版的平台、翻转动作、中心落点连击、特殊平台奖励、最高分、音效和背景音乐。应用 ID 为 `pxa-jump-jump-3d`，最高分保存在应用私有存储。旧 `*-cpp` 安装的升级注意事项见[应用命名说明](../docs/app-names.zh-CN.md)。

0.1.7 支持 DOIT GameKit 手柄，继续保留触摸：

| 按键 | 操作 |
| --- | --- |
| A | 按住蓄力，松开跳跃；结算页按下重开 |
| B | 返回上级界面，同时取消当前蓄力 |
| Start | 结算页重开 |

触摸和手柄共用时间戳驱动的蓄力逻辑，各自保留操作归属，不会互相松开。
周期状态上报不重复触发，也不额外提交帧。后台或手柄断开会取消手柄蓄力，
恢复后先松开再操作。使用 SDK 应用级输入回调，不创建输入 Canvas。

DOIT GameKit 构建：`tools/app.sh build jump-jump-3d --board doit-gamekit --aot-only`。
操作、计时、重复上报、输入归属和恢复测试位于
`common/cpp/tests/reference/jump_controller_test.cpp`。


渲染仍为原来的等距视角和 painter 几何，不新增离屏 UI 层。背景改为 RGB565 量化前的 8×8 Bayer 抖动，用一个 8×256 的小纹理批量平铺；减少原来渐变条带。文字使用真实背景上的 alpha 抗锯齿，按 DPI 和可用安全区域选择三档字体，由 Host 按需加载当前档位的三个字体，Guest 只保留字形度量。圆屏、圆角屏和非对称安全边距用于 HUD，世界背景仍铺满面板。

按参考仓库 `yaoshanliang/weapp-jump` 的实际时间计算蓄力和抛物线，翻转使用独立的 140 + 180 ms 动画；低帧率和合并时钟事件不会放慢跳跃。短跳落回当前平台可以继续，快速跳奖励在松手时判定。蓄力粒子、落地散射、固定位置的多层波纹与特殊平台动画复用现有绘制资源。切后台暂停音频和时钟，前台恢复时重置计时。所有绘制与上传缓冲固定容量，调色板、资源上传与命令编码按使用时段共用 32,788 B 临时内存；音频由 Host 解码和调度。三帧缓冲、不分配深度缓冲。可选分辨率控制器使用 Host 光栅耗时，保留 C 版的预热、降档和升档迟滞；固定画质基准可编译 `J3_FORCE_SCALE_SHIFT=0`。构建保持 O3。

从 PXA workspace 根目录构建：

```sh
bash tools/app.sh build jump-jump-3d --target esp32s3 --aot-only --source-root local/pxa-apps --output local/cpp-games/jump-device
bash tools/app.sh build jump-jump-3d --target simulator,esp32s3,esp32s31 --source-root local/pxa-apps --output local/cpp-games/jump-all
python3 tools/pxadb/pxadb.py package install local/cpp-games/jump-device/pxa-jump-jump-3d.pxa --port /dev/ttyACM0
```

原生检查需要支持 C++26 的编译器：

```sh
PXA_CPP_GAME_TEST_BUILD="$PWD/local/cpp-games/native-build" bash local/pxa-apps/common/cpp/tests/run.sh
local/cpp-games/native-build/jump_preview --frames 30 --auto 8 --width 296 --height 240 --out /tmp/jump-cpp
```

`tools/selftest.cpp` 验证原版跳跃、评分、奖励与失败状态；`tools/preview.cpp` 使用真实 Host 光栅器输出 PPM。音频文件来自同仓库 C 版，保持逐字节相同。共享的绘制、输入和音频适配器位于 `../common/cpp/`，生产构建不依赖 C SDK。

验证结果、截图和真机性能数据见 [本轮报告](docs/2026-10-10-validation.zh-CN.md)。

蓄力计时修复、真实设备回调对照及临时内存复用的验证见 [蓄力修复报告](docs/2026-10-10-charge.zh-CN.md)。生产包不启用 `J3_TRACE_TIMING`；该宏仅用于计时诊断，不能用于 FPS 基准。

本轮原版参数对照、时间推进修复、特效、屏幕矩阵和 Korvo 验证见 [2026-10-13 报告](docs/2026-10-13-upstream-motion.zh-CN.md)。此前按 C 移植版量化时钟的报告保留为历史记录。

0.1.3 新增 14 类程序绘制的平台，总计 20 类：钟表、发光方块、软盘、高尔夫果岭、咖啡杯、药瓶、纸巾卷、词典、快递箱、礼盒、小板凳、游戏机、表情方块和钱箱。四种原版停留奖励保持不变。造型、碰撞、命令预算及音频缓存修复见[平台扩充报告](docs/2026-10-13-platforms.zh-CN.md)。

0.1.4 修复暂时资源不足导致部分音效永久失效，以及取消后的长音效加载重新占用缓存；补充音乐流的错误恢复。原因、故障注入检查和连续游玩记录见[音频恢复报告](docs/2026-10-13-audio-recovery.zh-CN.md)。

SenseCAP Watcher 原生 412×412 的板级优化见[第一阶段修复](docs/sensecap-watcher-20261014.zh-CN.md)和[显示流水线报告](docs/sensecap-watcher-pipeline-20261014.zh-CN.md)。同画质下，静止约 18.6 → 33.6～34.5 FPS，跳跃约 17.8 → 29.7～31.9 FPS，稳定内存和匹配窗口峰值均下降。后者包含优化手段、原始数据与截图，并指向主仓库的实现复用指南；冷启动与 UI 后结果分别保留，尚未逐帧锁定 30 FPS。
