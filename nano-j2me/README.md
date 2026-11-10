# nano J2ME for PXA

独立的 PXA Core 2.1 应用，通过 `nj_port` 接入 nano_j2me。`upstream/` 是
<https://github.com/lucinhu/nano_j2me.git> 的 Git submodule，固定提交
`fca2cd857a91d38e6cd26538871a92e00888ecb1`（0.5.6 后续兼容性修复）；其中的源码保持原样。
PXA 适配、构建选择和测试全部放在外部。菜单使用原生 Canvas 系统文字，
悬浮按键标签与 MIDP 游戏文字通过 UI 0.10 `font-glyph` 接口复用 PXA 系统字形；
需要配套支持该接口的 Host/固件。

## 构建与运行

从本工作区根目录执行：

```sh
git -C local/pxa-apps submodule update --init --recursive nano-j2me/upstream
tools/app.sh build nano-j2me --source-root local/pxa-apps --target simulator
tools/dev.sh sim nano-j2me --source-root local/pxa-apps
# ESP32-S3 / ESP32-S31：
tools/app.sh build nano-j2me --source-root local/pxa-apps --target esp32s3,esp32s31
```

沿用工作区的 WASI SDK、wamrc 和 `common/recrof` Binaryen 工具链。
可移植字节码解释器单独使用上游公开的 `NJ_INTERP_OPT_LEVEL=3`，其余内核仍用
`-Os` 控制代码体积；原生 JIT 和 computed-goto 分派保持关闭。
如未安装 Binaryen，运行 `local/pxa-apps/common/recrof/tools/setup.sh`。
应用源码属于 `local/pxa-apps` 仓库；父工作区忽略 `local/`。

## 导入与操作

游戏库下方的「打开JAR」调用 PXA 系统文件选择器，选择本地或存储卡中的
MIDlet `.jar`，系统将其导入应用媒体目录；新导入的单个游戏会直接启动。
也可提前将 JAR 放入该媒体根目录，然后点「刷新」。支持最多 128 个 JAR，
单个不超过 2 MiB，不递归扫描子目录，不内置商业游戏。

游戏画面保持纵横比，界面按屏幕尺寸、密度、安全边距与圆屏区域布局。
游戏库和设置使用原生系统字体；游戏像素与悬浮按键共用 RGB565 Surface。
游戏内点右上「菜单」或按 START 打开暂停菜单，再进入「运行设置」：

- 运行分辨率默认 240×320，提供 128×160、176×208、176×220、320×240、
  240×400、320×480、360×640 等预设；自定义宽高可用 +/- 或手柄左右调整，
  每边 64–640，最多 307,200 像素。运行中点「应用重启」重新启动 MIDlet。
- 悬浮按键默认紧凑方向键、确认和左右软键；左上「123」展开数字键盘。
  支持多指按键和 MIDP 单指点击/拖动；按键可调整透明度、大小及自动/显示/隐藏。
  自动模式下使用手柄会收起按键，触碰屏幕先恢复按键，再接收游戏触摸。
- 「Java 内存」可按 JAR 保存 1、1.5、2、2.5、3 MiB 的 Java 堆，默认 2 MiB。
  图片或场景加载时提示内存不足，可尝试增加至 2.5–3 MiB；大 JAR/高分辨率
  会占用同一线性内存预算，过大的 Java 堆也可能导致启动失败。运行中通过
  「应用重启」生效，原有配置按 2 MiB 兼容读取。
- 「手柄按键映射」可逐项修改方向、A/B/X/Y、L/R、START/SELECT，或点
  「识别按键」后按实际手柄键。可映射数字、方向、软键、FIRE 等 MIDP 按键，
  也可取消映射。「手机键位」一键将方向映射为 2/8/4/6、确认映射为 5，
  适用于直接读取数字键的游戏。START + SELECT 始终打开菜单，避免修改映射后无法返回。
- 默认手柄方向对应 MIDP 方向，A 为 FIRE，X 为左软键，B/Y 为右软键，
  SELECT 为数字 0，L/R 为 */#，START 为菜单。支持两个控制器合并输入；
  同一个游戏键被多个来源按住时，全部释放后才发送抬键。
- 菜单支持手柄上下选择、左右调整、A 确认、B 返回，以及触摸列表滚动。

