# Pocket Play 运行时

Pet Garden、Crisp Arcade、Marble Islands、Lantern Grove、Harbor Cafe 与 Tiny Elements 使用的 PXA SDK 2.1 主循环运行时。代码和原创 5×7 字体以 CC0-1.0 提供。它从已验证的 [Pocket Game](../pocket-game/README.md) 独立派生；2048、Pocket Chess 与 Pocket Sokoban 继续使用 Pocket Game。

## 游戏接口

实现 [runtime.hpp](runtime.hpp) 中全部 `game::` 钩子，并将 [runtime.cpp](runtime.cpp) 加入 `pxa_add_callback_component`。Tiny Elements 以 [chunked-runtime.cpp](chunked-runtime.cpp) 替代默认实现；同一应用只能链接其中一份。`init()` 先于 `load()`；所有游戏绘图只发生在 `draw()` 内。改变画面后调用 `pocket::redraw()`，改变需要保存的状态后调用 `pocket::save()`。

游戏在固定 288×224 RGB565 逻辑画布绘制，运行时等比 fit 后最近邻重采样到实际 surface。296×240 屏幕得到原点 (0,5)、296×230；320×240 得到 (6,0)、308×240；480×320 得到 (34,0)、411×320；240×240 得到 (0,27)、240×186。更大屏幕先利用 Host 的 1/2/4 倍显示能力，guest surface 始终限制在 480×320 内，极大屏幕保持比例并居中。环境尺寸变化会取消输入，完成当前创建事务后关闭并重建 surface。`Pocket: viewport canvas=288x224 origin=X,Y size=WxH display=WxH` 中原点和尺寸均为最终物理像素。

触摸先逆变换 Host 整数缩放，再逆变换 guest 采样，命中实际画出的逻辑像素；黑边上的起始触点被忽略。右上角逻辑 24×20 区域属于运行时菜单。产品模拟器宿主底部 20 个物理像素可能由主页触摸层捕获：320×240 布局约覆盖逻辑 `y >= 205`，296×240 约为 `y >= 209`。可点击控件需按实际 viewport 保留安全区，不能仅凭截图可见判断可点击；底部背景和非交互提示仍可绘制。

参考 UI 启用返回手势时，左侧也保留一条输入区域，宽度为宿主安全边距加 16 个物理像素。控件的有效触摸区域需伸到条带以外，自动化点击应选择区域内侧。`INPUT` 请求被接受只说明注入成功，仍需通过游戏状态或画面变化确认触点实际送达。

`framebuffer()` 仅在 `draw()` 内返回可用指针，其他时候返回空指针；`pixel()` 在绘图期间进行边界裁剪。游戏必须自行保证直接 framebuffer 写入不越界。Crisp 原库在更新期间兼做碰撞与绘图，因此使用自己的离屏缓冲区，再在 `draw()` 内缩放复制。

`tick(elapsed_ms)` 只在前台且未暂停时执行，最小周期 20 ms，单次 elapsed 上限 100 ms。按工作量设置明确的内部追赶上限。`millis()` 是扣除后台停留时间的前台单调时钟，菜单停留仍会计时；物理和游戏倒计时应使用 `elapsed_ms`。每轮在排空最多 32 条事件后处理存档、计算与呈现，等待到下一次更新；绘图耗时不会再叠加固定 20 ms 等待。

## 输入与菜单

`key()` 收到按下边沿，方向键另有 330 ms 首次延迟与 110 ms 重复。持续动作使用 `held(Key)`；它在菜单、后台或退出期间始终为 false。进入菜单或后台会取消手势，并屏蔽此前按住的控制器按钮，直到观察到释放。关闭菜单的按钮也会先被消费，避免继续操作误触游戏。

触屏捕获第一个有效指针，保留真实指针 ID，允许捕获后的越界移动与释放；其他指针不会接管当前手势。菜单和生命周期变化会发送 `CANCEL`。实现时须将 `CANCEL` 与 `UP` 一并清理，但不要把取消当作一次有效点击。

START 或右上角按钮打开菜单；菜单依次为继续、重新开始、退出、语言。前三行逻辑按钮仍为 x56..231、y72..99 / 102..129 / 132..159；语言行 y162..189 在中文和 English 间切换，切换后保持暂停。`reset()` 应重开当前局或返回安全的起始界面；持久成长、解锁与最高分由各游戏保留。正常退出先冻结输入，再等待最新存档和语言偏好提交。默认运行时最长等待 2 秒；Tiny Elements 使用的分块运行时最长等待 6 秒，以容纳分块、提交头和完整重试。

语言默认中文，游戏字面量使用 `pocket::tr("EN", "中文")` 显式选择。UTF-8 绘制与测宽支持 ASCII 5×7、CJK 10×10，字符步进分别为 6/10，换行步进 10，均乘 scale；未知字形绘制可见方框。语言以独立 `<app_id>.prefs.v1` 键保存 4 字节 `P L 1 locale`（中文 0、英文 1），不会改变游戏存档或 Tiny 分块格式。异步 GET/SET 错误按 1 秒重试，晚到的读取不会覆盖用户新选择；切换输出 `Pocket: language zh/en`。

