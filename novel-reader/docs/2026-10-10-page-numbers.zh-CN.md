# 阅读页页码与页数（2026-10-10）

正文底部居中显示小号页码，跟随正文主题，按 DPI 缩放并避开安全边距、圆角和圆屏弧边。完整缓存显示 `12 / 320 页`；部分缓存显示 `12 / 已缓存 86+ 页`。尚在计数时总页数为 `…`，任意位置跳转后尚未定位页码时当前页为 `—`，后台任务完成后更新。

页数按本设备当前字号、行距及屏幕形状计算，属于阅读器的排版页数。更换排版会重新计算；不同设备的页数可以不同。页脚预留了高度，正文每页容量会相应变化，字体大小不因此缩小。目录跳转仍落在章节起始字节，页码显示该位置所属的全书排版页；跳到某页中部后向前翻，会先回到该排版页的起点。Gutenberg 正文首次识别到章节标题时以正文起点计数，继续沿用原有跳过许可前言的阅读行为。

同一份《西游记》缓存、480×480 / 305 DPI、相同字号与行距：

| 修改前 | 增加页码后 |
| --- | --- |
| ![原阅读页](2026-10-10-page-numbers/before.png) | ![页码](2026-10-10-page-numbers/480x480-305dpi/page-number.png) |

| 部分缓存 | 完整缓存 | 圆屏 |
| --- | --- | --- |
| ![缓存页数](2026-10-10-page-numbers/cache/partial-page-number.png) | ![全书页数](2026-10-10-page-numbers/cache/downloaded-reading.png) | ![圆屏页码](2026-10-10-page-numbers/480x480-320dpi-circle/page-number.png) |

计数复用实际正文分页器，按最多 4 KiB 的文件窗口分段运行并让出执行，不等计完整本书才显示正文。它与章节扫描合用现有任务和 I/O 缓冲；网络任务开始前终止借用缓冲区的扫描，防止跨调度的待处理文本被覆盖。没有按页增长的索引：固定稀疏检查点逐级合并，用来定位任意字节位置和没有近期记录时的上一页。近期上一页记录从 64 项调整为 24 项，较旧的位置通过检查点恢复；同时移除了原先上一页回退路径额外的 16 KiB 临时正文字符串。

最终代码通过以下验证，结果、日志、截图和摘要在附件目录：

- 完整系统模拟器五种屏幕：296×240 / 160 DPI、480×480 / 305 DPI、360×480 / 320 DPI、480×480 / 320 DPI 圆屏、800×480 / 160 DPI。检查实际 Host 字体的页脚宽高和截图中的文字边界；覆盖 12–28 字号、行距、目录、重新打开书籍、轻点翻页往返、拖动与单独抬起不翻页。
- 在线 CC0 验收小说 258,777 B，测试中部分缓存页脚为 `1 / 已缓存 509+ 页`，完整后为 `1 / 1842 页`；章节跳转清空近期记录后，上一页定位到 `681 / 1842 页`。覆盖暂停、重开、续传、增量目录，以及断流、书源变化、空正文、无效 UTF-8 拒绝发布并保留完整缓存。
- 原始《西游记》2,264,071 B、100 回，ASan / UBSan 全文检查：四种分辨率 × 12 / 16 / 28 字号。流式计数与逐页实际排版的总页数一致，稀疏定位的页码和页起始字节一致，全文非空白字节摘要保持一致。另覆盖 512 / 769 / 4,096 B 窗口、跨窗口长空白与段落边界。
- simulator / ESP32-S3 / ESP32-S31 均以 O3 构建，正式包只携带 AOT。已替换安装到用户的 esp-mosaico 完整系统模拟器，保留其书架、缓存和设置。

内存及包核对：

| 项目 | 修改前 | 修改后 | 核对方式 |
| --- | ---: | ---: | --- |
| ReaderApp 固定对象，Wasm 32 位 ABI | 15,096 B | 15,088 B | 同一 SDK 的编译器布局 |
| Page，包含在 ReaderApp 内 | 1,548 B | 1,548 B | 编译器布局 |
| PageNumbers，包含在 ReaderApp 内 | 无 | 152 B | 由减少近期记录腾出空间 |
| 固定绘制缓冲 | 3,600 B | 3,600 B | 配置未扩大 |
| 正文 / 网络包 / 共用流缓冲上限 | 16 KiB / 4 KiB / 4 KiB | 相同 | 没有新增整书缓冲 |
| Guest 线性内存，本次运行峰值 | 3 页 / 192 KiB | 3 页 / 192 KiB | 五个新进程实际日志 |
| 固定协程池 | 16,400 B / 8 槽 | 相同 | 运行日志及配置 |
| 五屏回归实际最高使用槽数 | 7 | 7 | 本次进程内峰值，失败 0 |
| 在线及故障回归最高使用槽数 | 8 | 8 | 固定池内使用，失败 0 |
| 五屏测试绘制命令最大长度 | 2,208 B | 2,027 B | 同类测试日志；正文内容容量变化 |
| S3 AOT 文件 | 949,528 B | 969,432 B | 最终包解析 |
| S3 AOT TEXT | 895,316 B | 914,532 B | 最终包解析 |
| S3 TEXT 按现有粒度映射预算 | 917,504 B | 917,504 B | 32 KiB 粒度计算 |

没有新增帧缓冲、深度缓冲、字体图集或全屏覆盖层。AOT 文件新增约 20 KiB 是 Flash 包的变化；INIT_DATA / FUNCTION 等元数据尺寸也记录在附件，不能仅靠 TEXT 映射推断所有 Host 加载开销。本轮没有重新实测 ESP 的 SRAM / PSRAM 稳定占用或本次运行峰值：pai-touch 上一轮只余 4,096 B 存储，本轮 `/dev/ttyACM0` 查询未收到 HELLO，未更新或删除应用与数据，未改设备设置。另一个设备正在做输入测试，未打断。真机安装、触摸及完整 Host 内存验证仍待可用设备；上表 Guest 和绘制结果来自模拟器，S3 映射值是计算预算。

工作区根目录复现：

```sh
SANITIZE=1 bash local/pxa-apps/novel-reader/tests/test.sh local/reader-typography-20261010/xiyou.txt
python3 local/pxa-apps/novel-reader/tests/build_playtest.py --target simulator --source http://127.0.0.1:18766/catalog.json --output local/reader-pages-20261010/qa-verified
python3 local/pxa-apps/novel-reader/tests/fixture_server.py --port 18766
# 在另一终端中运行；仅使用隔离的 QA 身份和私有数据：
python3 local/pxa-apps/novel-reader/tests/ui_sdk_probe.py --package local/reader-pages-20261010/qa-verified/artifacts/pxa-novel-reader.pxa --cached-state local/reader-typography-20261010/cache-stable-state --real-book local/reader-typography-20261010/xiyou.txt --font-boundaries --output local/reader-pages-20261010/real
python3 local/pxa-apps/novel-reader/tests/ui_sdk_probe.py --package local/reader-pages-20261010/qa-verified/artifacts/pxa-novel-reader.pxa --fixture --failure-cases --output local/reader-pages-20261010/cache-verified
bash tools/app.sh build novel-reader --target simulator,esp32s3,esp32s31 --aot-only --output local/reader-pages-20261010/release
```

缓存模板与真实文本沿用上轮排版验收，原文来源见该轮报告。附件 `provenance.json` 记录本轮最终源码、正式包、QA 包及真实文本摘要，`memory-comparison.json` 区分对象尺寸、包节尺寸、计算映射和各新进程运行峰值。调试候选包不属于本轮验收结果。
