# Mosaico 小字与滚动分页退出修复

2026-10-16，480×480、305 DPI。设备 `/dev/ttyACM2`，2,000,000 baud。

## 问题与修复

- **文字小：Host 字号没有按 DPI 换算。** 商城已用 dp 布局和语义字体角色，但 ESP Host 给 FreeType 的 caption/label/body 等仍为 12/14/16 个物理像素，模拟器已正确缩放。因此不能用模拟器正常来推断设备正常。Host 按 `round(sp × DPI / 160)` 生成原有六套字体，限制在 1–256 px，未新增字体实例或缓存；305 DPI 的 caption/label/body 为 23/27/31 px。修复作用于所有使用语义字体的应用。板级 DPI 在 Host 初始化前发布。
- **分页退出：PATCH 删除并重建相同 ID 的页脚。** 保留现有列表、卡片和页脚，新卡片追加后使用 MOVE 将页脚放回末尾，原地更新页脚文案、事件及网格位置。追加页仍只用一次 PATCH，不增加容器或常驻状态。旧路径先出现 COMMIT `status=-1`；模型已有新项，但旧界面没有相应节点，后续图片子树更新可能 BEGIN `status=-5`。日志的 opcode 是事务操作，`1` 为 BEGIN，不能误读为 CREATE。

网络超时警告不是退出原因的充分证据。本轮确定性分页测试不访问网络、不下载图片，同样能重现旧包失败。

## 验证

- 相同生产 Host、WAMR AOT 和 LVGL 的 `store_browse_probe` 新增 `paged` 模式：每页四项，共十页四十项；超过十二项后反复淘汰旧卡片。旧包在分页 COMMIT 报错后不能完成；修复包完成，并在之后 240 次滚动中保持 `scroll_commits=0`。见 [report.json](report.json)。这些 PC 耗时不是 ESP 帧率，也不是旧/新性能对照。
- 原有 412×412、284 DPI 圆屏 `flow` 回归通过，列表与头尾栏不重叠，240 次滚动无 UI 提交。
- 生产商城网络、异步图标和连续滚动：Mosaico 参数模拟器，旧包出现事务错误及回调失败，新包连续二十次滚动无此错误。
- 真机已安装修复包（0.8.6），二十次向下、八次向上滑动，最终仍 `active=1`，本次日志无 UI 事务失败或应用异常；截图 [device-scrolled.png](device-scrolled.png)。仅更新商城包，未删除其他应用或修改用户设置。
- Mosaico Host 字体固件构建通过，尚未烧入，因此上述真机截图仍是旧 Host 的小字；[simulator-final-home.png](simulator-final-home.png) 展示已有正确 DPI 字体实现的模拟器布局。不能将它当成新固件真机截图。
- 布局 ASan/UBSan 回归加入 305 DPI，ESP Host 原生 ASan/UBSan 回归通过。模拟器与 ESP32-S31 应用包均构建通过，最新模拟器 CTest 36/36、全部 C++ 应用原生 ASan/UBSan 回归通过。

## 内存与限制

没有新增应用常驻状态、字体数量、帧缓冲或命令队列。整机固件对比本轮前的 HUD 候选，`.dram0.data=39196`、`.dram0.bss=31320`、`.ext_ram.bss=36016` 字节均不变，Flash text 增加 48 字节。较大的字形会影响缓存填充及绘制成本；未完成新固件真机测量，不能据此宣称动态内存峰值或帧率不变。设备 `MEMORY` 的累计最低空闲/峰值不代表本次单轮峰值。

本轮修复不将日志中的触摸 I²C 错误累计计数、TE 超时或网络等待一并归因或宣称解决。

## 重跑

在 PXA 产品仓库根目录：

```sh
tools/app.sh build store --target simulator,esp32s31 \
  --source-root local/pxa-apps --output local/store-check
cmake --build build/simulator/sensecap-watcher --parallel 4
python local/pxa-apps/store/tests/build_browse_probe.py \
  build/simulator/sensecap-watcher local/store-check/probe
mkdir -p local/store-check/report local/store-check/state
local/store-check/probe local/store-check/pxa-store \
  local/pxa-apps/.dev-signing/publisher-public.der \
  local/store-check/report paged local/store-check/state
```

