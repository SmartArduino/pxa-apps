# recrof 引擎的 PXA Core 2 移植

四个上游仓库对应五个独立应用：`quake`、`quake2`、`duke3d`、`command-conquer` 和 `red-alert`。后两者共享 Vanilla-Conquer 源码。每个应用的 `upstream.json` 固定了移植使用的上游提交。引擎源码和授权文件保留在各自目录中。

## 构建

从 pxa-projects 根目录执行。需要仓库的 PXA SDK、WASI SDK 34、wamrc；首次下载 Binaryen 130：

```sh
local/pxa-apps/common/recrof/tools/setup.sh
tools/app.sh build quake-sd --board doit-gamekit --source-root local/pxa-apps --target esp32s31 --output local/recrof-build/final
```

把 `quake-sd` 替换成其他应用名即可。GameKit 是 ESP32-S31 / RISC-V32，须使用 `esp32s31`；ESP32-S3 使用 `esp32s3`，两种 AOT 不能混用。`--target wasm` 生成便携 WASM 包；逗号分隔目标可合并多架构 AOT。输出文件为 `pxa-<应用名>.pxa`，使用本工作区的开发发布者密钥签名。设备必须信任对应发布者。

CMake 在链接后使用 Asyncify 将原引擎的阻塞主循环转换成可暂停的 PXA 事件回调。宿主只接收标准 PXA Core 2 导入；Asyncify 导入在构建时消除。SD 变体另外要求支持 FS 0.3 `app-media-root` 的固件；固定映射缓冲也要求宿主正确实现 pinned linear memory 的原地增长。

## 导入资源

引擎源码不包含商业游戏资源。为本地测试导入的数据不会加入 Git。请使用自己有权使用的版本，重新分发应用包前核对资源授权。

```sh
python3 local/pxa-apps/common/recrof/tools/import_data.py quake /path/to/pak0conv.pak
python3 local/pxa-apps/common/recrof/tools/import_data.py quake2 /path/to/Quake2
python3 local/pxa-apps/common/recrof/tools/import_data.py duke3d /path/to/DUKE3DCONV.GRP
python3 local/pxa-apps/common/recrof/tools/import_data.py command-conquer /path/to/CNC-DOS-data
python3 local/pxa-apps/common/recrof/tools/import_data.py red-alert /path/to/RedAlert-data
```

导入后重新构建。只读资源放在签名的 `assets/game` 中，通过 Assets 2 分块读取，不占用应用私有 FS 的 8 MB 配额。设置和存档写入私有 `data` 目录。导入器统一小写文件名，并检查大小写冲突和符号链接。

SD 变体只有代码，通过固定媒体根读取资源，不对缺失文件查询包内 Assets。
两页共享缓存合并小于512字节的只读 SD 请求，按打开的句柄隔离，关闭即失效；
大块顺序读取直接走 READ_AT，私有文件不缓存。C&C/RA SD 的 DOS 虚拟路径统一
ASCII 小写，包括私有设置和存档的写入，避免为每个归档成员重复扫描目录。
普通变体保留原有大小写解析。替换 SD 游戏资源后应重新打开游戏。

C&C SD 还复用最多两个空闲的只读 MIX 归档句柄，重开时恢复零游标并保留请求
cookie。空闲500ms后由Clock关闭；私有命名空间变化会使旧的覆盖层判断失效，
读取报错的句柄不复用。这样不必为每个归档成员重新执行打开/关闭协议。

Quake 必须使用上游 MG24Quake 2.0 转换器生成的 PAK；普通 PC PAK 不能直接使用。转换器源码位于 `quake/tools/MCUPackConverter`，在有 `pak0.pak` 的目录运行转换器会生成 `pak0conv.pak`。目前只接入 `pak0.pak`，未验证完整注册版的其他 PAK。转换器可在 Linux 下编译：

