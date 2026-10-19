# OpenLara for PXA

0.5.2 默认使用 [XProger/OpenLara](https://github.com/XProger/OpenLara) 的完整 TR1 主引擎 `src/`，上游固定为 `3b268ca4bf986c380f44a26ffd9218bf6d376249`。目标为 PXA Core 2、DOIT GameKit / ESP32-S31、320×240 RGB565 软件渲染。保留原版敌人、机关、碰撞、动画、翻转房间、物品、背包和关卡流程；支持 TR1 的 15 个主线关卡、Lara's Home、标题和 4 个引擎过场关卡。引擎及修改遵循 BSD-2-Clause，见 `vendor/LICENSE`、`vendor/classic/UPSTREAM.md`。

**完整引擎不包含完整商业游戏资源，也不代表全关卡已经验收。** 默认资源只有公开 PC 演示版的标题和 City of Vilcabamba（1/15 主线），缺失关卡在菜单中标记 missing。完整原版数据尚未提供，15 关通关、引擎过场和大型关卡内存仍待验证。外部 FMV 视频暂未接入；SD 音乐支持独立导入。旧 fixed/GBA alpha 作为开发备用保留，说明见 `README-fixed.md`。

## 准备游戏数据

在 workspace 根目录执行。也可把原版 `DATA/` 或 `GAME.GOG` 放进 [game-data/](game-data/README.zh-CN.md)，脚本自动识别；没有原版数据时准备固定 SHA-256 的公开 PC 演示数据。已导入原版数据时，重复执行不会覆盖成演示版。

```sh
python3 local/pxa-apps/openlara/tools/prepare-data.py --upstream local/upstream-openlara
# 导入你持有的原版 PC DATA 目录，或游戏根目录。
python3 local/pxa-apps/openlara/tools/prepare-data.py \
  --game-data /path/to/TombRaider/DATA --audio /path/to/TombRaider/audio
# 也支持只读提取 GOG/ISO/BIN 光盘镜像中的 PHD 文件。
python3 local/pxa-apps/openlara/tools/prepare-data.py --game-data /path/to/GAME.GOG
```

完整导入要求 21 个 PHD 文件：TITLE、GYM、LEVEL1、LEVEL2、LEVEL3A、LEVEL3B、CUT1、LEVEL4、LEVEL5、LEVEL6、LEVEL7A、LEVEL7B、CUT2、LEVEL8A、LEVEL8B、LEVEL8C、LEVEL10A、CUT3、LEVEL10B、CUT4、LEVEL10C。导入前检查全部文件和二进制布局，缺失/截断时保留之前资源；`--allow-partial` 仅供开发。PHD 原样复制，不改变几何或游戏脚本。光盘镜像只读，不删除原文件。

可选 `--audio` 接收编号 MP3/OGG/WAV/FLAC，调用 ffmpeg 转成 10512 Hz 有符号 PCM8 流式音乐银行。默认识别 PC/PSX 编号方式，可用 `--audio-numbering pc|psx` 指定。不传音乐目录时保留上游固定的 AD4 音乐银行，**不声称包含完整原版配乐**。输出目录中的 `data-manifest.json` 记录已安装关卡、缺失清单、来源和 SHA-256。

资源独立于引擎许可证，不提交游戏二进制到 Git。准备结果位于 `assets-simulator/game/`。复制到真机时追加实际端口和串号：

```sh
python3 local/pxa-apps/openlara/tools/prepare-data.py \
  --port /dev/ttyACM0 --baud 2000000 --serial 1c:29:04:d0:af:5e
```

脚本校验设备身份，等待独占串口释放，最后上传关卡索引；私有存档及其他应用资源不受影响。默认开发签名的 SD AppKey 为 `/sdcard/PXA/media/78670e6ca3833ee694be1ba9443f4d04b9e6ffc7617c7fbd7fe4a5363419135c/`。更换签名身份需传 `--publisher-id`。每关原始文件上限 6 MiB。也可直接复制原版 `DATA/` 到这个 SD 目录，再在暂停菜单选择 **Import game data**、按 A 扫描；支持 DATA/data 与大小写 PHD 文件名。GAME.GOG 必须先在电脑提取。扫描检查文件头和大小，完整二进制验证在实际载入时执行。

## 模拟器和真机

```sh
./tools/dev.sh sim --board doit-gamekit openlara
./tools/app.sh build openlara --board doit-gamekit --target esp32s31 \
  --output local/openlara-full-build --aot-only
deps/pxa-system/bin/pxa publish local/openlara-full-build/pxa-openlara.pxa --check-only
python3 tools/pxadb/pxadb.py package install local/openlara-full-build/pxa-openlara \
  --port /dev/ttyACM0 --baud 2000000 --yes
python3 tools/pxadb/pxadb.py package run pxa-openlara --port /dev/ttyACM0 --baud 2000000
```

ESP32-S3 需要用 `--target esp32s3` 单独生成 AOT，尚未验收。0.5.2 完整引擎的 S31 AOT 约 2.6 MiB，设备需为安装保留足够闪存。更新保持 App ID、发布者身份及递增 release_sequence。archive 只归档代码并保留私有数据；不要清除数据。测试设备短 ID stop 可能返回 package_not_running，更新后通过设备重启确认新代码加载。首次允许媒体音频；拒绝后游戏仍可运行。

桌面 Host 支持 SDL GameController、25% 左摇杆死区及热插拔。键盘模拟支持组合输入；失焦、切后台或断开时清除按住状态。板载手柄使用同样的 controller 状态。

| 操作 | 手柄 | 模拟器键盘 |
| --- | --- | --- |
| 移动、转向、菜单选择 | 十字键/左摇杆 | 方向键 |
| 动作、确认 | A | Z |
| 跳跃、取消 | B | X |
| 拔枪/收枪 | X | A |
| 翻滚 | Y | S |
| 观察 | L | Q |
| 慢走 | R | W |
| 原版背包 | Start | Enter |
| PXA 暂停菜单 | Select / Back | Tab |

设置中的 Swap A/B 同时交换动作与跳跃、原版背包的确认与取消及其提示；保存设置后重启仍有效。PXA 暂停菜单自身保持 A 确认、B 返回。

触屏左方向区 `(120,170)` 转向、`(70,115)` 前进，右侧动作区，`(200,20)` 原版背包，`(180,220)` 暂停菜单。菜单提供保存/恢复、New Game、City、Home、标题、设置、操作说明、全关卡选择及原版数据导入。New Game 选择已安装的首个主线关；完整数据从 Caves 开始。全关卡列表两页，左右翻页，B 返回，缺失关卡不能进入。

## 存档、内存和音频

主引擎存档为私有 `data/classic-save.bin`，设置为 `data/classic-settings.bin`，与旧 fixed 存档隔离；旧文件保留，不自动转换。采用原版 OLS2 状态及外层版本/长度/CRC，验证物品、实体、房间和动画范围。写入临时文件后由 FS REPLACE 原子提交，失败保留前一笔；Host 没有 fsync 接口，不承诺任意断电时刻最新写入已落盘。

原始 PHD 数组借用输入缓冲，静态网格转移所有权，避免重复分配。音乐银行只驻留 2048 字节目录和 64 KiB FIFO，支持 SD seek、短读、大于 8 MiB 的音乐银行；关卡切换取消旧请求。完整 City 常驻约 7.4 MiB，20 次关卡切换的测试峰值约 8.63 MiB，随后保持稳定；线性内存最大 11 MiB、pinned。**其他关卡是否能在这块板的内存内运行仍需原版数据实测**。

原版音效经完整引擎混音，再从 44.1 kHz 立体声转成 Host 16 kHz 单声道。音乐支持原 AD4 和导入 PCM8，媒体增益 -6 dB。音频背压保留待提交包；无音源时跳过静音，后台与加载时清空队列。光栅与边缘步进独立 O3 编译，主引擎 O2；PXA 入口校验原版 PC PHD 格式。保留 `OL_OPTIMIZED=0` 作为原始扫描线参考。优化需以同关卡、同固件的真实 PERF FPS 为准；Node 固定时间运行不作为设备 FPS。

## 验证

```sh
python3 local/pxa-apps/openlara/tools/classic-data-tests.py --help
node local/pxa-apps/openlara/tools/classic-integration.mjs main.wasm \
  local/pxa-apps/openlara/assets-simulator integration.json
node local/pxa-apps/openlara/tools/controller.mjs main.wasm \
  local/pxa-apps/openlara/assets-simulator controller.json
node local/pxa-apps/openlara/tools/classic-controls-settings.mjs main.wasm \
  local/pxa-apps/openlara/assets-simulator controls-settings.json
node local/pxa-apps/openlara/tools/classic-compare.mjs reference.wasm optimized.wasm \
  local/pxa-apps/openlara/assets-simulator comparison.json
python3 local/pxa-apps/openlara/tools/validate-classic-simulator.py \
  --socket /tmp/pxa-simulator-1000/doit-gamekit.control.sock \
  --log /path/to/simulator.log --output /path/to/simulator-evidence --swap
python3 local/pxa-apps/openlara/tools/measure.py \
  --port /dev/ttyACM0 --serial 1c:29:04:d0:af:5e --classic --level 3 \
  --package local/openlara-full-build/pxa-openlara.pxa \
  --output local/openlara-full-measure --samples 60 --inventory --allow-audio
```

`classic-upgrade.mjs` 验证旧版存档在新版恢复，再重存后回到旧版读取的双向兼容。`classic-integration.mjs` 覆盖完整 Guest 的保存/跨实例恢复、损坏记录、失败原子替换、旧存档保留、音乐设置、SD seek/短读和缺失关卡。它从 PHD 元数据定位实际 Lara，独立比较玩家日志与可变长度 OLS2 实体记录；运行前用新版准备脚本更新元数据。City 演示专用回归请使用独立演示资源目录，保留已导入的完整资源。`classic-controls-settings.mjs` 检查交换后的确认/取消、提示、设置持久化及跳跃；`validate-classic-simulator.py --swap` 通过实际 SDL 键盘路径复验。`streaming-tests.mjs` 对正常、大银行及 97 字节短读比较实际 PCM。`classic-compare.mjs` 对同一 City 的 360 帧逐帧比较 RGB565。`validate-device.py --classic` 验证 City 的真机帧率、移动、背包、保存恢复及重启设置；演示资源不足以替代全关卡验收。

## 0.5.0 输入延迟修复

只由时钟生产画面，缓冲释放通知立即返回 Host，避免这些通知反复渲染并拖延排队的手柄状态；已被画面超越的旧时钟也不再重复绘制。支持细粒度时钟的 Host 在绘制前暂停周期计时，提交画面后再安排下一帧，令绘制期间到达的手柄命令先进入队列；不支持时继续使用 16 ms 周期计时。短按的按下沿保持到下一次游戏更新，松开、断开和后台状态分别处理。修复低于 4 FPS 时每帧重置引擎时钟导致更新停滞的问题；暂停菜单与后台仍冻结游戏时间。不透明暂停/帮助页跳过被覆盖的 3D 绘制。

`OpenLara input` 日志给出 Host 采样到 Guest 提交画面的平均/最大时间，并拆分 `ingress`（采样到应用收到事件）与 `frame`（收到事件到提交画面）。它不包括物理按键电气消抖与 LCD 扫描时间，不应称为完整按键到屏幕的实测值。帧统计保留 `render_us` 的更新与绘制合计，并新增 `logic_us`；两者相减得到绘制时间，便于区分逻辑追赶和光栅开销。

```sh
node local/pxa-apps/openlara/tools/classic-latency.mjs baseline.wasm candidate.wasm \
  local/pxa-apps/openlara/assets-simulator latency.json
node local/pxa-apps/openlara/tools/classic-catalog.mjs candidate.wasm \
  local/pxa-apps/openlara/assets-simulator catalog.json
python3 local/pxa-apps/openlara/tools/classic-device-latency.py \
  --port /dev/ttyACM0 --serial 1c:29:04:d0:af:5e --version 0.5.2 \
  --package local/openlara-full-build/pxa-openlara.pxa --output device-evidence --samples 60
```

真机脚本使用 GameKit controller-0 调试注入走板载上报路径，分别验证静止帧率、转向/前进、存档恢复和背包，退出时清除注入接管；物理按键接管或其他应用前台时终止/等待。`--install` 仅替换 OpenLara 代码，保持发布者和私有数据。

## 0.5.0 遮挡与裁剪

上游软件后端原先关闭深度缓冲，按提交顺序覆盖画面；PXA 现在启用 16 位逐像素深度测试及原引擎的深度写入状态。索引 0 的透明纹素不写颜色和深度，UI 仍使用无深度路径。320×240 深度缓冲增加 150 KiB 堆占用。投影深度使用有符号范围内的 15.16 固定点插值，避免近远顶点相减溢出。

跨过相机近面、软件远距离或安全固定点范围的面片按齐次坐标裁剪，保留可见部分；普通屏幕边界由原整数扫描线裁剪处理。裁剪前用面片的共同越界位快速剔除，并跳过未跨越的裁剪面。修复靠墙转向时整面消失，以及房间、门和人物部件的前后覆盖错误。RGB565 调色板、仿射贴图及软件光照仍与 OpenGL 有差异，不声称逐像素复刻。

下面的对照工具分别编译固定上游原版 OpenGL、原版软件后端和当前软件后端，在相同输入、分辨率和五个相同相机位采集画面，并核对相机与角色数据。需要 SDL2、GLES2、Pillow 和可用的桌面显示。

```sh
python3 local/pxa-apps/openlara/tools/validate-classic-render.py \
  --upstream local/upstream-openlara \
  --data local/openlara-full/classic-assets/game/level2.phd \
  --output local/openlara-render-reference
c++ -std=c++11 -O3 -fwrapv -Ilocal/pxa-apps/openlara \
  local/pxa-apps/openlara/tools/classic-depth-test.cpp \
  local/pxa-apps/openlara/classic-raster.cpp -o /tmp/openlara-depth-test
/tmp/openlara-depth-test
```

`classic-clip-test.cpp` 额外验证非零网格起始偏移、屏幕边界、跨近面和完全隐藏面；用当前 `vendor/classic` 包含路径与 `_GAPI_SW` 编译，音频解码库使用上游自带文件。

## 0.5.0 保持原画面的绘制优化

保留 320×240、活动敌人数、可见距离、原始关卡和玩法。普通三角形/四边形先执行原扫描线已有的整数背面和屏幕边界剔除，只有原本会绘出的面才进入光照计算和临时顶点数组；裁剪先测试原有近/远深度平面，完全隐藏的面不再计算屏幕 X/Y；跨近面可见部分仍完整裁剪。没有动态光源时不再计算无用法线归一化与逆矩阵。引擎直接写入 Host 已取得的空闲 RGB565 缓冲，提交后等待释放，省去整帧复制。深度测试、透明纹素和 UI 路径保持一致。

`PXA_APP_DEFINES=OL_RENDER_PROFILE=1` 构建可输出变换、光栅、清屏、面片、扫描线和像素计数；正式构建默认关闭。计时调用本身在真机有开销，只能用相同开关比较两个诊断包，最终 FPS 应使用关闭剖析的正式包。`classic-device-latency.py --timing-only` 只测静止场景，完整控制、短按及重启验证应另行执行。当前性能与逐帧一致性证据位于 workspace 的 validation 文档。

设置增加 **3D resolution**：320×240 原画面、240×180 和160×120。后两档仅缩小3D绘制目标并保持4:3视野，再放大到320×240；不改变逻辑更新、关卡、活动敌人数或可见距离。标题、原版背包和平台菜单仍使用320×240。默认保留原画面，选择会保存并在重启后恢复；该字段复用设置记录的保留字节，存档格式不变。

`classic-resolution.mjs` 用相同输入验证三档完整游戏存档逐字节一致、设置重启持久化及标题/平台UI不变。`classic-scale-test.cpp` 独立检查两个缩放比例与缓冲边界。真机测量脚本可用 `--resolution 320|240|160`，分别测量同一City起点。

## 0.5.2 原分辨率优化

保持 320×240，主引擎按 O2 编译；PXA 的四边形/三角形设置、边缘步进和像素循环放在同一个 O3 单元，像素内核强制内联，避免每条扫描线跨 Wasm 函数边界。保留原整数除法、UV/光照插值、深度测试/写入、透明索引和裁剪；未启用 fast-math。通用软件路径继续作为数值对照。

`classic-raster-batch-test.cpp` 对照 4000 组三角形/四边形的完整颜色与深度缓冲，覆盖绕序、非零裁剪框、透明纹素和深度开关。`classic-compare.mjs` 同时比较 360 帧 RGB565 和 PCM；使用 `--checkpoint` 可在同代适配层间额外比较完整存档，覆盖隐藏的游戏状态。旧版本的适配层可能有不同的内部状态初始化，旧版画面对照不要求存档逐字节相同；存档兼容另由升级测试验证。

同固件、同City起点的320×240真机平均FPS：0.5.0重测7.97（30个一秒窗口），0.5.2为8.82（60个），约提升10.7%。仍只有约9FPS。0.5.2真机移动、存档和背包通过；60秒计时完成后USB断开，20次短按及物理重启复验未完成。详细计数、画面对照和验收范围见工作区 `docs/validation/2026-10-19-openlara-320-perf.zh-CN.md`。
