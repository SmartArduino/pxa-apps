# 小智 AI for PXA

可安装的 PXA Guest 应用。协议按 Espressif `esp_xiaozhi`
`12ba34b0d5db3f6811f80c2de2dd292ddc29ef1b` 的 WebSocket v1 路径移植。
会话、像素人物、配置、字幕及 Opus 解码运行在应用包内；系统提供通用 TCP/TLS
Socket、麦克风采集和 PCM 播放。需要包含 **Window 0.4、Net 0.6、Audio 0.15** 的 PXA
系统及对应 C++ SDK，并支持 UI `text-input-control` 和系统输入法，旧固件需先更新。

## 使用

1. 先在系统设置连接 Wi-Fi。未连接时，空房间会显示联网提示。
2. 首次启动默认使用小智官方配置服务 `https://api.tenclass.net/xiaozhi/ota/`，
   自动获取 WebSocket 地址及 Token。如返回激活码，按屏幕提示绑定设备，再轻触重试。
   首次启动会生成并保存独立 Device-Id 和 Client-Id；需要硬件证明的激活方式暂不支持。
3. 授予联网、播放和麦克风权限后自动聆听。说完后由服务器结束识别；聆听时轻触人物会
   结束聆听并回到待机，不会强行提交识别。待机时轻触或短按 A / Start 开始聊天；
   思考或回复时轻触可打断，然后重新聆听。
4. 长按画面或 A / Start 约 700 ms 进入设置；Select 也可直接打开。
   设置分为“连接、对话、画面”三个页签；可切换官方 / 自定义服务器、填写地址及
   Token，关闭自动聆听或语音回复，并选择场景。字段点开后使用系统输入法编辑，
   令牌隐藏显示。点击“保存并返回”持久保存配置并重新连接；返回键或 B 放弃
   未保存的修改。发送文字时会先保存当前接入配置。
5. “连接”页右上角“详情”可修改配置服务和设备标识；“对话”页右上角“文字”
   可以输入一句话并发送。
   进入设置或离开前台会关闭录音、播放和连接。
6. “画面”页的“对话时保持亮屏”默认开启，聆听、等待回答和播放期间不会自动锁屏。
   进入设置、断开连接或离开应用后恢复系统休眠策略；手动锁屏始终有效。

未配网时显示空房间和 Wi-Fi 提示；已联网但未激活时显示带激活码的邀请卡，
人物在本次启动的首次 WebSocket 会话验证成功后出场。已经见面后暂时断线，
人物以等待姿态留下，提示重连。进入设置、后台或重连不会下载图片，所有素材均在包内。

主页全屏叠加独立房间背景与透明像素女孩，显示识别文字与回复字幕。服务端
`llm.emotion` 映射到待机、开心、难过、思考、惊讶、生气、闭眼等整个人物帧，
保留挥手、托腮、歪头及肩膀动作；待机眨眼、轻微呼吸，说话时使用带手势的说话姿态。
所有表情都切换完整人物图片，不使用脸部贴片。未识别的心情使用待机姿态。

默认按系统当地时间选择房间：05:00–08:59 清晨、09:00–16:59 白天、
17:00–19:59 傍晚，其余时段为夜晚，每分钟检查一次。使用 Host Clock 的时区偏移
（含夏令时），系统时间尚未同步时先显示白天；设置中可固定某个时段。
场景切换只替换背景，心情切换只替换人物；显示时合成，不保存组合图片。
人物使用带透明度的预乘 BGRA 图像，背景为 RGB565；按需加载，一次只替换一层，适配默认
512 KiB 图片缓存。

录音目前接入 pai-touch 的 JL701（16 kHz、单声道、60 ms Opus）和桌面 SDL
麦克风（20 ms）。应用按采集源返回的时长协商 WebSocket，首次手动录音如需变更
格式会先重新连接，再发送音频。其他板型需要实现 capture source，可在设置关闭自动聆听并使用文字对话。
使用小智 `listen.start` 的 `auto` 模式，由服务器判定说话结束。回复结束后先等待
播放队列排空，再开始下一轮采集；点击打断会立即清空当前回复。没有本地唤醒词、
AEC 或 VAD，采集与回复播放轮流进行。

配置保存于应用私有 Storage，Token 输入框隐藏内容。配置总长最多 2048 字节，
官方会话地址与 Token 只在内存中使用，不覆盖自定义服务器配置；重新连接不会
重复写入未变化的设置。旧版配置迁移保留 Device-Id、Client-Id 和账号绑定。
关闭语音回复后，回复文字会保留到下一轮识别；空行和超长字幕按 UTF-8 边界换行与省略。
麦克风暂时未就绪时会有限重试，约十秒后提示手动重试或文字输入。
可接受的 WebSocket 消息最多 4096 字节。当前不包含 MQTT+UDP、设备 MCP、
摄像头、离线唤醒；配置服务只读取接入信息和激活码，不执行系统升级和重启。配置响应失败、缺少
WebSocket 地址或仍待激活时，不会误用旧缓存地址建立会话。
联网状态查询反映本机网络接口及 IP 状态，服务器连接失败会单独提示重试。

## 构建与素材

在 PXA workspace 根目录：

```sh
tools/app.sh build xiaozhi --source-root local/pxa-apps \
  --target simulator,esp32s3
# 安装、启动到对应设备：
tools/dev.sh device xiaozhi --source-root local/pxa-apps --port /dev/ttyACM0
```