游戏库中的设置作为默认值；游戏内设置按 JAR 内容指纹单独保存。
正常返回游戏库或重新启动时保留设置与 RMS，文件名改动不会改变存档身份。
存档在私有 `data/rms/<指纹>/`，游戏配置在 `data/profiles/<指纹>.njp`；
配置通过临时文件和原子替换保存。卸载或清除应用数据会删除这些内容。
悬浮按键使用固定容量的形状/字形缓存和 RGB565 混合表，游戏每帧不再请求字形，
不创建全屏透明图层；无输入、无游戏更新时不额外刷新。

## 范围与许可

Java 堆可配置为 1–3 MiB（默认 2 MiB），PXA 线性内存固定预留 6 MiB，为 ESP32-S31 的原生代码
和系统图形缓存保留连续 PSRAM 空间。使用解释器，经 Wasm/AOT 编译成
PXA 应用；Java 原生 JIT 关闭。保留上游 2D/3D、图像与媒体解码实现，
实际兼容性与帧率取决于 JAR 和设备资源，不能将模拟器结果视为真机性能。
大 JAR 与高分辨率同时使用可能超出内存预算；启动失败时可先降低运行分辨率。
音频通过 PXA PCM 服务输出，音符序列最多 64 项；音频权限可拒绝。
此版没有接入联网、蓝牙、传感器、JSR 75 FileConnection、原生 IME 和快照 UI。
RMS 正常通过移植接口读写；被系统强制停止时不能保证 MIDlet 的 destroyApp 被执行。

系统字体通过 `nj_run_opts.primary_fonts` 提供三个字号的普通/粗体字形；粗体在应用
缓存中加粗，透明混合、离屏 Image、裁剪与文字度量仍由上游绘图代码处理。
缓存固定 128 项，采用四路组相联与组内 LRU，避免常用字形哈希冲突时反复请求 Host；
约 195 KiB，单字形上限 32×48 像素，未复制整份系统字体。
上游内置 CJK 字库与 TTF 解析器
已在外部 CMake 关闭。系统字库缺少的字符遵循 Host 缺字回退。接口支持 Unicode
标量，MIDP 字形回调使用 UTF-16 单码元，非 BMP 字符仍受上游能力限制。

nano_j2me 是 MIT；本适配器与复用的应用桥接代码为 GPL-3.0-or-later，
仅链接在独立应用包中。系统、固件和 Host 不引入这些实现。
版权与许可随包放在 `assets/licenses/`，完整上游许可保留于 `upstream/LICENSE`。
上游注册表仍保留少量兼容用 Latin 位图，已核对 Liberation Mono 源字体哈希并保留
其 OFL 许可；实际 MIDP 与悬浮标签使用系统字形提供器，菜单使用系统 Canvas 字体。

## 回归测试

需要 JDK；测试用上游签名桩编译本目录原创 `tests/PxaProbe.java`，不修改上游目录。
在真实 PXA 产品模拟器验证 JAR 启动、显示、手柄/触屏输入、异常恢复与 RMS 重启持久化：

```sh
python3 local/pxa-apps/nano-j2me/tests/frontend_test.py \
  --output local/artifacts/nano-j2me-model/$(date +%Y%m%d-%H%M%S)
python3 local/pxa-apps/nano-j2me/tests/simulator_test.py \
  --package local/app-output/pai-touch/pxa-nano-j2me \
  --output local/artifacts/nano-j2me/$(date +%Y%m%d-%H%M%S)
# 自备 JAR 的独立试跑，输出原始截图、运行日志及退出统计：
python3 local/pxa-apps/nano-j2me/tests/game_trial.py \
  --package local/app-output/pai-touch/pxa-nano-j2me \
  --jar /path/to/game.jar --heap-kib 3072 --resolution 240x320 \
  --output local/artifacts/nano-j2me-game/$(date +%Y%m%d-%H%M%S)
```

模拟器测试可用 `--width 480 --height 800 --density 216` 等参数检查其他分辨率，
或用 `--width 240 --height 240 --round` 检查圆屏。原生模型测试启用 ASAN/UBSAN，
覆盖布局边界、缓存绘制、多输入源、重复映射、配置校验及原子替换失败。
日志、原始截图、生成的 JAR 与模拟器状态均留在指定本地产物目录。
试跑记录中的正常退出和绘制计数不代表游戏可玩；需结合截图和实际关卡输入判断，
特别是只停留在标题、菜单或加载界面的游戏。
