# 真实蓄力手感与 MOVE 绘制修复（2026-10-10）

修复 C++ 版按住时额外绘制导致的蓄力变慢。此次直接比较相同真实按住时长下的蓄力、释放速度和跳距，未调整速度、重力、蓄力上限或平台距离。生产代码只增加 MOVE 的提前返回，不新增状态、缓冲或分配；继续使用 O3。

## 原因与此前验证的缺口

此前恢复了 C 的 `pxa_clock_tick_steps` 公式，但测试只重放同一串时钟回调。它没有覆盖两版实际接收到的回调频率不同：C 忽略按住期间的 MOVE，C++ 却对每个 MOVE 执行音效状态处理并提交整帧。pai-touch 即使手指位置不变也产生 MOVE；额外绘制占用 Guest 和渲染队列，使时钟间隔常常超过两步补偿上限 40 ms。丢弃的时间没有进入蓄力，必须按住更久才能跳到相同位置。

修复后 MOVE 直接返回。按下、松开、取消仍立即处理，时钟负责持续更新和绘制。原版的 20 ms 舍入、两步补偿、物理和音频资源均保留。

## 真机真实按住对照

同一次开机的 pai-touch，MAC `a4:cb:8f:d5:d4:f0`、Host `2ef8b1a-dirty`，当前串口 `/dev/ttyACM0`。296×240、160 DPI、安全边距 8/10/8/10、圆角 58、全分辨率、三帧缓冲，初始种子 `0x9e3779b9`，第一块目标平台中心 `(2,0)`。每次重新启动、等待初始化和音频预热 5 秒，再发送 DOWN/UP；使用设备输入时间戳计算实际按住时间，不将命令发送或截图等待当作按住时间。

诊断仅在**松开后**输出一条记录；按下、MOVE、tick 不打印诊断日志。C 和 C++ 都在独立源目录中加同样的计数，正式代码无诊断字段。旧 C++ 测六个时长各一次；C 与修复后的 C++ 每个时长重复三次，共 42 次。原始记录：[旧 C++](input-pacing-before.json)、[C](input-pacing-C.json)、[修复后 C++](input-pacing-fixed.json)、[成对比较](input-pacing-comparison.json)。

| 请求按住时长 | 旧 C++ 蓄力 | C 蓄力中位数 | 修复后 C++ 蓄力中位数 |
|---|---:|---:|---:|
| 0.15 s | 0.08 s | 0.14 s | 0.14 s |
| 0.36 s | 0.24 s | 0.28 s | 0.30 s |
| 0.70 s | 0.32 s | 0.60 s | 0.64 s |
| 0.82 s | 0.48 s | 0.74 s | 0.72 s |
| 1.20 s | 0.60 s | 1.10 s | 1.08 s |
| 2.00 s | 1.00 s | 1.74 s | 1.66 s |

0.70 s 按住时，旧 C++ 收到 8 个时钟回调、17 个 MOVE，尝试绘制 26 帧，最大时钟间隔 183 ms；修复后中位数为 24 个时钟回调、23 个 MOVE、25 次绘制，最大间隔约 42 ms，接近 C 的 24 回调、25 次绘制、39 ms。实际跳距由 0.870 世界单位恢复到 1.799，C 中位数为 1.680。

0.82 s 按住时，C 的跳距中位数为 2.102，修复后为 2.041，旧版为 1.327，已经恢复到第一平台附近。所有记录还检查了释放的水平/垂直速度和理论落点，不能仅凭蓄力动画判断结果。

**实际限制：**原版每次舍入并丢余数，因此真实时间和游戏蓄力不是严格 1:1，个别样本有波动。C 在 0.70 s 时的蓄力范围为 0.56–0.70 s。修复后的六组中位数与 C 差异为 0、+20、+40、−20、−20、−80 ms；比较检查采用一个 20 ms 步长或 10% 的中位数容差，不宣称每次按住都逐毫秒一致。这个改动消除了此前接近一半时间未进入蓄力的问题。

## 正式包显示、画质与内存

旧生产包和新生产包均关闭诊断日志，使用自动画质，实测始终 296×240、scale 0、三帧缓冲。在同一 Host/同次开机期间各冷启动三次，启动预热 5 秒，按住并继续预热 5 秒，固定在人物/平台压缩下限 0.5 后采集 4 秒。音效已预热，蓄力循环在采集时播放。原始数据：[旧生产包](input-pacing-perf-before.json)、[新生产包](input-pacing-perf-fixed.json)、[对比与像素校验](input-pacing-performance-comparison.json)。

