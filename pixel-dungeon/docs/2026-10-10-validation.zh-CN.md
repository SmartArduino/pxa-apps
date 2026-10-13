# Pixel Dungeon C++：实现与验证（2026-10-10）

独立 C++ SDK 版已完成，保留同仓库 C 版的 25 层、五个地区、三种职业、五槽存档、装备、背包、地图缩放、日志、排行榜、触控/手柄规则、23 种音效与六首完整音乐。采用 O3，没有为包体积削减玩法或音轨。原生和签名 AOT 模拟器验证已通过；pai-touch 的固定种子三轮游戏显示与 SRAM/PSRAM 局部峰值已经验收；两版真实保存状态一致，FPS 持平。设备音效听感未做定量评价。最新数据见 [真机报告](2026-10-10-device.zh-CN.md)。

源基线为 pxa-apps `1a9b506` 的 C 版，SDK 起点 `d600bc2`、Host 起点 `c5bdd6a`。新应用生产构建不依赖 C SDK。参考用 C 代码只用于可选比较测试。其他任务的既有工作区改动已保留，没有混入本应用提交。

**采用的改动**

| 改动与原因 | 性能或体验收益 | 内存与验证 |
|---|---|---|
| C++ SDK App、RAII 资源、固定容量异步任务与存档队列 | 生命周期和失败处理集中；防止并行写入共用存档暂存 | 无热路径动态容器；游戏/存档自测及冷启动加载检查通过 |
| 九张静态 INDEX8 图集外置为签名 PXR，直接绑定 Host Asset | 免除 Guest 大图复制与上传 | 生产代码不嵌入大图，上传暂存从 65,556 B 降为 1,044 B；原生像素等价及实际签名包检查通过 |
| 无深度的 2D painter 路径，三帧缓冲 | 免除无用深度分配 | 296×240 省去 142,080 B 深度缓冲；最终画面与原 C 版等价 |
| Host 不透明 sprite 专用内核 | 减少最常见贴图路径的逐像素工作 | 无额外缓存；Host 光栅回归通过 |
| DPI 选整数呈现比例，安全边距和屏幕轮廓参与布局 | 高 DPI 控件与文字保持可读，不重复缩放触控坐标 | 480×480/305 DPI 使用 240×240 逻辑目标；六种屏幕交互检查通过 |
| 静态画面按需绘制，动画及音量淡入分别调度 | 避免空闲重复渲染，保持音乐独立播放 | 固定 draw buffer、八槽音效 LRU、六个待播放音效；无额外 UI 帧层 |
| 图集→存档目录→音频的串行初始化 | 降低同时在途资源请求 | 固定任务池；完整签名包启动无资源失败 |
| 第二团标题火焰使用 `((frame+12)%24)/8` | 修复原版动画帧可能超出图集范围 | 无新增资源；全部 24 个标题动画位置经过 Host 验证 |
| 字体使用 Host 图集元数据，不检查已外置的 Guest 像素指针 | 修复外置资源版本文字消失 | 无新增字图；实际签名 AOT 截图验证 |
| SDK 手柄解码按真实 Host 协议读取事件标志 | 修复将 flags 当作数据长度导致所有真实手柄输入被丢弃 | 不新增存储；Host 常量单元测试及三轮签名 AOT 手柄搜索/落盘验证通过 |

九个图集原始 texel 没有重新量化或降低分辨率。保留中国大陆字形的 Source Han Sans CN 黑体及原来的抗锯齿字图。像素游戏使用整数缩放；高 DPI 下放大像素而不使用模糊滤镜。地图、HUD 和菜单都画进同一 GameRender 目标，仅一个透明 Canvas 接收输入，没有额外全屏 UI 帧缓冲。

截图：[480×480 / 305 DPI 游戏画面](screenshots/mosaico-play.png)、[圆屏加载存档](screenshots/round-loaded.png)、[竖屏设置](screenshots/portrait-settings.png)。

**同场景 C/C++ 原生比较**

