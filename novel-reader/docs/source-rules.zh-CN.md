# 书源规则、按需阅读与离线缓存

0.2.2。书源是声明式 JSON 配置，不是可执行插件。默认网站使用 HTML 搜索、普通 / 分页 HTML 或分页 JSON 目录；
用户也可提供 JSON 目录，或用以下规则适配返回 UTF-8 HTML 的简单阅读网站。
默认联网阅读不写正文文件；只有用户明确下载离线，或启用新书自动下载时才保存正文。

## 最小 HTML 书源

将配置文件放到可访问的 HTTP(S) 地址，在应用“书源 → +”输入该配置地址。
下面的路径是示例，需按实际网站修改。可直接复制[配置模板](../source-examples/simple-html.json)。

```json
{
  "format": "pxa-reader-source-2",
  "name": "我的静态书站",
  "kind": "html",
  "searchUrl": "https://example.org/search?q={{key}}",
  "search": {
    "items": "#books .book",
    "title": "a.title",
    "link": "a.title@href",
    "author": ".author",
    "next": "a.next@href"
  },
  "toc": {
    "items": "#chapters a",
    "title": "",
    "link": "@href"
  },
  "content": {
    "selector": "#content",
    "remove": [".ad", ".toolbar"]
  }
}
```

数据流为：搜索页 → 每项的 `link` 指向目录页 → 目录中的章节链接 → 章节正文。
当前要求搜索结果直接给出目录 / 含目录接口标识的详情页地址；不另抓一个详情页来寻找另一个目录页面。
支持下文的分页 JSON / HTML 目录，以及明确规则指向的分页章节。

搜索页可以是这样的结构：

```html
<div id="books">
  <article class="book">
    <a class="title" href="/book/1/index.html">山间来信</a>
    <span class="author">林舟</span>
  </article>
</div>
<a class="next" href="?q=山间&amp;page=2">下一页</a>
```

目录和正文示例：

```html
<div id="chapters">
  <a href="./1.html">第一章 山间来信</a>
  <a href="./2.html">第二章 河边书屋</a>
</div>
```

```html
<div id="content">
  <p>清晨的风沿着山路吹来。</p>
  <div class="ad">这部分将被去除</div>
  <p>林舟推开木窗，看见一只白鸟。<br>旅程开始了。</p>
</div>
```

## 字段含义

| 字段 | 含义 |
| --- | --- |
| `searchUrl` | 配置地址的相对或绝对 URL；所有 `{{key}}` 替换为 URL 编码关键词；最终最长 512 字节 |
| `search.items` | 每一个搜索结果的容器；最多展示 32 项 |
| `search.title` / `author` | 在该容器内找元素并提取文字；作者可省略 |
| `search.link` | 指向该书目录页的字段；必填 |
| `search.next` | 整个搜索页面中的下一组链接；可省略；不自动预取 |
| `search.method` / `body` | 默认 GET；POST 时 body 为表单模板，必须含 `{{key}}`，最长 512 字节；只在首次搜索 POST，下一组使用 GET |
| `search.single` | 普通结果列表为空时，按 `title/link/author` 提取直接命中的书籍页；`stripTitleSuffix` 可去除标题末尾固定文字 |
| `referer` / `userAgent` | 可选的固定 HTTP 请求头，应用于该来源搜索、目录和正文；各最多 256 字节，不支持脚本或任意请求头 |
| `toc.items` | 单个章节链接的容器，按文档顺序；最多 1024 章，同 URL 去重 |
| `toc.title` | 相对章节容器的标题；空字符串表示容器自身文字 |
| `toc.link` | 相对章节容器的链接；`@href` 表示容器自身属性 |
| `content.selector` | 正文容器，使用第一个匹配元素 |
| `content.remove` | 正文内排除的子树，最多八个简单选择器；脚本、样式和注释本来就会被去除 |

字段写法为 `选择器` 或 `选择器@属性`。例如 `a.title` 取文字，`a.title@href` 取链接；
`@href` 读取当前结果 / 章节容器的属性。没有 `searchUrl` 关键词占位符时，在当前页按书名或作者本地筛选，ASCII 不区分大小写。
POST 表单中的 `{{key}}` 同样使用 URL 编码；POST 搜索不再做本地关键词筛选。空关键词不会触发请求；搜索历史只保存用户实际提交的关键词，最多六条，可清空。

## 分页 JSON 目录

网站有时只在详情页放前十章，再用 JSON 接口加载余下目录。可以在 `toc` 中配置 `paged`，取代 HTML 的 `items/title/link`：

