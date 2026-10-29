# 四博学习宝

独立 PXA 应用，包 ID `pxa-sibo-learning`。诗词、儿童故事、蒙学诵读、小学听题、英语词卡、同步教材与科学图文放在同一个安装包中。

- 75 首课本古诗原文离线阅读；在线每日诗词、随机诗词、按诗人查找、拼音伴读、诗意讲解及小挑战。
- 故事按分类浏览，支持分页阅读、现有插图、朗读及选择题。
- 蒙学书目和一至六年级听题按目录、分段进入。书目与在线内容随原服务更新。
- 英语单词岛接入中考、高考、大学四/六级、考研、托福、雅思、GRE、学术英语共9个词包，按单元读取；词卡先隐藏词义，确认翻卡后直接进入下一词，并支持单词片段发音及词义选择题。
- 同步小课堂按语文、数学、英语、科学与年级上下册选课，读取知识点、例题与解析。选择/判断题自动判分；填空、排序、计算等开放题先思考再核对答案并自评，适合无键盘设备。当前教材以文字学习为主，不提供其 Opus/M4A 音频。
- 科学发现馆按主题和章节阅读 vistep 图文，逐章查看原有插图与来源。
- 全屏原生界面，支持触摸及“↑、↓、确认”。进入内容后默认选择阅读，进入新题后默认选择第一项答案。
- 保存正文阅读位置、阅读篇数、答题记录与最多 12 篇收藏。单篇收藏正文最多 8 KiB，新增收藏的序列化数据合计最多 20 KiB，个人快照最多 28 KiB，以适配 64 KiB 应用存储；达到上限会提示移除收藏，不自动删除个人记录。

在线内容来自原四博学习宝的 `https://wtoy.doit.am` 和同步教材 `https://pindou.doit.am/study/` 服务。应用不会开启设备 WiFi；用户先在系统中联网，并授权网络及音频播放。75 首古诗原文不触发网络，拼音、讲解、朗读、插图由用户主动请求。每日和随机诗词优先获取新内容，网络失败时尝试上次缓存。

最近内容使用 4 个缓存槽，单份与合计最多 62 KiB，预算按个人记录体积自动缩减，为旧、新两份个人快照和必要开销留出空间；较大的响应仅在内存使用；收藏与阅读位置另行保存到内部存储。不需要 SD 卡。MP3 流式解码为 PCM，音频和插图不写入持久缓存。小书包里的“清理在线缓存”保留收藏及阅读位置。个人记录原子替换失败时会释放在线缓存并重试；临时文件失败或重启时清理。正文与收藏合计超过个人快照上限时会提示阅读位置未保存，原记录保留。

原生版本中需要独立设备账号授权的教材音频服务不包含在本应用中。小程序的微信登录、家庭账号和蓝牙配网等平台功能未移植。在线内容、音频和配图的可用性取决于原服务；没有自动创建新插画的请求。

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

`tests/test_learn.py` 验证英语、教材和科学数据模型，可用 `--fixtures` 读取本地 API 样本。`tests/test_extensions.py` 验证词卡、题型、自评和重启恢复；构建专用导航模拟器时使用 `--private-quota-bytes 65536`，可覆盖 Gen4 的空间不足与重试保存流程。

`tests/test_learning_live.py` 显式运行时验证现有词包、单元首尾单词的音频片段和科学插图。

`tests/test_live.py` 使用相同路径参数，显式运行时访问已有的公开 MP3、插图、目录与书目；不请求 AI 插画或 TTS 生成。加 `--wait-completion` 可验证整段朗读自然结束。

## 来源与许可

内容模型与服务契约参考 `paixueji_nertc_demo_new` 中的学习宝实现；75 首古诗原文沿用其 chinese-poetry 数据。原项目与数据、ECDICT、vistep 的 MIT 声明，以及 minimp3 的 CC0 声明保存在 `assets/` 并随包分发。本应用界面、图标和代码使用 MIT。在线故事、音频及图片按需访问原服务，不随安装包分发或重新授权。
