# 历史、会话与轻量网站验证（2026-10-15）

本报告记录历史、会话与第一轮网站探测。GIF 真机回归及整机窗口峰值已完成，见下方 GIF 报告。最新导航、GB2312/GBK/GB18030 和努努书坊/微信读书结果见 [增量报告](sites-encoding-20261015.zh-CN.md)。
GIF 动画已接入 Assets 2.3、可选 C++ SDK 和浏览器；真实模拟器通过 12 项检查，含画面变化、
透明处置、可见性/前后台暂停、预览、签名过滤、取消和正常停止后的预算归零。
原生逐帧参考比对、ABI 生命周期和证据边界见 [GIF 验证报告](gif-native-20261015.zh-CN.md)。

## 已实现及验证

- 菜单浏览历史直接复用最多 24 项的导航记录，无第二份历史缓存；倒序显示、标记当前页、打开任意记录及清空。重定向后更新为最终地址，恢复阅读位置。历史只在本次运行保留。
- 普通搜索表单和 POST 密码表单；正确编码 `&`、`=`、`+`、空格和引号，保留隐藏字段。密码字段不允许通过 GET 提交，避免进入 URL/历史。读取 HTML `label` 改善字段名称；登录页初次打开定位到输入区域。
- 有界、按需分配的内存 Cookie 会话：最多 16 项、字符串合计最多 4 KiB，处理重复 Set-Cookie、路径、Secure、Max-Age、Expires、删除和重定向。保守限定到设置 Cookie 的主机；即使带 Domain 属性也不扩展到兄弟子域，不打包公共后缀数据库。不持久化账号、密码或 Cookie；菜单可清除会话。
- UI 0.9 可选 `password-input` / C++ `TextInput(...).password()`；动态编辑器可绑定 `State<bool>`。复用 LVGL 密码模式和系统 IME 敏感模式，立即遮盖、不学习、不联想，支持界面事务失败回滚。普通输入框不增加默认字段或绑定。
- 图片和 GIF 设置独立保存，关闭图片时停止加载并释放缓存；Host 的格式签名过滤验证了 GIF 改后缀或使用 octet-stream MIME 时也不会绕过关闭设置。
- 精简主页增加独立「网站导航」入口，保留用户自己添加的网站。
- 基础文本 WML、card 锚点、输入与 GET go 变量替换。没有 WMLScript、计时任务、完整 WAP 协议或 WML POST；不把基础兼容称为完整 WAP 浏览器。
- 为 bookshuku 提供隔离的静态表单适配器：其搜索与登录入口原本由脚本生成，适配器只使用已核验的公开端点，不执行 JS、不包含账号资料。静态卡片中单独的 `location.href='...'` 字面量转换成普通链接；表达式、转义和附加脚本不执行。

原生 ASan/UBSan 核心测试、SDK 密码绑定零分配测试、Host UI 校验与 LVGL 回滚测试通过。
模拟器完整登录/清除会话/历史/偏好测试通过；既有书签、音量键翻页、拖动滚动条和菜单退出回归通过。
296×240、分数 DPI、480×480/320 DPI、圆屏、竖屏和宽屏 6 组显示测试通过。

pai-touch 已通过实际截图核对密码输入、POST/303 登录、Cookie 刷新、历史列表、前后台恢复和菜单退出，测试前后偏好文件一致。
早先只依赖输入回执或旧日志的粗略结果不作为证据；最终结果见本地 `device-session-final-proof/results.json` 与同目录截图。

用户指定的 bookshuku 账号已在真实 PXA 模拟器中通过普通输入流程验证：匿名与退出后均出现注册字段，登录后访问同一受保护地址时注册字段消失、页面标题标识改变；退出后恢复匿名状态。
适配器注入的两个搜索字段仍然保留，因此以匿名→已登录→退出的字段数和标题标识变化核实认证，而非只看 HTTP 200。测试后已退出网站；代码、运行日志和报告不保存真实凭据、Cookie 或私有页面正文。

## 所给网址逐项检查

以下表格是更早一轮的历史探测，不代表现在的导航清单。按用户后续要求已移除维基百科、xkcd、15pmm01、Bs0Dd、Cobra，并合并 TXT 图书入口。

在开发机分别请求 HTTP/HTTPS，共 35 个地址、70 次请求；保留状态码、最终 URL、类型、耗时和前 64 KiB 公开正文，另用真实 C++ 解析器检查成功响应。失败表示本次网络环境下未成功，不能推断全球永久失效。HTTP 200 也不代表内容正确或可用。

