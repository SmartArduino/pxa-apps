# 商城应用更新与验证（2026-10-16）

本轮更新八个应用的签名安装包、中文介绍、图标与多张实际运行截图。
每个应用分别提供 ESP32-S3、ESP32-S31、Linux x86_64 模拟器与可移植 Wasm 包，共 32 个文件。
发布目的地为 [PXA 应用商城](https://app.doit.am/)；源码、既有应用数据及其他任务的工作区改动均保留。

| 应用 | 版本 | release_sequence | 截图数 | ESP32-S31 完整 .pxa 字节数 |
| --- | --- | ---: | ---: | ---: |
| [应用商城](https://app.doit.am/apps/store) | 0.8.13 | 21 | 3 | 401242 |
| [体素世界](https://app.doit.am/apps/voxel-craft) | 0.12.8 | 21 | 4 | 610842 |
| [Jump Jump 3D](https://app.doit.am/apps/jump-jump-3d) | 0.1.7 | 8 | 3 | 1531566 |
| [小说阅读](https://app.doit.am/apps/novel-reader) | 0.2.5 | 9 | 3 | 1089823 |
| [PXA 浏览器](https://app.doit.am/apps/browser) | 0.1.1 | 2 | 3 | 1409532 |
| [拼音输入法](https://app.doit.am/apps/pinyin-ime) | 1.0.2 | 2 | 3 | 1186780 |
| [天气](https://app.doit.am/apps/weather) | 0.3.5 | 7 | 3 | 135690 |
| [像素地牢](https://app.doit.am/apps/pixel-dungeon) | 0.2.2 | 4 | 4 | 1645047 |

所有目标的字节数、版本、发布序号和 SHA-256 见 [artifacts.json](artifacts.json)。
来源为本轮冻结的源码快照，摘要见 [source-snapshots.json](source-snapshots.json)；并行任务后续修改不自动进入已签名文件。
浏览器保持主组件 AOT、脚本组件 Wasm 的组合，其余原生目标使用 AOT-only，避免无意更换脚本执行路径。
天气与像素地牢本轮主要更新包、商城资料和截图；没有宣称新增未实现的玩法。

## Store 搜索与大小显示

旧搜索框没有声明单行模式，输入法回车插入换行，无法触发提交；另外，忙状态直接忽略提交，HTTP 失败分支也没有重放排队查询。
0.8.13 使用单行输入和与查询缓冲一致的字节上限，输入法“完成”及可点击放大镜共用提交逻辑。
后到的查询只保留最新一份，沿用已有固定缓冲；无论旧请求成功、HTTP 失败还是传输失败都能继续处理。
目录返回时保留正在编辑的控件；搜索图片完成后只修改图片，避免替换输入框导致文字、光标或键盘丢失。
签名清单明确声明 UI 的 `dynamic-text` 和 `text-input-control` 特性。

设备 API 的 `size` 原本就是所选架构**整个 .pxa 文件**的字节数，而非单个代码制品的大小。
旧 Store 按 1024 换算却写成 KB/MB，并直接截断小数，容易产生比文件小的观感。
现在紧凑显示使用四舍五入后的 KiB/MiB，展开“兼容性与权限”显示精确字节数；网站同步更正为 MiB。
例如 ESP32-S31 的体素世界为 610842 B，模拟器包为 394505 B，不能拿它们当成同一个文件比较。
服务器测试及公开下载检查同时比较 API `size`、`Content-Length`、实际下载字节数及本地完整包大小。

## 修复兼容性索引

最初发布后的公开回读发现：旧 `*-pxa-0.3.0` 配置仍是 UI 0.8 等能力，更新的应用需要 UI 0.9、Window 0.3、Net 0.4 与 Assets 2.3。
仅核对 Core 和 CPU 会导致包上传成功，但设备继续看到旧版本，甚至无法找到浏览器。

新增独立的 `*-pxa-0.3.0-r2` 能力快照，并保留原配置。`r2` 是商城能力修订，不是新 SDK/固件发行版。
发布脚本按全部签名服务版本、特性、目标、引擎与 ABI 选择实际兼容的配置。
重跑对已有发布仅修正索引与经审核的资料，包文件保持内容寻址和不可变，不复用已发布序号。
SDK 能力目录也补齐与实际 Host 一致的 Assets 图片解码、帧图片标志；未增加运行时缓存或内存。

Store 0.8.11 起使用新快照。旧系统/旧商城仍只获得其能力允许的版本；不会向旧固件虚报新服务。
首次升级应通过匹配的新固件预置商城或 PXADB 更新 Store。安装任何应用仍需原生系统确认，未开放静默安装。

## 截图与测试

26 张图片全部来自签名 Guest 的产品模拟器或真实系统输入法 Provider；没有拼接虚构界面。
逐图来源、分辨率、DPI、包摘要及图片摘要见 [screenshot-provenance.json](screenshot-provenance.json)。
阅读器使用原创 CC0《林间书屋》；浏览器展示实际 PXA 网站；天气实际查询城市与网络数据。
像素地牢覆盖标题、职业、游戏和装备背包；体素世界覆盖游戏、背包、设置和标题。

验证包括：

- 32 个容器在发布前校验签名、发布者、lineage、版本、序号与兼容性；公开下载重新核对完整文件摘要与字节数。
- 商城八个公开详情、全部截图变体及网页图片链接检查；完整结果见 [public-verification.json](public-verification.json)。
- Store 真实 Host/LVGL/AOT 回归：六种配置（Mosaico、pai、圆屏 Watcher、240×240、低 DPI、高 DPI），覆盖截图页码、缩放、拖动、滚动恢复、单安装按钮、更新图标，以及六次搜索请求、排队失败恢复和输入框身份保留。
- 旧 0.8.12 在单行输入断言处失败；修复后六组全部通过。[store-ui-regression.json](store-ui-regression.json) 记录最终结果。截图拖动不产生 Guest UI commit，UI 当前占用 4328 B、峰值 11000–11064 B，保持此前水平。
- 实际签名拼音 Provider：候选上屏、完成键关闭键盘并提交；输入“天气”后完成可返回天气应用。另有完整/轻量引擎原生检查、13 种屏幕配置与每组八次卸载重开检查。
- 天气解析 ASan/UBSan 检查，以及实际定位、城市搜索、保存城市后重启、逐小时/七日预报；像素地牢游戏与光栅 ASan/UBSan 两项测试通过。
- 体素世界控制、物品、世界/树木、UV/光栅回归，阅读器核心/分页/网络源，浏览器脚本、DOM、异步请求与实际网页检查通过。
- 后端发布/媒体/完整开发者流程共八项测试通过；包含旧配置不变、能力选择、索引重跑幂等、图片幂等及下载大小一致性。

模拟器截图不能证明设备 FPS 或音频听感，本轮没有据此声明帧率收益。像素地牢沿用小体积 Opus 音乐，没有删除音乐或缩短时长。
Mosaico 的 Store 0.8.12 已实际安装、启动；之后设备串口断开，0.8.13 的真机安装未完成。详细状态见 [device-verification.json](device-verification.json)，不能将此前版本当作新搜索修复的真机证据。

## 复现

在 PXA workspace 根目录，使用本轮对应源码与签名密钥构建单目标包；已发布容器禁止用不同内容覆盖相同序号：

```sh
bash tools/app.sh build weather --target esp32s31 --aot-only \
  --source-root local/pxa-apps --output local/app-output/esp32s31
bash tools/app.sh build pixel-dungeon --target simulator --aot-only \
  --source-root local/pxa-apps --output local/app-output/simulator
python local/pxa-apps/store/tests/run_viewer_probe.py \
  --build build/simulator/sensecap-watcher \
  --package local/app-output/simulator/pxa-store \
  --publisher deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der \
  --ime-package local/app-output/simulator/pxa-pinyin-ime \
  --output local/store-ui-check
```

服务器已更新后端和页面模板，使用 `/opt/doit-app-store` 的 Compose 服务部署，保留数据库与上传卷。
发布前数据库/文件及旧代码备份位于服务器 `backups/20261016T123322Z`。
可用设备 API `GET /api/v2/catalog?profile=esp32-s31-pxa-0.3.0-r2&view=compact` 核对版本；四目标完整公开下载结果是最终验收依据。
