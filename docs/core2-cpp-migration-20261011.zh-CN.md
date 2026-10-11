# Core 2 与全应用 C++26 迁移

本轮范围：19 个生产应用，SDK C++ 示例另行全量检查。生产目录统一使用
CMake、C++26、O3 与 Core 2.0；保留现有 App ID、版本升级序列、资源及
存档格式。Jump Jump、Pixel Dungeon、Voxel Craft 的 C 生产包已移除，
需要的 C 对照源移至 common/cpp/tests/reference/c，不再发布。

## 调用方式

新的应用优先使用 Context、类型化服务、Task 与声明式 UI。复杂的既有
回调应用使用可选 pxa::call(stateless) 经 Application.cmake 接入同一 C++
transport，沿用经过验证的 C 格式 wire 编解码函数。目前不宣称这些应用
已经全面改写为声明式 UI；这样避免同时改变玩法、数据布局及默认内存。
两种路径的 PXA 导入均只有 Core 2 pxa_call / pxa_io，不含 pxa_submit；
使用 WASI 的组件另行显式声明 libc 导入和能力。
Device 示例展示的 AOT `engine_abi` 是独立引擎兼容标识；现有历史名称
含 `core-1`，不能当作 Core 协议版本。实际包和导入均验证为 Core 2。
本轮开始时已有的同步借用命令、64 位 token/handle 与命令队列削减
属于前一轮 ABI 重构，不计为本轮 C++ 迁移新增的性能收益。

## 已修复

- Jump / Pixel：帧序号统一由 Renderer 管理，WOULD_BLOCK 不再让下一帧
  永久 BAD_STATE；去掉 Pixel 多余的 8 字节计数器。背压重建最新画面，
  帧序号仅在接受后递增；新增实际命令序列回归。
- Garden：快速更新存档会重复提交同一个未完成 token；改为合并最新
  进度，完成后继续写入，提交及完成的临时背压在时钟事件重试。复用原
  1 字节状态，无队列、无分配；新增进度覆盖和两类背压回归。
- Voxel：写入、校验读取、加载读取的文件 close 错误参与成功判定，
  避免未成功提交的文件被宣布为存档成功。
- Maze Evil：输入 Canvas 的默认主题背景遮住了已经提交的 RGB565 Surface；
  直接创建透明输入节点，删除不需要的空 Canvas 帧、备用包缓冲及帧状态。
  补上生命周期处理，后台停时钟，恢复清空触摸并重置时间基准；Guest
  静态数据共减少约 200 B，无新增帧缓冲。模拟器 WAMR 动态稳定值
  231,617 → 231,618 B，本次峰值 246,420 → 246,421 B（均 +1 B）；
  有效线性区保持 475,136 B。代码元数据的微小差异如实记录，不能把
  静态字段缩减当成整个线性内存下降。
- Plane Shooter：菜单和战斗的 Canvas 环境改用 dp，修复高 DPI 菜单
  错位及触摸坐标不一致。只改变计算，无新增状态或缓冲。
- SDK：修正编解码入口残留的 Core 主版本宏，由 1 改为 2；实际导入
  原先已为 Core 2，避免开发者读取错误版本后选择错误路径。
- CMake：多架构使用同一个加锁的 Wasm 增量目录，避免三份重复编译。
  可选 PXA_COMPILER_CACHE=ccache；不影响运行时内存或优化等级。
- 回归分层：SDK 检查可独立于 app 仓库运行；玩法测试迁入 apps。
  资源生成工具同步产生 .cpp，避免下次生成恢复成 C 文件。

## 验证记录

验证原始日志和签名构建输出位于工作区 local/core2-migration-20261011。
下述记录区分启动覆盖、交互覆盖和真机性能覆盖；不以启动检查代替全部玩法验证。

### 最终检查清单

