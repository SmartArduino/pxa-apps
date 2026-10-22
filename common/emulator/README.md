# PXA 多平台模拟器前端

SNES、Mega Drive、Master System、Game Gear 和 PC Engine 各有独立包 ID、媒体根目录和存档。此目录复用文件浏览、手柄/触摸、系统字体、i18n、音频 FIFO、帧调度和 Surface 显示，GB/GBC 与 NES 原有实现不依赖此目录。

## 内核接口

`core.h` 定义借用 ROM、逐帧推进、RGB565 视频、交错立体声 PCM、双手柄和电池存档接口。调用方保留可写 ROM 到 `emu_core_close()`；部分内核会在加载时去头、重排或交换字节。分配空间使用各平台 `EMU_ROM_CAPACITY(size)`，不能只按文件长度分配。

分辨率、像素步长、显示比例、视频时钟和采样率由 `emu_core_info` 提供。跳过视频绘制时仍须推进 CPU、IRQ、计时器及全部音频状态；静音仅停止向 Host 提交 PCM。PAL/NTSC 与运行中切换分辨率由前端同步，游戏画面等比例居中。SNES/MD/PCE 当前按各旧内核使用的标称 50/60 Hz 调度，SMS/GG 使用内核的扫描线时钟比值。

## 大 ROM 与缓存

七个平台（含 NES、GB/GBC）共用 [`common/rom`](../rom/README.md) 的有界银行缓存。`emu_core_load_source()` 接受文件大小、CRC32 和精确的按偏移读取回调；原始文件保持打开直到内核关闭。ROM 可大于应用当前可分配内存，无需扩大 Wasm 线性内存。平台地址空间及 mapper 的限制仍然适用。

五个新前端先预留内核初始化所需空间，再尝试整块驻留；放不下的 ROM 使用至多 512 KiB 缓存。这样能装进 RAM 的游戏保持原来的直接读取路径。CRC 分块扫描，不保留整份文件；SMD 反交错、MD 字节交换、HuCard 位反转在填页时执行。菜单和存档用的文件句柄独立于运行中的 ROM。

SDK 已有 FS `read-at`，C/C++ 均支持；复用该接口，不添加模拟器专属的文件 ABI。SD 使用 App media root，Flash 使用原有私有文件 `OPEN`，两者均按原应用身份校验。缺页读数、累计 I/O 时间和最大单次读耗时记录在 `EMU ROM`；存储错误会停止模拟，保留已有存档。缓存命中仍是 Guest 内存访问，低速 SD 或频繁换页的 ROM 可能降低帧率，须使用设备数据判断。

## 热路径

- ROM 直接借给内核，避免保留两份整 ROM。
- SMS/GG 优先把原始索引画面和调色板同步借给 Host GameRender，直接转换并缩放到映射的双缓冲 Surface，避免中间 RGB565 转换与纹理上传；不支持该接口的 Host 自动使用 RGB565 路径。其他内核生成原生分辨率 RGB565，按需缩放到 Surface。获得 Surface 发生在内核绘制之后，避免运行中改变源分辨率时写越界。
- 每个显示缓冲各保留源画面快照，只合成变化的行；按键隐藏时不因按钮状态改变强制重绘，额外快照内存不足时退化为完整绘制。LCD/DMA 与下一帧模拟可由 Host 重叠执行。
- 音频使用有界 FIFO 与非阻塞写入，重采样保留跨块相位；屏幕绘制和写声音都不等待设备耗完数据。
- 单个游戏机的 CPU/PPU/APU 有紧密的时序依赖，当前在同一客体线程中执行；不能随意分到多个线程。公共输出路径允许 Host 的 LCD 和音频任务并行工作。
- CPU 时间债务与绘制成本分别计量。降低绘制频率时继续推进完整模拟帧，不用重置时间债务掩盖性能不足。

## 构建与回归

包版本常量仅注入前端源文件；调整版本号不会使第三方 CPU/PPU 内核的全部对象失效。

设备部署建议用 `--target esp32s31` 生成单目标包；桌面模拟器可单独构建 `--target simulator`，二者复用加锁的 Wasm 缓存。多架构包会同时占用各份 AOT 的 Flash 空间，安装峰值还包含暂存文件。使用官方 PXADB 的空间预检；支持目录接收和原子接管的设备可避免保留一份压缩容器再解包的额外空间。

原生诊断不需要设备或 SDK，增加 SDK 路径后还会检查实际 PXA 事件解析与前端状态：

```sh
out=local/artifacts/console-native/$(date +%Y%m%d-%H%M%S)
cmake -S local/pxa-apps/master-system-emulator -B "$out" -DEMU_NATIVE_TEST=ON \
  -DEMU_GUEST_SDK_INCLUDE="$PWD/deps/pxa-system/sdk/guest-c/include"
cmake --build "$out" -j4
ctest --test-dir "$out" --output-on-failure
```

`tests/make_test_rom.py` 生成自行编写的诊断卡带，不包含商业游戏。内核测试检查加载/关闭、RGB565、PCM、跳帧及存档；前端测试检查独立双手柄、完整按钮、组合键和多点触摸。渲染测试覆盖动态高分辨率源、源/目标步长、双缓冲缓存和不同尺寸屏幕。地址及未定义行为检查可增加 `-DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g'`。

`emu_benchmark ROM frames draw_interval` 测量本机内核吞吐；该结果不代表 MCU 速度或 LCD 帧率。真实设备加载 ROM 后，可使用：

```sh
python3 local/pxa-apps/common/emulator/tests/device_perf.py \
  --port /dev/serial/by-id/<target> --serial <device-serial> --app-id pxa-master-system \
  --output local/artifacts/console-perf/$(date +%Y%m%d-%H%M%S) --seconds 32
```

测量脚本识别 `EMU`、`GB`、`NES` 前缀及 ROM 缓存计数，分别记录 `EMU PERF` 的完成模拟帧与固件 `PERF START display` 的显示完成计数；画面提交数和光学面板刷新率不能代替这些数据。控制器注入只验证软件输入链路，不能称为机械按键测试。脚本最后发送 `INPUT CANCEL` 恢复物理 GPIO 上报。默认会注入方向与 A 键，启动前应选好允许自动操作的场景。

日志、ROM 输出、截图、跑分和报告均写到工作区被忽略的 `local/artifacts/<topic>/<timestamp>/`；不作为发行包资源或正式文档提交。

## 许可证

公共前端为 GPL-2.0-or-later；部分显示、时序代码保留 MPL-2.0 声明，字体遵循 `licenses/ui-font-OFL.txt`。内核许可证分别保留于各 `vendor/`。Snes9x、PicoDrive 及 SMS Plus 使用的 Z80 等代码包含非商业等历史许可限制，不能仅凭前端的 GPL 声明推断整个 App 的分发条件。具体以各文件的许可证和权利人要求为准。