把 `paged` 换成 `flow`、另用空状态目录，可重跑原圆屏布局与滚动回归。真机输入与截图脚本、本轮完整日志及冻结固件位于产品仓库忽略目录 `local/store-mosaico-20261016/`；新字号仍需按板级正常流程更新应用分区固件后验证。


## 后续修正：加载更多残留

之前验证只覆盖了分页完成和应用存活，没有断言页脚顺序和状态，这部分验证不足。

- 紧凑布局 `status_height=0` 原先直接跳过状态更新；自动请求开始后没有及时把页脚改成“加载中”并禁用点击。现在所有布局都更新页脚，加载完后隐藏整个节点（退出布局和点击），失败保留“重试”。重新点击当前“应用/游戏”页签可刷新目录，无需在已加载完的列表末尾保留刷新按钮。
- 部署中的 LVGL Host 忽略 `CREATE.before`，使新卡片仍排到旧页脚后方。本次应用改用已支持的 MOVE，一次追加 PATCH 内把保留的页脚移到最后，不依赖此 CREATE 参数，也不增加常驻状态。
- 强化回归：每页人为延迟 600 ms，检查页脚始终在最后、加载时文字为“加载中”且不可点击；十页完成后页脚隐藏且不可点击。上一轮包在加载状态断言失败，新包通过 818 次加载中断言及最终隐藏断言，后续 240 次滚动仍为零 UI 提交。见 [footer-report.json](footer-report.json)。同样是十二项环形目录，UI 节点数/当前/峰值、Guest 线性内存保持相同；不把两次 PC 进程的其他内存差异当作整机内存证明。
- 本轮应用包 `local/store-mosaico-20261016/footer-fixed-final/pxa-store.pxa` 已构建 ESP32-S31 与模拟器 AOT。上述页脚修正尚需安装到设备；设备此前安装的包仅含第一轮崩溃修复。
- 字号固件第一次更新在 ROM SYNC 阶段失败（无数据返回，尚未擦写）。等待新的 BOOT+RESET 枚举重试，不能宣称设备字号已经变化。只写应用分区的候选仍为 `store-dpi-firmware.bin`，SHA-256 见忽略目录的 `candidate.json`。

真实商城目录模拟器的加载末尾截图：[footer-fixed-end.png](footer-fixed-end.png)。本轮已等待两轮新的 ROM 枚举，均未出现；等待程序已退出，没有后台烧录任务。设备字体与本轮页脚修复的真机验证仍待设备恢复通信。


## 再次定位：烧录后仍然偏小的真实原因

用户自行重新烧录后，文字仍小。复查生产启动路径发现：`pxa_integration_start()` 先执行 `pxa_host_initialize()` 创建字体，随后 `CreateSystem()` 才把板级 DPI 写入 LVGL。之前仅在字体加载处乘 DPI 的修改仍会读到默认 130 DPI；305 DPI 面板的 12 sp caption 被创建为 10 px，16 sp body 为 13 px。上一次把“显示参数在 Host 初始化前发布”作为前提是错误的。

修复把板级 profile 读取和 DPI 发布提前到 Host 初始化之前，并把同一份 profile 传给后续系统创建，不再重复查询板级参数。六套字体仍只创建一次，无额外字体实例或缓存。启动日志新增 `Display prepared for Host: 480x480 dpi=305`，Host 字体日志给出 sp、px 和实际 line height，字体加载失败也有明确警告，避免继续靠页面观感推测是否生效。

新增 `firmware/components/pxa_integration/tests/test_startup.py` 直接提取执行生产启动函数和字体加载函数，仅模拟板级、LVGL 与服务边界。旧启动函数稳定失败：`DPI=305, 12 sp: got 10 px, expected 23 px`；修复版覆盖 0/80/130/160/216/305/480 DPI、两种挂载顺序、显示缺失和 Host 初始化失败，ASan/UBSan 通过，并接入常规 ESP Host 原生测试。Mosaico 固件完整构建通过。