| 检查 | 结果与边界 |
| --- | --- |
| 全量签名构建 | 19 个生产包 + 21 个 SDK 示例，43 个组件；每个组件均有 Wasm / linux-x86_64 / ESP32-S3 / ESP32-S31 产物 |
| 导入与源码一致性 | 40 个包的当前 SDK/源码指纹、包 SHA256、显式 WASI 权限及 Core 2 call/io 导入均通过 |
| 模拟器启动/生命周期 | 120 组通过：296×240 / 160 DPI，480×480 / 305 DPI，176×176 圆屏 / 160 DPI；后台无新增 Surface 提交，恢复及关闭通过 |
| Host 原生回归 | 88/88 通过 |
| C++ SDK | 完整脚本通过，含 56 个主测试、诊断、动态国际化、三种任务池、直接调用与生成器检查 |
| C 格式 wire 编解码 | 完整脚本通过，含 Canvas BEGIN 后三类失败重试和跨语言布局 |
| 应用原生 | 23 次调用及 UI/i18n 检查通过，启用 ASan/UBSan；游戏公共层 11 项普通及 11 项 sanitizer 对照通过 |
| Voxel 原生 | ASan/UBSan 下世界、树冠、UV、深度、边界、编辑重建、零帧分配及存档错误回归通过 |
| 资源集成 | 19/19：Plane 5、Store 4、Pixel 音频 1、Resource Scenes 9；含低预算、慢 I/O、后台加载、取消、退出及预算归零 |
| Jump / Pixel 交互 | 6 种屏幕配置 × 2 个游戏，共 12 组通过；覆盖蓄力或角色创建、背包、设置、存档恢复 |
| 构建与调试工具 | 工具 32 项、PXADB 66 项、CMake/WASI 完整流程及分配器正反例通过 |
| pai-touch | 同负载渲染各三轮、控制路径三轮、Voxel 工具/配方/存档恢复通过；原固件与原包已恢复 |

[最终产物、导入与源码指纹](core2-cpp-migration-20261011/final-audit.json)，
[应用矩阵](core2-cpp-migration-20261011/apps-matrix.json)，
[SDK 示例矩阵](core2-cpp-migration-20261011/sdk-matrix.json)
（[小圆屏截图](core2-cpp-migration-20261011/sdk-startup-round.png)），
[资源集成](core2-cpp-migration-20261011/resource-integration.json)，
[真机渲染对照](core2-cpp-migration-20261011/matched-render.json)。
真机测试的 C++ benchmark 与最终生产包的 Wasm、S3 AOT 逐字节一致，
SHA256 一并记录在最终产物审计中。

截图：[普通屏](core2-cpp-migration-20261011/apps-startup-touch.png)、
[高 DPI](core2-cpp-migration-20261011/apps-startup-dense.png)、
[小圆屏](core2-cpp-migration-20261011/apps-startup-round.png)、
[Garden 三屏](core2-cpp-migration-20261011/garden-dpi.png)。
显示缺陷的实际前后截图：[Maze Evil](core2-cpp-migration-20261011/maze-evil-touch-before-after.png)、
[Plane 高 DPI](core2-cpp-migration-20261011/plane-shooter-dense-before-after.png)。
ABI smoke 本来没有可见 UI，渲染基准首阶段为纯色负载；不能根据这些
截图的纯色区域判断没有执行。Surface 与 GameRender 的计数分别记录，
避免原始 Surface 的正常输出被错误统计为 GameRender 的 0 FPS。

### 分配器与 DPI 回归

手动回调组件默认显式 `NO_ALLOCATOR`；Lab、WASI Lab 使用 `ALLOCATOR`
保留动态分配检查。SDK 其余组件的默认行为不变。链接检查覆盖无分配器
包、意外 malloc 的拒绝，以及正常分配器导出，防止两套 Guest 堆重叠。

Canvas 的环境宽高是 Surface 像素，绘制及指针坐标是 dp；新增显式
Canvas 换算，safe insets 已是 dp，不能再次缩放。Garden 标题菜单改为
同一组布局常量控制绘制与命中，覆盖 480×480 / 305 DPI 与 176×176。
Arcade 菜单使用既有 safe insets。旧 Garden 棋盘等固定尺寸玩法在极小
圆屏上的完整重排仍属限制，启动通过不代表所有内容都适配。

