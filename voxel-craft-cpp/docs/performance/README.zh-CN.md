# Voxel Craft C++ 性能与验收记录

报告、测量 JSON 和截图与应用源码一起维护。实验中的 Host、SDK 和设备配置
以各报告记录为准；不同地图、HUD、画质或固件的 FPS 不直接作为语言性能对比。

| 报告 | 内容 | 测量数据 |
| --- | --- | --- |
| [3D、游戏与 SDK 优化](pxa-3d-20261009.zh-CN.md) | 0.9 阶段固定场景 C/C++ 对照、阶段计时、裁剪/树冠正确性与内存 | [measurements.json](pxa-3d-20261009/measurements.json) |

复现命令在 pxa-workspace 根目录执行，应用仓库默认位于 `local/pxa-apps/`。
完整原始串口日志、测试包、实验源码与 SDK 归档保留在报告列出的 workspace
`local/` 目录；此处提交报告所需的测量汇总和截图。
