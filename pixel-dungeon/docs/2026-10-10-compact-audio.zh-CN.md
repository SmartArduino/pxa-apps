# Pixel Dungeon：所有目标统一小体积音频（2026-10-10）

此前打包工具优先选择 `assets-<target>`：模拟器读取 `assets/music` 的原始 Vorbis，ESP32-S3 读取 `assets-esp32s3/music` 的 24 kbps VBR Opus。这是资源目录的历史选择，与 C/C++ SDK 或模拟器能否播放 Opus 无关；23 个 PCM 音效本来就相同。

现在 C/C++ 两版都将现有 ESP32-S3 音轨原样放到 `assets/music`，删除两版的 `assets-esp32s3` 重复目录。模拟器、ESP32-S3、ESP32-S31 和多架构包均使用同一套完整时长、双声道 Opus。没有改变曲目、增益、淡入淡出、音效、游戏代码或音频缓存容量。Opus 属于有损压缩：模拟器采用此前设备已有的音质，而非原始 Vorbis；没有再次压缩现有 Opus 文件。

## 体积

| 项目 | 之前（字节） | 之后（字节） | 减少 |
|---|---:|---:|---:|
| 六首音乐 | 1,913,567 | 897,857 | 1,015,710（53.08%） |
| C 模拟器 `.pxa` | 2,312,526 | 1,307,405 | 1,005,121 |
| C++ 模拟器 `.pxa` | 2,583,736 | 1,578,614 | 1,005,122 |

模拟器包按同一份游戏源码、SDK 和构建参数生成；前后可执行文件逐字节一致。C++ 包仍含原有 Wasm 回退，并未更改 O3、内存上限或代码优化选项。多架构构建验证包含 S3、S31、x86_64；由于资源目录不再存在覆盖分支，其目标顺序不会改变音频。完整包与可执行文件哈希见 [compact-audio-packages.json](compact-audio-packages.json)。

ESP32-S3 音频体积及内容保持原样。两版工作树的音频和图标资源合计减少 4,113,980 字节；Git 历史仍保留旧文件，不能据此声称已有 Git 仓库占用同步缩小。

## 解码和播放验证

- 两版默认资源及四个实际包共六份拷贝：六首音乐、23 个音效逐字节相同，所有音乐识别为双声道 Opus，见 [compact-audio-verification.json](compact-audio-verification.json)。
- 六首原始 Vorbis 和紧凑 Opus 均从头到尾解码，时长差最多为一个 48 kHz 采样，未截断片尾，见 [compact-audio-duration.json](compact-audio-duration.json)。
- 真实签名 AOT Guest 和 SDL Host 均播放新标题音乐。C 版使用现有 `pxsys_audio_app_test` 验证准备音效、音乐 READY、淡入、后台暂停、前台恢复和退出；C++ 使用已有 AOT lifecycle probe 验证时钟和音乐暂停恢复。见 [compact-audio-host.json](compact-audio-host.json)。
- 原生音频后端回归通过 Vorbis/Opus 解码、结束与循环、替换、暂停恢复、背压、PCM 混音，以及损坏/缺失资源错误处理。
- 生成工具验证从上游 Vorbis 生成两版相同文件，并在产生任何输出之前拒绝 Opus 输入，避免重复有损编码。

模拟器使用 SDL dummy 输出：验证实际解码、取样和播放状态，不构成主观听感结论。地区曲目均完整解码；本次真实 Guest 播放观察的是标题曲目，未重新进行六个地区的长时真机听感测试。ESP32-S3 沿用已有音频和解码路径，本次未刷设备、改设置或重新测设备 SRAM/PSRAM。

## 内存及桌面解码代价

固定 296×240 标题、相同签名 AOT，C++ 每版运行三个独立进程，包含前后台切换；每次均从本次进程启动统计峰值。所有短时观察的音乐缓冲欠载、恢复、缺失采样均为零，资源预算没有分配失败。

