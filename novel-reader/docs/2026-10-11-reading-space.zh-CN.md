# 阅读页空间利用率（2026-10-11）

阅读页使用独立的紧凑书名区，不再占用菜单按钮行的高度。书名直接选用 Host 的 caption 字体；修复了高 DPI 下按像素选字体角色、再由 Host 缩放造成的二次放大。圆屏书名居中，矩形/圆角屏左对齐。顶部和底部安全区外的附加间距略微收紧，页脚使用系统安全区与 4 dp 最小底距中较大者，正文、页码之间仍留出间隔，四角和圆屏继续逐行避让。

正文字号、用户行距、首行缩进和主题不变。正文与后台页数统计共用新的起止区域，换设备或字号仍重新分页。不是所有留下的空隙都可以放进一整行；没有拉伸行距、缩小正文或将半行文字画到页码上。

## 相同正文的前后对比

同一系统字体（SHA-256 `7b89dc0cb85fec0458461953a7e9a08c4012a0f84c3ce44e64407743a248bd10`）、原始《西游记》、第二回原文件字节偏移 22819、16 dp 正文、6 dp 行距、安全区 12 px。按 DPI 得到的正文像素大小前后一致。每次使用私有 QA 状态，不接触用户书架。统计的汉字数是本页截图逐字检查数，不代表整书平均收益。

| 屏幕 | 正文顶端 y：前 → 后 | 正文行数：前 → 后 | 可见汉字：前 → 后 |
| --- | ---: | ---: | ---: |
| 296×240 / 160 DPI | 50 → 37 | 4 → 5 | 43 → 58 |
| 480×480 / 305 DPI | 81 → 56 | 5 → 6 | 53 → 65 |
| 360×480 / 320 DPI | 84 → 59 | 5 → 5 | 31 → 31 |
| 480×480 / 320 DPI，圆屏 | 84 → 59 | 4 → 5 | 38 → 49 |
| 800×480 / 160 DPI | 52 → 37 | 11 → 12 | 346 → 387 |

| 480×480 修改前 | 修改后 |
| --- | --- |
| ![前](2026-10-11-reading-space/before/480x480-305dpi/fixed-second-chapter.png) | ![后](2026-10-11-reading-space/after/480x480-305dpi/fixed-second-chapter.png) |

| 小圆角屏修改后 | 圆屏修改后 |
| --- | --- |
| ![圆角屏](2026-10-11-reading-space/after/296x240-160dpi/fixed-second-chapter.png) | ![圆屏](2026-10-11-reading-space/after/480x480-320dpi-circle/fixed-second-chapter.png) |

360×480 在本场景下没有增加完整一行；减少的标题空间仍不足以容纳下一行及其间距。圆屏首尾行可用宽度变化，也会影响每行字数。章节内的诗歌缩进是原文排版，不按普通散文强制拉平。

## 固定预算和分页边界

正文区域增加后，800×480 小字号的密集文本可能超出原有 3600 B 绘制缓冲。分页器现在同时检查几何和实际编码预算，预留清屏及最长页码命令，只提交完整行；超出的原文留到下一页。流式页数统计和上一页定位使用同一规则。预算字段放入原有游标的对齐空隙，没有增加其尺寸，也没有新增动态容器。

布局构造函数由各条渲染/索引路径共用，保持 O3，避免重复展开造成 S3 TEXT 跨越原有映射分段。没有新增字体对象缓存、帧缓冲、深度缓冲或覆盖层。

| 项目 | 前 | 后 | 依据 |
| --- | ---: | ---: | --- |
| ReaderApp 固定对象（Wasm32） | 15088 B | 15088 B | 编译器布局 |
| Page / PageNumbers / PaginationCursor | 1548 / 152 / 12 B | 相同 | 编译器布局 |
| Canvas 固定命令容量 | 3600 B | 3600 B | 稠密正文预算测试 |
| 正文 / 网络包 / 共用 I/O 缓冲上限 | 16 / 4 / 4 KiB | 相同 | 配置与实现 |
| 协程池保留空间 | 16400 B | 16400 B | 新进程日志 |
| 五屏流程最高实际使用槽数 | 7 | 7 | 失败 0 |
| Guest 线性内存本次运行峰值 | 3 页 / 192 KiB | 3 页 / 192 KiB | 新进程日志 |
| 五屏流程最长绘制命令 | 2027 B | 2247 B | 实际日志，容量未增加 |

