# SNES模拟器

独立 PXA App，包 ID 为 `pxa-snes`。内核为 Snes9x（retro-go 分支），公共前端位于 [`common/emulator`](../common/emulator/README.md)。

## ROM 与存档

- 文件格式：.sfc / .smc / .fig，文件名扩展名不区分大小写；当前直接读取未压缩文件。
- SD 卡通常使用 `/sdcard/PXA/media/lucinhu/pxa-snes/`；空游戏库的 SD 提示中显示系统返回的实际目录。
- Flash 使用本 App 私有目录中的 `roms/`。在游戏库中用左右方向键切换来源，Y 刷新目录。
- 电池存档按平台与 ROM CRC32 分开保存；切换游戏、暂停和退出时写入临时文件并原子替换。
- 范围：8 MiB（另允许 512 字节复制头）；大 ROM 使用有界缓存，支持 LoROM/HiROM 及扩展映射。支持普通 LoROM/HiROM、DSP、C4、OBC1、S-RTC；当前不支持 SA-1、Super FX、S-DD1、SPC7110、SETA。

## 操作与画面

SNES 的 A/B/X/Y/L/R，Start、Select。 支持两个独立手柄输入。Start+Select 打开/关闭游戏菜单，松开组合键后再接收游戏按键。手柄有按键操作后自动隐藏悬浮键；触摸屏空白区域继续游戏，触摸 MENU 打开菜单，支持多指同时按方向和动作键。

默认按平台显示比例放大到屏幕安全区内的最大尺寸并保留黑边，菜单可切换原始尺寸。界面通过 PXA i18n 跟随系统语言，中英文翻译位于 `i18n/`。菜单使用系统字体，游戏中的透明按键使用预计算遮罩与 RGB565 混色表。

声音默认开启，并自动申请播放权限；在游戏菜单切换后按应用保存，下次启动恢复。CPU、图像和声音按同一模拟帧推进；自动跳过部分绘制以减少负载，不通过丢失 CPU 时间来伪造满速。性能与实际支持范围需要结合目标硬件和 ROM 判断。

## 构建与测试

在 PXA 固件工作区根目录执行：

```sh
tools/app.sh build snes-emulator --board doit-gamekit-1 --target simulator,esp32s31 --aot-only \
  --output local/artifacts/console-build/$(date +%Y%m%d-%H%M%S)
```

可运行的原生测试与测量方法见公共前端说明。内核来源、固定版本和本地修改列在 `vendor/` 的 `*upstream.json`；原许可证保留在对应源码中。使用和分发必须同时遵守这些许可证。

默认启用 CPU/APU 事件同步：在端口访问、DMA、CPU 等待和扫描线事件前追上 SPC，保留 CPU 指令边界以维持原来的等待时序。缓存有固定上限，帧结束前清空，不改变存档格式。CPU 等待路径也在解释器内部批量执行 SPC，保留原来的 `APU.Cycles < CPU.NextEvent` 继续执行条件及至少执行一条指令的语义；回归包含端口、空闲循环、SLEEP/STOP 与事件边界。原生构建可用 `-DEMU_SNES_EVENT_SYNC=OFF` 对照逐指令同步。

65C816 指令通过同一编译单元内的直接分派执行，减少 Wasm 间接调用及重复地址检查。构建时从五张 CPU 模式表生成内部 ID 和分派列表，ID 不进入存档。SPC 使用固定的 64 KiB RAM，保留原有 `IAPU.RAM` 别名和指针重定位规则；直接页访问显式限制为 RAM 的前两页。

可选 PGO 使用公共构建模块的 `EMU_PGO_GENERATE`、`EMU_PGO_PROFILE` 和打包环境变量 `PXA_EMU_PGO_PROFILE`。原生目标 `emu_pgo_train ROM [frames=12000] [draw_interval=3]` 提供确定性的启动与操作序列，读取 ROM 并输出音视频校验值，不写游戏存档。用 `LLVM_PROFILE_FILE` 将每个 ROM 的采样保存到独立本地产物文件，再用匹配 Clang 版本的 `llvm-profdata merge` 合并；用多个有代表性的 ROM 训练，并另做回归和设备性能对照。训练配置必须与使用配置相符，修改核心控制流后重新采集。未提供 profile 时保持普通编译，发布包不包含运行时采样代码。

原生目标 `emu_snes_spc_fingerprint <test.rom>` 输出全部 256 条 SPC 指令在 8192 组输入下的状态指纹。重构解释器时，将同一份 `tests/spc_fingerprint.c` 分别链接参考内核和候选内核，逐行比较输出；指纹按字段编码，排除宿主指针和结构体填充。该测试不写存档，也不能代替整段游戏的画面、音频和设备性能回归。

`emu_snes_tile_fingerprint <test.rom>` 用同样方法比较 4096 组普通和裁剪图块，覆盖双向翻转、透明像素、深度边界、非对齐位置及原有深度不变的情况。两端须使用相同缓存格式和宿主字节序；同时比较颜色、深度和源图块，检查未覆盖区域是否被改写。

`-DEMU_SNES_PROFILE=ON` 启用核心、PPU、SPC 和音频混音的阶段采样；原生测量程序还需设置 `EMU_BENCH_LOG=1` 才输出日志。诊断代码会影响耗时，不能用该构建验收 FPS；发布构建默认关闭采样。模拟 Hz 与 LCD 完成 FPS 应分别测量，前端提交计数不等于屏幕刷新率。
