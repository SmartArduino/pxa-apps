# 提交前回归检查（2026-10-15）

本次整理浏览器、SDK 和 ESP Host 提交。SDK 与 Host 均从提交内容导出独立快照检查，
不依赖工作区中另外保留的输入法、商城或设备显示改动。

| 检查 | 结果 |
| --- | --- |
| 浏览器核心与编码 ASan/UBSan | 通过 |
| C++26 SDK 主测试及参数变体 | 66 项全部通过 |
| SDK 诊断、代码生成与国际化目录检查 | 通过 |
| 独立 SDK 原生 ASan/UBSan | 8 项全部通过，GIF 实际执行，无跳过 |
| 不含 LVGL 的 SDK 配置与核心测试 | 5 项全部通过 |
| 独立 Host 快照 ESP 适配器回归 | 通过 |
| ABI 规范检查 | 52 个 Core 向量、5 个包载荷通过 |

C++ 运行测试与 `deps/pxa-system/tools/package/test_guest_cpp.sh` 使用相同用例和编译参数，
生成器与诊断检查完成后在独立快照并行执行。原生检查使用 O1、ASan、UBSan，禁用 sanitizer 恢复；
覆盖 HTTP、资源/图片服务、PNG/JPEG/GIF、Core、UI 与 LVGL 事件重入。
另修正未接入 LVGL 时图像测试目标的配置条件，并在测试配置中实际启用 GIF。

各测试名称与对应源码提交见 [结果记录](commit-validation-20261015.json)。
原始工作区日志位于 `local/browser-validation/commit-20261015/`。

这次提交整理未刷写固件或重新测量 FPS。先前的真机图片、GIF、布局和状态提示证据见
[图片](images-20261015.zh-CN.md)、[GIF](gif-native-20261015.zh-CN.md)、
[布局](layout-volume-20261015.zh-CN.md) 与 [状态提示](status-20261015.zh-CN.md)。
JavaScript 引擎仍处于研究阶段，尚未接入浏览器。
