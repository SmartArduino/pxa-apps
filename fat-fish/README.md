# 大肥鱼 PXA 版

默认通过 Window 0.5 请求横屏；横屏后按实际屏幕比例重新布局房间、角色与操作区。方屏保持当前方向，不能旋转的设备继续使用原生尺寸；退出或回到桌面后由系统恢复方向。

独立 PXA 电子宠物应用。使用 dsh-pet 的大肥鱼透明动画，提供摸摸、喂食、玩耍、睡觉、房间漫步、三套像素房间、16 个动作点播、自动存档，以及 dsh-pet 文字和语音聊天。支持中文、English、触屏和手柄。

商城提供按运行目标区分的签名安装包、封面和功能截图。[大肥鱼商城页面](https://app.doit.am/apps/fat-fish)

## 源码与上游更新

本应用位于独立的 `pxa-apps` 仓库。`vendor/dsh-pet` 是原项目的 Git 子模块，固定上游提交。PXA 实现与资源转换工具位于本目录，更新上游后重新转换资源即可。

在 `pxa-apps` 仓库根目录执行

```sh
git submodule update --init --depth 1 fat-fish/vendor/dsh-pet
python3 fat-fish/tools/prepare_assets.py
```

显式更新到上游 main 时执行

```sh
git submodule update --remote --checkout fat-fish/vendor/dsh-pet
python3 fat-fish/tools/prepare_assets.py
git add fat-fish/vendor/dsh-pet fat-fish/animation_data.hpp fat-fish/assets
```

转换依赖 Python 3、Pillow、zopfli 0.4.0 和支持 `libvpx-vp9` 的 FFmpeg。工具从 `clips.json` 读取动作清单，在固定区域中缩放为 192×144、12 帧/秒、每段独立调色板的透明索引帧，以最多间隔 12 帧的独立关键帧和逐帧差分保存，再使用 DEFLATE 压缩。独立关键帧经过 Zopfli 优化，差分帧恢复与原 RGB565 像素一致。RGB565 结果相同的调色板索引合并后编码，透明索引保持独立。资源清单保存上游提交、原视频摘要、编码方式及输出摘要。生成的 `animation_data.hpp` 与 `assets/` 随应用维护，普通构建直接使用这些资源。

启动器图标使用 `art/launcher.png` 中的像素近景头像，生成提示词和授权记录位于 `art/launcher.json`。修改头像后运行 `python3 fat-fish/tools/prepare_icon.py` 生成 192×192 的 `assets/icon.png`。动画资源转换与头像维护分开进行。

## 构建

在 `pxa-projects` 工作区执行

```sh
tools/app.sh build fat-fish --target simulator,esp32s3 --source-root local/pxa-apps
tools/simulator.sh product --profile pai-touch \
  --package local/app-output/pai-touch/pxa-fat-fish \
  --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der
```

应用依赖 PXA SDK 2.1 与 Assets 2.0，运行时通过包资源服务异步分块读取动画。动画以 12 帧为一组双缓冲预读，随机定位从最近关键帧开始恢复，连续播放复用上一帧。静态房间背景按当前分辨率缓存。帧解码与输出缓冲区有固定上限，切换动作会取消旧请求并丢弃过期结果。读取遇到繁忙、资源不足或临时 I/O 错误时，保留当前画面，依次等待 100、200、400 毫秒，最多重试三次。持久错误记录阶段和状态码。应用线性内存上限为 3 MiB，用于房间背景缓存和双缓冲动画预读。

## 验证工具

`tests/native.cpp` 检查照顾规则、新旧存档、图标命中布局、畸形输入及全部动画帧解码。`tests/simulator_test.py` 使用真实签名包和产品模拟器，检查横竖屏及圆角屏幕上的触摸、手柄、动作点播、暂停、双语切换与存档重载。`tests/display_test.py` 覆盖 240–720 像素、160/240/320 DPI、房间切换、悬浮栏收起与图标操作。

`tests/dsh_protocol.cpp` 覆盖配置、请求转义、Unicode、业务错误、超长回复与含点号的宠物 ID。`tests/dsh_fixture.py` 提供回环地址上的流式模型测试替身，回复带“协议测试”标记，可供隔离的 dsh-pet 实例验证对话记忆与动作标记。

在 `pxa-projects` 工作区执行，输出目录使用本机忽略的产物目录

```sh
run_dir="local/artifacts/fat-fish/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run_dir"
bash local/pxa-apps/fat-fish/tests/native.sh "$run_dir"
python3 local/pxa-apps/fat-fish/tests/simulator_test.py \
  --simulator build/simulator/pai-touch/pxsys_product_simulator \
  --packages local/app-output/pai-touch \
  --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der \
  --output "$run_dir/simulator"
```

## 操作

- 点击角色进行摸摸，按住拖动可调整角色在房间里的位置。
- 底部爱心、饭碗、皮球、月亮、场记板与齿轮图标，分别用于摸摸、喂食、玩耍、睡觉、动作点播及更多设置。睡着后点角色或太阳图标起床。
- 房间铺满屏幕，状态条和操作栏悬浮在房间上。操作栏右上方的小箭头可收起整栏，右下角省略号可重新打开。
- 点击左下方的气泡图标打开聊天，手柄 Select 也可打开。聊天页可输入文字、发送快捷问候、取消等待，以及设置服务地址、宠物 ID 和局域网令牌。
- 在聊天页按住“说话”录音，松开发送，单次最多 20 秒。首次使用需允许麦克风和播放权限，允许录音后再次按住。识别结果会作为聊天消息显示，回复会自动朗读，点击“取消”可停止当前请求或播放。
- 房间左下方的麦克风图标开启连续对话。图标高亮且气泡提示正在聆听时直接说话，本地识别服务检测停顿后发送对话，回复会显示气泡并通过扬声器朗读。朗读期间麦克风暂停，结束后自动继续聆听。再次点击按钮、打开聊天页、离开房间、进入后台或让宠物睡觉时关闭该模式。此开关仅对当前前台会话生效。

- 状态条用饭碗、笑脸、闪电和爱心表示饱食、心情、精力及亲密度，数字范围为 0–100。
- 状态下方的细进度条辅助读取数值。半透明面板、动作收藏、设置开关与暂停菜单随房间使用海盐蓝、月光紫或薄荷色调，按钮提供按压反馈。
- 顶部菜单与状态条同高并使用对称边距。短对话以带尾巴的圆角气泡出现在角色上方，自动换行，收起操作栏后仍会显示。
- 文字复用 Host 预加载的系统 Caption 字体，字号与换行按系统 DPI 和字体缩放计算。首次绘制使用包内的待机或睡眠预览帧，动画随后异步读取；设置和署名等静态页面按交互与数值变化重绘。
- 图标使用可缩放绘制。按钮根据系统 DPI、内容区和分辨率自适应，窄屏先显示摸摸、喂食和省略号，点击省略号展开其余操作。绘制与触摸命中共用布局。
- 方向键选择按钮，A 确认，B 返回或打开暂停菜单。X 睡觉或唤醒，Y 打开动作收藏，L/R 在收藏中翻页。
- 齿轮设置页的首行依次切换晴日海景、月光卧室、海底观景室，也可开关漫步与随机动作、查看素材署名。睡觉时临时使用月光房间，醒来恢复所选房间。右上角菜单提供语言切换、重新开始和保存退出。重新开始会重置宠物进度。
- 饱饱、开心、精力随前台陪伴时间缓慢变化，睡觉恢复精力。喂食与玩耍有短暂冷却，睡觉时优先休息。
- 每 15 秒、照顾后及退出时保存。记录状态、亲密度、摸摸次数、陪伴时长与房间设置，后台及暂停时停止养成计时。
- 设置中的“自动照顾”默认开启，旧存档升级后也会开启。饱食不高于 30 时自行吃东西，精力不高于 20 时自行睡觉并在恢复到 90 后醒来，心情不高于 35 且精力至少 35 时自行玩耍。按饱食、休息、玩耍的顺序处理需求，复用手动照顾的数值、动画与存档。当前动作完成并空闲至少 5 秒后执行，聊天和 AI 语音期间等待互动结束。自动入睡标记与开关一同保存，手动入睡后可通过摸摸或休息按钮唤醒。


## dsh-pet 聊天服务

电脑、NAS 或服务器运行 dsh-pet 的 Node.js 服务，PXA 应用通过 HTTP 发送文字并显示回复。模型调用和持久化聊天记忆由服务端处理。模型密钥保存在电脑端。应用使用系统输入框和字体，并将回复动作映射到已收录的动画。语音模式增加电脑端的 `tools/dsh_local.py`，通过 `/voice/hear` 接收 Ogg Opus 录音，通过 `/voice/speak` 返回 MP3。

上游独立模式监听本机回环地址，先按 `vendor/dsh-pet` 的说明构建并启动服务，再在其 AI 设置页填写模型配置。为设备提供局域网入口时，使用 `tools/dsh_bridge.py`，它只转发 `/dsh-pet-7340/chat`、`/dsh-pet-7340/state` 与 `/dsh-pet-7340/voice/` 下的三个语音接口，并从上游的 `standalone-runtime.json` 读取当前令牌。

将以下配置保存到源码之外的私有 JSON 文件。`bind` 使用电脑的局域网 IP，`token` 使用独立随机令牌，`runtime_file` 指向当前上游实例的运行状态文件。

```json
{
  "bind": "192.168.1.10",
  "port": 7341,
  "token": "REPLACE_WITH_RANDOM_BASE64URL_TOKEN",
  "runtime_file": "/path/to/DSH_HOME/dsh-pet/standalone-runtime.json"
}
```

```sh
python3 fat-fish/tools/dsh_bridge.py --config /path/to/private-bridge.json
```

设备与电脑连接同一网络，在聊天页设置中填写 `http://192.168.1.10:7341/dsh-pet-7340`、宠物 ID（通常为 `main`）和相同的局域网令牌。首次发送时允许联网。服务地址和令牌通过独立的 Storage 键保存，宠物进度沿用原存档。

开发安装可通过被 Git 忽略的 `assets/dsh.local.json` 预置 `url`、`pet`、`token`。应用首次配置时读取此文件，后续优先读取设备已保存的设置。该文件随本机构建的应用包分发，适合个人调试。

客户端发送 `POST /chat?pet=…&surface=any`，请求体为 `{"text":"…"}`，同时检查 HTTP 状态和响应中的 `ok`。发送前读取 `/state` 作为动作基线。桥接服务会将生成后的当前宠物动画快照加入成功回复的可选 `pxa_state` 字段，客户端随即申请朗读，省去设备再次读取状态的往返。快照读取限时 250 毫秒，读取失败时保留聊天回复，由客户端通过原有 `/state` 接口补充动作。只播放本次更新且已收录在应用中的动画。每次最多输入 512 个 UTF-8 字节，显示最近三轮对话，单条回复最多显示 2048 个 UTF-8 字节。取消会停止客户端等待，服务端已接收的生成可能继续完成。请求采用显式发送，失败后可手动重试。

### 本地语音与 Responses

连续模式在 `/voice/hear` 请求中增加 `session`，持续提交约一秒音频的 Ogg Opus 片段。本地服务复用该会话的 Vosk 识别器，检测语句结束后返回 `finished=true` 和完整文字。静音会话每 20 秒音频重置，识别器最多保留四个会话，后续请求会回收闲置超过 45 秒的会话。设备在模型生成和朗读期间关闭麦克风，朗读结束后等待 600 毫秒再恢复采集。

本地伴随服务使用 Vosk 中文小模型 0.22 识别和 Melo 中文浮点模型合成，均在电脑 CPU 上运行。识别模型文件约 66 MiB，浮点合成权重约 163 MiB，另有词典文件和约 42 MiB 的识别模型下载缓存。`prepare_voice.py --tts-model int8` 可下载约 51 MiB 的量化合成权重，配置中的 `tts.model` 对应 `model.int8.onnx`。中文小模型可能出现同音字错误。设备录音使用 16 kHz 单声道 Opus，回复使用 22.05 kHz 单声道 MP3，朗读前 240 个 UTF-8 字节，最长 30 秒。设备解码并通过 PXA Audio 0.10 的 PCM 接口播放。

在 `pxa-apps` 仓库根目录使用 Python 3.12、uv 和已安装的 FFmpeg 执行

```sh
uv venv --python 3.12 ~/.local/share/pxa-fat-fish/venv
uv pip install --python ~/.local/share/pxa-fat-fish/venv/bin/python \
  -r fat-fish/tools/local_voice_requirements.txt
python3 fat-fish/tools/prepare_voice.py
mkdir -p ~/.config/pxa-fat-fish
chmod 700 ~/.config/pxa-fat-fish
```

创建私有文件 `~/.config/pxa-fat-fish/voice.json`，填写模型目录、独立随机本机令牌及网关配置，然后设置权限 `chmod 600 ~/.config/pxa-fat-fish/voice.json`。`base_url` 可填写网关根地址或以 `/v1` 结尾的地址。

```json
{
  "models_dir": "/home/USER/.local/share/pxa-fat-fish/models",
  "local_token": "REPLACE_WITH_RANDOM_BASE64URL_TOKEN",
  "tts": {"model": "model.onnx"},
  "llm": {
    "base_url": "http://GATEWAY:8080",
    "api_key": "REPLACE_WITH_API_KEY",
    "model": "gpt-6-luna",
    "wire_api": "responses"
  }
}
```

```sh
~/.local/share/pxa-fat-fish/venv/bin/python fat-fish/tools/dsh_local.py
```

桥接服务收到聊天回复后会异步预合成朗读内容，本地服务合并相同文本的并发合成请求，并缓存最近八条语音。

此服务监听 `127.0.0.1:7344`，将上游 Chat Completions 请求转换为 Responses，请求使用 `store=false`、`reasoning.effort=minimal` 和 256 输出词元上限。上游 dsh-pet AI 设置填写 `openai-compatible`、地址 `http://127.0.0.1:7344/v1`、模型 `gpt-6-luna`，密钥填写 `local_token`，超时 60000 毫秒。语音识别和合成由本地模型执行。dsh-pet 的 `voiceEnabled` 保持 `false`，设备自行请求并播放本地合成结果。

在局域网桥接配置中增加 `"voice_port": 7344` 和 `"voice_token": "与 local_token 相同的值"`，再启动桥接服务。运行时需要同时启动上游 Node.js、本地语音服务和局域网桥接三个进程。建议人设使用简短中文回复，并把 `chatMemoryRounds` 设为 4，以控制回复时长和上下文用量。

本地语音服务、Python 依赖、FFmpeg 可执行程序和模型作为应用的电脑端伴随工具安装。模型来源、固定版本及 SHA-256 位于 `tools/voice-models.json`，Vosk 中文模型使用 Apache-2.0，Melo 模型使用 MIT，下载时保留模型的许可证。设备 MP3 解码复用共享媒体模块的 CC0 minimp3。

## 房间美术

三套像素房间通过内置 imagegen 工具生成，主题分别为晴日海景、月光卧室与海底观景室。原始 PNG 与完整提示词位于 `art/rooms/` 和 `art/rooms/sources.json`，房间背景以 CC0-1.0 提供。

应用使用 384×256 的索引色房间资源。`sources.json` 中的 `rug` 记录原图垫子中心与水平半径，转换工具同步生成站立锚点。背景裁切、角色脚底及地面阴影共用该锚点，拖动和漫步限制在垫子内。旧版存档的位置首次读取时回到垫子中央，其余养成进度继续保留。使用共享转换工具重新编码

```sh
python3 local/pxa-apps/fat-fish/tools/prepare_rooms.py
```

## 许可与署名

状态与功能图标使用原创 SVG 源文件，位于 `art/icons/`。它们与生成脚本采用 MIT 许可。`tools/prepare_icons.py` 使用 Pillow 与 CairoSVG 离线生成透明索引图集，普通构建直接使用已生成资源。

PXA 适配代码及转换工具由 lucinhu 提供，采用 MIT 许可。上游代码的 MIT 声明保存在 `assets/LICENSE-UPSTREAM.txt`。

逐帧解压使用 zlib 1.3.1 的 inflate 子集，采用 Zlib 许可，仅链接到应用。来源摘要和原始声明保存在 `vendor/zlib/SOURCE.json`、`vendor/zlib/LICENSE` 与包内的 `assets/LICENSE-zlib.txt`。

动画来自 [PC2005-cloud/dsh-pet](https://github.com/PC2005-cloud/dsh-pet)。上游动画、提示词与源视频按其 README 限非商业开源使用。本应用转换得到的动画与启动图标保留 CC BY-NC-SA 4.0 的署名、非商业及相同方式共享条件。

「溟月」原型由上善无形创作，DeepSeek 大肥鱼女仆鲸鱼娘设计来自 ZipZipPipe。完整来源和授权链接保存在 `assets/NOTICE.txt`，应用「关于」页面也显示署名。[上善无形](https://space.bilibili.com/4456176) · [ZipZipPipe](https://space.bilibili.com/4168597) · [形象授权声明](https://www.bilibili.com/opus/1247156596114456581) · [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)