种子 `0x51ed270b`，296×240、相同布局/相机/资源、英语与中文、标题/游戏/背包三个状态，每种 200 帧，GCC 16、C++26、Release/O3。每组断言游戏及布局结构尺寸一致、32 字节帧头以后的全部命令字节一致、Host 输出的全部像素一致。原始数据见 [native-performance.jsonl](native-performance.jsonl)。

| 编码及 Host 验证 CPU（不含光栅） | C | C++ |
|---|---:|---:|
| 英文游戏画面 | 10.549 µs | 10.324 µs |
| 中文游戏画面 | 9.987 µs | 9.888 µs |
| 英文背包 | 18.203 µs | 15.974 µs |
| 中文背包 | 17.076 µs | 15.782 µs |

两版相同命令经过同一个 Host 内核；不同轮次光栅时间的波动不能作为语言收益。这里没有测 AOT Guest 更新、设备队列或实际 LCD FPS。240×240 高 DPI 版本也不能直接与 C 版 480×480 的结果比较：内部像素量不同。正式帧率验收必须固定同一内部分辨率和画质。

**固定存档的签名 AOT 游戏比较**

同一普通新游戏存档，种子 `0x51ed270b`、战士、第一层、296×240 中文、160 DPI、相同画质/地图布局、同一 Host、完整音乐启用。两版各冷启动三次，实际从标题进入存档槽并加载；预热 3 秒后，每 240 ms 触控搜索一次，共 12 次，测量至少 4 秒的实际呈现通知。随后切入后台触发保存，检查真实私有存储的 CRC、快照世代与存档内容。搜索按原规则消耗两个回合，六次运行均达到 24 回合；最终存档 SHA-256 全部为 `066324e8c62dceee57a91a20067995d85676055616eeb3704b59259ec43700df`。没有使用只推进提交计数的空闲场景。原始汇总见 [aot-gameplay.json](aot-gameplay.json)。

| 三次运行的中位数 | C | C++ |
|---|---:|---:|
| 桌面实际呈现 FPS | 24.39 | 25.21 |
| 时钟帧 CPU：Guest tick、Host 及下一轮 LVGL | 2,739 µs | 2,945.5 µs |
| 时钟帧 Host 光栅 | 287.5 µs | 287 µs |
| 同步触控注入及事件处理 CPU | 159.5 µs | 165.5 µs |

CPU 是桌面经过时间，存在调度波动；本轮未证明 C++ 时钟帧 CPU 更低。呈现计数覆盖输入、动画和空闲阶段，光栅/CPU 分位数只覆盖观察到的时钟帧；输入处理单独记录，不能把两者相加当作完整帧耗时。这是桌面 AOT 性能证据，不包含设备排队或 LCD 传输，也不是纯语言收益。两版输入期间会覆盖尚未绘制的帧，结束计数各约 13–14 帧，不能隐去废弃帧。

手柄另做三轮 C++ 集成验证，同样 12 次搜索且落盘存档与触控结果完全一致，见 [aot-controller.json](aot-controller.json)。原 C 版把手柄订阅放在 root 1，却仅处理 node 2 的事件；真实 Host 会拒绝 node 2 的手柄注入，root 1 事件又被应用过滤。因此没有把无效的 C 手柄负载作为性能基线，也没有修改 C 参考实现来掩盖差异。

**签名 AOT 与内存**

真实桌面 AOT 冷启动，296×240 中文标题，预热 3 秒，三次独立进程。标题静态时不重复绘制，这是按需渲染行为，不能用其空闲 FPS 评价游戏性能。原始值见 [aot-performance.json](aot-performance.json)。运行峰值计数随每个进程重新建立，不使用历史累计峰值。手柄修正后的固定存档游戏测试再次得到相同的线性内存、WAMR 与资源预算稳定/峰值值。