| C++ 项目 | 之前 | 之后 |
|---|---:|---:|
| Guest 线性内存当前/本次峰值 | 262,144 B | 262,144 B |
| WAMR 当前/本次峰值 | 51,826 / 101,748 B | 51,826 / 101,748 B |
| 帧缓冲 / 深度 / 命令队列 | 426,240 / 0 / 98,304 B | 相同 |
| Host 资源内部当前/峰值 | 560 B | 560 B |
| Host 资源外部当前/峰值 | 401,074 B | 401,073 B |
| 图集缓存当前/峰值 | 386,592 B | 386,592 B |
| 桌面进程稳定 RSS（三次中位数） | 27.004 MiB | 27.629 MiB |
| 桌面进程启动至退出 RSS 峰值（三次中位数） | 27.191 MiB | 27.789 MiB |
| 三次运行中的最大 RSS 峰值 | 27.363 MiB | 27.945 MiB |

Host 元数据有一个字节差异，测试包路径也不同，不将其当作内存优化收益。C 版的 Guest、WAMR、帧缓冲、深度和命令队列同样不变；本次稳定快照和每次运行的资源峰值均单独保存。数据见 [compact-audio-memory.json](compact-audio-memory.json) 和 [compact-audio-host.json](compact-audio-host.json)。

**桌面总进程 RSS 并未保持不增长。** 额外一次 `smaps_rollup` 分解显示，文件页面 PSS 增加约 638 KiB、匿名内存增加约 60 KiB；推断主要是不同解码库和 SDL 转换路径的驻留页面，不能把 RSS 全部归为 Guest 堆。该分解为各一轮诊断观察，见 [compact-audio-smaps.json](compact-audio-smaps.json)。设备没有发生 Vorbis→Opus 切换，因此这不是 ESP32-S3 本轮新增开销，但不能声称所有平台的总内存都下降。

相同约三秒进程运行中，音乐纯解码累计时间中位数从 16.222 ms 增至 48.971 ms（包含约 2.2 秒实际消费和缓冲预填充，不含输入 I/O）。桌面 Opus 解码成本较高，但仍无欠载。这里验证的是压缩资源和音频流水线，没有测游戏移动场景 FPS，不将包体积下降描述为 FPS 提升。

## 复现

从应用仓库根目录生成和检查资源（需要 FFmpeg 的 libopus 编码器）：

```sh
python3 pixel-dungeon/tools/generate_music.py --spd-assets /path/to/shattered-pixel-dungeon
python3 pixel-dungeon/tools/generate_audio.py --spd-assets /path/to/shattered-pixel-dungeon
python3 pixel-dungeon/tools/check_audio.py
```

从 PXA workspace 根目录构建并检查实际包：

```sh
bash tools/app.sh build pixel-dungeon --target simulator,esp32s3,esp32s31 --source-root local/pxa-apps --output local/pixel-audio/C-all
bash tools/app.sh build pixel-dungeon-cpp --target simulator,esp32s3,esp32s31 --source-root local/pxa-apps --output local/pixel-audio/CPP-all
python3 local/pxa-apps/pixel-dungeon/tools/check_audio.py \
  --package local/pixel-audio/C-all/pxa-pixel-dungeon \
  --package local/pixel-audio/CPP-all/pxa-pixel-dungeon-cpp
build/simulator/pai-touch/pxsys_audio_app_test local/pixel-audio/C-all/pxa-pixel-dungeon local/pxa-apps/.dev-signing/publisher-public.der
python3 deps/pxa-system/tools/package/test_audio_backend.py build/simulator/pai-touch/pxsys_audio_test deps/pxa-system/simulator/desktop/tests/audio-assets
```

已有验证工具 `common/cpp/tests/reference/aot_probe.c` 的 `static lifecycle` 模式可重做 C++ 音乐和时钟前后台检查；C 版使用上面的专用音频检查，其后台时钟行为不能直接套用 C++ 断言。本轮隔离的前后源码、未压缩原始音频备份、构建日志、probe 以及每次运行原始日志位于 workspace 的 `local/pixel-compact-audio-20261010/`，前后源码均不包含其他任务正在进行的 SDK 事件接口改动。
