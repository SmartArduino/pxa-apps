# OpenLara fixed/GBA 0.2.7 历史说明

0.3.x 默认主引擎见 README.md；以下为旧 alpha 路线。使用本文构建命令时需设置 `PXA_APP_DEFINES=OL_CLASSIC=0`。

移植 [XProger/OpenLara](https://github.com/XProger/OpenLara) 的 fixed 定点引擎，目标为 PXA Core 2.0、DOIT GameKit / ESP32-S31 / 320×240。保留原来的 Lara、敌人、动画、碰撞、关卡、背包、门洞裁剪和软件光栅化。上游固定在 `3b268ca4bf986c380f44a26ffd9218bf6d376249`，引擎为 BSD-2-Clause，见 `vendor/LICENSE`、`vendor/UPSTREAM.md`。

0.2.7 包含原版标题、音乐与音效、私有存档和设置、触屏暂停菜单。上游这个 fixed 分支是早期 alpha，提供 TITLE、GYM、Caves、City of Vilcabamba；不等同于完整桌面版的全部关卡和功能。

## 构建和安装

在 PXA workspace 根目录执行，用实际端口和串号替换示例。ESP32-S3 需改用 `--target esp32s3` 重新生成 AOT；本次真机验收仅覆盖 S31。

```sh
python3 local/pxa-apps/openlara/tools/prepare-fixed-data.py \
  --upstream local/upstream-openlara \
  --port /dev/ttyACM0 --baud 2000000 --serial 1c:29:04:d0:af:5e
# 省略 --upstream 时从固定上游提交下载资源。
tools/app.sh build openlara --board doit-gamekit --target esp32s31 \
  --output local/openlara-build --aot-only
deps/pxa-system/bin/pxa publish local/openlara-build/pxa-openlara.pxa --check-only
python3 tools/pxadb/pxadb.py package install local/openlara-build/pxa-openlara \
  --port /dev/ttyACM0 --baud 2000000 --yes
python3 tools/pxadb/pxadb.py package run pxa-openlara --port /dev/ttyACM0 --baud 2000000
```

资源是 `title.scr`、`tracks.ad4`、`title.pkd`、`gym.pkd`、`level1.pkd`、`level2.pkd`，脚本逐个校验固定 SHA-256。游戏资源独立于引擎许可证，不纳入源码 Git，也不嵌入硬件包。资源位于该签名身份的固定 SD AppKey 目录。本工作区开发签名对应 `/sdcard/PXA/media/78670e6ca3833ee694be1ba9443f4d04b9e6ffc7617c7fbd7fe4a5363419135c/`；使用自己的签名时传入 `--publisher-id`。

应用使用 FS 0.3 `app-media-root`、`scoped-roots`。每回调最多读 8192 字节，显示加载状态并返回 Host。首次启动请在系统权限窗口允许媒体音频；拒绝后游戏仍可运行。音乐银行约 3 MB，固定驻留线性内存，避免播放时 SD 阻塞；线性内存当前约 6.38 MiB，上限 8 MiB。

更新时保持 App ID、发布者身份和递增 release_sequence。空间不足可使用 Host 的 **archive（保留私有数据）**，再安装；不要卸载或清除数据。此测试设备固件的短 ID `PACKAGE stop` 会返回 `package_not_running`，对活动应用再次 run 会切回原实例；更新后通过设备重启确认新代码已加载。应用内存档完成后再重启，不修改固件。

## 桌面模拟器

先运行不带 `--port` 的资源准备命令，再运行原开发命令：

```sh
python3 local/pxa-apps/openlara/tools/prepare-fixed-data.py --upstream local/upstream-openlara
./tools/dev.sh sim --board doit-gamekit openlara
```

桌面 Host 支持 SDL GameController 手柄（十字键、左摇杆、A/B/X/Y、肩键、Start/Back），左摇杆使用 25% 死区，可热插拔。键盘模拟使用下表，支持同时按下移动与动作。窗口失焦、游戏切后台、断开手柄时清除按住状态。

| 操作 | 手柄 | 模拟器键盘 |
| --- | --- | --- |
| 移动/转向、菜单选择 | 十字键/左摇杆 | 方向键 |
| 动作、菜单确认 | A | Z |
| 跳跃、菜单取消 | B | X |
| 拔枪/收枪 | X | A |
| 翻滚 | Y | S |
| 观察 | L | Q |
| 慢走 | R | W |
| 原版背包 | Start | Enter |
| 暂停菜单 | Select / Back | Tab |

GameKit 板载手柄由既有固件发送同样的 12 位 controller 状态。系统保留 Select+Start 返回桌面等组合键。0.2.7 修正 Start/Select 反置，并消费菜单确认/取消键直到松开，避免返回游戏时触发跳跃或动作。游戏“Controls”页面也显示模拟器键位。

## 操作

方向键移动/转向；A 动作、B 跳跃、X 武器、Y 翻滚、L 观察、R 慢走、Start 原版背包、Select 暂停菜单。设置可交换动作/跳跃、切换音效/音乐/FPS、调亮度。

触屏左侧为固定方向区：`(120,170)` 转向，`(70,115)` 前进，`(70,170)` 无动作。右下为动作/跳跃/武器区，`(200,20)` 打开原版背包。**`(180,220)` 打开暂停菜单**，菜单提供继续、存档、读档、三张可玩关卡、标题、设置和操作说明；每行可直接点击。加载失败时显示缺失资源提示。

存档和设置分别放在应用私有 `data/save.bin`、`data/settings.bin`。写入临时文件、关闭句柄，再由 FS REPLACE 原子替换；收到成功回执才显示保存成功。版本、长度、CRC 和物品记录校验失败时拒绝读档，保留可用游戏。写入失败保留先前已提交文件。主循环在存储操作期间暂停游戏更新。Host 未提供 fsync 接口，因此不宣称在任意断电时刻都能保证最新一笔写入落盘。

## 音频与优化

移植原 GBA C++ ADPCM4 解码器及六通道 PCM 混音器，修正 C++ 有符号输出混音；从 10512 Hz 线性重采样为 Host 的 16 kHz、单声道、16 位 PCM，媒体增益 -6 dB。音频最多一次提交四个 20 ms 包；WOULD_BLOCK 保留原包重试，不跳样或重复推进解码。限制满队列重试频率；无声源且缓存尾音已播完时不提交静音包，后台和关卡切换时清空队列。

原生 320×240，保留 `OL_OPTIMIZED=0` 渲染基线及 `OL_AUDIO_IDLE_SKIP=0` 静音包基线开关。普通 RAM 光栅访问、每多边形纹理指针快照、四像素批量 RGB565 转换、游戏逻辑 Os/光栅 O3、Surface release 驱动生产和 16 ms 最短生产间隔。仿真固定按 30 Hz 时间累积。修正原版光照、启用完整音频的同固件 A/B：0.2.5 静音包基线 20 样本中位数 34.8 FPS，0.2.6 静音优化 60 样本中位数 36.8 FPS（+5.75%）。此前 0.1.5 的无音频、偏亮结果仅作历史记录；0.2.7 修正控制器后独立 20 样本为 35.6–37.8 FPS、中位数 36.75 FPS。完整验收及官方 ROM 对照见工作区 docs/validation/2026-10-18-openlara-complete.zh-CN.md。

```sh
# 实际 WASM、异步文件、短写、失败替换、跨实例存档、设置、音频背压/拒绝、全部关卡。
node local/pxa-apps/openlara/tools/integration.mjs \
  main.wasm local/pxa-apps/openlara/assets-simulator integration.json
# baseline / optimized 使用相同固定时间和输入逐帧比较 RGB565。
node local/pxa-apps/openlara/tools/compare-render.mjs \
  reference.wasm optimized.wasm local/pxa-apps/openlara/assets-simulator comparison.json
python3 local/pxa-apps/openlara/tools/measure.py \
  --port /dev/ttyACM0 --serial 1c:29:04:d0:af:5e \
  --package local/openlara-build/pxa-openlara.pxa \
  --output local/openlara-measure --samples 60 --inventory --allow-audio
```

完整设备回归使用 `tools/validate-device.py --port PORT --serial SERIAL --baseline BASELINE_PACKAGE_DIR --candidate CANDIDATE_PACKAGE_DIR --output EVIDENCE_DIR --allow-audio`。它等待其他任务释放设备，持续持有串口，分别冷启动两组包，采样 20/60 次，验证保存后移动再恢复、升级保留存档、重启保留设置、三张关卡和后台恢复。比较包需要按 baseline → candidate 的顺序使用更高且递增的 release_sequence；重复执行时重新构建递增版本，保留应用和发布者身份。

测量脚本核对串号、从标题选择关卡，用 PERF FPS 采样实际 LCD 完成帧率。截图和内存查询在计时窗口之外，INFO 不用于轮询性能。首次权限窗口只在显式 `--allow-audio` 时自动点击允许。Node 固定时间结果不代表设备 FPS。`assets-simulator/game` 在 simulator 构建时独立打入资源包。桌面 Host 现提供只读应用媒体根 `local/simulator/PROFILE/media/<AppKey>/`，没有对应文件时使用包内测试资源。模拟器结果不替代硬件验证。

未进行全关卡通关、其他板型验收、物理按键扫描延迟或扬声器声学录音。软件验证覆盖设备音频后端消费计数和非零混音样本，不宣称测量了扬声器音压。上游震动钩子无 PXA 硬件实现。

0.2.6 根据作者发布的 GBA ROM 实际对照，修复继承自上游 C++ fallback 的房间光照二次右移错误，恢复原版明暗和雾。原先“优化前后逐帧一致”不等同于原版正确，旧版偏亮截图和帧率保留为历史记录。