| 桌面分类（字节） | C 稳定 / 本次峰值 | C++ 稳定 / 本次峰值 |
|---|---:|---:|
| Guest 线性内存 | 663,552 / 663,552 | 262,144 / 262,144 |
| WAMR 计量分配 | 426,201 / 447,992 | 51,826 / 101,748 |
| Host 外部资源预算 | 403,529 / 403,529 | 401,064 / 401,064 |
| 其中 Asset 缓存 | 2,958 / 2,958 | 386,592 / 386,592 |
| 三帧缓冲 | 426,240 | 426,240 |
| 深度缓冲 | 142,080 | 0 |
| 桌面命令队列 | 98,304 | 98,304 |

Asset 缓存增大是静态图集从 Guest/直接上传移入 Host 资源系统的结果，已包含在资源预算内，不能重复相加。SDK/应用固定缓冲、代码映射与音乐解码等仍需分别计量；桌面 WAMR/OS 映射不能代替 ESP SRAM/PSRAM。1 MiB 包声明是最大上限，实际线性内存不是 1 MiB。产物大小与哈希见 [package-hashes.json](package-hashes.json)。

生产 Guest 保留 49,152 B 绘制缓冲、五个 1,024 B 存档槽、1,024 B 序列化缓冲、1,152 B 存储报文、1,044 B 上传暂存及两个小型效果图，未新增大型默认缓存。设备代码映射、初始化峰值、稳定 SRAM/PSRAM、帧缓冲及命令队列已在 [真机报告](2026-10-10-device.zh-CN.md) 中分别列出；本页桌面数据作为独立工作负载保留。

**自验证结果**

- 联合原生套件 **7/7**：游戏规则、楼层生成、背包/装备/存档、布局、真实光栅器、SDK helper 与 C 参考比较。ASan/UBSan **5/5**，Release 断言启用。
- 手柄修正后再次完成联合原生 **7/7**、ASan/UBSan **5/5** 和完整 C++ SDK Host 回归；三次真实 Host 手柄事件重放均落盘 24 回合，与触控存档相同。
- 签名 AOT 模拟器的两游戏六屏幕矩阵 **12/12**；本应用六组均完成标题→存档槽→职业→进入游戏→等待→背包→暂停→设置→返回标题→退出进程→同一私有目录重新启动→加载已保存槽。检查真实状态转换日志，不只比较截图是否非空。
- 屏幕矩阵：296×240/160 DPI、480×480/305 DPI、176×176 圆屏、320×480/240 DPI、800×480/160 DPI、480×800/320 DPI。圆角、安全边距和实际渲染尺寸见 [display-matrix.json](display-matrix.json)。
- 签名 AOT 前后台检查三轮：后台时钟停止、400 ms 内无新增渲染、音乐暂停；恢复渲染和音乐，音乐实例保持相同，见 [aot-lifecycle.json](aot-lifecycle.json)。
- 23 音效与六音乐的默认/S3 资源共 58 个文件，与原版逐字节一致，见 [audio-parity.json](audio-parity.json)。模拟器检查资源加载、音乐解码和缓冲，欠载为零；dummy 音频设备不构成真机听感结论。
- 本阶段保留了 ESP32-S3 全长 Opus 音轨与默认 Vorbis 音轨。后续已将两版、所有目标统一为原有小体积 Opus，详见 [统一音频报告](2026-10-10-compact-audio.zh-CN.md)；上面的 `audio-parity.json` 保留为此阶段的历史记录。

**复现命令**

从 PXA workspace 根目录执行：

