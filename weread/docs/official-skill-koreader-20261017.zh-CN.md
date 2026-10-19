# 官方 Skill 与 KOReader 插件参考核对

2026-10-17。此轮检查上游代码并用现有账号做只读业务请求；未安装 Skill、未运行上游安装脚本，也未修改 PXA 产品代码、设备固件、应用包或设备私有数据。研究结果不等同于新功能已经在 PXA 实现。

## 固定参考版本

| 来源 | 核对版本 | 内容与许可 |
|---|---|---|
| [Tencent/WeChatReading](https://github.com/Tencent/WeChatReading/tree/d98e66ee60a8c7bcd35da549d0643c4470439643) | `d98e66ee`，Skill 1.0.5 | Apache-2.0；主要是接口说明和调用工作流，没有独立的正文解码客户端 |
| [finlater/weread.koplugin](https://github.com/finlater/weread.koplugin/tree/2bee0cc66c871c8781fef5078f1636662618e24e) | `2bee0cc6` | AGPL-3.0；Lua 插件，依赖 KOReader 的 UI、阅读引擎、文件系统与后台工作机制 |

两者在独立目录检出，没有覆盖原仓库。此轮未把插件 Lua 代码移入 PXA 应用；后续若采用上游实现代码，需要一并处理其许可。下面区分代码事实、实际服务结果和待验证设计。

## 官方 Skill 能提供什么

[官方调用说明](https://github.com/Tencent/WeChatReading/blob/d98e66ee60a8c7bcd35da549d0643c4470439643/skills/SKILL.md)使用 `POST https://i.weread.qq.com/api/agent/gateway`，Bearer Key 鉴权，JSON 顶层包含 `api_name`、`skill_version` 和业务参数。用户身份由 Key 绑定，无需复制网页 Cookie 到该网关。

现有网页登录会话成功调用只读的 `/api/skills/apikeyGet?only_show=1` 取得已经存在的 Key；再调用 `/_list` 返回 HTTP 200、`errcode=0`，实际列出 17 个接口。没有创建或重置 Key，无需重新扫码。

已公开并由服务枚举的能力包括书架、搜索、图书信息、目录、云端进度读取、个人笔记与划线、书评、推荐和阅读统计。枚举中没有章节全文、PDF、漫画流、听书音频、购买、进度写入或新建笔记接口。不能由此断言腾讯其他服务完全没有这些能力。

特别需要保留的接口语义：

- `/shelf/sync` 返回完整书架及分组、专辑等数据，文档未给出分页；不能因为是 JSON 就默认峰值很小，也不能把全部条目默认驻留内存。
- `/book/chapterinfo` 区分实际章节 UID、章节索引、层级与节内锚点。目录可见、`paid` 字段和真实正文获取是不同证据。
- `/book/getprogress` 的 `progress` 是 0–100 的整数；文档没有说明 offset 与本地 UTF-8 字节偏移的等价关系。
- `/book/bookmarklist` 主要返回划线；`/book/readreviews` 是段落划线下的想法，不是读取正文。
- 笔记分页使用 `lastSort` 等服务游标，不能统一改为整数 offset。

Key 仅保存在本地私有验证目录，没有写入产品源码、安装包、公共报告或诊断输出。更早由截图转录 Key 的尝试返回过 401；本轮通过现有会话取得 Key 的成功结果取代“尚未验证鉴权”的旧结论，不推断之前 401 的具体原因。

## KOReader 插件补充了哪些实现

### 登录与凭据分离

[`qr_login.lua`](https://github.com/finlater/weread.koplugin/blob/2bee0cc66c871c8781fef5078f1636662618e24e/weread/lib/qr_login.lua)使用 Skill 页面对应的扫码流程，处理轮询、取消、可选验证码，再读取已有 Skill Key。新账号使用新的 Cookie 集合，避免沿用旧账号凭据。

[`client.lua`](https://github.com/finlater/weread.koplugin/blob/2bee0cc66c871c8781fef5078f1636662618e24e/weread/lib/client.lua)把网关 Bearer 请求与网页 Cookie 请求分开；跨来源重定向清理敏感头。续期只有明确成功才提交 Cookie。这些处理值得借鉴。其错误诊断还会记录完整响应体，PXA 应继续仅记录数量、阶段和错误码，避免把正文或凭据带进日志。

### 正文与图片

[`content.lua`](https://github.com/finlater/weread.koplugin/blob/2bee0cc66c871c8781fef5078f1636662618e24e/weread/lib/content.lua)的实际正文路径不依赖 Skill 全文接口：

1. 从正式 Web Reader 页面读取当前阅读会话状态。
2. 以真实章节 UID 获取 EPUB 正文分片；CSS 使用独立分片。
3. 校验响应摘要、恢复编码并生成 XHTML；特定响应分支尝试 TXT 路径。
4. 根据章节资源描述获取图片，再生成 KOReader 可打开的单章或整本 EPUB。

代码还考虑了多个 `<body>` 拼接、脚注、章节目录层级、临时文件提交、断点继续和图片写盘。并不是简单抓取 HTML 第一个正文节点即可完成阅读。

这条路径有参考价值，但不能预设比现有轻量阅读接口更省资源：插件每章仍刷新 Web Reader 会话页面，本次页面约 345–349 KB，正文分片是额外请求。当前 PXA 本来就不需要 JavaScript 引擎，新增这条路径也不会额外获得“移除 JS”的收益。

### 预取、进度与阅读时长

[`chapter_prefetch_worker.lua`](https://github.com/finlater/weread.koplugin/blob/2bee0cc66c871c8781fef5078f1636662618e24e/weread/lib/chapter_prefetch_worker.lua)把正文、图片、加工和提交分阶段，设置取消检查；后台执行器一次运行一个子进程任务，并丢弃被替代的请求。PXA 可借鉴任务生命周期，但不能照搬进程模型。

[`progress_sync.lua`](https://github.com/finlater/weread.koplugin/blob/2bee0cc66c871c8781fef5078f1636662618e24e/weread/lib/progress_sync.lua)比较本地与服务端位置并处理冲突；[`read_report.lua`](https://github.com/finlater/weread.koplugin/blob/2bee0cc66c871c8781fef5078f1636662618e24e/weread/lib/read_report.lua)补充了 Web 阅读上报流程。

但 [`position_mapper.lua`](https://github.com/finlater/weread.koplugin/blob/2bee0cc66c871c8781fef5078f1636662618e24e/weread/lib/position_mapper.lua)有根据章节字数和本地阅读比例推算 offset 的部分。这是近似映射，不是原始 DOM 坐标证明。PXA 需要验证原始坐标、UTF-8、段落/锚点和排版后的页边界，不能直接把本地字节偏移上传，也不能把自动测试或后台时间记作阅读时间。

## 同一账号的只读正文对照

本次仅在开发机请求腾讯官方 HTTPS 服务，拒绝跟随重定向。没有调用续期、进度上报、购买、Key 生成或重置接口，也没有写回设备会话。

对用户说明已购买的《C++ Primer Plus》核对结果：

| 检查 | 实际结果 | 能证明什么 |
|---|---|---|
| 官方网关 `/_list` | 200，17 个接口，`errcode=0` | 官方 Key 鉴权与接口枚举成功 |
| 网关 `/book/chapterinfo` | 200，49,442 B，34 个实际章节 | 新网关目录可用；不等同于所有章节可读 |
| Web `/web/book/chapterInfos` | 200，51,792 B，34 个章节 | 与实际章节数量相符；原轻量页面的 635 个标题还包含节内目录 |
| UID 1 正文 | 3 个 EPUB 分片校验通过，解码 1,638 B UTF-8 XHTML，含正文节点 | 同一会话下，该正文路径有成功的正对照 |
| UID 11 正文 | EPUB 分支返回元数据和空 content；按插件分支尝试 TXT，两片各 33 B，摘要正确但最终解码 0 B | 这条路径也没有拿到该后续章节的正文 |
| UID 11 目录授权字段 | 网关和 Web 目录均为 `paid=0` | 服务回包与用户实际购买情况仍存在待定位差异，不能据此认定用户未购买 |

摘要正确、HTTP 200、目录完整，都不能单独作为正文读取成功条件。必须继续检查解码后内容；插件的格式回退也不能代替明确的授权错误分类。

这次对照没有解决已购书后续章节的问题，尚未确定是账号权益、版本/书目、Web 可用范围还是其他协议上下文导致。不能用伪造付费标记或无条件重试掩盖这个问题。

这是服务与协议验证，没有部署新 PXA 路径，没有产生新的真机 FPS、内存或延迟结论。不能用这些 HTTP 字节数代替同场景性能测试。

### 续期后复核与停止条件

后续通过应用自身的正常续期流程取得有效会话，再只读核对同一本书的 UID 11。Web EPUB 分支仍返回匹配书目和章节的元数据、空 content；TXT 回退两片摘要正确，但解码正文仍为 0 B。另一次对照补上官方来源响应的 Cookie 合并，结果相同。没有导入电脑凭据、修改购买状态或把 Cookie 写回设备。

这些结果只证明当前账号、该版本图书、已测轻量及 Web 分片路径未取得这章正文；没有直接证明真实 Kindle 浏览器中的官方页面同样不可读，更不能推断所有已购书都不支持。

2026-10-17 用户明确要求：如果 Kindle 路径也不能读已购内容，就不再尝试。对本书停止继续投入轻量路径和 Web 分片替代的正文尝试，不接入尚未取得后续正文的回退实现，不因同一失败要求再次扫码。现有能正常取得的正文、书架、封面和元数据能力保留；界面区分内容获取失败与未购买，不否认用户已确认的购买事实。

## 建议在 PXA 中采用的设计

| 部分 | 处理方式 | 内存与维护边界 |
|---|---|---|
| 官方元数据 | 原生 C++ 类型化网关调用，按功能启用目录、详情、笔记和云端进度读取 | 复用 SDK HTTP 与已有 JSON 借用视图；全量书架需要有界扫描或文件索引，不默认缓存完整 JSON 树 |
| 正文 | 独立内容服务，现有轻量路径继续可用；Web 分片作为经过验证的可选路径 | 逐片校验、分段处理、可取消；切换格式不能吞掉权限错误；避免 HTML、分片和新旧正文无预算地同时驻留 |
| 图片 | 复用现有 PageImages 与 Host 解码 | 可见页优先、共用像素预算、无 Cookie/Key 传给 CDN，不为封面或预取增加第二套像素缓存 |
| 凭据 | 网关 Key、网页 Cookie、图片请求各自受限到来源 | Key 跟随账号切换和退出；读取已有 Key 不要求用户重复扫码；不自动创建新 Key |
| 阅读位置 | 保留本地位置，再逐步建立服务端坐标映射 | 先实现可靠拉取；上传前验证坐标，冲突由用户选择；不能用百分比静默覆盖较新的进度 |
| 预取与离线 | 用户可选，当前页请求优先，最多一个后台任务 | 使用 SDK 协程及请求代次取消；缓存落盘需限额和原子提交，默认关闭整书下载 |
| 诊断 | 数量、阶段、限额、错误分类 | 不复制上游完整响应体日志；服务变化时保留旧页与账号数据 |

优先顺序是官方元数据与现有会话接入、准确的目录/权益分类、正文兼容验证、位置映射，再做笔记和可选预取。暂不引入 Lua、QuickJS、KOReader 阅读引擎或 EPUB 打包器，也不需要为这些接口新增 Host ABI。

每条新路径仍需合成故障/取消/限额检查、AOT/Wasm 模拟器与同固件真机对照；速度和资源收益只有在同账号、同书同章节、相同图片与排版条件下才能确认。