Canvas 复用的 wire helper 也补上同类背压修复：Host 接受 BEGIN 后即消耗
frame generation，即使 WRITE、流写或 PRESENT 失败也必须使用新编号。
原实现会在这些失败后反复重试旧编号，导致永久 BAD_STATE。新增三种失败
注入回归；只提前更新已有计数器，不增加字段或缓冲区。

## 真机同负载对照

pai-touch 的实际枚举端口为 `/dev/ttyACM1`，已核对序列号
`a4:cb:8f:d6:8d:a4`。相同 Core 2 固件启动周期、296×240 显示（148×120 渲染、2× 呈现）、
相同 11 组 GameRender 命令与画质，C/C++ 各连续运行三次。C++ 先运行，
C 后运行；以下内存范围仅取双方预热后的第 2、3 次。首次初始化另保留
原始记录，不能混入稳定峰值对比。

| 项目 | 冻结 C 基线 | 最终 C++26 / O3 |
| --- | ---: | ---: |
| 11 组实际显示 FPS | 30 | 30，极个别窗口边界计为 31 |
| GM-3D Host 光栅化平均耗时 | 12 ms | 12 ms |
| 2D-TX Host 光栅化平均耗时 | 12 ms | 13 ms |
| SRAM 稳定占用 | 267,216 B | 267,216 B |
| SRAM 本次峰值 | 268,512 B | 268,512 B |
| PSRAM 稳定占用 | 1,856,864–1,856,884 B | 1,799,124–1,799,336 B |
| PSRAM 本次峰值 | 2,109,956–2,110,376 B | 2,052,632–2,052,712 B |
| Wasm 声明最小线性内存 | 128 KiB | 64 KiB |
| WAMR 实际 Guest 线性内存 / 本次峰值 | 100 / 100 KiB | 44 / 44 KiB |
| WAMR 动态占用 / 峰值（不含线性内存） | 21,282 / 21,286 B | 21,476 / 21,480 B |
| ESP32-S3 AOT 文件 | 49,476 B | 47,332 B |
| AOT 代码映射页容量（分配器开销另计） | 65,536 B | 65,536 B |
| 帧缓冲 / depth-scratch / mailbox | 106,560 / 35,520 / 24,576 B | 相同 |

显示已受 30 FPS 上限约束，结论是本组负载显示性能持平、内存下降，
不能推导无上限吞吐提升。Host 光栅化遥测精度为整数毫秒，2D-TX 的
1 ms 差异如实保留，不能据此分离语言或调用开销。全程 PERF 缓冲只有
256 条，已溢出，因此不把它当作完整的逐帧排队/显示/Guest 分阶段数据。
应用提交 FPS 不参与这一结论。

`NO_ALLOCATOR` 避免不用堆的回调包因 malloc/free 导出而拉入 libc 分配器；
显式 8 KiB Guest 栈只应用于静态数组、非递归且经过 11 组真机检查的
benchmark。SDK 默认栈、其他游戏的栈均未缩小；动态分配应用继续使用
统一分配器。稳定 PSRAM 下降约 56 KiB、真实峰值下降约 56 KiB，并非
拿历史全局最低空闲量作为本次峰值。WAMR 线性区来自实例的有效地址范围，可小于 Wasm 声明页数；
物理总占用由 heap 计数核对，不把地址范围、页预留和映射重复相加。
C++ 的 WAMR 动态元数据增加 194 B，已包含在总 PSRAM 里，没有隐藏。
AOT 文件大小与映射页、WAMR 元数据
和堆占用属于不同口径，不把它们直接相加。

三轮控制路径测量的每次调用中位数：启动阶段约 10.5–10.8 μs，
稳定阶段约 10.5–10.6 μs，错误为零，transport 对象 16 B。这些不是游戏
帧率，也没有可比的旧 ABI 同负载延迟记录，故不宣称提升百分比。

## 设备与画面

测试使用私有 App ID。原有 5 个包、版本和用户存档均未覆盖。结束后移除
私有包及其私有存档；原应用分区镜像恢复并通过 Flash digest 校验，
原 Counter 的启动、截图及停止也通过。未修改亮度、音量、DPI 或状态栏
设置；未刷写 bootloader、分区表或数据分区。

