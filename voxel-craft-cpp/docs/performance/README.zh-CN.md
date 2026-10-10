# Voxel Craft C++ 性能与验收记录

报告、测量 JSON 和截图与应用源码一起维护。实验中的 Host、SDK 和设备配置
以各报告记录为准；不同地图、HUD、画质或固件的 FPS 不直接作为语言性能对比。

| 报告 | 内容 | 测量数据 |
| --- | --- | --- |
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
