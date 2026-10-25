# GBA 模拟器

独立 PXA 应用 `pxa-gba`，使用 gpSP 的 ARM/Thumb 解释器、逐行图像和六通道声音内核。复用 [公共前端](../common/emulator/README.md) 的手柄菜单、系统字体和配色、i18n、透明触摸按键、等比缩放、自动跳帧和多个即时存档。

## 游戏与存档

直接打开 `.gba` / `.agb` 文件，扩展名不区分大小写，最大 32 MiB。SD 卡使用系统分配的 `PXA/media/<发布者标识>/pxa-gba/` 目录，准确路径可在游戏库查看。Flash 使用应用私有 `roms/`；左右切换存储来源，Y 刷新。

ROM 始终通过 FS `read-at` 读取，由 gpSP 管理最多 2 MiB 的有界缓存，页大小 32 KiB。大于可用内存的 ROM 无需整体驻留，也不会保留第二份完整卡带。首次加载分块扫描 CRC；缺页 I/O 失败停止模拟，避免继续覆盖已有进度。SD 卡延迟和游戏的访问模式会影响性能。

ROM 缓存按卡带大小和调用方预算分配，最多 2 MiB；预算或内存不足时可使用 1 MiB。启动时只预读首个 MiB，其余容量按 32 KiB 页逐步填充，避免增加打开游戏的等待时间。大于缓存的 ROM 仍通过位置读取运行。

电池存档保存 SRAM/Flash/EEPROM，按 ROM CRC 隔离。首次打开游戏记录电池数据的初始校验，暂停或进入后台时仅保存已改变的数据，减少不必要的存储写入。即时存档还保存 CPU、RAM、PPU、DMA、计时器、RTC、声音通道和未消费的混音数据，读档保持声音连续性。即时存档按需新增，不设置固定槽位数量，实际受存储空间限制。当前格式只用于本应用，不是 gpSP/libretro 的通用存档格式。

## 操作与声音

A/B、L/R、方向键、Start、Select 对应 GBA 按键。Start+Select 打开暂停菜单。手柄操作后自动隐藏悬浮键；触摸恢复，支持多点触摸。暂停菜单与游戏库都支持手柄。

画面以 240×160、3:2 比例放大到可用屏幕内的最大尺寸，保留黑边。声音默认开启，菜单中的开关会在下次启动时恢复，首次使用遵守系统播放权限。核心以 32768 Hz 输出立体声，由公共前端非阻塞提交给 Host 混音和 DMA。跳过绘制仍推进完整 CPU、计时器与声音状态。

## 内核与性能范围

固定上游版本和本地修改见 `vendor/upstream.json`。附带上游 GPL 开源替代 BIOS 及完整对应源代码，无需随应用分发 Nintendo BIOS。替代 BIOS 与解释器仍存在兼容性边界；目前 Classic NES 系列的 GBA 版《Super Mario Bros.》启动黑屏，原版 NES 游戏可使用独立 NES 模拟器。OpenLara 目前存在明显花屏，不列为已支持游戏。当前不支持联机和外部 BIOS。

gpSP 的本机动态重编译器不能直接在 PXA Wasm/AOT 沙箱中执行，因此本应用使用可移植解释器，经 PXA AOT 编译。自动跳帧减少绘制成本，Host 显示和音频可并行；实际模拟 Hz 与屏幕完成帧率需要分别测量，不保证所有游戏满速。

ARM 和 Thumb 使用各自的执行循环，切换指令集时交还调度器。NZCV 标志留在局部变量中，在 PSR 访问、具有 I/O 副作用的存储、硬件更新、中断及执行循环退出等可观察边界同步，避免每条 ALU 指令都重复写回。GBA 在持续落后时保留 70 ms 的绘制预算；跳帧成本会重新采样，避免首次加载或 ROM 缺页的慢样本长期影响后续场景。

普通 EWRAM/IWRAM 写入，以及 VRAM 的半字和字写入使用解释器内的直接路径；调色板、OAM、I/O 和 VRAM 字节写入仍使用原处理函数。已对齐的映射读取在有效地址中显式保留对齐约束，便于 RV32 AOT 生成整字访问；未对齐读取仍保留 ARM 旋转语义。ROM 存档芯片签名在一次扫描中检测，避免重复逐字节调用比较函数。

