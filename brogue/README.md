# Brogue CE

Original Brogue CE roguelike, text mode, desktop simulator port

PXA SDK 2.1 / Core 2 移植，保留归档中的游戏实现与许可证。使用[共享运行层](../common/retro-games/README.md)的显示适配、生命周期和诊断配置。

在固件工作区根目录构建：

```sh
tools/app.sh build brogue --target simulator,esp32s3,esp32s31 --aot-only \
  --output "local/artifacts/retro-build/$(date +%Y%m%d-%H%M%S)/brogue"
```

操作、声音与存档范围见[运行层说明](../common/retro-games/README.md)。游戏来源固定在本目录的 `upstream-lock.json`、`source-lock.json` 或共享引擎的来源记录中；各 `LICENSE`、`COPYING` 和 `NOTICE` 文件分别保留原授权。构建不下载游戏源码。

Portable 构建保留 `vendor` 上游文件原件及其 AGPL 许可。`compat/generate_architect.py`、`compat/generate_grid.py`、`compat/generate_items.py` 和 `compat/generate_movement.py` 在构建目录生成固定版本的 `Architect.c`、`Grid.c`、`Items.c`、`Movement.c`，仅把地图填充、循环分析及视野列扫描的递归记账改成有界显式 DFS/窗口扫描帧。原有局部判定、邻居和写入顺序、返回计数、边界 `break`、短整数累加与随机数调用顺序均保留。源文件 SHA-256 或唯一函数匹配不符时构建停止。地图遍历显式帧使用现有线性 C 栈；物品热图保留无符号短整数回绕和重访，使用现有线性内存堆中可检查增长的帧，分配失败时停止应用。两者均不扩大 Host 执行栈或内存预算。

`compat/tests/` 下的 `test_*_stack.py` 配合 `--output <被忽略的本地产物目录>` 可编译上游递归 oracle 与适配实现，检查完整调用/写入/返回顺序、最终网格、容量边界及生成器的拒绝行为；这些辅助测试使用本机 C 编译器和 ASan/UBSan。

兼容遍历循环通过前端单调时钟预算提供约 8 ms 的协作检查点，沿用既有 Asyncify 与 TICK 恢复路径；不推进游戏时钟，不改变 RNG 或地图/回合更新顺序。纯函数 oracle 使用空调度钩子比较原算法顺序。

子机器构建还使用显式堆上下文保留每个原局部变量，在原来的两个子调用返回点继续执行，保持构建、回滚和 RNG 顺序。正常完成按原路径释放地图及机器数据；上下文分配失败则释放活动上下文与独立临时数据并停止组件，不把资源失败当作生成失败重试。

续接的分配检查仅覆盖所持有的上下文、机器数据和网格临时缓冲，不替代引擎其他分配点的失败处理。

菜单和输入循环通过 `compat/generate_ui.py` 生成调度单元：每次调用的局部变量保留在既有线性 C 栈的自动上下文中，仅在原来的游戏子调用处返回工作函数，由调度函数执行同一个调用后继续。工作函数不内联，以免大型菜单/输入解释器帧占据地形效果处理所需的执行栈；初始化、对象生命期、调用返回和 RNG 顺序保持不变，不调整任何栈大小或超时。