```sh
cc -O2 local/pxa-apps/quake/tools/MCUPackConverter/*.c -lm -o /tmp/pxa-quake-pakconvert
# 在包含原版 pak0.pak 的目录运行 /tmp/pxa-quake-pakconvert
```

Duke3D 必须先转换 GRP：

```sh
python3 -m pip install mido
python3 local/pxa-apps/duke3d/tools/PythonCVT/mcugrpcvt.py DUKE3D.GRP DUKE3DCONV.GRP
```

Quake II 使用标准 `baseq2/pak0.pak`。C&C 可导入 DOS 试玩版的 DEMO.MIX、DEMOL.MIX、SOUNDS.MIX、SPEECH.MIX，或完整零售版资源；DOS/试玩版自动使用低分辨率。Windows Gold 需要自己的 CCLOCAL.MIX 等完整配套资源。Red Alert 使用 REDALERT.MIX 和战役 MAIN.MIX，或完整的展开资源；默认采用 Windows 素材模式并缩放输出。不要把 Windows 的 CCLOCAL.MIX 直接重命名成 DOS 的 LOCAL.MIX。仅有 OpenRA 的最小联机资源集不能作为完整单人战役的验收依据。

## 操作

射击游戏：左半屏拖动控制前后移动和横移；右半屏拖动转向；按钮 MENU 打开菜单，WPN 切武器，USE 使用，JMP 跳跃，FIRE 射击。菜单内用左侧上下拖动选择，FIRE 确认。手柄方向键移动，A 射击/确认，B 跳跃，START 菜单，SELECT 回车，X 使用，Y 切武器。

策略游戏：触摸映射鼠标左键，带右键标志的指针映射右键；右上角 OK 确认任务/简报，RMB 按钮在上一个游戏光标位置执行右键，便于触屏取消选择。策略游戏的原有阻塞键盘等待会主动让出执行权，以便接收后续 PXA 输入事件。当前没有联机功能。

GameKit 手柄使用 PXA 的 controller 0。普通版与 SD 版共享以下映射：

| 按键 | Quake / Quake II / Duke3D 游戏内 | 射击游戏菜单 | C&C / Red Alert |
| --- | --- | --- | --- |
| 方向键 | 前进、后退、左右转向 | 导航 | 移动光标 |
| A | 射击 | 确认 | 左键，按住并移动可拖选 |
| B | 跳跃 | 返回 | 右键、取消选择 |
| X | 使用（按引擎原有绑定） | — | 回车、确认简报 |
| Y | 下一武器 | — | H，回到基地 |
| L / R | 左右平移 | — | L + 方向键滚动地图，R 加速光标 |
| START | 打开菜单 | 返回 | Escape、菜单 |
| SELECT | 回车、引擎原有功能 | 回车 | 回车 |

方向移动由 Clock 按经过时间累计，重复的完整状态刷新不重复按下，不重新开始
计时。长时间阻塞每次最多累计64ms，避免恢复时突然转向或跳动。进入或离开
射击游戏菜单会释放旧动作；当时按住的键须松开后再按。切后台、环境变化和
controller 0 断开会释放输入；其他编号不会抢占这一单人席位。触摸和手柄可以
并用，某一来源松开不会释放另一来源仍按住的动作。策略游戏的手柄点击使用
游戏光标位置，不会被触摸 RMB/OK 覆盖层改写。

目前 SDK 的控制器事件只有12位数字按键，没有模拟轴、扳机强度或震动数据；
这里不将其描述为模拟摇杆支持。SELECT+START、SELECT+X、SELECT+L/R 等
GameKit 系统组合键由板卡先处理，保留原有桌面、鼠标与音量操作。

手柄策略与 RTS 适配器的原生 ASan/UBSan 测试：

```sh
bash local/pxa-apps/common/recrof/tests/run_controller.sh
```

## 平台适配与限制

