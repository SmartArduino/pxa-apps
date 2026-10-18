# 源码适配清单

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