```json
{"toc":{"paged":{
  "url":"/chapters?id={{id}}&page={{page}}",
  "id":"#bookDetails@data-bookid",
  "items":"list","title":"title","link":"pic",
  "pages":"totalPage","total":"total"
}}}
```

`id` 从详情页提取，最多 96 字节，替换到 URL 时编码；`page` 从 0 开始。其余字段是 JSON 对象的直接键名，不是 JSONPath。
接口示例：`{"total":"12","totalPage":2,"list":[{"title":"第一章","pic":"/book/1/1.html"}]}`。
计数可为非负整数或十进制数字字符串。每页最多 96 KiB，整本仍最多 1024 章，接口最多 128 页；没有默认的大型目录缓存。
按需阅读先读取当前章所需的目录页，后续翻章或翻目录时再补齐；不会为了打开首章预取所有目录。
离线下载会先读完目录；各页总数变化、缺页、重复链接、超限或最终章节数对不上会中止，不把前十章误认为整本书。
失败页不改变已验证的目录前缀，可重试；完整仅表示收齐了网站接口声明的条目，站点自身的章节编号、内容及完整性仍以其实际数据为准。
已加载的目录仅留在当前阅读会话中；按需阅读不将它或正文写成下载文件。
完整可用配置见 [努努书坊](../source-examples/nunu.json)。此功能只请求明确的接口，不执行网站 JavaScript。

## 分页 HTML 目录

某些网站每页展示一百章。保留普通 `toc.items/title/link`，增加：

```json
{"toc":{
  "items":".book_list2 a","title":"","link":"@href",
  "paged":{"format":"html","url":"index_{{page}}.html","start":1,
           "pages":".pagination .disabled a"}
}}
```

首次详情页即目录第一页，不重复请求。`pages` 选择器读取含 `1/9` 或 `第(1/9)页` 的可见文字；
客户端验证每页页号和总页数。`start` 为 URL 模板的起始页号，只允许 0 或 1，HTML 默认 1、JSON 默认 0。
按需阅读只取需要的目录页。尚未到末页时显示“已读取 N 章 · 后续按需加载”，不会把前一百章当作全书；
读完最后一页才确定实际章节总数。重复 URL、页数变化、空页、超限会拒绝该页，保留已验证前缀供重试。
示例见 [biquche.json](../source-examples/biquche.json)。

## 同一章的多页正文

以下模式用于有明确页数的章节：

```json
{"content":{"selector":"article.font_max","remove":["ins"],"paged":{
  "url":"{{stem}}_{{page}}.html","pages":"article.font_max","start":1,
  "header":"第({{page}}/{{total}})页","detectLink":true
}}}
```

`pages` 读取当前页 / 总页数；页号须从 1 连续增加。`url` 支持 `{{page}}` 和 `{{stem}}`，
后者是原章地址去除查询、片段和文件扩展名后的部分。`header` 可选，用于核对并移除正文开头的分页标记。
`detectLink:true` 用于同站某些章节不显示页数的情况：只跟随页面中与模板计算出的“同章下一页地址”完全相同的链接，
不会把普通“下一章”当作续页。无计数且未启用此选项时，读取失败。

也可指定下一页链接及其必须匹配的可见文字：

```json
{"content":{"selector":"#content","paged":{
  "next":"#next_url@href","text":"下一页"
}}}
```

这两种模式互斥。显式链接模式仅在文字为“下一页”时跟随，“下一章”表示本章结束。
一个逻辑章节最多 128 页，单次响应与合并后的正文均不超过 64 KiB；循环、页数变化、缺页或网络失败会中止整章。
只在整章收齐后发布阅读页或追加离线正文，不将前半章误认为完整章节。正文仍只保留当前章，不为新规则预分配整书缓冲。

## 搜索直接命中详情页

有的网站精确搜索会直接重定向到书籍页，可以增加兜底：

```json
{"search":{"items":".book-search-results-item","title":".book-search-results-name",
  "link":"a.book-search-results-link@href",
  "single":{"title":"h1","link":"link[rel=canonical]@href",
            "author":".book-author-name","stripTitleSuffix":"全文在线阅读"}
}}
```

仅在普通结果为空时使用 `single`，书名和合法目录链接均须存在。示例见
[文林社](../source-examples/wenlinshe.json)。规范链接由网页给出，客户端不猜测重定向后的相对目录地址。

## 支持范围与失败处理