Quake另有默认关闭的`PXA_QUAKE_LIT_CACHE`性能实验：在变体的`CMakeLists.txt`
中于`add_subdirectory(../quake ...)`前设置该选项为`ON`，可测试保持逐像素相同
结果的光照系数缓存。短跨度或纹理行变化快时走原内核，不增加堆缓存。先运行
`python3 local/pxa-apps/common/recrof/tests/run_quake_lit_span.py`；该测试从当前
vendor提取参考算法，检查20万组跨度并运行ASan/UBSan。桌面热点收益不代表
设备FPS，尚未通过相同场景设备A/B前保持默认关闭。

- 软件绘制、RGB565 双缓冲 guest-mapped Surface；以 320×240 为逻辑屏幕，按显示器尺寸缩放。
- PXA Clock 驱动，独立的引擎 C 栈与 Asyncify 续体栈，后台暂停并释放按键。
- 可选 `audio.playback` 权限，PCM 混音后输出 16 kHz 单声道；拒绝权限仍可游戏。Duke 已在真机开启音乐和音效并核对设置，持续输出与欠载情况仍需分别量化。
- 标准 stdio/POSIX 调用桥接到私有 FS 与签名资源，虚拟 Flash 使用文件保存 NOR 数据。
- 不使用 ESP32 的 GPIO、SDMMC、DMA、FreeRTOS 或 Flash 直映射驱动。
- 目前清单的线性内存上限：Quake 3 MiB、Quake II 12 MiB、Duke3D 4 MiB、C&C 10 MiB、RA 14 MiB；另需宿主、AOT 和显示内存。SD 变体另有独立上限：Quake 3.5MiB、Quake II 10.375MiB、Duke3D 3.5MiB、C&C 7MiB、RA 9.5625MiB；实际场景与设备装载资格见 `VALIDATION.md`。不能据此承诺在 8 MiB PSRAM 的设备上均可运行。大型资源包还需要足够的存储空间。
- WAMR 无 Wasm 异常处理，原引擎 setjmp/longjmp 错误恢复改为显示错误并等待重新打开应用。正常主循环不依赖 longjmp。
- `duke3d-sd` 已在 16 MiB PSRAM 的 GameKit / ESP32-S31 上自动读取固定 SD 资源并进入屋顶关卡，完成短时 FPS 与内存测量；其他游戏、音频连续性及长期稳定性仍待真机验收，见 `VALIDATION.md`。

## 同步上游

上游引擎、转换器和许可证的映射集中在 `upstreams.json`，PXA 平台实现单独维护。Duke SD 复用 Duke 引擎；C&C 与 RA 共用引擎并一起更新两个版本锁。无需复制第二份源码。

在工作区根目录执行：

```sh
python3 local/pxa-apps/common/recrof/tools/upstream.py check all
python3 local/pxa-apps/common/recrof/tools/upstream.py prepare duke3d --output /tmp/duke-update
# 审阅 plan.json、files/ 以及 adaptations.patch。
# 有冲突时编辑 files/ 内的文件，再登记解决结果：
python3 local/pxa-apps/common/recrof/tools/upstream.py resolve /tmp/duke-update duke3d/vendor/Game/src/control.c
python3 local/pxa-apps/common/recrof/tools/upstream.py apply /tmp/duke-update
```

`check` 只查询远端提交。`prepare` 缓存 Git 对象，用锁定的旧上游、本地 PXA 版本和目标上游做三方合并，输出审阅目录；它不修改工作区，也不执行下载的代码。默认目标为远端 HEAD，`--ref` 可指定完整提交 SHA；指定 SHA 且缓存已有对象时可以离线准备。全局 `--cache` 可以复用已有仓库，例如 `--cache local/upstream`。

冲突不会自动覆盖，二进制冲突也需明确解决；`resolve --take local` 或 `--take upstream` 可明确选择一边。`apply` 在写入前校验本地与候选哈希、文件模式、路径及全部冲突状态，拒绝过期方案，对普通写入异常回滚，并最后更新版本锁。它不自动提交或推送。合并后检查 Git diff，重新构建并做真机回归；上游资源格式变化时须重新运行转换器。干净合并仅说明源码合并成功。