| 原地址 | 本次结果与适配判断 |
| --- | --- |
| wap.15pmm01.com | HTTP/HTTPS 200，真正 WML；PXA 模拟器能显示输入与链接，搜索接口实际返回结果。收录基础入口。 |
| csd.15pmm01.com | 200，真正 WML；新闻分类接口返回文字链接。更小并不等于所有功能已验证，暂不重复收录。 |
| wap.bs0dd.net | 200，WML；英文页也返回正常 WML。PXA 模拟器验证首页，收录。 |
| wap.hao123.com | 200，现代移动 HTML，前 64 KiB 已较大；不能按旧 WAP 描述推荐，暂不收录。 |
| 2345.com/wap.htm | HTTP 跳转到旧帮助/网址页面，GB2312；存在大量过时入口，暂不支持其编码，不收录。 |
| 3g.qq.com | HTTP 断开、HTTPS 握手失败，未通过。 |
| 3g.sina.com.cn | 跳转 sina.cn，200；有服务端新闻文字，但仍含 JS 提示和大量现代页面内容，暂不收录。 |
| wap.163.com | HTTP 断开、HTTPS 握手失败，未通过。 |
| kannel.org/wap/main.wml | 跳转 HTTPS 时证书验证失败；没有关闭证书校验来制造成功结果。 |
| wap.localize-friends.de | HTTP 200，WML；只读检查，没有向聊天室发送内容，暂不收录社交站。 |
| webelements.com/wapelements/index.wml | 返回 200 的 JS 等待/验证页，而非元素周期表 WML，不收录。 |
| dvdtalk.com/index.wml | 403 验证页，不收录。 |
| slovo.bg/wap/index.wml | 返回 200 的 JS 等待/验证页，不收录。 |
| cobradile.neocities.org/index.wml | 200，实际 WML，但服务器标注 octet-stream；PXA 按 `.wml` 入口兼容并验证文字/card 链接，收录。 |
| dan-dare.net/index.wml | 返回 200 的 JS 等待/验证页，不收录。 |
| GitHub KedalionDaimon/list-of-wap-wml-sites | 清单本身可访问；清单中的站点仍需逐一验证，不能据此认定全部有效。 |
| wenku8.net | 跳转登录页，GBK，且出现 HTTPS→HTTP 跳转；当前不收录。 |
| 10000txt.com | 返回 JS 等待/验证页，不收录。 |
| bookshuku.org / www.bookshuku.org | HTTP 200、UTF-8；HTTPS 握手失败。独立登录页有标准 POST，搜索 POST 实际返回结果；手机版另行探测并在 PXA 打开。账号验证结果单独记录，报告不含凭据。 |
| gushiwen.cn | HTTP/HTTPS TLS 或超时失败，未通过。 |
| guoxuedashi.net | HTTP 200，但正文是“郭师影库”等影片列表，不是所述国学站；不推荐此域名。 |
| gmanhua.com | 跳转 www，能读到漫画目录；正文图片页尚未验证，不能宣称已能看漫画，不收录。 |
| manwa.cc | 200，但当前解析没有可读文字或普通链接，需要 JS；不收录。用户已有书签不删除。 |
| gocomics.com | 能连接但本次正文解析没有可读内容；前 64 KiB 不能证明完整站点行为，暂不收录。 |
| xkcd.com | HTTPS 200，静态 HTML；PXA 能打开页面。收录，图片解码范围以 PNG/JPEG 实测为准。 |
| albitereader.com | 连接/TLS 失败；且其 J2ME 本地阅读器不能直接在 PXA 运行。 |
| zh.m.wikipedia.org | 跳转中文桌面域名，200；PXA 仍能显示文章/导航文字，收录规范地址。复杂首页可能达到 96 条链接上限。 |
| baike.baidu.com | 200，但小型验证/脚本页，不能按“无需 JS 的百科”推荐。 |
| wap.people.com.cn | 连接/TLS 失败，未通过。 |
| wap.xinhuanet.com | HTTP 跳转 m.xinhuanet.com，200；有文字和新闻链接，但现代页面较大，暂不收录。 |
| m.xianguo.com | 跳转域名出售页面，非原资讯服务。 |
| google.com/gwt/n | HTTP/HTTPS 均 404，不能继续推荐此旧转码入口。 |
| skweezer.net | 当前是社交平台涨粉营销服务，不是原网页转码器，不收录。 |
| wap.ip138.com | 跳转 m.ip138.com，200，UTF-8、基础查询表单可解析；查询提交尚未在 PXA 实测，暂不收录。 |

选入导航的 7 个具体入口已用实际 PXA 包在模拟器打开，并保存截图和 `BROWSER DONE` 统计；这是浏览器实际加载结果，不是主机 curl 的速度。普通网络测试受线路和时间影响，不作为设备性能基准。

bookshuku 搜索已进一步通过 PXA 模拟器正常输入“三国演义”、提交 POST 并滚动至结果列表验证；实际画面出现“搜索结果页面”、约 50 条记录及“[三国演义 / 作者：罗贯中]”。证据在本地 `public-search-ui/`，与仅请求端点返回 HTTP 200 的探测分开记录。

![PXA 实际搜索结果](branding-20261015/bookshuku-search.png)

## 复现

- 核心：`bash tests/test.sh`。
- `tests/fixture.py --bind 0.0.0.0 --port 31878` 提供公开测试账号、重复 Cookie、303 重定向和受保护页面。`/session/login?prefilled=1` 用于真机表单流程；不含真实账号资料。
- `tests/session_smoke.py`、`tests/ui_smoke.py`、`tests/display_matrix.py` 使用实际模拟器、已签名应用包和控制套接字。
- `tests/device_session_smoke.py` 用 pai-touch，读取并核对已有偏好，测试后退出应用；真机截图必须等 Guest 处理和实际呈现，不能只依赖输入注入回执或命中旧日志。
- 本地原始证据：工作区 `local/browser-validation/sites-20261015/`。不将网站原文、登录正文、真实凭据或 Cookie 放入仓库。

## 后续验证

GIF 端到端验证现已完成：应用分区更新、真机实际画面、显示完成数、分阶段整机峰值、取消/后台和重复启动均有证据，最后恢复正式主页包。详见 [GIF 报告](gif-native-20261015.zh-CN.md)。

最新网站和文字编码的验证见 [网站与编码报告](sites-encoding-20261015.zh-CN.md)，本报告上方旧探测表保留时间顺序，避免把历史失败误认为永久不可用。
