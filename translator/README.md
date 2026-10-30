# 随译翻译

PXA 在线翻译应用 `pxa-translator`。支持最多 500 字、1536 UTF-8 字节的短文本、九种语言、交换翻译方向、在线语音识别、译文朗读及四条会话记录。界面使用系统字体，按 Host 提供的逻辑分辨率、密度、安全区及圆屏内接区域布局：宽屏并排展示，竖屏上下排列，高度很小的屏幕使用单卡片。点译文进入完整结果，用「上页／下页」阅读长译文。

声音、麦克风和联网权限按操作请求，进入后台会取消请求、关闭录音与播放。录音不写入文件；退出后清空会话记录。语言与接口配置保存在应用私有 KV 中，不输出到日志。

## Gen4 三按键与触屏

上、下依次选择系统焦点框，确认执行。所有按钮均有真实原生控件，应用不捕获产品按键为手柄输入。首页从「输入原文」开始，语言、交换、语音、记录、设置均可三键到达。子页面的「返回」位于焦点顺序最后，从首项按上即可返回；设置中有明确的「退出应用」。短屏的设置、记录、语言和接口字段均提供分页控件。

输入页选中文本框后确认，打开设备系统输入法，文字会在下方自动换行预览。使用输入法的「完成」或页面「完成并翻译」提交，返回保留未提交的原文草稿。接口地址、密钥和模型同样通过系统输入法编辑，密码框隐藏密钥。系统输入法的语言和键盘布局由设备管理。触屏直接点击这些原生控件，使用相同的页面操作。

界面保留系统焦点，普通刷新不销毁输入框，输入或切换小屏字段时不会丢失草稿。网络、录音和播放由 20 毫秒时钟推进，不依赖继续按键。

## 文本接口

默认 [MyMemory](https://mymemory.translated.net/doc/spec.php)，不要求 API Key。源语言需要手动选择。每次请求按服务与 Host 的地址长度限制进行 UTF-8 安全分段，全部成功后才展示结果。MyMemory 有每日额度，服务不可用、限流或返回异常时显示错误。应用只使用查询接口，不向公共翻译记忆库贡献用户内容。

设置 → 文本翻译接口可切换 [LibreTranslate](https://github.com/LibreTranslate/LibreTranslate)。填写完整 `/translate` 地址和可选 API Key；请求使用 JSON `{q, source, target, format, api_key}`。输入的地址由使用者选择，带密钥的地址要求 HTTPS。切换页面不会把未保存的密钥发给服务。

## 在线语音

设置 → 在线语音服务提供 Groq、OpenAI 和自定义预设，填写 API Key 后保存。默认 Groq 地址为 `https://api.groq.com/openai/v1/audio/transcriptions`，模型为 `whisper-large-v3-turbo`。OpenAI 预设使用 `https://api.openai.com/v1/audio/transcriptions` 与 `gpt-transcribe`。模型和完整地址均可修改；账号可用模型、额度与计费由对应服务决定。

按麦克风开始录音，再按「完成并翻译」上传。最长二十秒，并受 128 KiB 录音缓冲上限约束。设备通过 Audio 0.15 返回的 16 kHz 单声道原始 Opus 帧，封装为 Ogg Opus，不在 Guest 解码或重编码。multipart 请求包含 `file=speech.ogg`、`model`、`language` 和 `response_format=json`，识别接口应返回 `{"text":"..."}`。先识别原文，再交给所选文本接口翻译，不使用仅输出英文的音频翻译端点。

接口参照 [Groq Speech to Text](https://console.groq.com/docs/speech-to-text) 和 [OpenAI Transcriptions](https://developers.openai.com/api/reference/resources/audio/subresources/transcriptions/methods/create)。只有设备 Host 提供 Audio 0.15 麦克风采集源时才能录音，未提供时仍可使用文本翻译。Groq 等服务支持 Ogg 上传；自定义服务也需支持上述协议与音频格式。

## 译文朗读

使用 Google Translate 的在线 MP3 朗读入口，按短段落依次获取音频，经共享的 minimp3 解码并提交 Host PCM 播放，直到最后一包被消费后继续下一段。入口属于非正式网页接口，可能限流或变动；不承诺持续可用。相关开源项目：[gTTS](https://github.com/pndurette/gTTS)。Google 文本翻译网页接口与朗读入口是独立功能，应用没有将未经验证的网页翻译入口作为默认文本服务。

## 构建与验证

在 PXA workspace 根目录执行：

```sh
tools/app.sh build translator --source-root local/pxa-apps \
  --target simulator,esp32s3,esp32s31 --aot-only \
  --output "local/artifacts/translator/$(date +%Y%m%d-%H%M%S)/packages"
```

基础能力要求 Core 2.1、UI Canvas/dynamic-text/password-input、Net 0.5 和 Audio 0.10。Guest 最大线性内存 2 MiB，栈为 64 KiB。麦克风通过现有服务的可选操作请求，不在系统或固件中添加依赖。

`tests/model_test.cpp` 验证 UTF-8 分段、JSON、配置校验、密钥头和 Ogg CRC；其可选参数为输出 Ogg 文件路径。`tests/navigation_test.cpp` 检查各页面在不同逻辑尺寸下的控件边界、命中互不覆盖、草稿返回、退出和时钟单位。

`tests/build_gen4_simulator.py --workspace /path/to/workspace --gen4 /path/to/mz79-firmware --output <本地产物目录>` 从已配置的 `build/simulator/doit-gamekit-1` 单独链接测试模拟器，复用 Gen4 的实际 `Mz79Navigation`。仅测试 Host 选择内置输入法解码器与英文全键偏好，应用包没有测试开关。该模拟器传给 `tests/native_ui.py --simulator ... --package ... --output ... --mode fixture`，全流程导航只用上、下、确认，系统键盘实际输入字符后提交翻译；长文本和限流夹具另用 `TEXT` 注入。`--mode live` 测实际在线翻译和朗读，`--mode layouts` 测大小屏、Gen4、密度、安全区与圆屏。`tests/simulator.py` 保留相同命令入口。

模拟接口测试不能代替持有真实 API Key 的在线识别测试；模拟器验证不能代替 Gen4 实机测试。日志、截图、音频捕获与报告均传入 workspace 的 `local/artifacts/`。

新增代码和图标 MIT。共享 minimp3 为 CC0-1.0，原许可及固定来源位于 `../common/media/vendor/`，完整声明在包内 `assets/NOTICE.txt`。没有复制或嵌入 Googletrans、gTTS、LibreTranslate 或 Whisper 的服务端实现。图标源图在 `assets/icon.svg`；小型功能图标由 `tools/generate_icons.py` 生成。
