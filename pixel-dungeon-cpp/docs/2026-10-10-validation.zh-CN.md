# Pixel Dungeon C++：实现与验证（2026-10-10）

独立 C++ SDK 版已完成，保留同仓库 C 版的 25 层、五个地区、三种职业、五槽存档、装备、背包、地图缩放、日志、排行榜、触控/手柄规则、23 种音效与六首完整音乐。采用 O3，没有为包体积削减玩法或音轨。原生和签名 AOT 模拟器验证已通过；真机 C++ 显示性能、音效听感及 SRAM/PSRAM 峰值仍未验收。

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

**签名 AOT 与内存**

真实桌面 AOT 冷启动，296×240 中文标题，预热 3 秒，三次独立进程。标题静态时不重复绘制，这是按需渲染行为，不能用其空闲 FPS 评价游戏性能。原始值见 [aot-performance.json](aot-performance.json)。运行峰值计数随每个进程重新建立，不使用历史累计峰值。

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

生产 Guest 保留 49,152 B 绘制缓冲、五个 1,024 B 存档槽、1,024 B 序列化缓冲、1,152 B 存储报文、1,044 B 上传暂存及两个小型效果图，未新增大型默认缓存。设备代码映射、初始化峰值、稳定 SRAM/PSRAM、帧缓冲及命令队列真实值仍待安装后实测，不能仅根据这些静态预算宣告满足总峰值约束。

**自验证结果**

- 联合原生套件 **7/7**：游戏规则、楼层生成、背包/装备/存档、布局、真实光栅器、SDK helper 与 C 参考比较。ASan/UBSan **5/5**，Release 断言启用。
- 签名 AOT 模拟器的两游戏六屏幕矩阵 **12/12**；本应用六组均完成标题→存档槽→职业→进入游戏→等待→背包→暂停→设置→返回标题→退出进程→同一私有目录重新启动→加载已保存槽。检查真实状态转换日志，不只比较截图是否非空。
- 屏幕矩阵：296×240/160 DPI、480×480/305 DPI、176×176 圆屏、320×480/240 DPI、800×480/160 DPI、480×800/320 DPI。圆角、安全边距和实际渲染尺寸见 [display-matrix.json](display-matrix.json)。
- 签名 AOT 前后台检查三轮：后台时钟停止、400 ms 内无新增渲染、音乐暂停；恢复渲染和音乐，音乐实例保持相同，见 [aot-lifecycle.json](aot-lifecycle.json)。
- 23 音效与六音乐的默认/S3 资源共 58 个文件，与原版逐字节一致，见 [audio-parity.json](audio-parity.json)。模拟器检查资源加载、音乐解码和缓冲，欠载为零；dummy 音频设备不构成真机听感结论。
- 原版 C 的 ESP32-S3 全长 Opus 音轨与默认 Vorbis 音轨分别保留。多目标打包的资源随主目标选择，按设备单独构建；不能假定多架构包里一定是 Vorbis。

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
```

显示矩阵使用独立 simulator PXADB 服务，避免干扰默认实例。先构建两游戏的 `*-all` 及原生测试，再执行：

```sh
python3 tools/simulator_pxadb.py --state-root local/simulator/pai-touch@cpp-games --socket /tmp/pxa-simulator-1000/pai-touch@cpp-games.sock --installer build/simulator/pai-touch/pxsys_package_installer --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der --control-socket /tmp/pxa-simulator-1000/pai-touch@cpp-games.control.sock
# 保持服务运行，在另一个终端执行：
python3 local/pxa-apps/common/cpp/tests/simulator_matrix.py --artifacts local/cpp-game-ports-20261010 --native-build local/cpp-game-ports-20261010/native-build --output local/cpp-game-ports-20261010/matrix-check
```

安装空间足够后，设备专用包及采样命令为：

```sh
bash tools/app.sh build pixel-dungeon-cpp --target esp32s3 --aot-only --source-root local/pxa-apps --output local/cpp-games/pixel-device
python3 tools/pxadb/pxadb.py package install local/cpp-games/pixel-device/pxa-pixel-dungeon-cpp.pxa --port /dev/ttyACM0
# 先确保没有其他活动应用。菜单中点击进入固定场景后再采样。
python3 tools/measure-device-voxel.py --port /dev/ttyACM0 --app pxa-pixel-dungeon-cpp --output local/cpp-games/pixel-capture --warmup 5 --seconds 4 --repeat 3 --local-heap --unlock-swipe 148 218 148 60
```

上述通用设备采样器虽然文件名含 voxel，也支持其他游戏；Pixel 静态标题不会产生持续帧，需先准备固定移动/动画场景。不能把它的标题采样失败当作 FPS 为零。记录内部目标尺寸、种子、角色、回合、相机与音频状态；C/C++ 两版保持一致。

**尚未完成的真机验收**

pai-touch 数据分区首次诊断只剩 471,040 B，复位后最新为 1,110,016 B；仍小于本应用 ESP32-S3 容器的 1,651,136 B，且安装还需解包空间。esp-mosaico 上传 Jump C++ 包约 90% 后返回 `file_write_failed`，旧 Host 未返回 errno，空间不足只是推断。两个 C++ 游戏尚未成功在真机安装，未宣称 FPS ≥ C 版。保留全部已有应用与数据，没有通过卸载或清空存储绕过空间限制。

pai-touch 已做授权范围内的应用分区固件更新，增加空间/写错误诊断和 Host 光栅优化；未改分区表或数据分区。固件及原 C Jump 的真实显示基线详见 [Jump 报告](../../jump-jump-3d-cpp/docs/2026-10-10-validation.zh-CN.md)。仍需在足够安装空间下验证实际显示 FPS、更新/编码/提交/光栅/排队/显示耗时、持续音频、冷/热峰值、前后台及重复启动。许可证与作者归属保留在 [LICENSE.txt](../LICENSE.txt) 和 [README](../README.md)。
