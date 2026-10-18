# OpenLara for PXA

0.3.x 默认使用 [XProger/OpenLara](https://github.com/XProger/OpenLara) 的完整 TR1 主引擎 `src/`，上游固定为 `3b268ca4bf986c380f44a26ffd9218bf6d376249`。目标为 PXA Core 2、DOIT GameKit / ESP32-S31、320×240 RGB565 软件渲染。保留原版敌人、机关、碰撞、动画、翻转房间、物品、背包和关卡流程；支持 TR1 的 15 个主线关卡、Lara's Home、标题和 4 个引擎过场关卡。引擎及修改遵循 BSD-2-Clause，见 `vendor/LICENSE`、`vendor/classic/UPSTREAM.md`。

**完整引擎不包含完整商业游戏资源，也不代表全关卡已经验收。** 默认资源只有公开 PC 演示版的标题和 City of Vilcabamba（1/15 主线），缺失关卡在菜单中标记 missing。完整原版数据尚未提供，15 关通关、引擎过场和大型关卡内存仍待验证。外部 FMV 视频暂未接入；SD 音乐支持独立导入。旧 fixed/GBA alpha 作为开发备用保留，说明见 `README-fixed.md`。

## 准备游戏数据

在 workspace 根目录执行。不带 `--game-data` 时准备固定 SHA-256 的公开 PC 演示数据；已导入原版数据时，重复执行不会覆盖成演示版。

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

脚本校验设备身份，等待独占串口释放，最后上传关卡索引；私有存档及其他应用资源不受影响。默认开发签名的 SD AppKey 为 `/sdcard/PXA/media/78670e6ca3833ee694be1ba9443f4d04b9e6ffc7617c7fbd7fe4a5363419135c/`。更换签名身份需传 `--publisher-id`。每关原始文件上限 6 MiB。

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

ESP32-S3 需要用 `--target esp32s3` 单独生成 AOT，尚未验收。完整引擎的 S31 AOT 约 2.6 MiB，设备需为安装保留足够闪存。更新保持 App ID、发布者身份及递增 release_sequence。archive 只归档代码并保留私有数据；不要清除数据。测试设备短 ID stop 可能返回 package_not_running，更新后通过设备重启确认新代码加载。首次允许媒体音频；拒绝后游戏仍可运行。

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

触屏左方向区 `(120,170)` 转向、`(70,115)` 前进，右侧动作区，`(200,20)` 原版背包，`(180,220)` 暂停菜单。菜单提供保存/恢复、New Game、City、Home、标题、设置、操作说明、全关卡选择。New Game 选择已安装的首个主线关；完整数据从 Caves 开始。全关卡列表两页，左右翻页，B 返回，缺失关卡不能进入。

## 存档、内存和音频

主引擎存档为私有 `data/classic-save.bin`，设置为 `data/classic-settings.bin`，与旧 fixed 存档隔离；旧文件保留，不自动转换。采用原版 OLS2 状态及外层版本/长度/CRC，验证物品、实体、房间和动画范围。写入临时文件后由 FS REPLACE 原子提交，失败保留前一笔；Host 没有 fsync 接口，不承诺任意断电时刻最新写入已落盘。

原始 PHD 数组借用输入缓冲，静态网格转移所有权，避免重复分配。音乐银行只驻留 2048 字节目录和 64 KiB FIFO，支持 SD seek、短读、大于 8 MiB 的音乐银行；关卡切换取消旧请求。完整 City 测试 Guest 峰值约 7.4 MiB；线性内存最大 11 MiB、pinned。**其他关卡是否能在这块板的内存内运行仍需原版数据实测**。

原版音效经完整引擎混音，再从 44.1 kHz 立体声转成 Host 16 kHz 单声道。音乐支持原 AD4 和导入 PCM8，媒体增益 -6 dB。音频背压保留待提交包；无音源时跳过静音，后台与加载时清空队列。光栅热循环独立 O3 编译，其余主引擎 O1，保留 `OL_OPTIMIZED=0` 作为原始扫描线参考。优化需以同关卡、同固件的真实 PERF FPS 为准；Node 固定时间运行不作为设备 FPS。

## 验证

```sh
python3 local/pxa-apps/openlara/tools/classic-data-tests.py --help
node local/pxa-apps/openlara/tools/classic-integration.mjs main.wasm \
  local/pxa-apps/openlara/assets-simulator integration.json
node local/pxa-apps/openlara/tools/controller.mjs main.wasm \
  local/pxa-apps/openlara/assets-simulator controller.json
node local/pxa-apps/openlara/tools/classic-compare.mjs reference.wasm optimized.wasm \
  local/pxa-apps/openlara/assets-simulator comparison.json
python3 local/pxa-apps/openlara/tools/measure.py \
  --port /dev/ttyACM0 --serial 1c:29:04:d0:af:5e --classic --level 3 \
  --package local/openlara-full-build/pxa-openlara.pxa \
  --output local/openlara-full-measure --samples 60 --inventory --allow-audio
```

`classic-integration.mjs` 覆盖完整 Guest 的保存/跨实例恢复、损坏记录、失败原子替换、旧存档保留、音乐设置、SD seek/短读和缺失关卡。`streaming-tests.mjs` 对正常、大银行及 97 字节短读比较实际 PCM。`classic-compare.mjs` 对同一 City 的 360 帧逐帧比较 RGB565。`validate-device.py --classic` 验证 City 的真机帧率、移动、背包、保存恢复及重启设置；演示资源不足以替代全关卡验收。