共享 [Pocket UI](../pocket-ui/README.md) 提供字体、fit 数学与偏好辅助。画布 129024 字节加两个最大 480×320 RGB565 输出缓冲区 614400 字节，共 743424 字节；输出以实际 frame_bytes 连续排列，基址 64 字节对齐。CJK 子集表目前约 16 KiB，使用 SIL OFL-1.1；每个游戏的 assets 必须携带 `OFL-NotoSansCJK.txt` 与 `FONT-NOTICE.txt`。原创 ASCII 字体和运行时代码保留 CC0-1.0。

## 存档与生命周期

存档键为 `<app_id>.save.v1`。SDK 2.1 的线格式允许单值最多 2048 字节，但当前 ESP 固件把实际单值上限配置为 **960 字节**；真实模拟器使用 2048 字节。直接使用默认运行时的游戏必须遵守较小的宿主限制；不能仅凭模拟器保存成功就认定设备可保存。每款游戏负责明确的二进制版本、长度、取值范围、保留字段和校验验证；只有完整验证后才能替换活动状态。不要将编译器结构体布局直接作为磁盘格式。

`save()` 合并 200 ms 内的写入，以独立且不可变的缓冲区保留正在提交的数据；默认实现失败后按 1 秒间隔重试最新状态。菜单暂停、进入后台和正常退出会请求当前状态的快照。后台停止计算、绘制和音频供给，继续接收存储完成事件；恢复时重画并清理旧输入。

Tiny Elements 的分块实现支持 1–1792 字节载荷，将当前 1780 字节完整世界拆成 896 与 884 字节，在两个 bank 间交替保存，最后写入原存档键中的 24 字节提交头。它只读取提交头指向的 bank，校验版本、长度、代次、保留字段与完整载荷 FNV 后才调用游戏的 `load()`。写入失败后每隔 1 秒从首块重试同一份不可变快照；新游戏修改排入下一代，提交头收到成功回执后才切换 bank。宿主必须保证单个键成功持久化的原子性，并提供足够的总存储配额。精确键名与线格式见 [Tiny Elements 存档说明](../../tiny-elements/README.md)。

放置类游戏若需要后台生产，可像 Lantern Grove 一样使用声明过的 WASI 标准单调时钟，在下一次回调结算一次时间差，并立即推进基准。它不依赖后台 tick，也不应再叠加 `elapsed_ms`。跨进程离线奖励另需有效 UTC、保存的时间高水位和收益上限；无有效实时时钟时只能结算同一进程内的单调时间差。

每 10 秒最多输出一条 `Pocket: perf`，包含成功呈现的帧数、采样周期与平均游戏/菜单绘制耗时。它用于定位绘制成本，不能代替主机 PERF 的端到端呈现统计。

## 可选音频

[audio.inc](audio.inc) 实现 16 kHz 单声道 PCM、16 个有界音符槽和三角波包络。每次最多补充三块 20 ms 音频，最多提前约 40 ms；输出不阻塞输入或存储。无权限、PCM 服务不可用或权限被撤回时继续静音运行。

`tone(frequency, duration_seconds, when_seconds)` 的 `when_seconds` 为 **`millis()/1000` 对应的绝对前台时间**；0 表示立即播放。支持 30–6000 Hz、最长 4 秒、最多提前 60 秒，超出限制或队列已满的音符会丢弃。`silence()` 取消队列并请求刷新主机音频。暂停、后台和退出同样清理声音，恢复时不会补播旧音符。

应用需声明 `permission`、最低版本 0.10 的可选 `audio` 服务，以及 `audio.playback`、`required:false`、`scope:"media"`。音频权限取得独立于游戏启动、窗口和存储状态机。

六款游戏仅播放操作和游戏事件音效，不播放背景音乐。[music.hpp](music.hpp) 保留历史文件名，提供 `pocket_music::Effects`：`reset(enabled)` 清理旧声音和冷却，`set_enabled(enabled)` 应用已有静音偏好，`cue(Cue::Collect)` 等调用触发 15 类短音效。没有 Track、Player 或周期 tick API，也没有曲谱或自动播放状态。一次事件最多四个音符；普通操作、绘画及重要奖励分别有 40、150、350 ms 冷却，调度队列有界。Lantern Grove、Harbor Cafe、Tiny Elements 通过它触发音效；Pet Garden、Marble Islands 保留直接短音符，Crisp Arcade 保留上游生成的动作音效并在非 vendor 桥接层禁用配乐。现有静音字段和游戏存档格式均不变。

## 构建和测试

每款应用使用最大 1 MiB 固定线性内存和 64 KiB 栈，`execution_model=main-loop`，不启用 Asyncify。工作区根目录中运行：

