# 《西游记》缺字修复（2026-10-11）

缺字来自系统字体。缓存及分页日志中有完整的 `第一回 靈根育孕源流出　心性修持大道生`，原字体却没有“靈”，同一段的“亂、濛、闢、茲”也没有字形。原文是繁体，保留原文及原始字节位置，不做繁简替换。此前全文摘要和排版边界测试只能确认文字没有在分页时丢失，不能证明每个字都画出了字形；本轮补充字符覆盖、实际位图和截图逐字检查。

默认系统字体更新为内部名称 **PXA Sans SC** 的中国大陆黑体。保留原有全部字符与字形，增加 Big5 常用字区的汉字及未修改《西游记》的全部用字，共增加 **1,807 个字符**。新增轮廓来自 Noto Sans CJK SC Regular 2.004 的 SC face，避免取到 TTC 默认的日文字形。原有轮廓直接保留，只合并轮廓、指令和横向度量完全相同的重复字形。继续使用抗锯齿及现有字号/DPI 选择；字体名和文件许可证见系统字体目录的 NOTICE、OFL.txt。

同一 QA 应用包、同一缓存、296×240 / 160 DPI、相同字号及行距：

| 原字库 | 修复字库 |
| --- | --- |
| ![缺字](2026-10-11-font-coverage/before.png) | ![完整字形](2026-10-11-font-coverage/after.png) |

完整验收记录与五种屏幕截图在附件目录：

- 原字库的独立检查准确报出 `亂 U+4E82`、`濛 U+6FDB`、`茲 U+8332`、`闢 U+95E2`、`靈 U+9748` 缺失，能复现截图问题。
- 未修改《西游记》2,264,071 B，**4,638 个非空白、非控制用字**，在 12 / 16 / 28 / 56 像素下均有映射及实际可见位图，缺失和空白字形均为 0。旧字库在这些用字中缺失 **1,209 个不同字符**。
- 使用 Host 实际采用的 FreeType `FT_LOAD_NO_AUTOHINT` 等参数，对所有原有可绘制字符在四种像素大小下逐字比较：**53,304 次比较，位图、位置和水平进距差异 0**。原来有意为空白的空格/格式字形仍保持相同。
- 完整系统模拟器五种屏幕：296×240 / 160 DPI、480×480 / 305 DPI、360×480 / 320 DPI、480×480 / 320 DPI 圆屏、800×480 / 160 DPI。正文最小/最大字号、首回标题及后续正文逐个汉字检查截图中的实际墨迹；同一流程还覆盖页码、目录、翻页往返、字号设置、输入法和搜索。
- 字符索引编码测试覆盖稀疏/连续字符、共享字形、随机缺口、BMP 边界和 Big5 常用字区末尾。生成后的映射逐项复查；选择日文 face 会被构建工具拒绝。
- 已更新工作区系统字库及 pai-touch 模拟器的阅读器包，保留用户书架、缓存与设置。重新启动用户的 pai-touch 完整系统模拟器，并打开现有《西游记》缓存，确认标题显示完整；当前画面见下图。esp-mosaico 模拟器的阅读器包沿用上一轮已安装的同一正式包。字库由新启动的 Host 读取。

![用户 pai-touch 模拟器的当前正文](2026-10-11-font-coverage/user-pai-reading.png)

内存和存储分别核对：

| 项目 | 原字库 | 新字库 | 范围 |
| --- | ---: | ---: | --- |
| 字库文件 | 3,224,544 B | 3,879,128 B | Flash/文件占用，增加 654,584 B |
| 映射字符 | 13,328 | 15,135 | 原有覆盖全部保留 |
| FreeType 打开后活动请求字节 | 6,119 B | 6,080 B | 每次新进程，原生分配回调 |
| 四种字号绘制后活动请求字节 | 36,871 B | 36,832 B | 同一真实小说全部不同用字 |
| 本次 FreeType 请求峰值 | 46,267 B | 46,228 B | 打开、绘制、关闭完整运行 |
| 关闭后活动请求字节 | 0 | 0 | 完整释放 |
| Guest 线性内存运行峰值 | 3 页 / 192 KiB | 3 页 / 192 KiB | 五屏完整系统日志 |
| 固定协程池 | 16,400 B / 8 槽 | 相同 | 五屏最高使用 7 槽，失败 0 |
| 绘制命令固定缓冲 | 3,600 B | 相同 | 五屏命令实际最大 2,027 B |

字库仍通过现有 FreeType 文件流读取，没有整份加载进 Guest，没有新增字体回退 face、图集、全屏缓冲或缓存池。C++ SDK、阅读器运行代码和 AOT 包没有为本轮字体修复变化；代码映射及帧缓冲配置沿用上一轮。构建工具与检查属于离线能力，不进入热路径。

FreeType 数据是原生库请求的活动字节与本次进程峰值，排除分配器头、LVGL 字形缓存及其他 Host 分配。Host 已禁用 auto-hinting，测量必须采用相同参数；开启 auto-hinting 的结果有不同开销，不能混作前后对比。上述结果证明当前字体加载/光栅化请求没有增加，**不是整台 ESP 的 SRAM / PSRAM 测量**。本轮只查询了可响应的 `/dev/ttyACM1`（序列号 `a4:cb:8f:d6:8d:a4`），未确认其为指定 pai-touch；截图请求时端口已被其他进程独占，未打断，也未写入字体或改设备设置。真机字体替换、整体内存和屏幕显示仍未验收。

字体保留原有路径 `factory/base/system/fonts/noto_sans_cjk_common.ttf`，现有模拟器/固件读取逻辑可继续使用。**只更新阅读器包不会更新系统字库**，需要更新字体文件并重启 Host，释放已有字体 face。字体替换应采用原子发布；必须保留应用和缓存数据，不能靠清空 LittleFS 安装。出厂字体打包同时携带 OFL.txt 和 NOTICE。

当前覆盖包含原子集、Big5 常用汉字及此验收小说的全部用字，仍不是完整 Unicode 字库。超出覆盖的字使用可见缺字标记，不再悄悄消失；后续可通过重复 `--corpus` 扩充，或选用更大的可选 Big5 全字区并重新测量存储及内存。新字体增加的约 639 KiB 是 Flash 文件占用，需要有实际空间才能安全安装到设备。

从工作区根目录复现核心检查：

```sh
python3 -m unittest discover -s tools/fonts -p 'test_*.py'
python3 local/pxa-apps/novel-reader/tests/font_coverage_test.py --book local/reader-typography-20261010/xiyou.txt
cc tools/fonts/ft_font_probe.c -I/usr/include/freetype2 -lfreetype -Wall -Wextra -Werror -o /tmp/pxa-font-probe
/tmp/pxa-font-probe factory/base/system/fonts/noto_sans_cjk_common.ttf local/reader-glyphs-20261011/unique-novel.txt
python3 local/pxa-apps/novel-reader/tests/ui_sdk_probe.py --package local/reader-pages-20261010/qa-verified/artifacts/pxa-novel-reader.pxa --cached-state local/reader-typography-20261010/cache-stable-state --real-book local/reader-typography-20261010/xiyou.txt --font-boundaries --output local/reader-glyphs-20261011/after-ui-verified
```

构建步骤见工作区 `tools/fonts/README.md`。附件记录最终字库、生成工具、源 TTC、正文和复用 QA 包摘要；原文及缓存沿用上轮排版验收。CFF、大字区及调试候选不属于本轮交付。