```sh
bash tools/app.sh build pixel-dungeon --target simulator,esp32s3 --source-root local/pxa-apps --output local/cpp-game-ports-20261010/pixel-c-baseline
bash tools/app.sh build pixel-dungeon-cpp --target simulator,esp32s3,esp32s31 --source-root local/pxa-apps --output local/cpp-game-ports-20261010/pixel-all
PXA_CPP_GAME_TEST_BUILD="$PWD/local/cpp-game-ports-20261010/native-build" bash local/pxa-apps/common/cpp/tests/run.sh -DPXA_C_REFERENCE_BENCH=ON
local/cpp-game-ports-20261010/native-build/pixel_reference_bench
PXA_CPP_GAME_TEST_BUILD="$PWD/local/cpp-game-ports-20261010/asan-build" bash local/pxa-apps/common/cpp/tests/run.sh -DSANITIZE=ON
cmake --build build/simulator/pai-touch --target pxsys_voxel_resources_test --parallel
python3 local/pxa-apps/common/cpp/tests/reference/run_aot_probe.py --simulator-build build/simulator/pai-touch --artifacts local/cpp-game-ports-20261010 --output local/cpp-game-ports-20261010/aot-check
python3 local/pxa-apps/common/cpp/tests/reference/run_aot_probe.py --simulator-build build/simulator/pai-touch --artifacts local/cpp-game-ports-20261010 --output local/cpp-game-ports-20261010/lifecycle-check --lifecycle
python3 local/pxa-apps/common/cpp/tests/reference/run_aot_probe.py --simulator-build build/simulator/pai-touch --artifacts local/cpp-game-ports-20261010 --native-build local/cpp-game-ports-20261010/native-build --pixel-play --output local/cpp-game-ports-20261010/pixel-gameplay-check
python3 local/pxa-apps/common/cpp/tests/reference/run_aot_probe.py --simulator-build build/simulator/pai-touch --artifacts local/cpp-game-ports-20261010 --native-build local/cpp-game-ports-20261010/native-build --pixel-play --controller --output local/cpp-game-ports-20261010/pixel-controller-check
```

显示矩阵使用独立 simulator PXADB 服务，避免干扰默认实例。先构建两游戏的 `*-all` 及原生测试，再执行：

```sh
python3 tools/simulator_pxadb.py --state-root local/simulator/pai-touch@cpp-games --socket /tmp/pxa-simulator-1000/pai-touch@cpp-games.sock --installer build/simulator/pai-touch/pxsys_package_installer --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der --control-socket /tmp/pxa-simulator-1000/pai-touch@cpp-games.control.sock
# 保持服务运行，在另一个终端执行：
python3 local/pxa-apps/common/cpp/tests/simulator_matrix.py --artifacts local/cpp-game-ports-20261010 --native-build local/cpp-game-ports-20261010/native-build --output local/cpp-game-ports-20261010/matrix-check
```

设备固定种子包使用可选 `PD_BENCHMARK_SEED=1374496523`（即 `0x51ed270b`），普通包仍按时间生成地图。C 参考通过 `reference/prepare_pixel_device.py` 复制到独立目录，只增加相同的种子条件宏，保留原目录；完整复现见 [真机报告](2026-10-10-device.zh-CN.md)。通用采样器的 `--pixel-search` 会重置指定测试应用的私有数据，进入战士第一层、注入十二次搜索并校验真实提交的二十四回合存档。不能用于保留个人存档的普通安装。

**真机补充与限制**

用户已授权清理应用，pai-touch 已成功安装两款 C++ 游戏并轮换完成真机检查。使用同一固件/同次开机采集 C 和 C++ 的同场景数据；保留阅读器，不擦除数据分区或修改分区表。安装曾出现客户端等待超时，但随后确认签名包已经提交，清理本测试包的暂存容器后继续，没有重复覆盖安装或误判为损坏。

两版十二次搜索均落盘为二十四回合，六个最终存档哈希一致，真实显示 FPS 为 C 18.77 / C++ 18.80，整机稳定 PSRAM 减少约 577 KiB，初始化和稳态最大局部峰值也降低。此结果是完整游戏路径持平，未证明 Guest CPU 或 Host 光栅显著加速；全部轮次、命令/像素覆盖范围、局部堆统计和截图见 [最新报告](2026-10-10-device.zh-CN.md)。不同 DPI 下的整数缩放场景不能用于语言性能比较。

esp-mosaico 真机安装与音效主观听感仍未验收；六种尺寸/DPI/屏幕形状的实际签名 AOT 交互矩阵已通过。许可证与作者归属保留在 [LICENSE.txt](../LICENSE.txt) 和 [README](../README.md)。
