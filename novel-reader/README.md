# 书页阅读（C++）

PXA C++ SDK 小说阅读器。借鉴 [KOReader](https://github.com/koreader/koreader) 的阅读菜单、字号和进度操作，界面与解析器自行实现，没有复制其 AGPL 代码。详见 [第三方说明](THIRD_PARTY.md) 和 [验收报告](docs/ACCEPTANCE-20261010.md)。

## 使用

- **书架**：继续阅读、上一页/下一页、移除本书。点击正文左/右侧翻页，中间打开菜单。
- **发现**：点击输入框使用系统中文输入法，输入书名或作者后搜索；点击书源名称切换来源。提供“西游记”“红楼梦”快捷搜索。
- **阅读菜单**：目录、下载全文/停止下载、保存进度、阅读设置、返回书架。
- **设置**：字号 12–28 dp、行距、纸张/护眼/夜间配色、5–30 秒自动翻页。切到后台或重启后自动翻页关闭。
- **书源**：启用、移除或添加自定义来源。最多 8 个来源、16 本书；移除书籍只删除本应用的对应缓存。

内置“古腾堡中文小说”和“古腾堡世界文学”是 **同一 Gutendex / Project Gutenberg 服务的两个检索范围**，并非两个独立供应商。筛选 `copyright=false` 且有 UTF-8 TXT 的作品，提供免费经典文学；正文可能使用繁体中文。`西游记`等常用简体关键词会转换为对应的目录书名。

来源：[Gutendex API](https://gutendex.com/)、[Project Gutenberg 西遊記](https://www.gutenberg.org/ebooks/23962)。本地《林间书屋》为原创 CC0 验收短篇，不需要网络。

## 添加书源

在“添加书源”输入一个 HTTP(S) JSON 地址。可以是下面的配置文件，也可以直接指向 `books` 目录。当前系统输入事件最多 64 个 UTF-8 字节，长地址需要短链接；配置文件内的请求地址允许 512 字节。

```json
{
  "format": "pxa-reader-source-1",
  "name": "我的免费书源",
  "kind": "json",
  "searchUrl": "/catalog.json?q={{key}}"
}
```

`searchUrl` 相对于配置文件地址解析，`{{key}}` 替换为 URL 编码的关键词。没有占位符时，客户端在当前结果页按书名/作者筛选。结果示例：

```json
{
  "books": [
    {"title": "小说一", "author": "作者", "url": "/book.txt"},
    {"title": "分章小说", "author": "作者", "toc": "/toc.json"}
  ],
  "next": "https://example.org/catalog.json?page=2"
}
```

TXT 正文必须为 UTF-8。分章目录格式：

```json
{"chapters": [{"title": "第一章", "url": "/chapter-1.txt"}]}
```

章节可以返回 UTF-8 TXT，也可以返回 `{"text":"正文"}`。每章最多 64 KiB、目录最多 1,024 章；空目录、错误 URL 或超限目录会拒绝整次下载，避免把残缺目录标记成完整小说。每次搜索最多展示 32 项，通过“下一组”翻页。

## 下载与离线

打开未缓存作品后开始下载，首段完成便可阅读，菜单可停止或重试。TXT 使用 32 KiB HTTP Range 请求，校验 Content-Range、总长度及 ETag/Last-Modified；UTF-8 校验跨分段保留状态。服务器不支持 Range 时，仅接收已知长度且不超过 32 KiB 的正文。

正文写入 `.part`，完整校验并关闭文件后使用 FS 原子替换发布 `.txt`。刷新失败保留已有离线版本。已缓存作品重启后直接从私有文件读取；阅读位置、书源和设置存于 `reader-state.json`。重试从头下载，当前不支持断点续传。

单书应用上限 8 MiB。pai-touch / esp-mosaico 的应用私有磁盘配额为 8 MiB，但**实际文件系统剩余空间也必须足够**；多本书和更新时的临时副本共享这个空间。配额没有扩大 Flash 分区，也不预留 8 MiB RAM。空间不足时显示错误。

## 显示与输入

按照 Host 的分辨率、DPI、安全边距和四角半径排版；圆屏逐行计算可用宽度，底部导航避开弧边。小面板会缩小布局以保留操作行。正文使用 Host 中国大陆黑体字形和抗锯齿，不在 Guest 中携带字库。

正文和输入框文字跟随阅读配色；输入框按 DPI 和可用行高选择 Host 已有的正文、标题或大字档位。中文候选和键盘使用系统主题及系统字号，切换系统深浅色/主色会保留输入内容。阅读器复用 `TextInputRef`，不携带拼音词典或另造输入法。

书页使用原生 Canvas 命令与一个原生 TextInput。没有额外的全屏阅读帧缓冲、深度缓冲或字体图集；只有页面变化才重绘。系统输入法打开时使用系统已有的 UI 覆盖层。

## 构建与验收

需要当前 Host 与 SDK：UI 0.7 (`sized-text`, `text-input-control`)、FS 0.2、Net 0.3。旧 Host 不能直接运行本应用，需要更新应用固件。C++ / Wasm / AOT 使用 O3，最大线性内存 2 MiB，按需增长。

从 PXA 工作区根目录执行：

```sh
SANITIZE=1 bash local/pxa-apps/novel-reader/tests/test.sh
bash tools/app.sh build novel-reader --target simulator,esp32s3 --aot-only
```

完整系统验收必须用 `tools/simulator.sh ui run`，独立 `product` 模拟器没有系统键盘。可复现脚本和真机结果见 [验收报告](docs/ACCEPTANCE-20261010.md)。

## 当前边界

这是 JSON/TXT 阅读器，不兼容 Legado 的 JavaScript/CSS 书源规则，也不支持 EPUB/PDF、网页抓取、图片正文、账号登录和付费章节。拉丁字母按一字一个 em 保守换行，长英文段落的行利用率较低；不进行自动繁简转换。后台暂停下载，取消后需重试。网络来源的稳定性由服务端决定。

正文索引从本地 TXT 中识别常见“第…章/回/卷”和英文 Chapter 标题，不能保证识别所有书的目录格式。缓存书源和图书字符串有容量上限，界面热路径没有无限增长的容器。