```sh
app=lantern-grove # 或 harbor-cafe、tiny-elements、pet-garden、crisp-arcade、marble-islands
artifact_root="$PWD/local/artifacts/pocket-play/$(date +%Y%m%d-%H%M%S)/$app"
mkdir -p "$artifact_root/cache" "$artifact_root/tmp"
PXA_BUILD_CACHE_DIR="$artifact_root/cache" TMPDIR="$artifact_root/tmp" \
  tools/app.sh build "$app" --source-root local/pxa-apps \
  --target esp32s31,simulator,esp32s3 --aot-only \
  --output "$artifact_root/package"
```

原生规则、进度、存档和绘图测试保留在各游戏的 `tests/` 中；真实模拟器和设备检查也从对应应用的集成测试入口运行。截图、报告、构建缓存、日志和性能数据只能输出到忽略的 `local/artifacts/`。

[tests/run-music.sh](tests/run-music.sh) 沿用历史入口名，编译 [tests/music.cpp](tests/music.cpp) 并使用 ASan/UBSan 检查 15 类事件音效、冷却、队列上限、静音、重置、时钟回绕及空闲时不生成音符。

[tests/run-sfx-pcm.py](tests/run-sfx-pcm.py) 将六款真实游戏的 `game::` 钩子与 [audio.inc](audio.inc) 中原样提取的音符队列和 PCM 渲染函数链接，不修改生产混音器。它对每款游戏验证初始空闲 PCM 全零、实际触摸或手柄操作产生非零 PCM、音效结束后再次全零；Crisp 还检查四款小游戏切换和重开均未恢复配乐。测试使用 ASan/UBSan，输出原始 16 kHz 单声道 s16 PCM、日志和 JSON；不连接音频服务或设备，也不能代替签名包的真实模拟器检查和扬声器听感检查。可用 `--apps` 选择应用。

```sh
artifact_root="$PWD/local/artifacts/pocket-play/$(date +%Y%m%d-%H%M%S)/sfx"
sh local/pxa-apps/common/pocket-play/tests/run-music.sh "$artifact_root/effects"
python3 local/pxa-apps/common/pocket-play/tests/run-sfx-pcm.py \
  --output "$artifact_root/pcm"
```

Python 集成测试需要 Pillow；串口设备测试还需要同一 Python 环境中的 pyserial。设备测试先查询 `INPUT CAPABILITIES`：`controller_mask=0` 的触屏板只验证触屏，手柄行为由支持控制器的模拟器验证。首次运行可能显示独立的音频播放授权弹窗；脚本确认本应用的权限请求并截图后允许播放，再等待音频就绪，避免把弹窗拦截误判为游戏输入故障。关闭或拒绝音频权限时仍可静音游玩。

设备空间紧张时，可按 `common/pocket-game/README.md` 中的安装方式单独构建 ESP32-S3 并安装签名展开目录。这样不会携带另外两个目标，也无需同时保留压缩包与解包副本。设备安装和测试应串行进行，避免多个进程争用串口。

启用 `PXA_POCKET_STUDIO=1` 的游戏使用 [Pocket Studio](../pocket-studio/README.md) 的原生图形和系统字体覆盖层；逻辑坐标与存档接口保持一致，圆角屏按安全区内缩。

组件可按需定义 `PXA_POCKET_SERVICES=1`，并实现 `service-hooks.hpp` 的事件与轮询回调，接入独立的异步服务。运行时传入包含暂停、后台和退出状态的前台标记及单调时间；实现应及时释放资源，并关闭取消后才完成的资源句柄。未启用宏的应用没有额外回调。

工具应用可定义 `PXA_POCKET_UTILITY=1` 使用测量菜单文案，定义 `PXA_POCKET_SILENT=1` 不请求音频权限或打开音频。对应 `game::reset()` 应实现菜单里的恢复绝对基准语义，不清除仪器标定。

回调应用可用 `pocket::portrait_display()` 判断物理显示方向，并用 `pocket::open_menu()` 将共享菜单作为普通可聚焦按钮提供给三键设备。Gen4的实体上/下/确认对应 `Key::UP`、`Key::DOWN`、`Key::A`；确认按下只派发一次，方向键支持运行时连按。启用 `PXA_POCKET_UTILITY` 时 `WIDTH/HEIGHT` 为布局时确定的值：竖屏宽240、高度按比例限定在260..360，横屏288×224；应用绘制与命中测试须共同使用这两个值。

`PXA_POCKET_FILL` 启用满屏Surface与动态逻辑画布，通常与 `PXA_POCKET_STUDIO`、`PXA_POCKET_RESPONSIVE` 一起使用。逻辑尺寸按实际安全内容区域比例计算，应用须使用 `WIDTH/HEIGHT` 排布绘制和命中区域。Surface始终覆盖完整输出，背景延伸到安全边缘，控件按同一内容区域变换；PXA整数缩放仍受预分配输出缓冲上限约束。`pocket::portrait_display()` 依据物理显示方向。该宏为应用单独启用，不改变其他应用的固定比例布局。