解释器会合并已验证的纯等待循环、ARM 插值运算及对齐拷贝指令。首次命中核对完整指令，IWRAM 验证结果仅在当前硬件事件内复用，CPU 写入、DMA、模式切换和作弊处理会使缓存失效。寄存器、NZCV、原始访存周期、调色板转换和精灵更新标记均保留；不满足条件时使用原解释路径。指令追踪和详细指令分析模式关闭指令合并，避免统计失真。

## 构建与测试

在固件工作区根目录执行：

```sh
tools/app.sh build gba-emulator --board doit-gamekit --target esp32s31 --aot-only \
  --output local/artifacts/gba-build/$(date +%Y%m%d-%H%M%S)
```

S31 的优化构建可复用固件仓库的 `fixed-memory` AOT 编译器配置。它利用固定大小的线性内存和可证明的地址对齐减少重复访问，保留运行时边界检查；编译器在独立目录构建，不改变 SDK 默认编译器：

```sh
gba_out="$PWD/local/artifacts/gba-build/$(date +%Y%m%d-%H%M%S)"
gba_compiler=$(python3 tools/game-perf/build_compiler.py --output "$gba_out/compiler" --jobs 3)
WAMRC="$gba_compiler" tools/app.sh build gba-emulator \
  --board doit-gamekit --target esp32s31 --aot-only --output "$gba_out/package"
```

原生回归使用公共前端文档中的 CMake 命令，将源目录换成 `local/pxa-apps/gba-emulator`。测试包括自行编写的 ARM 诊断卡带、RGB565、可听信号、全绘制/跳帧 PCM 一致性、精确存档回放、分页镜像和读取失败。`gba_rom_probe ROM [frames] [draw_interval] [output.ppm]` 使用真实文件读取回调及最多 2 MiB 的有界缓存，输出本机耗时与状态回放结果；本机吞吐不能代表设备 FPS。

`gba_cpu_flags_test` 检查算术标志、PSR 控制位写入、指令集切换、存储、SWI 和 IRQ 边界；`gba_pacer_test` 检查 GBA 的绘制预算。诊断构建可设置环境变量 `PXA_GBA_PROFILE=1`，原生测试使用 `-DGBA_PROFILE=ON`，每 60 个模拟帧记录绘制、音频和 CPU 指令区域统计。只需要绘图和音频耗时时，可使用 `PXA_GBA_PROFILE_PHASES=1`（原生 `-DGBA_PROFILE_PHASES=ON`），保留指令合并、不统计逐条指令。此模式有采样开销，只用于定位瓶颈；正式帧率必须用关闭诊断的包测量。SDK 的 CMake 缓存每次重新读取诊断环境变量，取消变量即可恢复正常构建。

`gba_idle_test` 与 `gba_fusion_test` 比较优化开关两侧的完整机器状态、周期边界、自修改代码和转换后的调色板。`gba_memory_access_test` 比对直接路径与原总线处理函数的镜像、未对齐访问、调色板/OAM/VRAM 副作用及总线范围检查；`gba_backup_detect_test` 检查签名边界和存档芯片类型优先级。

比较核心优化时可设置 `PXA_GBA_BENCHMARK=1`（原生为 `-DGBA_BENCHMARK=ON`）：强制每帧绘制、默认游戏输入为零，每 60 帧输出 `GBA BENCH`。只比较相同 ROM、相同帧号且 PCM、画面、CPU 校验一致的区间；`core_us` 排除校验和日志时间。此模式改变跳帧与输入行为，不能用来报告正式版本的 LCD 帧率，也不能作为交付包。

`gba_gameplay_probe ROM frames draw_interval input.csv output_prefix [battery.sav]` 支持按模拟帧输入按键并输出每 60 帧的完整状态、画面及 PCM 校验。可选电池存档用于复现已有进度下的启动过程。CSV 为递增的 `frame,held_core_pad_mask`，键位见公共 `core.h`；最后一个状态持续生效。实机诊断构建可通过 `PXA_GBA_BENCHMARK_INPUT` 指向同一 CSV，正式包必须取消此变量及 `PXA_GBA_BENCHMARK`。这些运行产物写入固件工作区的 `local/artifacts/`。

代码及内核使用 GPL-2.0-or-later，开源 BIOS 的授权见其源文件，图标为项目原创 CC0。商业 ROM 不随源码或安装包分发。
