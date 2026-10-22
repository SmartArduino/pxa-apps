# PC Engine模拟器

独立 PXA App，包 ID 为 `pxa-pc-engine`。内核为 PCE-GO（retro-go 分支），公共前端位于 [`common/emulator`](../common/emulator/README.md)。

## ROM 与存档

- 文件格式：.pce，文件名扩展名不区分大小写；当前直接读取未压缩文件。
- SD 卡通常使用 `/sdcard/PXA/media/lucinhu/pxa-pc-engine/`；空游戏库的 SD 提示中显示系统返回的实际目录。
- Flash 使用本 App 私有目录中的 `roms/`。在游戏库中用左右方向键切换来源，Y 刷新目录。
- 电池存档按平台与 ROM CRC32 分开保存；切换游戏、暂停和退出时写入临时文件并原子替换。
- 范围：4 MiB（另允许 512 字节复制头）；按 8 KiB HuCard 银行缓存，支持现有 SF2 窗口映射。支持 HuCard 及 SF2 分页。当前不提供 CD-ROM² / SuperGrafx。

## 操作与画面

I/II 使用 A/B；RUN 对应 Start；Select 对应 Select。 支持两个独立手柄输入。Start+Select 打开/关闭游戏菜单，松开组合键后再接收游戏按键。手柄有按键操作后自动隐藏悬浮键；触摸屏空白区域继续游戏，触摸 MENU 打开菜单，支持多指同时按方向和动作键。

默认按平台显示比例放大到屏幕安全区内的最大尺寸并保留黑边，菜单可切换原始尺寸。界面通过 PXA i18n 跟随系统语言，中英文翻译位于 `i18n/`。菜单使用系统字体，游戏中的透明按键使用预计算遮罩与 RGB565 混色表。

声音在游戏菜单中打开并申请播放权限。CPU、图像和声音按同一模拟帧推进；自动跳过部分绘制以减少负载，不通过丢失 CPU 时间来伪造满速。性能与实际支持范围需要结合目标硬件和 ROM 判断。

驻留 ROM 加载后先扫描候选指令序列，不含候选的游戏每帧直接选择原 CPU 路径，普通指令循环保持原样。分页 ROM 保留检测而不额外读取文件。HuC6280 在扫描线执行边界识别 ROM 中的 RAM 计数/轮询短循环，合并能够完整执行的迭代，并保留计数器进位、标志位和准确周期。最后一次不完整迭代仍由原指令处理；读写 I/O、数据互相别名或在 RAM 中执行代码时回到普通路径。该优化不按游戏名称、CRC 或固定地址打补丁，也不会跨过中断、定时器和视频事件边界。原生 `pce_batch` 测试逐一比较两条路径的 CPU、RAM 和周期，包括部分循环入口、计数溢出、循环退出和回退条件。

## 构建与测试

在 PXA 固件工作区根目录执行：

```sh
tools/app.sh build pc-engine-emulator --board doit-gamekit-1 --target simulator,esp32s31 --aot-only \
  --output local/artifacts/console-build/$(date +%Y%m%d-%H%M%S)
```

可运行的原生测试与测量方法见公共前端说明。内核来源、固定版本和本地修改列在 `vendor/` 的 `*upstream.json`；原许可证保留在对应源码中。使用和分发必须同时遵守这些许可证。
