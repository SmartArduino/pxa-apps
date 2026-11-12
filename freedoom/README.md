# Freedoom for PXA

独立应用 ID 为 `pxa-freedoom`，支持 Freedoom Phase 1（四章）和 Phase 2。
Freedoom 是游戏资源项目，运行引擎复用 `../doom/` 的 PureDOOM 与
`../common/recrof/` 的 PXA main-loop、Surface、输入、文件系统和 PCM 适配。
上游资源的名称与内容保持原样，PXA 文件回调将引擎的 `doomu.wad` / `doom2.wad`
探测映射到 `freedoom1.wad` / `freedoom2.wad`，不修改上游。

## 子模块与升级

`upstream/` 是 https://github.com/freedoom/freedoom 的 Git 子模块，固定到
v0.13.0，提交 `cfb8644b1a8dc7d7d2177e6a892ccaa2922bdaae`。
在 pxa-apps 仓库中初始化：

```sh
git submodule update --init freedoom/upstream
```

`upstream.json` 记录版本、来源以及发行 ZIP、各 WAD 和许可文件的 SHA-256。
升级时先在子模块中 fetch / checkout 选定版本，再更新配方与哈希、重新准备资源，
运行下述测试，最后在 pxa-apps 中提交新的 gitlink 和配方。不要使用浮动分支替代版本锁定。
PXA 构建只编译适配和引擎，不要求每次重建全部上游素材。

## 准备游戏数据

下列命令在 pxa-projects 工作区根执行。默认使用官方 v0.13.0 发行资源并逐项校验
大小与 SHA-256；无需安装 DeuTex。不下载 id Software 的商业或试玩资源。

```sh
# 默认只准备 Phase 2，约 28 MB 原始数据
python3 -B local/pxa-apps/freedoom/tools/prepare-data.py

# 准备两篇章（约 58 MB），可用于模拟器集成测试
python3 -B local/pxa-apps/freedoom/tools/prepare-data.py --phase both

# 使用已下载的同版官方 ZIP，避免重复下载
python3 -B local/pxa-apps/freedoom/tools/prepare-data.py --phase both \
  --archive /path/to/freedoom-0.13.0.zip
```

默认媒体目录是 `local/pxa-apps/.game-data/freedoom/files/`，包含原始 WAD 和
`licenses/` 版权、许可与音乐作者声明。媒体不嵌入应用程序包。
应用同时发现两个 WAD 时优先启动 Phase 2。要玩 Phase 1，只在应用媒体根放
`freedoom1.wad`；切换前退出应用。准备工具不会静默删除另一篇章，要求显式选
`--phase both` 或使用新的 `--output` 目录。

也可以从子模块构建原始 WAD。安装上游 `COMPILING.adoc` 指定的 make、Python、
Pillow 和带 PNG 支持的 DeuTex 后，先将源码导出到本地产物目录再构建，避免污染子模块：

```sh
source_out="$PWD/local/artifacts/freedoom/$(date +%Y%m%d-%H%M%S)/source"
mkdir -p "$source_out"
git -C local/pxa-apps/freedoom/upstream archive HEAD | tar -x -C "$source_out"
make -C "$source_out" VERSION=v0.13.0 wads/freedoom1.wad wads/freedoom2.wad
python3 -B local/pxa-apps/freedoom/tools/prepare-data.py --phase both \
  --source-wads "$source_out/wads" --output "$source_out/../media"
```

源码构建会按生成字节记录哈希，不要求与官方发行 ZIP 完全相同；原始 WAD 不做转换。
升级后的资源仍须使用 vanilla-compatible Doom 格式。

## 构建和安装

```sh
./tools/app.sh build freedoom --board doit-gamekit-2 \
  --source-root local/pxa-apps --target simulator,esp32s31 --aot-only

# ESPP4-A
./tools/app.sh build freedoom --board espp4-a \
  --source-root local/pxa-apps --target simulator,esp32p4 --aot-only
```

输出为 `local/app-output/doit-gamekit-2/pxa-freedoom.pxa`。其他芯片使用对应
`--target esp32s3` / `esp32p4`。Host 需要 Core 2.1 main-loop、Window 0.5、
Audio 0.10、FS 0.3 和 GameRender 1.2。启动时请求横屏并保持完整的 4:3 画面。
应用预留 12 MiB pinned 线性内存，引擎 zone 为 8 MiB；Host、AOT 和音频仍需
额外内存，适用于有足够 PSRAM 的设备。

