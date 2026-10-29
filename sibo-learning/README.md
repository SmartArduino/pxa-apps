# 四博学习宝

独立 PXA 应用，包 ID `pxa-sibo-learning`。诗词、儿童故事、蒙学诵读与小学听题放在同一个安装包中。

- 75 首课本古诗原文离线阅读；在线每日诗词、随机诗词、按诗人查找、拼音伴读、诗意讲解及小挑战。
- 故事按分类浏览，支持分页阅读、现有插图、朗读及选择题。
- 蒙学书目和一至六年级听题按目录、分段进入。书目与在线内容随原服务更新。
- 全屏原生界面，支持触摸及“↑、↓、确认”。进入内容后默认选择阅读，进入新题后默认选择第一项答案。
- 保存正文阅读位置、阅读篇数、答题记录与最多 12 篇收藏。单篇收藏正文最多 8 KiB，收藏序列化数据合计最多 96 KiB。

在线内容来自原四博学习宝的 `https://wtoy.doit.am` 服务。应用不会开启设备 WiFi；用户先在系统中联网，并授权网络及音频播放。75 首古诗原文不触发网络，拼音、讲解、朗读、插图由用户主动请求。每日和随机诗词优先获取新内容，网络失败时尝试上次缓存。

最近内容使用 4 个缓存槽，每槽最多 96 KiB；收藏与阅读位置另行保存到内部存储。不需要 SD 卡。MP3 流式解码为 PCM，音频和插图不写入持久缓存。小书包里的“清理在线缓存”保留收藏及阅读位置。长篇内容可通过首页“继续读”保留正文与位置。

原生版本中需要独立设备账号授权的教材音频服务不包含在本应用中。在线内容、音频和配图的可用性取决于原服务；没有自动创建新插画的请求。

## 构建与运行

在 `pxa-projects` 根目录构建 ESP32-S3 安装包：

```sh
tools/app.sh build sibo-learning --board doit-gamekit-1 --target esp32s3 --aot-only \
  --source-root "$PWD/local/pxa-apps" --output "$PWD/local/app-output/sibo-esp32s3"
```

需要模拟器包时将 `--target` 改为 `simulator`，或用 `esp32s3,simulator` 构建双架构开发包。发布到设备时使用 ESP32-S3 单架构包以节省存储。

要求 PXA Core 2.1、Window 0.3、FS 0.2、Net 0.5、Audio 0.10、Assets 2.2。音频使用应用内 minimp3 解码及 Host PCM 输出，不依赖 Host Music 解码器。

模型测试不需要设备或网络，可选传入本地保存的公开 API 样本：

```sh
python3 local/pxa-apps/sibo-learning/tests/test_model.py \
  --output local/artifacts/sibo-learning/<时间戳>/models
```

`tests/test_simulator.py` 使用 `common/study/tests/build_three_key_simulator.py` 构建的实际 Gen4 导航桥接模拟器，仅以 ↑↓确认验证核心流程。通过 `--workspace`、`--package-root`、`--executable` 和 `--output` 指定路径。测试状态、日志和截图全部保存在指定的本地产物目录。

`tests/test_live.py` 使用相同路径参数，显式运行时访问已有的公开 MP3、插图、目录与书目；不请求 AI 插画或 TTS 生成。加 `--wait-completion` 可验证整段朗读自然结束。

## 来源与许可

内容模型与服务契约参考 `paixueji_nertc_demo_new` 中的学习宝实现；75 首古诗原文沿用其 chinese-poetry 数据。原项目与数据的 MIT 声明、minimp3 的 CC0 声明保存在 `assets/` 并随包分发。本应用界面、图标和代码使用 MIT。在线故事、音频及图片按需访问原服务，不随安装包分发或重新授权。
