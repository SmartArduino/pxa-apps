# Jump Jump 3D C++：实现与验证（2026-10-10）

已经完成独立 C++ SDK 应用、O3 构建、原生检查和签名 AOT 模拟器验证。原版玩法、评分、特殊平台、最高分和全部 19 个音频资源保留。真机 C++ 显示帧率和内存峰值尚未验收：pai-touch 容量不足，esp-mosaico 完整包写入失败。本报告不将桌面 CPU 时间或提交 FPS 当作设备显示 FPS。

基线为 pxa-apps `1a9b506` 的 C 版；工作开始时 Host 为 `c5bdd6a`，SDK 为 `d600bc2`。工作区有另一个阅读器任务的既有 UI、FS、网络等改动，均予以保留。测试固件包含这些改动，不能视为完全隔离的 Host 实验。可复用 SDK 提交为 `f10f873`、`d78142f`、`da7163a`。

**采用的改动**

| 改动与原因 | 收益 | 内存变化与验证 |
|---|---|---|
| 8×256 INDEX8 天空贴图，在 RGB565 量化前进行 8×8 Bayer 抖动，批量平铺 | 减少渐变色阶；避免逐条生成背景矩形 | 复用上传缓冲，无全屏背景缓存；同覆盖量原生基准见下表 |
| Host 不透明 sprite 专用路径，相邻重复 tile 直接复制已生成扫描线 | 减少每像素坐标计算和调色板查找 | 无额外缓存；Host 光栅回归与边缘裁剪检查通过 |
| C++ `Frame::palette_triangles`，固定 768 个顶点，painter 路径 | 保留等距几何及遮挡顺序，减少命令编码开销 | 无动态顶点容器，无深度缓冲；原版游戏测试及实际 Host 光栅检查通过 |
| 三档 DPI 字体，只上传选中档位的三个字图；在真实背景上做 alpha 抗锯齿 | 高 DPI 可读，文字边缘更平滑 | 复用字图槽及上传缓冲；六种屏幕截图检查通过 |
| 固定物理步长与显示 tick 分开 | 避免时钟抖动产生零次物理更新时跳过画面 | 无额外帧缓存；实际 AOT 桌面显示通知频率恢复到约 50 FPS |
| Host 光栅耗时控制分辨率，保留预热与升降档迟滞 | 避免排队或显示停顿误触降档 | 可选小型 POD，无默认大型缓存；迟滞、重置、边界检查通过 |
| Host 解码音效、八槽 LRU、单个在途加载；预热四个常用音效 | 保留声道、蓄力循环、音量和 BGM ducking | 固定容量；资源逐字节校验，模拟器无资源失败/音频欠载 |
| 后台取消游戏音效、暂停音乐并保留流位置，前台恢复 | 避免音乐被后台切换提前截断 | 复用现有声音状态与结束时间字段；签名 AOT 生命周期检查 |

天空有细小的有序抖动颗粒，这是降低 RGB565 色带的明确画质取舍。世界几何、相机、平台和视距没有为基准缩减。参考截图：[480×480 / 305 DPI](screenshots/mosaico-ready.png)、[176×176 圆屏](screenshots/round-ready.png)。

**固定场景原生基准**

GCC 16、C++26、Release/O3、同一 Host 光栅器，296×240，种子 `0x51ed2701`，30 个准备帧与 8 次自动中心跳跃，共 38 帧；C/C++ 交替运行三轮。数字为三轮“每轮帧平均值”的中位数。完整原始数据见 [native-performance.json](native-performance.json)。

| 指标 | C | C++ |
|---|---:|---:|
| 每帧命令字节 | 8,189.26 | 5,926.84 |
| 每帧命令数 | 24.89 | 25.89 |
| 每帧像素覆盖 | 87,679.71 | 87,679.71 |
| 完整场景 Host 光栅时间 | 95.39 µs | 84.58 µs |
| 仅背景 Host 光栅时间（隔离子工作负载） | 24.05 µs | 12.47 µs |

