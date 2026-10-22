# GB/GBC模拟器

PXA Core 2 应用，使用 gnuboy 的 GB/CGB 内核，包含中文、英文游戏库、暂停菜单、声音、触摸与手柄操作。ROM 和存档不随安装包分发。

## ROM 放置位置

先解压游戏 ZIP，再复制 `.gb` / `.gbc` 文件。应用不解压 ZIP，也不递归扫描子目录。

- SD 卡：应用媒体目录。通常为 `/PXA/media/lucinhu/pxa-game-boy/`；PXADB 对应 `/sdcard/PXA/media/lucinhu/pxa-game-boy/`。空游戏库和 `GB SD media=...` 日志显示主机查询得到的实际目录。
- Flash：应用私有目录下的 `roms/`。

复制后点“刷新”，或按手柄 Y。文件扩展名不区分大小写；每个来源最多列出 128 个文件，文件名最长 127 字节。应用 ID 保持 `pxa-game-boy`，媒体和私有目录按签名身份隔离。

菜单支持一号、二号手柄：首行向上进入顶部存储来源，左右或 L/R 切换，A/Start 确认、B 返回、Y 刷新；方向长按连续移动。GameKit 固件需支持 Canvas 菜单的 controller 订阅路由。

## 操作

| 操作 | 手柄 | 触摸 |
| --- | --- | --- |
| 选择游戏或菜单 | 方向键；按住连续移动 | 点列表，拖动滚动 |
| 确认 | A / Start | 点选项 |
| 暂停或继续 | X | 悬浮 MENU / 菜单继续选项 |
| 游戏按键 | 方向、A/B、Start/Select | 多点悬浮按键 |
| 返回 / 刷新 | B / Y | 菜单选项 / 刷新按钮 |

手柄活动后自动隐藏悬浮按键，断开手柄或再次触摸后恢复。游戏中点击空白处不会暂停。暂停菜单可开启声音、调整按键透明度、绘图间隔和原始/适屏显示。

默认按 10:9 等比例放大并居中留黑边；320×240 显示为 266×240，800×480 显示为 533×480。界面按分辨率、DPI、安全区与系统 i18n 排版。游戏运行时不绘制系统字体菜单层。

## 内核与存档

内核固定于 retro-go 的 gnuboy 分支，版本和本地改动见 `vendor/upstream.json`。编译范围仅包括 CPU、硬件、逐行视频、声音和内存 ROM 接口，不依赖 ESP-IDF、retro-go 运行时或内核内部文件系统。

支持 ROM、MBC1、MBC2、MBC3、MBC5 和 HuC1 常规卡带。MBC3 支持 RTC；SGB 标记的游戏按 DMG 原生画幅运行。HuC3、TAMA5、MBC6/7、摄像头及其他未实现的特殊硬件会明确拒绝加载。旧版 mGBA 内核的特殊外设支持不作保证。

电池数据写入私有目录的 `gnuboy-<ROM CRC32>.sav`。MBC2 保存 512 字节，其他卡带按真实 RAM 大小保存；MBC3 额外保存 48 字节显式小端 RTC 状态。RTC 支持 512 天计数、停止位、溢出和离线时间推进。新内核不迁移开发阶段的 mGBA 存档，旧 `gb-*.sav` 文件保留。

暂停、切换游戏或正常退出触发保存，先写临时文件再原子替换；写入失败保留原文件并阻止切换。没有即时状态存档；游戏仍需使用其自身的存档功能。强制结束进程可能丢失尚未保存的进度。

## 性能设计

模拟目标为 `4194304 / 70224 = 59.72750057 Hz`。每次执行固定模拟周期，最后一条指令的超出周期计入下一帧；关闭 LCD 时也继续推进 CPU、定时器、声音和 RTC。跳帧仅省略绘制，不把未执行的模拟工作算成帧率。

跳帧调度分别采样绘制帧与跳绘帧的实际耗时，结合持续模拟积压、Surface 背压和用户设置调整绘图间隔。昂贵的 PPU 绘制仍可通过跳帧释放时间；持续过载时保留有界的画面更新机会，完整执行 CPU、定时器和 APU。

gnuboy 按整条指令执行，计时器沿用上游每 8 个机器周期的批量推进策略。HALT 等待按完整的计数批次合并，严格停在下一次 LCD 转换、定时器溢出、串口完成或帧预算之前，保留原来的批次相位与中断处理。对已经稳定的 `LD A,(RAM); AND/OR A; JR Z/NZ` 轮询（可带 NOP）合并完整循环，严格停在下一次 LCD 转换、定时器溢出、串口完成或帧预算之前；寄存器不匹配、I/O、待处理中断、禁用中断或跨 ROM 页时回退原执行器。纯 RAM 轮询作为 `GB_WAIT_BATCH` 实验选项保留，默认关闭；含 HALT 的循环使用前述 HALT 路径。ROM、VRAM、WRAM 使用直接页映射；跨页、未对齐和 16 位回绕访问使用明确的字节操作。小 ROM 直接引用前端已有数据；较大卡带使用有界银行缓存，避免保留整份游戏。