Host 保留的命令有效负载可能随可见文字增加，不等同于扩大 Guest 的固定缓冲。系统进程 RSS / HWM 单独保存在附件，包含 SDL、字体缓存和系统组件，不能当作 ESP SRAM / PSRAM。真机的 Host 稳定占用与真实峰值没有在本轮实测，不能仅凭固定容量或 AOT 映射声称其全部保持不变。下面的代码映射为编译产物按 32 KiB 粒度计算的预算。

| AOT 指标 | 前 | 后 |
| --- | ---: | ---: |
| S3 文件 | 969432 B | 969856 B |
| S3 TEXT 映射预算 | 917504 B | 917504 B |
| S3 TEXT | 914532 B | 914576 B |
| S31 文件 | 1002852 B | 1003368 B |
| S31 TEXT 映射预算 | 917504 B | 917504 B |
| S31 TEXT | 897836 B | 898204 B |

INIT_DATA / FUNCTION 等节尺寸也在 `aot-comparison.json` 中，不能只据 TEXT 推断 Host 全部加载开销。正式包包含三个架构的 O3 AOT，不含 `.wasm`。

## 验证

- ASan / UBSan：原始《西游记》2264071 B、100 回，四种分辨率 × 12/16/28 字号全文非空白字节摘要不变；流式总页数、稀疏定位和实际分页一致。另测 512/769/4096 B 输入窗口，以及 0/6/12 dp 行距下的密集正文与命令预算。
- 五种完整系统模拟器：12–28 dp 字号、阅读偏好双行预览、轻点翻页往返、目录跳转、书架长按、原生输入法。截图检查 Host 字体的每个正文汉字有墨迹，书名与正文不重叠，页码不截断。
- 原有在线缓存验收继续覆盖边缓存边阅读、增量目录、暂停/重开/续传，以及断流、源内容变化、空正文、非法 UTF-8 拒绝发布并保留完整缓存。
- 测试日志读取改为只解析完整行，避免把日志写入中的半条触点记录误判为失败；字号边界测试后恢复 6 dp 行距，保证前后工作负载相同。

## 模拟器正式安装

三架构正式包已安装到用户的 pai-touch 与 esp-mosaico 模拟器存储。pai-touch 完整系统界面重新启动并打开原来的《西游记》位置，字号 12、行距 0、纸张配色均保留，6 个私有文件的摘要与安装前完全一致；当前字号 12 / 行距 0 的这页从 8 行增加为 9 行。esp-mosaico 的此存储中没有该应用的私有书籍文件，使用离线安装器更新包，没有删除应用或系统数据。

| 用户原有偏好，修改前 | 同位置、同偏好，修改后 |
| --- | --- |
| ![前](2026-10-11-reading-space/user-before.png) | ![后](2026-10-11-reading-space/user-after.png) |

正式包需在完整系统 UI 运行。当前 `pxadb package run --simulator` 会进入独立 product runner；本次尝试返回 activation plan unsupported，未用它作为验收。已改为重新打开完整系统 UI 并在书架进入，最终画面正常。启动工具这条路径的问题不属于阅读布局，保留记录供后续修复。

## 复现

在工作区根目录：

```sh
SANITIZE=1 bash local/pxa-apps/novel-reader/tests/test.sh local/reader-typography-20261010/xiyou.txt
python3 local/pxa-apps/novel-reader/tests/build_playtest.py --target simulator --source http://127.0.0.1:18766/catalog.json --output local/reader-space-20261011/qa-footer
# 可分别指定 --profile 以并行检查独立的私有状态：
python3 local/pxa-apps/novel-reader/tests/ui_sdk_probe.py --package local/reader-space-20261011/qa-footer/artifacts/pxa-novel-reader.pxa --cached-state local/reader-typography-20261010/cache-stable-state --real-book local/reader-typography-20261010/xiyou.txt --font-boundaries --profile 480x480-305dpi --output local/reader-space-20261011/check
# 在线故障回归需要另一个终端运行 fixture_server.py --port 18766：
python3 local/pxa-apps/novel-reader/tests/ui_sdk_probe.py --package local/reader-space-20261011/qa-footer/artifacts/pxa-novel-reader.pxa --fixture --failure-cases --output local/reader-space-20261011/cache-footer
bash tools/app.sh build novel-reader --target simulator,esp32s3,esp32s31 --aot-only --output local/reader-space-20261011/release-footer
```

来源及许可沿用排版验收报告，原文、部分缓存和包的摘要记录在附件。未扩大存储配额或更改系统设置。

本轮 DPI 角色缩放和画面验收来自完整系统模拟器。当前 ESP Host 的 `load_ui_font` 仍使用固定像素字号角色，其高 DPI 原生标题字体需要固件端单独核对；正文的显式像素字号不走该角色路径。本轮没有刷写真机或将模拟器截图视作真机验证。