完整场景命令数据减少 27.6%，Host 光栅时间减少约 11.3%。背景单项收益不能当作整帧收益。两版的字体与背景画质不同，因此这也是绘制路径比较，不是单纯的 C/C++ 语言性能比较。原生结果不包含 Wasm/AOT Guest、设备排队及 LCD 传输。

**实际签名 AOT 与内存**

桌面 AOT 使用相同 296×240 初始准备场景、原始音效和音乐，预热 3 秒，每轮观察 60 帧，C/C++ 交替三轮。观察器测量 Guest tick、Host 执行及随后一次 LVGL CPU 工作，排除 sleep、设备传输与设备排队。结果在 [aot-performance.json](aot-performance.json)。

| AOT 指标（三轮中位数） | C | C++ |
|---|---:|---:|
| 桌面显示通知频率 | 50.30 FPS | 50.26 FPS |
| 帧 CPU 时间 p50 | 2.765 ms | 2.489 ms |
| Host 光栅时间 p50 | 210.5 µs | 161.5 µs |

显示节奏持平；桌面调度和 CPU 负载仍有波动，完整三轮数据保留，不能据此承诺设备加速比例。这不是 pai-touch 的显示 FPS。生命周期另做三轮实际签名 AOT 检查：后台 400 ms 内无新增渲染、时钟停止、音乐暂停；恢复后时钟及渲染继续，音乐实例保持相同，见 [aot-lifecycle.json](aot-lifecycle.json)。

以下为每个新进程冷启动后的稳定值和本次运行峰值，单位字节；这些项目并不覆盖全部 OS/设备内存，存在包含关系的资源预算与 Asset 缓存不能重复相加。

| 桌面分类 | C 稳定 / 峰值 | C++ 稳定 / 峰值 |
|---|---:|---:|
| Guest 线性内存 | 225,280 / 225,280 | 262,144 / 262,144 |
| WAMR 计量分配 | 86,888 / 179,383 | 98,430 / 217,796 |
| Host 外部资源预算 | 202,216 / 202,216 | 160,715 / 160,715 |
| 其中音效缓存 | 121,872 / 121,872 | 121,872 / 121,872 |
| 三帧缓冲 | 426,240 | 426,240 |
| 深度缓冲 | 142,080 | 0 |
| 桌面命令队列 | 32,768 | 32,768 |

Jump C++ 的 Guest 和 WAMR 分配增加，而 Host 资源和无用深度缓冲减少。AOT 文件及设备代码映射另见 [package-hashes.json](package-hashes.json)；不能用桌面映射方式证明 ESP SRAM/PSRAM 总量不增长。1 MiB 是包声明上限，不是已分配量。真机稳定内存、运行局部峰值及初始化峰值仍待 C++ 安装后核对。

pai-touch 原 C 版已测三轮，预热 5 秒、采集 4 秒：实际显示 FPS 为 **35.52 / 35.99 / 36.02**；Host 光栅平均 **11.264 / 11.101 / 11.074 ms**。探针无溢出，每次退出后帧缓冲、深度缓冲、队列和探针均释放。完整帧间隔、SRAM/PSRAM 稳定值与运行局部峰值见 [device-c-reference.json](device-c-reference.json)。该 BGM 是原版一次播放的约 2.81 秒音轨，5 秒预热后的采集窗口不包含播放；不能称为“持续音乐负载”。

**正确性与适配检查**

- 原版跳跃、连击、特殊平台、失败状态测试，以及真实 Host 命令验证/光栅检查通过。
- 联合原生套件 **7/7**，ASan/UBSan **5/5**；Release 测试显式启用断言。
- 签名 AOT 模拟器覆盖 296×240/160 DPI、480×480/305 DPI、176×176 圆屏、320×480/240 DPI、800×480/160 DPI、480×800/320 DPI。准备画面、蓄力、松手跳跃截图均通过；安全边距和圆角见 [display-matrix.json](display-matrix.json)。
- 全部音频源文件哈希见 [audio-parity.json](audio-parity.json)。保留原版一次 BGM 播放模式，不自行改为永久循环。模拟器使用 dummy 音频设备，检查解码/缓冲/触发，不代替真机听感验收。

