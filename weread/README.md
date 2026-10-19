# 微信读书 · PXA

独立的 C++26 原生客户端，使用 PXA C++ SDK，界面和正文排版不依赖浏览器或 JavaScript 引擎。**非腾讯官方客户端；当前为开发验证版，尚未具备官方客户端的全部功能。**

应用 ID 为 `pxa-weread`，独立安装和保存账号状态，不读取 PXA 浏览器的会话。

工作树正在验证本地书签功能；上一轮实际采用的 pai-touch 包为 `b2b44db1…`。书签候选的 S3 代码映射从 21 页增至 23 页；已临时安装完成四次默认成本测量，但真实功能测试在正文请求限额处停止，已恢复原包，尚未采用。不能把源码中的开发功能当作已完成的真机功能。进度见 [本地书签记录](docs/bookmarks-20261017.zh-CN.md) 和 [真机成本与阅读限制](docs/bookmarks-device-20261018.zh-CN.md)。

真实大页面的 `-16` 已定位到输入限额和序列化别名限额。开发源码已改用按需分配的有界输入、共享索引预算和正文借用视图；950096 B 完整私有响应的离线回放及 2538 项合成目录已通过模拟器验证。Guest 最大内存仍为 2 MiB，尚未替换设备正常包。方案比较与验证范围见 [有界输入记录](docs/reader-input-20261018.zh-CN.md)，被替代的筛选实验见 [早期诊断](docs/reader-stream-20261018.zh-CN.md)。

后续索引候选与真机复核见 [索引、内存与显示延迟记录](docs/reader-index-20261018.zh-CN.md)：开发源码 AOT/Wasm 88 项检查通过，真机完整 950096 B 页面和正确前后页已确认。桌面 WAMR 稳定/峰值减少 48/1640 B，但真机冷启 SRAM 峰值 +28 B、PSRAM 峰值 +2552 B，单次翻页输入确认到显示标记约 951 ms；严格内存与流畅性目标仍未达到，设备已恢复原包。没有将该候选称作已完成的性能优化。

**运行时要求：** 当前构建使用 16 KiB 辅助栈，ESP32 Host 必须包含 2026-10-17 的 WAMR 完整主栈修复。旧版启用 pthread 的 Host 会把声明栈分成五份，可能直接溢出；相同 ABI 版本或工具版本字符串不能识别这项差异。当前只完成 pai-touch 配套固件验证，不能把此包直接发布给尚未更新的旧 Host。修复不改变 Guest ABI，也不要求其他应用重新编译。配套固件、精确摘要和八次实测见 [运行时与内存报告](docs/runtime-stack-20261017.zh-CN.md)。

## 开发与验证

在 PXA 项目根目录执行：

```sh
tools/app.sh build weread --source-root local/pxa-apps --target simulator
tools/app.sh build weread --source-root local/pxa-apps --target esp32s3 --aot-only
```

开发机原生解析检查：

```sh
clang++ -std=c++26 -Wall -Wextra -Werror -fsanitize=address,undefined \
  local/pxa-apps/weread/tests/protocol_test.cpp -o /tmp/weread-protocol-test
/tmp/weread-protocol-test
clang++ -std=c++26 -O1 -g -fsanitize=address,undefined \
  local/pxa-apps/weread/tests/hydration_test.cpp -o /tmp/weread-hydration-test
/tmp/weread-hydration-test
```

真实 PXA 模拟器离线集成测试使用合成账号、合成正文和**非登录二维码**：

```sh
PXA_APP_DEFINES=WEREAD_FIXTURE_PORT=18934 tools/app.sh build weread \
  --source-root local/pxa-apps --target simulator --output local/weread-validation/fixture
python3 local/pxa-apps/weread/tests/smoke.py \
  --simulator build/simulator/pai-touch/pxsys_product_simulator \
  --package local/weread-validation/fixture/pxa-weread \
  --publisher-key local/pxa-apps/.dev-signing/publisher-public.der \
  --ime-package local/app-output/pai-touch/pxa-pinyin-ime \
  --output local/weread-validation/smoke
```