`art/girl-gestures-atlas.png` 是内置图像生成工具生成的八种整个人物姿态，
`art/room-atlas.png` 是独立的四时段背景。最终提示词分别保存在
`art/whole-body-prompt.txt`、`art/layered-room-prompt.txt`。
`tools/prepare_art.py` 按姿态之间的透明间隔切分人物图集，避免混入相邻人物的手肘，
然后在统一的 400×500 画布上对齐头部和底部基线，按同一比例缩放，避免姿态切换时
人物大小突变。更换图集时需重新核对切分边界和锚点。人物输出为
`art/characters/` 下的 176×220 透明图，使用 256 色调色板、无抖动，随后转换为
预乘 BGRA；房间输出到 `art/rooms/`。`resources.json` 将整个人物帧编译为带
透明度的 PXR，将四张房间编译为 320×240 RGB565 PXR。
修改图集后先运行脚本，再重新构建应用。
`python3 tests/art_import_test.py` 检查八种姿态是否混入独立的下半身残片，
并检查等比缩放、锚点一致性、完整人物保留，以及预乘 BGRA 的尺寸和透明度约束。

待机时人物保持放松表情、轻微呼吸和眨眼，清空旧字幕，显示“我在这里，轻触和我聊聊”
及较小的“长按打开设置”。用户主动进入待机后关闭采集和对话连接，不会自动重连；
进出设置或切换前后台保留本次暂停意图。有效识别结果到达后才显示思考，超过三十秒
没有回复会提示并恢复；取消等待会建立新会话，避免旧回复混入。

播放会话准备完成后才开始对话，不丢弃初始化期间提前到达的音频。
每轮语音回复结束时，应用输出 `Xiaozhi audio` 诊断日志，包含收到的音频包数、
解码及提交的 PCM 样本数和解码峰值，用于区分无音频、解码静音与输出链路问题。
日志不包含对话文字、录音内容或连接凭据。

`tests/simulator.py` 使用真实 PXA 模拟器和本地 WebSocket 协议夹具；
`--offline` 检查联网提示，`--scene morning|day|evening|night` 固定预览场景，
`--emotions` 检查整个人物姿态切换，`--interactive` 保留控制入口，
`--ime-package` 指定受信任的系统输入法包。截图、状态和日志通过
`--artifacts` 指向工作区 `local/artifacts/` 下的目录。测试不需要真实小智账号。

`tests/device_fixture.py --bind <电脑局域网 IP> --port 8765 --token-file <令牌文件>
--artifacts <本地产物目录>` 可单独运行实机测试服务。将应用临时设为自定义
`ws://<电脑局域网 IP>:8765/xiaozhi` 并填写同一令牌。夹具校验上行 Opus
为 16 kHz、单声道，并检查握手帧时长与实际音频一致（默认 20 ms；pai-touch 使用
`--frame-samples 960` 检查 60 ms）。先播放两秒测试音，恢复采集后再播放六秒测试音，
供检查点击打断。字幕是模拟数据。产物只包含事件、包数和音量统计，不保存麦克风音频；
结束后恢复官方服务器设置。

## 来源与许可

- 协议移植：Espressif `esp_xiaozhi`，Apache-2.0；保留原版权，见 `LICENSE`。
- `vendor/opus`：Xiph Opus 1.5.2，见其 `COPYING`。来自
  `https://downloads.xiph.org/releases/opus/opus-1.5.2.tar.gz`，SHA-256
  `65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1`。
  构建静态、定点解码器；编码器未被应用使用，链接器移除未引用代码。
- `vendor/cjson`：cJSON 1.7.19（Espressif 包 `1.7.19~2`），MIT，见其 `LICENSE`。

PXA 系统侧没有引入小智专用 Host 服务；WebSocket 和小智协议均由 Guest 实现。

`tests/simulator.py --scenario` 可选择配置错误、激活后重试、断线重连、网络反复切换、
握手取消和设置取消等回归场景。语音结尾回归使用 `tests/build_audio_probe.py`，
复用现有模拟器构建目录的 CMake 编译/链接参数（需启用
`CMAKE_EXPORT_COMPILE_COMMANDS=ON`）。将生成的 `xiaozhi-audio-probe` 传给
`tests/simulator.py --simulator`，并选择 `--scenario audio-tail`；探针检查恢复录音时
所有已提交的 PCM 均已被音频输出接收。所有输出用 `--artifacts` 指向本地产物目录。

回归场景还包括 `profile-separation`（官方凭据与自定义配置隔离、重连不写设置）、
`save-clicks`（连续点保存）、`muted`（配合 Host 探针验证无播放会话且保留字幕）和
`capture-warmup`（采集源延迟就绪）。字幕的空行与省略行为由 `tests/captions_test.cpp` 覆盖。

`art/icon-source.png` 是基于内置女孩生成的图标原图；`assets/icon.png` 是安装包使用的
128×128 像素图标，提示词见 `art/icon-prompt.txt`。图标沿用紫发、星形发夹和奶油色毛衣，
使用简洁对话气泡，便于小尺寸识别。

`idle-toggle` 覆盖聆听/待机切换及设置返回后的暂停保持；`text-audio`、`early-audio`
覆盖文字回复与提前到达的音频，`empty-utterance`、`response-timeout`、`cancel-wait`
覆盖空识别、回复超时和取消等待。语音夹具由服务端模拟 VAD 完成，点击屏幕只切换待机或打断。