| 三轮中位数 | 旧 C++ | 修复后 C++ |
|---|---:|---:|
| 实际显示 FPS（呈现探针） | 36.24 | 34.98 |
| Host 光栅整帧平均 | 10.886 ms | 10.549 ms |
| 整机预热后稳定 SRAM | 303,748 B | 303,748 B |
| 整机预热后稳定 PSRAM | 2,645,952 B | 2,647,420 B |
| 本轮空闲 PSRAM 基线 | 1,511,044 B | 1,512,424 B |
| 相对各轮空闲基线的 PSRAM 增量 | 1,134,908 B | 1,134,988 B |

这个完全静止的长按场景，显示 FPS **降低约 3.5%**，不能宣称修复提高了面板帧率。此前其中许多帧来自 MOVE，却没有对应的物理推进；0.70 秒的真实输入对照中，时钟更新从 8 次恢复到 24 次。体验收益是消除丢失的蓄力时间和重复绘制。Host 的平均光栅耗时小幅降低，未改变光栅内核。

未修改的 **C 正式包**也在同次开机、同分辨率、第一平台、相同压缩下限、蓄力循环音效的场景测了三轮，实际显示 FPS 为 **34.54 / 35.32 / 34.59**，中位数 **34.59**；新 C++ 为 **34.76 / 34.98 / 34.98**，中位数 **34.98**。在这组测量中两版帧率持平，约 1.1% 的差异不足以证明显著加速。C 的光栅整帧平均中位数为 10.292 ms。原始结果：[C 正式包](input-pacing-perf-C.json)，截图：[C](screenshots/input-pacing-C.jpg)。C 和 C++ 的字体与背景画质不同，这是完整绘制路径的比较，不是只比较语言；更不能将其差异归为此次 MOVE 提前返回的收益。

三轮截图的人物、平台、背景与 HUD，与旧包压缩到同一状态后**零像素差异**。校验排除顶部 24 行系统 FPS 数字，比较剩余 63,936 个像素；三张均为 0 个变化像素。截图：[旧](screenshots/input-pacing-before.jpg)、[修复后](screenshots/input-pacing-fixed.jpg)。未牺牲字体抗锯齿、背景抖动渐变或分辨率。

| 实际单轮局部峰值范围 | 旧 C++ | 修复后 C++ |
|---|---:|---:|
| 启动/预热 SRAM | 311,476–311,528 B | 311,528–311,564 B |
| 启动/预热 PSRAM | 2,783,608–2,787,952 B | 2,784,980–2,785,012 B |
| 采集期间 SRAM | 303,836–303,912 B | 303,836 B |
| 采集期间 PSRAM（含探针） | 2,650,188–2,653,380 B | 2,651,324–2,653,100 B |

每轮启动前和采集前分别重置 ESP-IDF 最低剩余堆计数。这里列真实局部峰值，而非开机以来的资源预算 `peak_total`。应用未新增固定状态或分配；Guest/WAMR、AOT 映射、光栅资源预算和表面分项相同。整机 PSRAM 稳定值上移约 1.5 KiB，空闲基线同时上移约 1.4 KiB；扣除各轮基线后的中位数差 80 B，启动 SRAM 最大值差 36 B。保留这些实测小幅差异，不声称整机每个瞬间都绝对不变，也不将它们归因为新增游戏缓存。

| 分项容量 | 两版相同 |
|---|---:|
| Guest 线性内存 | 131,072 B |
| WAMR 当前/本次峰值 | 36,739 / 41,005 B |
| AOT 映射请求/实际分配 | 402,144 / 458,752 B |
| 三帧缓冲 | 426,240 B |
| 深度缓冲 | 0 B |
| 命令队列 | 49,152 B |
| 测量探针（生产包平时无探针） | 2,112 B |
| Host 资源预算 SRAM/PSRAM（与整机值包含，不另行相加） | 45,372 / 174,778 B |

## 回归与发布产物

