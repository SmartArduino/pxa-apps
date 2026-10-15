# 0.2.2：新增网站、HTML 分页与整章读取

在已有努努书坊之外，加入书斋阁、笔趣阁 biquche 和文林社，共四个独立网站。
全部使用真实站内搜索；没有内置搜索结果或小说正文。每组都检查了搜索、完整目录和实际正文，再通过完整系统模拟器操作。
联网检查日期为 2026-10-15，站点日后的可用性和内容变化不在客户端控制范围内。

## 实际验证的书目

| 网站 | 查询 | 搜索结果 | 目录条目 | 正文检查 |
| --- | --- | ---: | ---: | --- |
| [努努书坊](https://www.nunubook.com/) | 长夜难明 | 1 | 69 / 7 页 JSON | 首项 282 字节、索引 11 为 6545 字节 |
| [书斋阁](https://www.shuzhaige.com/) | 三体 | 1 | 121 | 前两项 18136、26230 字节 |
| [笔趣阁 biquche](https://www.biquche.com/) | 庆余年 | 14 | 813 / 9 页 HTML | 首项单页；索引 12 合并 3 页为 5992 字节 |
| [文林社](https://www.wenlinshe.com/) | 儒林外史 | 1，直接到书页 | 56 | 第一回 21051 字节；另查“三国”返回 14 项 |

字节数是去除网页标签后的 UTF-8 正文，不含客户端加上的章名和换行。
目录条目去重并校验各页计数；“收齐”指网站提供的条目，不代表出版文本完整。
例如《庆余年》站点开头含读者文章，客户端保留原顺序，没有擅自改成正文第一章。
网络记录仅保留地址、状态、大小、计时与散列，没有把小说文本收入包或测试仓库。

另一个候选站 bqge9 的后续正文页反复超时，未纳入默认列表或分发配置。
不把只能访问首页、只能搜索或只能读取前半章的站点算作可用。

## 可复用的规则能力

- 属性存在 / 等值选择器，如 `link[rel=canonical]@href`，以及图片 `alt` 提取；仍不构建 DOM、不运行脚本。
- `search.single`：精确搜索直接重定向到详情页时，从书名、规范链接、作者提取单本结果。
- 可选固定 `userAgent` / `referer`；新站点按规则使用普通公开网页请求，不处理登录、验证码或付费章节。
- `toc.paged.format:html`：验证 HTML 页号，按需补充后续目录；无需为打开首章先取完九页目录。
  尚不知整本总章数时显示已读取数量，读到末页才确定总数。
- `content.paged`：支持页数 + URL 模板，或精确“下一页”链接。只有同一章全部页面成功才发布正文；
  失败保留原位置，重试重新读取。明确区分“下一页”和“下一章”，并检测循环与计数变化。

规则说明和可独立导入的四份 JSON 见[书源规则](source-rules.zh-CN.md)及[配置列表](../source-examples/README.md)。
仍最多 8 个来源、16 本书、1024 章；目录响应 96 KiB、单次正文及合并单章 64 KiB、分页最多 128 页。
部分网站的超长目录或章节会超过限制并报错；并非保证站内每一本书都可读。

## 状态、内存与界面

默认仍是按需阅读，四站实际读取后均未创建正文 `.txt` / `.part`。
下载离线、清缓存、已有书架和进度继续沿用原实现。
状态版本保持 4，新增 `sourceRevision:2`：已有默认来源的旧安装一次性补入三个新站点；
自定义规则、名称、开关与书籍不被覆盖。自定义列表、空列表以及本次目录版本中已删除的来源不会被自动补回。

内置规则移到共享只读存储，默认 `Source` 与状态文件不再各自持有完整 JSON；自定义覆盖规则仍可保存。
没有扩大固定缓冲：TXT 窗口 8192 字节、Canvas 3600 字节、协程池 16400 字节、线性 C 栈 64 KiB、线性内存上限 2 MiB。
没有新增帧缓冲、深度缓冲或字库。合并正文按当前章增长，不预分配整书或最大目录。

每站使用独立新进程测量：Guest 初始均为 2 页（128 KiB），努努观测最高 3 页（192 KiB），
本次其他三组较长正文 / 目录观测最高 4 页（256 KiB）；协程槽位峰值 8，分配失败 0。
相同努努样本的线性页数与 0.2.1 一致；其他书目工作量不同，不能据此声称峰值降低或与旧版等价。
这些是本次运行的线性页数观测，不是逐次分配器峰值，也不是设备 SRAM / PSRAM 实测。
新增解析能力增加映射代码和包体积，具体大小、相对旧包的差值及散列列在[验证数据](2026-10-15-more-website-sources.json)。

联调同时发现并修复大屏搜索的固定按钮容量问题：结果较多时保留导航容量，通过下一组访问剩余结果，
不扩大缓冲、不丢结果。书架和目录也按固定节点 / 回调容量限制单屏数量。

![真实站内搜索](2026-10-15-more-website-sources/search.png)
![同章三页合并，分页标记已去除](2026-10-15-more-website-sources/multipart.png)
![按需取到第二页目录](2026-10-15-more-website-sources/toc-page-2.png)
![文林社古典小说](2026-10-15-more-website-sources/classics.png)
![高 DPI 圆屏本地回归](2026-10-15-more-website-sources/round.png)

## 验证与复现

- Clang C++26、ASan / UBSan：选择器、请求规则、目录游标、分页正文、状态迁移、空列表、自定义规则以及超限 / 重复 / 缺页检查通过。
  2,264,071 字节、100 章真实长篇的索引及多尺寸排版检查通过。
- 四站联网矩阵：使用应用相同的 C++ 解析代码，收齐上述全部目录并读取样本章节，保存正文 SHA-256。
- 800×480 / 160 DPI 完整系统模拟器：每站独立安装与启动，实际搜索、阅读、保存进度；
  Guest 完整章节大小与原生结果核对（此项比较字节数，不是 Guest 全文散列）。biquche 另验证三页正文及第二页目录跳章。
- 296×240 / 160 DPI：HTML 分页目录、两页一章、缺页 503 后重试、跨章目录 503 后回滚、TXT 窗口与历史边界、
  重启、明确离线下载、尾段完整性、清缓存、设置、自动翻页和错误 UTF-8。
- 480×480 / 320 DPI 圆屏：JSON 分页目录、搜索、导入、阅读、离线下载、清缓存和偏好回归。
- 正式包包含模拟器、ESP32-S3、ESP32-S31 AOT；未插桩包重放原有 37 个正文页面，图像与参考一致，缓存和进度保留。

本轮没有刷写真实设备或上传商店；外网请求计时不是设备显示延迟或稳定性能基准。
从工作区根目录执行：

```sh
SANITIZE=1 bash local/pxa-apps/novel-reader/tests/test.sh
python3 local/pxa-apps/novel-reader/tests/source_matrix_probe.py \
  --output local/reader-check/native
python3 local/pxa-apps/novel-reader/tests/build_playtest.py \
  --target simulator --output local/reader-check/build
python3 local/pxa-apps/novel-reader/tests/source_ui_probe.py \
  --package local/reader-check/build/artifacts/pxa-novel-reader.pxa \
  --native-report local/reader-check/native/report.json --output local/reader-check/live-ui
python3 local/pxa-apps/novel-reader/tests/session_probe.py \
  --package local/reader-check/build/artifacts/pxa-novel-reader.pxa \
  --html-paged --stress --output local/reader-check/session
bash tools/app.sh build novel-reader --target simulator,esp32s3,esp32s31 --aot-only
```

模拟器脚本默认使用 `build/simulator/reader-validation` 的完整系统模拟器，也可传入同版本 `--binary`。
使用独立 QA 应用身份和临时数据，外网失败算失败，不用本地夹具冒充成功；本地夹具只用于可控错误与回归。