Garden 菜单的密度换算和命中区域一致，极小屏幕使用短标签避免截断；
旧固定棋盘等复杂游戏内容在极小圆屏上的完整重新布局仍未完成。
Plane 菜单仍采用 296 dp 设计宽度，可用宽度不足时有边缘裁切；本轮
修正像素与 dp 混用，没有把全部固定设计重新制作成响应式界面。
部分 SDK 演示界面的长标题和纵向控件在 176×176 圆屏上仍有裁切；
三屏矩阵通过仅表示激活/运行/生命周期正确，不代表所有演示布局已完整
适配。现有 SafeArea、滚动和条件布局能力可按界面继续应用。
Resource Scenes 修复小于 240 像素的屏幕创建过大渲染面导致的启动失败，
缩小同一组四纹理显示；常规 240 像素基准保持原负载。场景/重试计数器
收窄后容纳新的 2 字节 extent，状态字节总量减少，不增加默认缓存。
嵌入式模拟器入口与正式系统外壳使用同一 Host 路径，以覆盖阅读器及
system-input 所需的系统文字输入能力。独立 SDL 窗口不提供该能力，
其激活拒绝是原先测试配置问题，不能通过删除应用必要能力来掩盖。

Pixel 音频集成测试已改为真实菜单输入创建角色，检查 DESCEND 按需
准备及 PCM voice 的消费位置，不再等待已取消的启动音效预加载；
同时验证 Ogg READY 后渐入、后台暂停、恢复播放及测试私有存档清理。
此前失败日志保留在工作区 resource-integration-before-test-port。

Voxel 的透明叶片、深度、UV、世界生成及存档校验继续由原生和游戏测试
覆盖；本轮没有降低视距、画质或移除深度。Jump / Pixel 的 6 屏交互各
通过一次，含蓄力/起跳或角色创建、背包、暂停设置、保存及重新加载。
Voxel 真机六种工具/配方、错误工具、消耗、保存、音效和恢复检查通过。
音效检查验证触发及播放链路，不等同主观音质评价。

## 可复现入口与限制

在工作区根目录执行：

```sh
CC=clang bash deps/pxa-system/tools/package/test_guest_sdk.sh
CC=clang CXX=clang++ bash deps/pxa-system/tools/package/test_guest_cpp.sh
SANITIZE=1 bash local/pxa-apps/tests/native/run.sh
SANITIZE=ON bash local/pxa-apps/common/cpp/tests/run.sh -DPXA_C_REFERENCE_BENCH=ON
bash local/pxa-apps/voxel-craft-cpp/tests/test.sh
PXA_BUILD_CACHE_DIR="$PWD/local/core2-migration-20261011/build-cache" PXA_COMPILER_CACHE=ccache \
  python tools/test-app-matrix.py --output local/app-matrix --target all --sdk-examples \
  --probe local/abi-audit-20261011/simulator/pxsys_app_matrix_test
# 资源、音频、低预算、慢 I/O、前后台和加载中退出：
bash tools/test-pxa-resources.sh
# 先在测试固件上安装隔离 App ID 的相同负载包，随后执行：
python tools/measure-device-render-bench.py --port /dev/ttyACM1 \
  --app pxa-core2-render-check --package /path/to/check.pxa --repeat 3 --output /path/to/result
```

全架构签名构建覆盖 Wasm、linux-x86_64、esp32-s3、esp32-s31；实际硬件
仅 ESP32-S3。网络搜索/商店的真实互联网服务、传感器、电池等没有逐项
真实硬件端到端验收。所有应用都有本次模拟器线性内存/WAMR 稳定值与
进程内峰值，但不宣称已经得到每个应用完整玩法的真机前后内存对照。

C++ SDK 及回调桥已统一 Core 2，wire helper 保留 C 格式；源代码的 C++
迁移不等同所有应用已经全面采用协程、类型化资源和声明式 UI。其余
协议编码器可以继续按模块重写，前提是保留线格式、热路径分配和相同
工作负载证据。生产应用没有 C 编译单元，保留的 C 文件仅为测试对照。