新冻结候选：`local/store-mosaico-20261016/dpi-startup-firmware.bin`，SHA-256 `9dd69bcfebb689b9ccf493ad255dd7b35419f3439a2c6fb2a568ab57fb02efe7`。SRAM data/bss 和外部 BSS 不变；相较仅乘 DPI 的旧候选，Flash text +632 B、rodata +176 B（含启动诊断）。冷启动字形缓存的真机动态峰值仍需测量。

本轮页脚修复包已成功安装；新的启动顺序固件正在等待手动 BOOT+RESET 后验证。上面旧的 `store-dpi-firmware.bin` 不能用来验证本次最终字号修复。

本轮未检测到新的下载模式枚举，已停止等待程序并释放 USB 保持唤醒句柄；未烧入启动顺序修复固件，没有遗留后台烧录任务。用户后续应使用 `dpi-startup-firmware.bin` 或从最新源码重新构建应用分区，不应再使用此前的 `store-dpi-firmware.bin`。


## 图标仍小时的后续修复

文字已按 DPI 生效后，ICON 角色仍使用独立的 14 px 固定位图字体。因此仅调整文字或重烧字号修复版不会放大底栏图标。

新增通用、只读的图标字体尺寸选择：从 LVGL 自带 Font Awesome 中保留原有 60 个私用区图标的 4-bit 抗锯齿字形，去掉不需要的拉丁字形及 kerning，生成 8/12/16/24/32/48 px 六档；按 18 dp 选择最近尺寸。130 DPI 选 16 px，284/305 DPI 选 32 px，480 DPI 选 48 px。这是选择对应尺寸的原生位图，无图层放大、运行时重采样、额外字体 face 或图标缓存。自定义主题显式提供的图标字体保持优先；ESP 和模拟器使用相同选择函数，普通应用不必修改 ICON 角色用法。

商城底栏图标改为内容高度，清除原生按钮固定像素内边距，紧凑底栏 38→42 dp，为图标和文字各留足空间，避免固定高度裁切。[305 DPI 最新模拟器截图](icons-305.png)。

验证：

- 原生 LVGL ASan/UBSan：60 个 16 px 图标与原始字形的度量和像素逐字节一致；四个导航图标在高 DPI 下变大；字体选择和原始字形读取不分配 LVGL 堆；完整原生 LVGL UI 回归通过。
- 80/130/160/284/305/480 DPI，分别覆盖矩形、圆角矩形和圆屏：实际 Host/WAMR/LVGL 上断言正确的图标字体、完整字形边界、图标与标题不相交、两者都在按钮点击区域内。六组通过，见 [icons-layouts.json](icons-layouts.json)。每组后续 240 次滚动均零 UI 提交；这里的 PC 耗时不能当设备帧率收益。
- 延迟分页十页回归通过，包括页脚顺序、加载中禁点和完成隐藏；模拟器相关 visual/store/resource/reference/gestures 回归 16/16 通过；字体生成可逐字节重现。
- Mosaico 固件构建通过，仍适配原 4 MiB 应用分区：4,095,056 B。相较上一轮启动顺序修复，Flash text +224 B、rodata +111,544 B；`.dram0.data=39196`、`.dram0.bss=31320`、`.ext_ram.bss=36016` 不变。当前四块板均未启用 rodata 转 PSRAM；若产品将全部 rodata 搬入 PSRAM，必须另行计入这部分字体数据。未新增 Guest 线性内存、命令队列容量、图像资源或合成层。

本轮商城包已安装；图标字体位于 Host，新固件尚未写入设备，真机最终画面及绘制成本待确认。冻结固件为 `local/store-mosaico-20261016/dpi-icons-firmware.bin`，SHA-256 `1d2e67d59195b674e76a896ed2c8a6f8836c2ffbc33d9ae0e2d23c1398090f55`；它包含此前的文字初始化顺序修复。未改动设备的其他应用、用户数据或设置。

生成器与许可说明见 SDK `libpxa/adapters/lvgl/fonts/SYMBOLS.md`。图标位图保留 Font Awesome Free 的 OFL 许可文件，生成过程无需网络或新的构建期字体转换器。

本轮检测未出现新的 ROM 枚举，等待程序已停止并释放 USB 句柄，没有后台自动烧录任务。设备不应被描述为已经应用新的图标字体。