默认应用清单绑定签名的 Phase 2 资源包：先安装 `resources.pxa`，再安装
`pxa-freedoom.pxa`。系统负责校验和挂载资源，不需要手动复制 WAD。
更换发布者签名密钥时，使用下面的命令重新生成资源绑定，再构建应用。

生成系统安装器管理的独立签名资源包：

```sh
python3 -B local/pxa-apps/freedoom/tools/prepare-data.py --phase 2 \
  --output local/pxa-apps/.game-data/freedoom-phase2/files \
  --resource-output local/artifacts/freedoom/$(date +%Y%m%d-%H%M%S)/resources \
  --sdk-root deps/pxa-system \
  --signing-key local/pxa-apps/.dev-signing/publisher-private.pem \
  --bind-package
# 绑定后重新构建应用；使用同一个发布者密钥先安装 resources.pxa，再安装应用。
```

资源目录中同时生成 `binding.json`，`--bind-package` 将其写入应用清单。
更换篇章或资源版本后需要重新签名资源包并重建应用。

只进行本地开发、手动导入媒体时，可在构建前移除应用清单的 `resources` 字段。
使用 PXADB 的 `PACKAGE paths <publisher-key-id>:pxa-freedoom` 查询实际 `media` 路径，
把选定的 WAD 和 `licenses/` 复制到该目录。不要硬编码发布者目录，也不要在应用
运行期间替换媒体。独立产品模拟器测试直接提供自己的媒体目录，可切换两个篇章。

## 操作与存档

| 动作 | 手柄 | 模拟器键盘 |
|---|---|---|
| 前后移动 | 上 / 下 | ↑ / ↓ |
| 左右平移 | 左 / 右 | ← / → |
| 左右转向 | Y / A | S / Z |
| 开火 / 使用 | R / L | W / Q |
| 跑步 / 切武器 / 地图 | Select / X / B | Tab / A / X |
| 菜单 / 确认 / 返回 | Start / A / B | Enter / Z / X |

触屏左侧拖动移动，右侧拖动转向；MENU、WPN、USE、MAP、FIRE 按钮提供其他操作。
菜单内按钮切为 UP、BACK、DOWN、OK。手柄活动时隐藏触控层，首次触摸恢复显示。

原游戏 SAVE GAME / LOAD GAME 菜单提供六个存档槽，空槽自动填入名称。
两篇章的配置和存档分别保存到应用私有目录中的 `freedoom1-*` / `freedoom2-*`，
写入完成后原子替换旧文件。切换篇章不覆盖另一篇章存档；升级关卡前仍需备份。

## 背景音乐

从 0.2.0 开始，直接播放原始 WAD 内的多轨 MIDI，并使用同一个 WAD 的 `GENMIDI`
乐器库实时合成。标题、关卡、过关和结局随引擎选择切换曲目；需要重复的曲目按完整
MIDI 时长循环，包含尾部静默。无需新增音乐资源、外部 SoundFont 或离线转码；原
0.1.0 的资源包可继续复用，12 MiB 应用内存配置保持不变。
Phase 1 第四章使用其独立的 `D_E4M1`…`D_E4M9` 曲目，覆盖经典引擎复用前三章
音乐的选择规则。选项菜单使用 WAD 字体绘制扩展标签，避免按原版 Doom 图片宽度
拼字导致 Freedoom 素材越界。

OPTIONS → SOUND VOLUME 中分别调节 SFX VOLUME 和 MUSIC VOLUME；音乐设为零时
静音，但播放进度继续推进。关闭菜单时保存设置，重启后恢复。游戏暂停时音乐停止
推进，恢复后继续；普通菜单内音乐继续播放。应用进入后台时随公共运行层暂停输出，
不会按离开时长追赶音频。

从 0.2.2 开始，PCM 按单调时钟生成，不再随每个画面固定生成 1/35 秒音频。
以约 20 ms 为一包，提前准备约 40–60 ms，在引擎的计时回调和运行层等待前补充，
让低于 35 FPS 的渲染仍能维持正常音频速率。后台切换、长时间停顿后重新建立
播放时基，丢弃旧积压。曲目加载只在引擎更新结束后进行，避免在 WAD 读取中
递归改变文件位置。弯音使用预计算表与单精度插值，减少 ESP32-P4 上的双精度运算。