图块保留逐位展开；64 字节查表实现作为 `GB_TILE_LUT` 实验选项保留，S31 实测使塞尔达变慢，默认关闭。最多十个精灵使用稳定插入排序，保留相同 X 坐标时的 OAM 优先级。无精灵的行不复制背景快照。`GB_TILE_LUT`、`GB_HALT_BATCH`、`GB_SPRITE_SORT` 可在原生对照构建中分别关闭。

PPU 输出 RGB565，绿色从 5 位扩展到完整 6 位范围。原始尺寸直接借用 Surface 子区域并支持任意行跨度；帧结束立即恢复内核指针。适屏路径预计算采样坐标、复用重复行，并为双缓冲分别缓存源画面，只重画变化行和按键覆盖区域。先模拟再申请 Surface，使 LCD DMA 与内核执行重叠。

APU 直接生成 32768 Hz 立体声，省去旧内核的 131072 Hz PCM 与四倍抽取。声音关闭时继续推进通道、包络和扫频，只丢弃输出。前端连续重采样到 Host 返回的 8–48 kHz、单声道或立体声；开启时有 5 ms 渐入。PCM 入队非阻塞，Host 混音和 DMA 与 Guest 并行。

Guest 线性内存固定为 4 MiB，防止增长使 Surface 映射失效。卡带格式上限为 8 MiB；优先保留 512 KiB 内核初始化空间后尝试整块驻留；放不下时使用至多 256 KiB ROM 缓存，按 4 KiB 子页加载 16 KiB 银行，仅在首次访问时读取。八个 CPU ROM 窗口分别锁定，CPU 热路径保留直接页映射，8 MiB ROM 无需整块装入内存。复用 SDK 的 FS `read-at`，SD 与私有 Flash 均可使用（Flash 分页需要支持私有句柄 `read-at` 的 Host）；运行期间需要保留源文件及对应存储介质。缓存机制见 [`common/rom`](../common/rom/README.md)。支持的 Surface 范围为 200–800 × 160–480。

每五秒输出 `GB PERF`：`sim_x100` 是实际完成模拟帧率乘 100，`submit_x100` 是成功提交率，`cpu_avg_us` 包含 CPU/PPU/APU，`paint_avg_us` 是前端绘制，`debt` 是未完成模拟工作。分页模式的 `GB ROM` 记录缓存容量、缺页、淘汰、累计读取及耗时。缺页失败会停止游戏，并禁止用该帧状态覆盖原存档。`GB STATS` 补充跳帧、音频入队丢弃和按键隐藏状态。`GB DISPLAY` 统计 Surface 消费；LCD 完成率通过设备 `PERF START display` 单独测量。这些均不是光学测量的面板刷新率。

## 构建与验证

在 `pxa-projects` 根目录运行：

```sh
tools/app.sh build game-boy-emulator --target simulator,esp32s31 --aot-only
cmake -S local/pxa-apps/game-boy-emulator -B local/gb-native \
  -DGB_NATIVE_TEST=ON \
  -DGB_GUEST_SDK_INCLUDE="$PWD/deps/pxa-system/sdk/guest-c/include"
cmake --build local/gb-native -j4
ctest --test-dir local/gb-native --output-on-failure
```

要求 FS 0.3 `app-media-root`；FS 0.4 的实际媒体目录查询为可选能力。不同 CPU 架构必须使用对应 AOT。

本地测试覆盖 DMG/CGB、卡带生命周期、RAM/RTC、地址回绕、左侧窗口裁剪、RGB565、Surface 跨度、缩放缓存、音频流和前端输入。`tests/benchmark.c` 输出状态与音视频签名，可对比绘图间隔；桌面吞吐不能替代设备帧率。

实机加载游戏后运行 `tests/device_perf.py --port <端口> --serial <序列号> --output <本地产物目录> --rom-crc <CRC32>`。脚本核对设备和 ROM，分别记录模拟吞吐及 LCD 完成率，并在结束时发送 `INPUT CANCEL`。调试控制器注入不代表机械按键验收。

## 来源与许可

gnuboy 来源 <https://github.com/ducalex/retro-go>，固定版本 `4ced120669750ca7228fd0414211430c1d923166`，遵循 GPL-2.0-or-later；保留 `vendor/gnuboy/COPYING` 和 `CREDITS`。PXA 前端沿用 NES 应用的 GPL-2.0-or-later 代码。独立文件保留文件头许可；Noto Sans CJK 字体子集遵循 `licenses/ui-font-OFL.txt`。构建和测试产物存放于 Git 忽略的本地目录，不随源码或 ROM 分发。

即时存档、多存档列表、系统主题配色和悬浮按键布局与[公共前端约定](../common/emulator/README.md)一致。GB/GBC 状态包含 CPU、LCD、APU、RTC、工作 RAM、显存及卡带 RAM，恢复时重建指针和 ROM 分页映射；游戏帧路径不执行存档读写。
