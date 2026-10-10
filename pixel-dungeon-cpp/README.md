# Pixel Dungeon C++

使用独立 PXA C++ SDK 的回合制地牢游戏。保留 C 版的 25 层、五个地区、三种职业、五个存档槽、背包、装备、日志、排行榜、地图缩放、触控及手柄操作，以及全部音效和完整背景音乐。应用 ID 为 `pxa-pixel-dungeon-cpp`，存档格式兼容 C 版，私有存储按应用隔离，不自动迁移旧存档。

像素画按物理 DPI 选择整数呈现比例，并限制最小逻辑尺寸；480×480 / 305 DPI 使用 240×240 渲染目标，保持文字和按钮可读。安全边距和屏幕形状参与布局、触控坐标只换算一次。短圆屏使用紧凑布局；地图可以铺满轮廓，固定控件避开边缘。字体保留原版 Source Han Sans CN 黑体及抗锯齿贴图。

九张静态 INDEX8 图集在 `resources.json` 中声明，打包生成 PXR，运行时直接绑定 Host Asset；生产构建不嵌入大图或预留 64 KiB 上传暂存。`assets.cpp` 和 `font_data.cpp` 中的图数据仅在原生验证的 `PD_EMBED_ASSETS=1` 下编译，可运行 `python3 tools/export_resources.py` 从它们重新导出资源，维护新应用无需 C SDK。修复了原版标题第二团火焰动画可能读出图集边界的问题。

每帧命令缓冲、五个存档缓冲、异步任务池和音频 LRU 都为固定容量。动画按需绘制，音乐由 Host 时钟驱动；后台暂停时钟和音频并保存进度。初始化先绑定贴图、加载存档，再启动音频，降低同时在途的资源请求。三帧缓冲、不分配深度缓冲，构建保持 O3。

从 PXA workspace 根目录构建与验证：

```sh
bash tools/app.sh build pixel-dungeon-cpp --target esp32s3 --aot-only --source-root local/pxa-apps --output local/cpp-games/pixel-device
bash tools/app.sh build pixel-dungeon-cpp --target simulator,esp32s3,esp32s31 --source-root local/pxa-apps --output local/cpp-games/pixel-all
PXA_CPP_GAME_TEST_BUILD="$PWD/local/cpp-games/native-build" bash local/pxa-apps/common/cpp/tests/run.sh -DPXA_C_REFERENCE_BENCH=ON
python3 tools/pxadb/pxadb.py package install local/cpp-games/pixel-device/pxa-pixel-dungeon-cpp.pxa --port /dev/ttyACM0
```

所有目标（模拟器、ESP32-S3、ESP32-S31）统一使用 `assets/music/` 中完整时长、双声道、24 kbps VBR 的 Ogg Opus 音乐，与 C 版逐字节一致。沿用原有小体积设备音轨，六首共 897,857 字节；取消专用资源目录后，多架构包也使用同一套音频。23 个 16 kHz PCM 音效保持原样。可从应用仓库根目录运行 `python3 pixel-dungeon/tools/generate_music.py --spd-assets /path/to/shattered-pixel-dungeon`，直接从上游 Vorbis 原始文件重新生成两版资源，避免重复有损编码。设备包仍建议按目标单独构建，避免携带冗余架构和 Wasm 副本。

统一音频的安装包体积、完整解码与内存验证见 [音频报告](docs/2026-10-10-compact-audio.zh-CN.md)。`python3 pixel-dungeon/tools/check_audio.py --package /path/to/unpacked/package` 可检查默认资源、两版一致性和实际包内音频，`--package` 可重复指定。

原画、音乐及派生代码来自 Shattered Pixel Dungeon（Evan Debenham 等作者）及本仓库 C 版，遵循 GPL-3.0，完整许可证见 [LICENSE.txt](LICENSE.txt)。不声称这是上游完整游戏的等价重制。验证结果、截图与真机验收限制见 [本轮报告](docs/2026-10-10-validation.zh-CN.md)。

真机固定种子游戏对照、内存与普通包存档/生命周期检查见 [pai-touch 验收报告](docs/2026-10-10-device.zh-CN.md)。
