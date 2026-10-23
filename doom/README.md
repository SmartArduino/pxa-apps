# DOOM for PXA

基于 [Daivuk/PureDOOM](https://github.com/Daivuk/PureDOOM)，固定提交 `355cfbd16fac119718879239336ee2ea408886bd`。使用本仓库 `../common/recrof/` 的 Core 2.1 main-loop、文件系统、触控/控制器、Surface 和 PCM 运行层。

## 构建

以下命令在 `pxa-projects` 仓库根目录执行。需要已有 PXA Guest SDK、WASI SDK、wasm-opt 和对应 WAMR AOT 工具链；使用仓库构建入口自动选择工具。

```bash
# 为桌面模拟器和 ESP32-S31 构建同一个签名 AOT 包
./tools/app.sh build doom --board doit-gamekit \
  --source-root local/pxa-apps --target simulator,esp32s31 --aot-only

# ESP32-S3 使用 Xtensa AOT
./tools/app.sh build doom --board doit-gamekit \
  --source-root local/pxa-apps --target esp32s3 --aot-only

# 需要通用 WASM 调试包时
./tools/app.sh build doom --board doit-gamekit --target wasm
```

默认输出在 `local/app-output/doit-gamekit/pxa-doom` 和同目录的 `pxa-doom.pxa`；可传 `--output` 改位置。不同 AOT 架构不能互换。Host 需支持 Core 2.1 main-loop、FS 0.3 的固定应用媒体根及 READ_AT、Audio 0.10、GameRender 1.2 的 borrowed indexed blit。音频权限是可选的，拒绝后可继续静音运行。

当前清单预留 8 MiB pinned Guest 线性内存，其中引擎 zone 4 MiB；Host、AOT、SD 缓存和音频还需额外内存。这个配置适合有足够 PSRAM 的板型，构建成功不代表整机能容纳内存或达到目标速度。引擎按 35 tic/s 上限推进；实际渲染速度随硬件负载变化。长时间后台暂停后丢弃时间积压。

## 准备 IWAD

商城发行版绑定独立签名的 DOOM 第一章试玩资源包。点击安装时，系统一起下载、校验和安装程序及原版 `doom1.wad`，不需要手动复制文件。系统管理安装卷和媒体目录，不要求 SD 卡。更新时，未改变的资源代次可以复用。试玩数据及其许可独立于引擎 GPL；资源配方与构建入口见 `../common/recrof/resources/`。

以下准备工具用于本地开发或导入自己合法取得的 PC IWAD，默认获取固定提交的 DOOM shareware `doom1.wad`：

```bash
python3 -B local/pxa-apps/doom/tools/prepare-data.py

# 网络受限时，使用已有 PureDOOM checkout 的 doom1.wad
python3 -B local/pxa-apps/doom/tools/prepare-data.py \
  --upstream local/upstream-puredoom

# 导入自己的游戏；使用下述标准文件名
python3 -B local/pxa-apps/doom/tools/prepare-data.py --wad /path/to/doom2.wad
```

默认输出为 `local/pxa-apps/.game-data/doom/files/`，可用 `--output` 指定包目录外的位置。工具校验 IWAD 魔数、目录与 lump 边界、必要资源和 20 MiB 上限，记录来源、大小和 SHA-256。默认保留已准备好的有效数据；显式 `--wad` 可更新选定文件。固定 shareware 文件为 4,196,020 字节，SHA-256：

```text
1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771
```

支持小写文件名 `doom1.wad`、`doom.wad`、`doomu.wad`、`doom2.wad`、`tnt.wad`、`plutonia.wad`。不支持 PWAD/mod、网络联机或非 PC 变体。每个应用媒体目录建议只放要玩的一个 IWAD；多个文件同时存在时，引擎固定选择 `doom2 → plutonia → tnt → doomu → doom → doom1`。本地 `data-manifest.json` 是资源工具的来源记录，不控制引擎选择，也不上传设备。切换游戏时应先将旧 IWAD 移出应用媒体目录，工具不会删除其他文件。

WAD 按需随机读取，不会把整个 WAD 预载到 Guest 内存。启动时再次检查目录和资源边界，缺失或结构损坏会显示错误页，可通过系统 Home 返回。

## 安装与媒体目录

商城安装会自动准备绑定的试玩 IWAD。单独安装本地构建的引擎包时，需要将选定 WAD 放进 Host 为该应用绑定的固定媒体根。不要把开发电脑 `.game-data` 目录视为自动挂载目录，也不要硬编码设备上的发布者昵称路径；发布者公钥和应用 ID 共同决定身份。

仓库的设备开发入口会构建、安装并运行包，端口和板型按实际设备填写：

```bash
./tools/dev.sh device doom --board doit-gamekit \
  --source-root local/pxa-apps --port /dev/ttyACM0 --baud 2000000
```

首次启动缺资源时，先退出到启动器，再使用资源工具显式部署：

```bash
python3 -B local/pxa-apps/doom/tools/prepare-data.py \
  --port /dev/ttyACM0 --baud 2000000 \
  --serial YOUR_DEVICE_SERIAL --publisher-id YOUR_PUBLISHER_KEY_SHA256
```

`--serial` 必填；`--publisher-id` 可省略，此时读取 `local/pxa-apps/.dev-signing/publisher-public.der` 的 SHA-256。工具先核验序列号和已安装 `pxa-doom` 的发布者，再查询 `PACKAGE paths` 返回的媒体位置，只上传选定 IWAD；应用活动期间拒绝传输。工具不刷固件、不向 Flash 回退、不上传配置或存档。

## 操作

| 操作 | 控制器 | 模拟器键盘 |
|---|---|---|
| 前进 / 后退 | 上 / 下 | ↑ / ↓ |
| 转向 | 左 / 右 | ← / → |
| 左右平移 | L / R | Q / W |
| 开火；菜单确认 | A | Z |
| 使用门 / 开关；菜单返回 | B | X |
| 跑步修饰键 | X | A |
| 切换已持有武器 | Y | S |
| 自动地图 | Select | Tab |
| 打开 / 关闭菜单 | Start | Enter |

菜单用上下和 A/B 选择，确认提示用 A 表示 Yes、B 表示 No。OPTIONS 中可设置 Always Run、音量、画面大小等；关闭菜单时将设置保存到私有数据区。控制器布局由适配层固定，配置文件中的自定义键位不作为用户操作接口。

触屏左侧拖动前后移动/平移，右侧拖动转向。屏幕上的 MENU 打开菜单；游戏中还有 WPN、USE、MAP、FIRE。菜单中对应 UP、BACK、DOWN、OK。控制器活动时触屏控件隐藏，首次触摸仅恢复控件显示，再次触摸执行操作。

通过原游戏 SAVE GAME / LOAD GAME 菜单存取 6 个槽。空槽自动命名为 `PXA SAVE 1` 等，再按 A/OK 确认，无需软键盘。存档 `doomsav0.dsg` … `doomsav5.dsg` 与 `default.cfg` 写入应用私有 `/data`，先关闭临时文件再用 FS replace 发布。各 IWAD 共用这 6 个槽，切换游戏前应备份或清理不兼容存档。系统 Home 用于返回启动器，DOOM 的 Quit 会显示退出提示页。

画面原始为 320×200 INDEX8，按目标屏幕缩放；320×240 屏幕恢复原 DOOM 的 4:3 显示比例。引擎生成 11025 Hz、双声道 S16 音效，公共运行层转为 Host 普通音频会话的 16000 Hz 单声道 PCM。当前产品 Host 尚未实现 Audio 0.10 的 OPEN_PCM 后端，因此本应用关闭 `PXA_RECROF_NATIVE_PCM`，使用已有 OPEN_SESSION 路径。当前未接 MIDI 合成器，背景音乐选项隐藏。

## 维护与测试

`doom.c` 提供引擎回调、输入映射、35Hz 推进、PCM 分块和存档适配；`controls.c` 在公共触控绘制上替换标签与键值。公共运行层保持共享，不在本应用内复制。

```bash
python3 -B -m unittest discover \
  -s local/pxa-apps/doom/tools -p test_prepare_data.py -v
```

产品模拟器集成测试程序在 `tests/simulator.py`，运行 `--help` 查看参数。它会发现实际媒体绑定、准备 WAD，验证标题图、进入首关、移动/转向/开火、菜单暂停、触屏导航、存档及重启回读，并检查缺失/损坏资源。`--capture-audio` 使用 SDL disk 驱动记录实际 Host 混音输出，要求出现非零音效采样。

```bash
./tools/simulator.sh product build --profile doit-gamekit
python3 -B local/pxa-apps/doom/tests/simulator.py \
  --simulator build/simulator/doit-gamekit/pxsys_product_simulator \
  --package local/app-output/doit-gamekit/pxa-doom \
  --publisher-key local/pxa-apps/.dev-signing/publisher-public.der \
  --wad local/pxa-apps/.game-data/doom/files/doom1.wad \
  --output local/artifacts/doom/$(date +%Y%m%d-%H%M%S)/simulator \
  --capture-audio
```

测试输出目录必须为空。测试使用独立临时 state，退出后清理，不影响交互式模拟器或设备存档。截图、日志、音频和 JSON 保存在上述忽略目录。交互式运行可使用 `tools/simulator.sh product --profile doit-gamekit --package ... --publisher-key ...`；首次启动后根据 `PXA SIM app-media-root=` 日志将 WAD 复制进该目录，再使用同一 state 重新启动。

交互式模拟器启动时会自动提供 PXADB 服务。另开终端可使用仓库内客户端检查连接、注入触摸和截图（`--instance` 对应 `--simulator doit-gamekit@实例名`）：

```bash
python3 -B tools/pxadb/pxadb.py info --simulator doit-gamekit
python3 -B tools/pxadb/pxadb.py input tap 19 12 --simulator doit-gamekit
capture_dir="local/artifacts/doom/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$capture_dir"
python3 -B tools/pxadb/pxadb.py screenshot "$capture_dir/frame.png" \
  --simulator doit-gamekit
python3 -B tools/pxadb/pxadb.py input key home --simulator doit-gamekit
```

320×240 窗口中的 `(19,12)` 对应 MENU；首次触摸可能只唤出操作层。应用退出后也可用 `fs push` 上传 WAD，远端路径使用实际 app-media-root 相对该实例 state 的路径。手柄按键通过模拟器窗口的键盘映射或 SDL 手柄输入；模拟器 PXADB 桥当前提供触摸和系统键。DOOM 引擎日志查看产品模拟器的标准输出。

模拟器不能替代真机对 PSRAM、SD 吞吐、音频设备和帧率的验证。

## 来源与许可

- 引擎：上述 PureDOOM 固定提交，原始头文件保留在 `vendor/PureDOOM.h`；仓库提供的 GPL v2 文本保留为 `LICENSE`，历史 DOOM 通知保留为 `vendor/DOOMLIC.TXT`。上游保留了历史非商业许可通知，分发时应保留并结合其源码发布许可核对。
- 本适配及复用的 recrof 源码标记为 GPL-3.0-or-later；其许可文本见公共目录。游戏 WAD 的版权与分发条款独立于引擎许可，商业 IWAD 由用户自行提供。
- 相对固定上游的唯一引擎补丁：修正首次扩展 lumpinfo 时从空指针复制 1 字节的问题；旧表按实际大小复制并释放，分配失败先处理，避免泄漏和空指针访问。其他平台逻辑均在适配文件中。