2026-10-19 检查四个上游 HEAD 均与当前锁定提交一致；四套真实映射的无更新准备结果均为 0 个修改、0 个冲突，现有适配补丁已可单独导出。测试覆盖新增、删除、局部适配保留、文本/二进制冲突、过期编辑、符号链接/越界路径、模式变化和失败回滚。

## 测试工具

`PXA_RECROF_MAIN_LOOP=ON` 选择实验性的 Core 2.1 事件端口路径。包须声明至少 `[2,1]`，且唯一的 `main` UI Component 须包含 `"execution_model":"main-loop"`；Host 必须支持该执行模式。引擎在现有运行线程保留调用栈，`port_yield` 等待普通事件并直接分发，不使用 Asyncify 进口或续栈缓冲。RPC 等待期间仍处理输入和生命周期；后台暂停游戏，停止或替换由 Host 终止整个调用。默认仍为回调路径。此开关本身不证明真机帧率或完整音频质量。

`tests/host.mjs` 是协议级 Node 22 测试宿主，执行真实引擎并读取真实资源；它不能代替正式 PXA 模拟器。原生主循环包可设置 `PXA_TEST_MAIN_LOOP=1`，在同一文件、输入和后台测试中使用事件端口。`tests/simulator.py` 启动正式签名包、捕获运行日志和截图，检查异常、空白画面与正常 HOME 退出。`tests/fixture` 验证暂停期间的 C 栈对象、C++ 字符串、虚拟文件和路径解析。资源导入单元测试：

```sh
python3 -m unittest discover -s local/pxa-apps/common/recrof/tests -p 'test_*.py'
```

详细验收状态见同目录 `VALIDATION.md`，源码改动见 `PATCHES.md`，测试资源来源见 `TEST_DATA.md`。

Quake 的门和开关沿用原作自动触发机制，USE 按钮在 Quake 中没有独立动作。