测试版本只能请求编译时指定的本机端口。正式构建向 `https://weread.qq.com` 请求账号和正文，书内图片只允许已校验的腾讯图片 CDN；不向图片站点发送账号 Cookie。测试开关不出现在用户设置中。

## 实现与待验证范围

| 功能 | 当前状态 |
|---|---|
| 官方微信二维码获取、原生 PNG 显示、异步扫码确认 | 真实账号扫码登录成功；不公开二维码或会话 |
| 登录提交、私有会话保存、重复启动恢复 | 真实账号登录与重开恢复通过；合成集成检查通过 |
| 书架分页、刷新 | 真实书架和正文读取通过；其余分支继续验证 |
| 授权正文、章节/分节前后切换、目录 | 真实正文、下一章与本地翻页通过；官方分批目录只读接口已核验，635 条实际目录前后窗口通过；部分已购图书的后续 Web 授权差异仍未解决 |
| 本地分页、点按/滑动翻页、阅读菜单 | 四种屏幕的实际 PXA 模拟器通过 |
| DPI/安全区/圆形屏幕适配、深浅色、字号、行距 | 小屏、高 DPI、圆形屏、竖屏模拟器通过 |
| 本地阅读位置与设置持久化 | 四种屏幕重启后恢复同一页；跨端同步、阅读时长待实现 |
| 全局图书搜索、搜索结果阅读 | 四屏 AOT / Wasm 输入与搜索通过；真实结果列表与大型目录正文打开通过；总预算拒绝并保留原页的 AOT / Wasm 检查通过 |
| 书架与搜索结果封面 | 可见项异步加载、四屏 AOT/Wasm、取消/错误回退和真实封面通过；pai-touch 实际显示与退出检查通过 |
| 图书详情与云端进度读取 | 原生详情、简介分页、四屏 AOT/Wasm、凭据隔离和错误/取消检查通过；真实接口与 pai-touch 图书详情、进度、返回阅读及重开通过；不写入云端进度 |
| 自动登录续期 | 同账号原生续期、保存与重开真机通过；失败/超限/错误账号/取消等 9 类 AOT/Wasm 检查通过 |
| 书城/书架管理 | 待实现和验证 |
| 个人划线与想法读取 | 已采用；真实只读接口、六种布局及 pai-touch 划线、想法、局部翻页通过；依赖完整主栈运行时修复，早期失败和修复后的固定对照分别保留 |
| 本地阅读书签 | 开发候选；前版完整 AOT/Wasm 各 102 项及十一项原生检查通过，两行卡片/滑动相关 32 项通过；S3 四次默认测量通过，但真实正文请求遇限额，书签流程未通过，已恢复原包 |
| 书签及笔记新建、修改、删除与同步 | 待实现和验证 |
| 书内 PNG / JPEG / GIF 静态插图 | 四屏 AOT / Wasm、取消、重试、重定向和真实授权插图在模拟器通过；新版真机验证进行中 |
| 动态 GIF、漫画专用浏览、PDF、离线阅读 | 待实现和验证 |
| 听书、社交、会员与购买管理 | 待评估官方服务和设备能力，尚不支持 |
| 音量键翻页（可选） | AOT / Wasm 四屏检查通过；关闭后交还系统 |
| 真机性能、内存、前后台和重复启动 | 正文、位置恢复及目录流程已验证；已采用配套 Host/Guest 八次固定冷启动/重开与划线、想法通过，SRAM 稳定/窗口峰值不增，PSRAM 降低；端到端可见触摸延迟尚未测得 |

“功能完整”以实际服务和设备验证为准，不能由合成响应测试代替。需要会员或购买授权的内容保留官方服务限制。

详细证据与限制见 [验证记录](docs/verification.zh-CN.md)，服务约定见 [协议说明](docs/protocol.zh-CN.md)。