- 原生及 ASan/UBSan 各 **10/10** 通过；新测试直接包含实际 `main.cpp`，向真实输入处理器发送 2,501 个 MOVE，确认无额外绘制、无蓄力变化，并核对按下、tick、释放速度和落点与未改动的 C 核心一致。
- O3 签名 AOT 在 ESP32-S3、ESP32-S31 和 x86_64 全部构建成功。S3 正式包的 Guest 最小线性内存仍为 131,072 B；源码新增状态为 0 B。
- 正式包六屏幕模拟器检查通过：296×240/160 DPI、480×480/305 DPI、176×176 圆屏、320×480/240 DPI、800×480/160 DPI、480×800/320 DPI，覆盖准备、按住、松开截图。见 [显示矩阵](display-matrix.json)。这不代表已在 esp-mosaico 真机验证。
- [产物哈希](input-pacing-packages.json)区分 release-only 诊断、旧生产包、新 S3 生产包和三架构完整包。诊断包只用于状态比较，不作为显示 FPS 基准。

## 复现

在 workspace 根目录执行。旧 C++ 来源为 `af9b1c1`；C 游戏原始源文件始终未修改。准备诊断源后分别构建两个应用，再通过官方 PXADB 轮换安装，确保测试前没有其他活动应用。

```sh
python3 local/pxa-apps/common/cpp/tests/reference/prepare_jump_release_device.py local/pxa-apps local/jump-charge-diag-source
PXA_APP_DEFINES=J3_FORCE_SCALE_SHIFT=0 bash tools/app.sh build jump-jump-3d --target esp32s3 --aot-only --source-root local/jump-charge-diag-source --output local/jump-charge-diag-C
PXA_APP_DEFINES=J3_FORCE_SCALE_SHIFT=0 bash tools/app.sh build jump-jump-3d-cpp --target esp32s3 --aot-only --source-root local/jump-charge-diag-source --output local/jump-charge-diag-CPP
python3 local/pxa-apps/common/cpp/tests/reference/device_jump_release.py --port /dev/ttyACM0 --app pxa-jump-jump-3d --output local/jump-release-C --repeat 3 --allow-audio
python3 local/pxa-apps/common/cpp/tests/reference/device_jump_release.py --port /dev/ttyACM0 --app pxa-jump-jump-3d-cpp --output local/jump-release-CPP --repeat 3 --allow-audio
python3 local/pxa-apps/common/cpp/tests/reference/compare_jump_release.py local/jump-release-old/report.json local/jump-release-C/report.json local/jump-release-CPP/report.json local/jump-release-comparison.json
PXA_CPP_GAME_TEST_BUILD="$PWD/local/cpp-game-ports-20261010/native-build" bash local/pxa-apps/common/cpp/tests/run.sh -DPXA_C_REFERENCE_BENCH=ON
bash tools/app.sh build jump-jump-3d-cpp --target esp32s3,esp32s31,simulator --aot-only --source-root local/pxa-apps --output local/jump-charge-release
```

帧率与内存的长按场景通过 `tools/measure-device-voxel.py --hold-pointer 148 120 --warmup 5 --seconds 4 --repeat 3 --local-heap --wake-home --require-surface` 采集；锁屏时先解锁，或额外指定 `--unlock-swipe 148 210 148 40`。启动预热后按下，再预热 5 秒，确保两版人物和平台都达到相同压缩下限 0.5，再开始采样。脚本每秒向同一位置发送 MOVE，维持调试接口的合成触摸；否则接口会在五秒后自动取消。

首次缺少续按的数据因截图已进入结束页被排除；安装等待后锁屏、未处理的 C 音频权限提示和出现系统音量浮层的采样也被排除。新增 `--require-surface` 自动拒绝带系统遮挡的画面，最后纳入比较的九张正式包截图全部是直接游戏表面。这些无效采样不用于收益结论。真实蓄力对照最长两秒，没有触发调试输入超时。

修复提交为 `93a6b7`，生产包与真机测量基于该代码快照；当前工作区另有 SDK 事件类型改造，原生回归在该改造出现后再次运行，仍 **10/10** 通过。安装包哈希与测量时的 Host 身份均完整保留，避免将后续工作区或其他开机期间的数据混用。

结束时已保留新 S3 正式包，卸载用于对照的 C 包，无活动测试应用、无暂存容器。最后正常包实测按住 0.82 秒，完成蓄力、起跳、落到第一平台并得到 **2 分**，回到 READY；四张截图全部来自游戏表面，诊断日志关闭。见 [收尾记录](input-pacing-restoration.json)和[落地画面](screenshots/input-pacing-final-landed.jpg)。本轮未重刷固件、未显式写入音量、亮度或休眠设置。