- 选择器支持标签、`#id`、`.class`、组合 `a.title`、`div#books.list`、通配符 `*` 和空格后代关系，最多六段。
  也支持 `[attr]`、`[attr=value]`、`[title="带 空格"]`；字段分隔符 `@` 不会误拆属性条件内的文字。
  标签和属性名大小写不敏感，id / class / 属性值大小写敏感。`content.remove` 只接受单段组合。
- 不支持 `>`、兄弟关系、属性子串运算符、伪类、正则和 JavaScript；导入时拒绝超出语法范围的规则。
  使用标签、id、class 可清楚表达的页面最合适，不需要模拟浏览器 CSS、布局或脚本运行时。
  标识符使用 ASCII；冒号按标识符字面处理，不解释为伪类。通过语法检查不代表目标网页一定存在对应元素，应先用测试页面核对选择结果。
- 只支持 HTTP(S)，处理相对路径、`..`、查询参数和协议相对链接。拒绝脚本 / data / file URL、凭据和控制字符。
  不处理 HTML `<base>`；存在重定向时，应将规则配置成最终页面地址，或让网站返回绝对链接。
- 解码常见 HTML 实体和十进制 / 十六进制字符引用，保留段落及 `<br>` 换行；不携带 DOM 树、浏览器或字库。
- 页面须为 UTF-8；不进行 GBK 转码、自动繁简转换、登录、验证码或付费章节处理。
- 规则 JSON 最多 4096 字节，选择器 / 字段最多 160 字节；目录 / 搜索响应最多 96 KiB，单章响应最多 64 KiB。
  有限的嵌套扫描栈限制为 32 层，HTML 应有正常的闭合结构；这不是容错能力等同于浏览器的 HTML5 解析器。
- 没有匹配正文、非法章节链接、超限目录或错误 UTF-8 会中止本次读取 / 下载，不发布错误的完整离线文件。
  删除 HTML 书源后，已完成的离线 TXT 仍可阅读；再次联网需要重新添加相同搜索地址的规则。

四组内置规则和独立 JSON 示例见[书源列表](../source-examples/README.md)。内置规则共享只读存储；
状态文件仅保存名称、地址、开关，自定义覆盖规则仍原样保存。状态版本仍为 4，`sourceRevision:2` 记录目录迁移版本；
补入新增来源只执行一次，不恢复已清空的列表，也不改动自定义列表、书架或已有正文文件。

## 继续使用 JSON 书源

原来的 `pxa-reader-source-1` 仍可用：

```json
{
  "format": "pxa-reader-source-1",
  "name": "JSON 书源",
  "kind": "json",
  "searchUrl": "/catalog.json?q={{key}}"
}
```

```json
{"books":[{"title":"小说一","author":"作者","url":"/book.txt"},
          {"title":"分章小说","toc":"/toc.json"}],"next":"?page=2"}
```

```json
{"chapters":[{"title":"第一章","url":"/chapter1.txt"}]}
```

章节可返回 UTF-8 TXT 或 `{"text":"正文"}`。也可以直接将 `books` 目录 URL 当作书源导入。

## 缓存如何管理

| 内容 | 默认按需阅读 | 主动离线下载 |
| --- | --- | --- |
| TXT 正文 | 一个 8 KiB 原文窗口，包含少量回看空间；不写文件 | `.part` 增量下载，完整校验后原子替换成 `.txt` |
| JSON / HTML 分章正文 | 当前章暂存在内存；切章才请求下一章 | 按目录顺序追加为一个 UTF-8 TXT |
| 目录、页码 | 目录元数据在内存；不为统计 TXT 总页数拉取全书 | 可从本地正文分段统计总页数 |
| 书架、偏好、历史、位置 | 少量 JSON 保存在私有文件系统；位置合并写入 | 同左 |
| 停止 / 退出 | 取消网络任务，不产生正文临时文件 | 保留已关闭的有效 `.part`，之后可按条件续传 |

网络失败不会自动转成全文下载。翻页读取失败时保留原页和位置，可重试。
缓存管理中的“清除本书正文缓存”只移除本应用中当前书籍的 `.part` / `.txt`；不删除其他应用、书籍或存储分区。
TXT 清除后保留字节位置；分章离线书清除后重新从首章阅读，界面明确提示。

已有离线文件优先使用。旧版本留下的部分缓存也保留，不会因为升级默认模式而删除或继续写入。
新书自动下载是明确的偏好选项，初始关闭；没有默认的大型磁盘预留、预取整书或后台清理任务。