官方 Skill 与 KOReader 插件的源码参考、只读服务对照和后续接入边界见 [参考核对](docs/official-skill-koreader-20261017.zh-CN.md)。

本轮图书详情、自动续期、128 项模拟器检查与真机对照见 [详情接口记录](docs/agent-details-20261017.zh-CN.md)。

后续目录分段候选因真机内存和耗时未达标没有采用。设备最终安装原生产实现加目录溢出检查，代码映射页及 Guest/WAMR 峰值不变；136 项 AOT/Wasm 检查、真机阅读与连续重开通过。一次冷启动 DNS 解析失败未进入书架，后续同包冷启动未复现，失败时恢复行为仍需验证。对照指标、修复与验证范围见 [目录实验记录](docs/catalog-slices-20261017.zh-CN.md)。

个人划线/想法、早期否决的实验与会话保留说明见 [笔记开发记录](docs/notes-20261017.zh-CN.md)。最新运行时修复与八次固定测量见 [运行时与内存报告](docs/runtime-stack-20261017.zh-CN.md)：Guest 线性内存 128→64 KiB，代码映射仍为 21 页；同一新 Host 上 PSRAM 稳定/窗口峰值约少 64 KiB。WAMR 子计数随功能增加的部分也单独列出，没有用整机收益掩盖它。

## 维护边界

- `protocol.hpp`：固定服务端、登录数据、会话和书架模型。
- `notes.hpp`：有界个人划线/想法模型、精确游标与按需导航。
- `bookmarks.hpp`：按图书加载的本地书签、账号/书目校验与原子提交前的有界编码（开发候选，不表示云端书签）。
- `diagnostics.hpp`：按调用使用的固定容量数值日志，不预留常驻缓冲。
- `agent.hpp`：官方接口的固定来源策略、数据格式、详情文本与云端进度读取模型。
- `hydration.hpp`：官方页面序列化数据的受限字面量解析器；拒绝任意函数调用、属性访问和脚本执行。
- `reader_input.hpp`：按请求分配的有界响应字节；显式报告分配失败，供解析器借用，不预留常驻大缓存。
- `reading.hpp`：阅读数据、文本转换和有界分页。
- `book_image.hpp` / `document_layout.hpp`：有界插图元数据、正文与插图排版，不展开整本书的布局树。
- `image_page.hpp`：仅当前页的图片加载、共享像素预算、取消与重试。
- `catalog.hpp`：最多 300 项的目录窗口与官方目录分页协议。
- `search.hpp`：官方电子书搜索模型，按需分批读取。
- `state.hpp`：有界阅读位置 LRU、偏好设置与版本化私有存储。
- `html.hpp`：不执行脚本的服务端正文提取。
- `main.cpp`：异步生命周期、私有文件和原生界面。
- `tests`：合成协议、异常输入、会话恢复和真实模拟器检查。

正文中的字符映射来自官方页面的公开固定版本数据，只转换服务已返回的正文字符，不请求未授权内容。生成脚本核对来源 SHA256、码位范围及双射；来源变更必须审查并重新验证。来源及重建方式见 `tools/generate_mapping.py`。字体使用 Host 已有的简体中文黑体，二维码通过 Host 图像解码，不保留 Guest 像素副本。

网络请求有超时和响应上限；切到后台取消扫码等待；登录提交不会因结果不明而自动重试。登录会话仅存于应用私有文件，诊断日志不记录 UID、二维码链接、Cookie、令牌或正文。

插图设置可以关闭图片加载。当前页共享 65,536 解码像素预算，逐个下载并直接把响应体移交 Host；Guest 不保留压缩图片或像素副本。GIF 暂时只显示首帧。PNG 大源图的 Host 解码与调度改动、内存取舍及验证范围见 [插图验证](docs/images-20261017.zh-CN.md)。

封面实现与结果见 [封面验证](docs/covers-20261017.zh-CN.md)，长目录和阅读权限差异见 [目录与授权记录](docs/catalog-and-access-20261017.zh-CN.md)。
