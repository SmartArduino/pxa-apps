# Voxel Craft C++ 性能与验收记录

报告、测量 JSON 和截图与应用源码一起维护。实验中的 Host、SDK 和设备配置
以各报告记录为准；不同地图、HUD、画质或固件的 FPS 不直接作为语言性能对比。

| 报告 | 内容 | 测量数据 |
| --- | --- | --- |
| [整行深度遮挡优化](game-render-occlusion-20261015.zh-CN.md) | 在扫描线优化基础上跳过已完全遮挡行的透视计算；同存档多轮 A/B/A、恒定内存结构、未采用的常量除法候选 | [measurements.json](game-render-occlusion-20261015/measurements.json) |
| [Game-render 扫描线优化](game-render-20261015.zh-CN.md) | Watcher 同存档/视角 A/B/A、专用纯色与 cutout 深度内核、坐标运算外提、真实内存窗口和原生像素一致性；两游戏与三设备检查 | [measurements.json](game-render-20261015/measurements.json)、[validation.json](game-render-20261015/validation.json) |
| [Korvo-1 真机与整合优化](korvo-20261013.zh-CN.md) | 最终完整固件同负载 FPS、光栅阶段、冷/热真实峰值、输入延迟及透明手势区控件修正 | [measurements.json](korvo-20261013/measurements.json) |
| [pai-touch 正常游戏实测](gameplay-device-20261011.zh-CN.md) | 0.12.3 正式 AOT，标题/新游戏、行走、转向、挖掘的三轮 LCD FPS、输入延迟及真实内存峰值；默认自动视距的取舍 | [summary.json](gameplay-device-20261011/summary.json)、[captures.json](gameplay-device-20261011/captures.json) |
| [Core 2 全应用迁移中的 Voxel 回归](core2-cpp-migration-20261011.zh-CN.md) | 工具、背包、保存/恢复、世界生成及透明深度正确性；不作为游戏 FPS 对比 | [真机工具检查](core2-cpp-migration-20261011/device-tools.json) |
| [Core 2 后续验证](abi-core2-refine-20261011.zh-CN.md) | 固定场景约 13.2 FPS，SDK 及桌面借用路径、分项内存和输入采样 | [device-comparison.json](abi-core2-refine-20261011/device-comparison.json) |
| [Core 2 / 同步借用 DrawList](abi-core2-20261011.zh-CN.md) | 同场景旧 Core、排队和借用路径；实际 LCD、阶段和内存 | [comparison.json](abi-core2-20261011/comparison.json) |
| [3D、游戏与 SDK 优化](pxa-3d-20261009.zh-CN.md) | 0.9 阶段固定场景 C/C++ 对照、阶段计时、裁剪/树冠正确性与内存 | [measurements.json](pxa-3d-20261009/measurements.json) |
| [O3、HUD 与交互验收](voxel-craft-cpp-ui-20261009.zh-CN.md) | 0.10 阶段 O3/HUD 成本、背压、界面/字体/存档、冷/热启动与重复启动 | [performance.json](voxel-craft-cpp-ui-20261009/performance.json)、[memory.json](voxel-craft-cpp-ui-20261009/memory.json) |
| [有限背包、合成、材质音效与工具](voxel-gameplay-20261009.zh-CN.md) | 0.12 六工具与耐久、挖掘/裂纹、有限合成、全屏缓存修复；同一固件的等 HUD 显示、阶段及冷/热真实峰值 | [measurements.json](voxel-gameplay-20261009/measurements.json)、[validation.json](voxel-gameplay-20261009/validation.json) |

复现命令在 pxa-workspace 根目录执行，应用仓库默认位于 `local/pxa-apps/`。
完整原始串口日志、测试包、实验源码与 SDK 归档保留在报告列出的 workspace
`local/` 目录；此处提交报告所需的测量汇总和截图。

字节完全相同的附件共用一份文件：0.10 与 0.12 的设置页截图见
[412×412](voxel-gameplay-20261009/display-412x412-320dpi-settings.png)、
[800×480](voxel-gameplay-20261009/display-800x480-480dpi-settings.png)；
DPI 验证中返回后的相同画面见
[标题](voxel-dpi-20261009/sim-title.png)、
[暂停](voxel-dpi-20261009/sim-pause.png)；
0.11 与最终报告共用的显示检查结果见
[display-qa.json](voxel-gameplay-20261009/display-qa.json)。这些只合并附件副本，
各阶段的测量数据和测试配置仍以原报告为准。