策略游戏的资源版本说明参考上游 [VanillaTD 安装说明](https://github.com/TheAssemblyArmada/Vanilla-Conquer/wiki/Installing-VanillaTD) 和 [VanillaRA 安装说明](https://github.com/TheAssemblyArmada/Vanilla-Conquer/wiki/Installing-VanillaRA)。


## 固定 SD 资源与真机性能目标

`quake-sd`、`quake2-sd`、`duke3d-sd`、`command-conquer-sd`、`red-alert-sd`
自动使用 `/PXA/media/<AppKey>/`，无需选择目录。资源仍先按 `TEST_DATA.md`
导入到对应原版应用的 `assets/game`；SD 变体的包只携带引擎。私有配置与存档使用
系统提供的应用私有目录，随应用所在存储卷分配，也适用于仅有 SD 存储的设备。

先查看导入计划；实际传输只指定设备，不填写目录：

```sh
python3 local/pxa-apps/common/recrof/tools/stage_sd.py quake
PXADB_BAUD=2000000 python3 local/pxa-apps/common/recrof/tools/stage_sd.py quake --deploy \
  --port /dev/ttyACM0 --expected-serial 1c:29:04:d0:af:5e \
  --pxadb-dir /absolute/path/to/pxa-projects/tools/pxadb
```

默认使用开发根公钥 `.dev-signing/publisher-public.der`，发布身份可用
`--publisher-key` 指定根公钥 DER；有公钥轮换时应使用签名谱系的根公钥。
AppKey 包含发布身份与应用 ID，两个应用或发布者不会自动共用资源目录。
传输使用 PXADB 的完整 SHA-256 校验，重复导入相同文件可直接验证；资源导入不会发布应用。

本轮最低性能目标调整为 **真实设备、实际关卡至少 15fps**，保留原画质、音乐和音效。
分别记录稳定窗口的中位数、平均值、最低值和异常，加载或菜单不作为关卡 FPS。
构建与协议宿主运行均不能代替真机验收；各游戏结果与未完成项见工作区 `docs/validation`。
SD 变体优先协商 Clock 0.3 的 8000us，设备不支持或配置失败时回退合法的 16ms。
后台取消、回到前台恢复；周期表示调度请求，不能据此直接推断游戏帧率。

## GameRender 1.0 原生 span 试验

Quake 可通过 CMake `PXA_QUAKE_HOST_SPANS=ON` 试用同步的原生 INDEX8
光照 span；默认关闭，首版真机关卡测得性能回退，尚未达到 20fps 目标。
该路径只借用本次调用的 Guest 内存，不额外复制纹理、帧缓冲或深度缓冲。
失败时在原表面状态仍有效时回放原渲染器；显示仍沿用 GuestMapped Surface。

`PXA_QUAKE_STATIC_GLOBALS` 直接使用固定状态对象地址，真机关卡达到约 6.9fps，现默认启用。SDK 默认编译器
没有被替换；测试包通过私有构建配置选择已做边界检查的对齐编译器。

真实上游内核的像素差分检查（需要工作区中的新 SDK）：

```sh
python3 local/pxa-apps/common/recrof/tests/run_quake_host_spans.py
```

测试直接提取当前 vendor 的 lit-span 宏，覆盖 20 万组、40 万条 span、
八种内核、负坐标与重叠写入。它验证新内核，不替代完整场景的性能或画质验收。

Quake II SD 0.1.13 的线性内存上限调整为 163 页（10.1875MiB），
与 GameKit 16MiB PSRAM 的已验证装载配置一致；引擎 arena 仍为 7.5MiB。
Quake 另做过渲染相关文件的单精度常量试验；中位数仍为 6.9fps，未纳入公开构建配置。


GameRender投影试验：`PXA_QUAKE_PROJECTED_SPANS` 默认关闭。直接运行
`python3 common/recrof/tests/run_quake_projected_spans.py` 可将Host投影、
光照和深度输出与当前vendor函数对比（需主工程deps/pxa-system）。
GameKit实际出生点私有候选中位3.9fps，低于6.9fps默认基线；不依据合成吞吐
启用该选项。公开构建继续使用Guest原路径，最低15fps及期望20fps仍待后续优化。


## GameRender 1.2 跨材质批次

`PXA_QUAKE_FRAME_SPANS` 使用已有Surface后缓冲保存命令与材质快照，
世界绘制完成后同步执行，再覆盖为RGB565显示帧。Host不新增纹理、帧或深度堆缓冲。
`PXA_RECROF_HOST_BLIT` 可单独将INDEX8颜色转换及透明光标交给Host；保持原缩放规则。
两项都是默认关闭的实验选项，需要GameRender 1.2。不能把原生计算测试的吞吐当成游戏FPS。

`PXA_QUAKE_FRAME_BULK_COPY` 仅作用于整帧批次的大块资源快照，使用WASM `memory.copy`；
没有给整个引擎开启bulk-memory编译开关。可关闭此选项复现普通memmove候选。
上游原函数继续保留，原生批次拒绝时按捕获状态整体回退。

```sh
python3 local/pxa-apps/common/recrof/tests/run_quake_frame_adapter.py
PXA_QUAKE_TEST_BULK_COPY=0 python3 local/pxa-apps/common/recrof/tests/run_quake_frame_adapter.py
python3 local/pxa-apps/common/recrof/tests/run_host_indexed_blit.py
```

第一项在WASM中使用当前上游Rows函数验证来源复用后的回退，第二项覆盖非bulk配置，
第三项使用原`port_blit`作参考。主工程的`tools/game-perf/profiles`保留独立比较配置。

`PXA_RECROF_BULK_READ_COPY` 仅给至少128字节的文件及虚拟flash读取使用WASM
`memory.copy`；小型元数据保留普通复制。`PXA_RECROF_PACKED_GAME_CODEC` 启用SDK
的可选WASM布局编码：固定记录的大小与所有字段偏移必须在编译期逐项匹配wire布局，
然后整块编码；其他平台继续使用逐字段编码。两项默认关闭，按游戏实测决定启用。
没有增加缓存或扩大线性内存，也没有为整个引擎启用bulk-memory。

```sh
python3 local/pxa-apps/common/recrof/tests/run_read_copy.py
PXA_QUAKE_TEST_PACKED_CODEC=1 python3 local/pxa-apps/common/recrof/tests/run_quake_frame_adapter.py
```


2026-10-20 GameKit 同固件 START 出生点 60 秒比较：默认 Guest 路径中位 6.9fps、
平均 7.017fps；批次、原生输出、批量读取及编码组合中位 6.9fps、平均 6.817fps。
组合较早期 5.9fps 候选收回部分开销，仍未证明优于默认路径，故新选项继续默认关闭。
最低 15fps 和期望 20fps 均未达到。公开 Quake/SD 源码为 0.4.5（seq125），
本轮上机的是私有比较包，不把公开源码版本视为已验收的二进制发布。


## 同一 AOT 的私有渲染诊断

`PXA_QUAKE_RENDER_PROBE` 默认关闭，仅在整帧批次、原生输出和帧统计同时启用时可用。
它在显示提交完成后读取 `/data/pxa-render-probe-mode` 单字节文件：
0 为 Guest 路径，1 仅原生输出，2 仅原生世界批次，3 两项启用。
模式切换不发生在借用命令执行期间；非法或未完成写入保持原模式。
1/16 的引擎帧分别采样表面准备、梯度、跨度、光照读取/混合和光照打包时间，
以定位开销；诊断包的计时开销不能冒充发布配置的性能。
SD 包可使用现有只读 App-media 回退，从固定 `PXA/media/<AppKey>` 读取该文件。
PXADB 保持禁止写应用私有目录；测试只替换自己创建的 SD 探针文件。

```sh
PXA_QUAKE_TEST_RENDER_PROBE=1 PXA_QUAKE_TEST_PACKED_CODEC=1 \
  python3 local/pxa-apps/common/recrof/tests/run_quake_frame_adapter.py
```

Duke 原生输出候选在原有输入状态记录中附带 Host 批次、拒绝和执行计数，
用于核实加速路径实际执行。默认构建未因此增加新的日志条目。

2026-10-20 音频审计更正：现有公共桥接与 ESP Host 只提供 16kHz 单声道。
锁定 C&C 上游默认是 22.05kHz 立体声，其 PXA 混音器也曾改成 16kHz。
这意味着已有“音频开启”的 FPS 数据不能证明保留原音频质量；
完整采样率、声道和连续性验收仍待完成，不能靠静音或降低音质认定目标达标。

后续源代码先保留 U8 到 S16 转换中的左右声道，不在环形缓冲辅助函数提前下混。
旧的单声道 `port_pcm` 得到的平均值逐样本不变；源采样率、环绕顺序和每批最多
64 帧保持不变，临时转换块由 128 增至 256 字节，位于既有 Guest 栈内，不新增
Host SRAM/队列。GCC/Clang ASan/UBSan 与 WASM 测试覆盖全部 65536 种 U8 声道对、
U8/S16 环绕、空数据、批次及旧单声道结果。这只是保留来源的前置改动，当前 Host
仍然输出 16kHz 单声道，不能当作立体声或完整音频保真已实现。
公开源码 Quake/SD 为 0.4.8 / seq128，Duke SD 为 0.1.30 / seq31，Duke 为 0.1.3 / seq4；
这些新源码包尚未以新 AOT 上机，本轮真机数据对应更早、摘要已记录的测试产物。