**复现**

在 PXA workspace 根目录执行；两个 C 基线也需以当前工具构建，不修改它们的源代码。

```sh
bash tools/app.sh build jump-jump-3d --target simulator,esp32s3 --source-root local/pxa-apps --output local/cpp-game-ports-20261010/jump-c-baseline
bash tools/app.sh build jump-jump-3d-cpp --target simulator,esp32s3,esp32s31 --source-root local/pxa-apps --output local/cpp-game-ports-20261010/jump-all
PXA_CPP_GAME_TEST_BUILD="$PWD/local/cpp-game-ports-20261010/native-build" bash local/pxa-apps/common/cpp/tests/run.sh -DPXA_C_REFERENCE_BENCH=ON
python3 local/pxa-apps/common/cpp/tests/reference/compare_jump.py local/cpp-game-ports-20261010/native-build --repeat 3 > local/cpp-game-ports-20261010/jump-native.json
cmake --build build/simulator/pai-touch --target pxsys_voxel_resources_test --parallel
python3 local/pxa-apps/common/cpp/tests/reference/run_aot_probe.py --simulator-build build/simulator/pai-touch --artifacts local/cpp-game-ports-20261010 --output local/cpp-game-ports-20261010/aot-check
```

显示矩阵与真机采样命令参见 [Pixel Dungeon 报告](../../pixel-dungeon-cpp/docs/2026-10-10-validation.zh-CN.md)。新设备包须单独按目标构建，避免带入其他架构。固定画质基准使用 `J3_FORCE_SCALE_SHIFT=0`；桌面准备场景在预热期内不会触发自动降档。

**真机限制与后续验收**

pai-touch 的数据分区总 9,437,184 字节，已用 8,966,144，剩余 471,040；完整 C++ 包无法暂存。esp-mosaico 在上传完整包约 90% 时返回 `file_write_failed`，尚未安装成功；空间不足是推断，旧固件没有 errno/剩余容量诊断，尚不能完全排除其他写入错误。没有删掉原有应用或清除数据。已备份并校验 pai-touch 原 C Jump 包；受管理的安装目录写入被 Host 拒绝，未进行绕过。

本轮只更新 pai-touch 应用分区固件，未改分区表/数据分区。诊断固件 SHA-256 为 `683e695f22a4597889ca1e22556d4378add404ad363c7ba0ddbd777e5840afec`。补充 PXADB 空间、errno 诊断及有限重试，相关 62 项测试通过。esp-mosaico 保留旧 Host `57488b1-dirty`，不能混用不同 Host 的结果作为语言比较。

为准备可恢复的临时测试，已只读核对 pai-touch 实际分区表：`pxa_data` 从 `0x700000` 开始，大小 `0x900000`。连续整分区读取在 USB 传输途中中断，完整备份尚未形成；设备已复位回应用固件，未擦除或写入数据。临时清理必须先得到相应授权并完成可校验的完整备份，测试后恢复快照。

复位后再次 `doctor` 确认设备正常，最新存储已用 8,327,168 B、剩余 1,110,016 B（约 1.06 MiB），仍小于最终 ESP32-S3 容器的 1,428,288 B；还需解包临时空间。前述 471,040 B 是首次诊断时的值，不能当作复位后的剩余容量。最终两种设备容器哈希已单独记录。

仍需在获得足够安装空间后，使用同一 Host、同种子/相机/分辨率/画质，采集 C++ 实际显示 FPS、Guest 更新与提交、Host 光栅、排队、显示、局部内存峰值，并覆盖连续操作、音效听感、前后台与重复启动。真机帧率与总内存约束目前是未完成的验收项。
