# 网页兼容、UA 与图片

当前版本采用流式 HTML 阅读器，显示标题、正文、列表、代码、链接和基础表单。
普通英文单词优先整体换行；超长网址、代码和中文仍按字符换行。
这不是完整 CSS 布局：文字宽度采用保守估算，复杂表格、浮动、多栏和字体塑形尚未实现。
渲染只遍历可见片段；拖动仅重绘正文和状态行，完整命令列表仍供窗口重新显示使用。

## 页面模式

「更多」里的页面模式只改变后续请求，不自动重发 POST：

| 模式 | 请求身份 | 用途 |
| --- | --- | --- |
| 轻量，默认 | `PXA-Browser/0.1 (Mobile; TextMode)` | 如实表明移动文字阅读器身份 |
| 手机兼容 | Android Chrome 形式的 UA，带 `PXA-Browser/0.1` | 手动尝试网站的手机版本 |

两者都发送 `Save-Data: on`，并只接受未压缩的正文。UA 和 Save-Data 是服务器选择内容的输入，
不是强制“返回简版”的协议。移动 UA 也可能让网站发送更多脚本或验证码，不能据此宣称兼容完整浏览器。
相关定义见 [User-Agent](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/User-Agent)
和 [Save-Data](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Save-Data)。

2026-10-15 在同一主机用两种 UA 各请求两次：Example 均为 577 B，PXA 官网均为 11166 B；
两站各自正文哈希完全相同。DuckDuckGo Lite 的轻量请求返回 200、约 24 KB；手机请求返回
202、约 14 KB 的验证页面。后者的时间、反滥用状态和动态内容都可能参与结果，不能归因为 UA
单一因素，也不能将较小验证码页面当成简化成功。原始记录见
[ua-comparison.json](performance-20261014/ua-comparison.json)。

显式的 RSS、纯文本、打印版和网站自己提供的 Lite 页面通常比猜 UA 更可控。
本应用默认搜索已经选用 Bing RSS；没有把任意网页发送给外部转码代理。

## PNG 与普通 JPEG 网页图片

已实现独立的 Assets 2.2 / `image-decode` 可选能力。原有 `load(image, path)` 继续加载包内资源；
SDK 将 HTTP 响应体交给 `decode_image`，一个异步请求直接在 Host 内读取与解码，
编码图像不再经 Guest 缓冲区往返，也没有应用侧的解码器/finish 生命周期。
PNG 按流处理、直接写缩小后的 BGRA 像素，保留透明度；JPEG 在 Host 临时预算中保留压缩数据，
pai-touch 复用 ROM JPEG 并使用 IDCT 缩小输出 RGB565。桌面使用系统 libjpeg。Canvas 通过已有图片命令和资源引用绘制，不新增网页帧缓冲。

`<img src>`、部分 `data-src` 懒加载属性和直接图片地址可用；相对地址按网页 URL 解析。
正文先显示占位，自动加载可见图片，手指拖动时不开始新下载。最多 32 张图片描述、2 张应用缓存，
离屏缓存优先淘汰。停止、页面导航、任务取消、后台切换都会关闭进行中的 HTTP 与解码器；
后台保留有界的已解码缓存，返回前台可继续绘制。回到首页释放图片，Host 的 Canvas 引用在重绘后释放。

每张最多下载 128 KiB，驻留最多 32768 像素，输出边长至多 512；源图单边至多 4096、
总像素至多 1048576。PNG 像素最多 128 KiB、JPEG 最多 64 KiB，另计对象及有界解码临时存储。
pai-touch 上的图片及解码临时分配计入 Host 既有的共享资源预算；桌面 libjpeg 的内部工作区另由系统堆管理。图像按原始比例排版，输出像素不足时放大显示，
采用低成本采样，会损失原图细节；目前没有图片放大查看器或完整 CSS 图片布局。

支持普通 RGB JPEG 与 PNG；桌面另支持灰度 JPEG，ESP32-S3 ROM 暂不支持灰度 JPEG。
不支持渐进式 JPEG、WebP、GIF、SVG、CSS 背景图、
`srcset` 选源、data URL 或必须执行脚本才出现的地址。失败时保留替代文字和重试入口，
不会把不支持的图片冒充加载成功。HTTPS 页面拒绝明文 HTTP 图片和 HTTPS 到 HTTP 的图片重定向。
文件尺寸、CRC/截断、解码分配失败、JPEG 色序、透明度、Adam7、取消、缓存淘汰、六屏适配及
真机内存的验证见 [图片验证报告](images-20261015.zh-CN.md)。
