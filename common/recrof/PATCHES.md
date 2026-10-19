# 源码适配清单

2026-10-19 手柄适配集中在公共 `controller.c`、`runtime.c` 和各引擎平台
`port.c` / `input.cpp`，未修改 vendor 的游戏输入逻辑。新增菜单状态查询、
连续转向和 RTS 光标/地图滚动；RTS 左右鼠标按来源合并，防止触摸与手柄
同时按住时提前释放。上游同步工具继续维护相同源码边界。

固定提交见各应用的 `upstream.json`。保留引擎主体，替换 ESP-IDF/裸机平台边界。

| 应用 | vendor 内改动 | 目的 |
| --- | --- | --- |
| Quake | `quake/host_cmd.c` | 声明重启函数的正确 void 签名，避免 Wasm 间接调用签名不一致 |
| Quake II | `game/g_turret.c` | 使 infantry_die 的声明和调用与实际五参数定义一致 |
| Quake II | `other/q_arena.c` | 将引擎 arena 调整到 8 MiB，移除宿主不支持的返回地址诊断 |
| Quake II | `other/q_hunk.c`、`qcommon/common.c` | 移除仅用于日志的 `__builtin_return_address` |
| Duke3D | Engine 的 `display.c`、`filesystem.c`、`mmulti.c`、`tiles.c`；Game 的 `gamedef.c`、`menues.c` | 补齐 assert 声明，消除隐式外部函数导入 |
| Duke3D | `Game/src/midi/nh_midi.c` | 修正音乐函数返回值和参数类型，与引擎头文件一致 |
| Duke3D | `Game/src/control.c` | 每轮一次读取四轴，避免逐轴读取提前消耗触摸转向量 |
| RA | `redalert/startup.cpp` | 允许配置选择 DOS / Windows 素材模式，由 PXA 缩放到设备屏幕 |
| C&C / RA | `common/sockets.h` | 为禁用联网的 WASI 构建提供 socket 类型声明 |

各应用目录中的 port、video、audio、input、filesystem 等文件提供 PXA 平台实现。Quake 与 Duke 的 extmemory、internalFlash 等文件保留原引擎的数据格式与缓存逻辑，底层读写由公共 `vflash.c` 转接。Duke 资源转换器、Quake PAK 转换器来自同一固定上游提交。

公共适配层使用 GPL-3.0-or-later，字体来自 Daniel Hepper 的公有领域 font8x8。原引擎及转换器保留其原版权与授权条款。C&C/RA 还保留 EA 源码授权文件中列明的附加条款。

RTS 的 `*data.cpp` 关闭函数内联；其 post-link 步骤保持 C++ 初始化器同步，并限制 Binaryen 内联，避免 WAMR 快速解释器为常量池保留双倍帧空间时超过宿主的 16 KiB 操作数栈。图形、鼠标键盘等待和音频实现均由 PXA 适配层提供。

后续存储与性能实现：FS read-at 的逻辑 seek/有界同步读、保留 legacy 路径；Duke 已有批量保存边界接入分页 NVM 聚合；SD 变体独立配置 32ms 计算预算、合法 16ms Clock 与 128KiB continuation。320×240 的 RGB565 成对输出及控件绘制快速路径保持原像素，其他尺寸走通用缩放。Duke `Game/src/game.c` 中每帧调试耗时打印改为显式 `PXA_DUKE_FRAME_TIMING` 开关，默认关闭，仍保留每五秒汇总统计。同步工具通过三方合并保留这些 vendor 差异。


2026-10-19 增补：

- Quake `vendor/quake/host.c` 仅增加条件编译的分阶段计时钩子；统计实现、字节缓存与读取调度仍在平台层。
- Quake II `ref_soft/r_image.c` 用 `PXA_STREAM_PCX` 接入有界流式PCX解码；原实现保留。出生点回调通过平台层类型正确的链接包装适配。
- Quake II 离线网络层遵循上游无服务器套接字时立即返回的NET_Sleep语义，避免服务器10Hz等待限制渲染。
- C&C公共 `keyframe.cpp` 只增加可配置的可回收图形缓存大小；普通变体保留上游默认值。RA SD分别为256/128KiB。
- RTS文件解析缓存有16个固定条目、500ms期限，私有命名空间变化立即失效；实现位于vendor之外，通过链接符号重命名保留原解析函数。
- C栈与续体缓冲可按独立变体配置，默认值保持原普通应用配置；SD变体协商Clock 0.3的8ms，旧Host回退16ms。
- C&C公共 `timer.cpp` 改用单调时钟计算经过时间，避免系统实时时钟校准影响游戏计时。
- SD代码包不查询包内资源；只读SD小块读取使用两页有界缓存，关闭句柄即失效。
  C&C/RA SD 统一小写虚拟路径，省去归档成员的大小写目录扫描；普通变体保留原解析。
- C&C SD可保留两个只读MIX空闲句柄，Clock在500ms后关闭；任何私有命名空间
  变化在重用前使其失效。句柄请求cookie与只读页保持原所有权，不复用报错文件。

- ROM页缓存只增加槽索引，保留完全相联LRU；实现和独立语义回归均位于平台公共层。
- Quake `r_main.c`、`r_edge.c`、`d_edge.c`增加条件编译的渲染阶段计时；统计实现位于`port`。
  `scan`包含`surfaces`，两者不能相加当作独立阶段。着色缓存及批量内存试验未启用。


Duke 0.1.24 纠正扩展方向键：PXA `keymap.h` 使用引擎 `sc_*` 命名常量，避免
把PC原始Set-1码直接交给ESP入口。`input_stats.c` 在已有统计周期内报告实际
玩家坐标和上下键状态；原生回归使用引擎KB位图访问器，真机回归要求实际相反
方向位移及菜单选择变化。没有修改vendor源文件，上游同步仍保留平台边界。