`music.c` 使用至多 32 个轨道游标按时间合并 SMF 0/1，不分配完整事件列表或曲目
副本。支持 tempo、running status、GM 乐器/打击乐、音量、expression、延音踏板和
±2 半音弯音。逐次检查轨道边界与变长整数；异常轨道停止音乐，保留游戏和音效。
不支持 SMPTE 时基、SMF 2、SysEx 音源扩展或自定义 MUS 曲目。

合成器直接复用 `../duke3d/vendor/Game/src/midi/opl2.c` / `.h` 的 MCU OPL2 实现，
未修改其源码；`opl-compat/main.h` 提供独立于 Duke 平台的构建常量。
来源、许可、固定提交与文件哈希见 `music-upstream.json`。这是经典 AdLib 的九声部
单声道 FM 音色，复合乐器占两个声部，超出复音时回收旧声部；不模拟 GM 的声像、
混响和合唱效果。音乐混入左右声道并做饱和限制，保留音效原有立体声定位。

当前支持单人、320×200 软件渲染和 11025 Hz 双声道 PCM。沿用经典 Doom 规则和
渲染/存档限制；没有联机或通用 DeHackEd / Boom / ZDoom 扩展。
内嵌 DeHackEd 的关卡名、剧情文本和武器细节调整尚未应用，部分引擎文字仍显示 Doom。
模拟器首关验证不代表所有关卡通关或真机帧率保证。

## 验证

```sh
python3 -B -m unittest discover -s local/pxa-apps/freedoom/tests -p 'test_*.py' -v
python3 -B local/pxa-apps/freedoom/tests/music.py \
  --wads local/pxa-apps/.game-data/freedoom/files \
  --output local/artifacts/freedoom-music/$(date +%Y%m%d-%H%M%S)/native
# 音频计时、低帧率供给和生命周期重置的独立测试
audio_out="local/artifacts/freedoom-audio/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$audio_out"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  local/pxa-apps/freedoom/tests/audio_clock_test.c -o "$audio_out/audio-clock-test"
"$audio_out/audio-clock-test"
./tools/simulator.sh product build --profile doit-gamekit-2
python3 -B local/pxa-apps/freedoom/tests/simulator.py \
  --simulator build/simulator/doit-gamekit-2/pxsys_product_simulator \
  --package local/app-output/doit-gamekit-2/pxa-freedoom \
  --publisher-key local/pxa-apps/.dev-signing/publisher-public.der \
  --wads local/pxa-apps/.game-data/freedoom/files \
  --output local/artifacts/freedoom/$(date +%Y%m%d-%H%M%S)/simulator \
  --capture-audio
```

集成测试使用实际 PXA 产品模拟器和独立临时 state，验证两个篇章的标题图、首关、
移动、转向、开火、音效、菜单暂停、触控存档、重启回读和篇章隔离，以及缺失/损坏资源页。
音乐测试检查多轨与速度变化、完整曲目循环、暂停/静音、控制器事件和截断输入，
并在 ASan/UBSan 下合成所提供 WAD 的全部曲目。产品模拟器还通过真实设置菜单
关闭音效、静音和恢复音乐，检查 Host PCM、音量独立持久化及标题到关卡的曲目切换。
日志、截图、音频、JSON 均写入指定的本地产物目录。

## 许可

Freedoom 资源为 BSD-3-Clause，见 `upstream/COPYING.adoc`、`CREDITS` 与
`CREDITS-MUSIC`；资源工具在发行时保留原始通知。应用图标使用官方 Phase 2
桌面图标的原始配方：`sprites/heada1.png` 通过上游
`dist/pillow-compose` 居中放入 64×64 透明画布。来源和哈希见
`assets/icon-source.json`，适用同一许可。可在工作区根重新生成：

```sh
python3 -B local/pxa-apps/freedoom/upstream/dist/pillow-compose \
  local/pxa-apps/freedoom/upstream/sprites/heada1.png \
  local/pxa-apps/freedoom/assets/icon.png 64 64
```

PureDOOM 的原始通知、许可文本和已有补丁说明见 `../doom/README.md`、
`../doom/LICENSE` 和 `../doom/vendor/DOOMLIC.TXT`；PXA 应用适配为 GPL-3.0-or-later，
见 `../common/recrof/LICENSE`。这些实现仅构建进独立应用，不链接进系统或固件。
OPL2 合成器为 GPL-2.0-or-later，沿用原文件作者声明并在本应用中采用 GPL-3.0；
没有将合成器或 MIDI 播放器加入 PXA Host 服务。
