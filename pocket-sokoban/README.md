# Pocket Sokoban

适用于 PXA SDK 2.1 的推箱子游戏。移动和撤销核心移植自 MIT 许可的 NibbleSokoban；保留地形与箱子分层，以及方向加推箱标记的回退规则。画面为原创几何图形，没有导入上游的图片或原版关卡。

## 玩法和操控

把所有箱子推到绿色目标上即通关。一次只能推一个箱子，不能拉箱子；箱子进入没有目标的角落可能无法救回，可撤销或重试。

界面默认中文，可在公共暂停菜单切换中文或 English。标题、操作提示、关卡与完成/解锁数量、移动/推箱计数、受阻和通关提示及全部侧栏按钮随语言即时切换。语言偏好由公共运行时保存，原有版本 1 棋局存档保持兼容。正式包按屏幕原生分辨率绘图，并用 Host 系统字体显示清晰的中英文字。

| 输入 | 功能 |
| --- | --- |
| 方向键 | 走一步，持续按住可重复移动 |
| B / 侧栏「撤销 / UNDO」 | 撤销一步，包含推箱动作 |
| X / 侧栏「重试 / RETRY」 | 重试当前关，保留完成记录 |
| L / R、Y / 侧栏「上关 / 下关」 | 切换上一/下一已解锁关，循环选择 |
| A / 侧栏「下关 / NEXT」或「重玩 / REPLAY」 | 通关后继续，最后一关完成后从第一关重玩 |
| 棋盘内滑动 | 按滑动方向走一步 |
| 点击角色相邻格 | 朝该格走一步，也可以推箱子 |
| 点击较远的空地 | 显示路线并自动走过去，不自动推箱；触碰屏幕或按键即可中断 |
| START / 右上角菜单 | 公共暂停、重开、退出菜单 |

先有两关原创教学关，之后是 32 关 Boxoban。完成当前关会解锁下一关；移动计数、推箱计数、完成记录和解锁数量显示在画面上。切换关卡会从该关初始布局开始；当前这一盘、最近 2048 步撤销历史和整体进度会自动保存。离开再回来会继续当前盘。

## 来源与许可

- **核心**：[sdosenn/NibbleSokoban](https://github.com/sdosenn/NibbleSokoban/tree/4ef4e56d21fd2dffa5f0b49f3f254326fa0899cb)，固定版本 `4ef4e56d21fd2dffa5f0b49f3f254326fa0899cb`，MIT，Copyright (c) 2021 snsdosen。`upstream/NibbleSokoban/` 只保留原样的 `Player.ino`、`LifoBuffer.ino`、`LICENSE`、`README.md` 作为来源证据。`core.cpp` 合并四方向分支，增加完整边界检查、静态循环撤销栈、计数、进度与存档，移除 Arduino 动画、输入全局变量和声音依赖。未导入原仓库的关卡、sprites 或 bitmaps。
- **关卡**：[google-deepmind/boxoban-levels](https://github.com/google-deepmind/boxoban-levels/tree/78011d8c86ad423bfb2e1005012393b4c3c24409)，固定版本 `78011d8c86ad423bfb2e1005012393b4c3c24409`，Apache-2.0。选自 `unfiltered/test/000.txt` 前 64 个候选中离线求解成功、解路径较短的 32 关，按路径长度排列。每关 10×10，四箱四目标，格局未改；`levels.hpp` 保留原始 dataset 编号。原始选中 ASCII 留在 `upstream/boxoban/SELECTED.txt`，只进行 C 字符串数组转换。
- **教学关、界面、图标和移植辅助代码**：本项目原创部分以 CC0-1.0 提供。原创教学关在数组中的 dataset 编号为 -1、-2；不替代实际上游核心移植。

Boxoban 引用：Arthur Guez, Mehdi Mirza, Karol Gregor, Rishabh Kabra, Sebastien Racaniere, Theophane Weber, David Raposo, Adam Santoro, Laurent Orseau, Tom Eccles, Greg Wayne, David Silver, Timothy Lillicrap and Victor Valdes. *An investigation of Model-free planning: boxoban levels*, 2018. <https://github.com/deepmind/boxoban-levels/>。

完整 MIT、Apache-2.0、CC0 许可和来源声明随源码提供，并复制至 `assets/`，打包后仍可查阅。`SOURCES.json` 记录固定版本、SHA-256 与转换说明。

## 构建和维护

从工作区根目录构建，输出和缓存放在本次独立的本地产物目录。公共运行时接口、构建与集成测试的维护说明见 [公共运行时说明](../common/pocket-game/README.md)。

```sh
sokoban_artifacts="$PWD/local/artifacts/mini-games/$(date +%Y%m%d-%H%M%S)/pocket-sokoban"
mkdir -p "$sokoban_artifacts"/{cache,tmp,logs}
PXA_BUILD_CACHE_DIR="$sokoban_artifacts/cache" \
TMPDIR="$sokoban_artifacts/tmp" \
tools/app.sh build pocket-sokoban --source-root local/pxa-apps \
  --target esp32s31,simulator,esp32s3 --aot-only \
  --output "$sokoban_artifacts/release"
```

可复用 host 测试位于 `tests/`，使用 C++17 编译 `tests/host.cpp core.cpp main.cpp` 后运行，测试全部关卡可解路径、规则、撤销、触屏输入和存档拒绝行为。双语绘制测试覆盖活动、受阻、通关、末关重玩和最大计数界面，检查文字边界、文字重叠及语言切换前后的存档一致性。`tests/generate_levels.py` 接收固定版本 Boxoban 原文件路径与 `--report` 本地产物路径，可离线重建关卡与解路径夹具；游戏内没有求解器。

## 实际限制

逻辑布局 288×224，原生分辨率绘图；SDK 最大线性内存 1 MiB，栈 64 KiB。存档版本 1，CRC-32 校验，最大 2280 字节；校验、棋盘、进度与撤销链全部有效后才替换当前状态，损坏或不兼容存档保持当前有效状态。CRC 用于损坏检测。只保留当前关的局面；切换关卡会丢弃上一关未完成的盘面。撤销栈超出 2048 步会淘汰最老记录；完成记录即使撤销通关动作也会保留。仅识别墙角中的非目标箱子并提示撤销；不识别复杂死锁，不自动解题。

## 音效

版本 1.1.2 / release_sequence 4 取消背景音乐，保留行走、推箱、受阻、撤销、重试、选关和通关音效。音效由共享运行时以 16 kHz 单声道 PCM 实时合成，不含外部录音或素材；没有操作或玩法事件时保持安静。暂停、进入后台和退出时立即清空音符并 flush Host 队列。

audio.playback 权限为可选；拒绝权限、设备不可用、异步音频打开失败或写入受阻均不会阻止玩法和存档。公共菜单保持四行顺序，所有原有棋局与语言偏好字节布局保持不变。主机测试验证静置无声、操作反馈和静置 tick 前后的存档一致性。

点击页眉关卡按钮或按 SELECT 可打开 34 关选关网格。完成标记和锁定状态单独显示；方向键选择、A 进入、B 返回，也可直接点选。选择当前关卡会继续棋局，锁定关卡及取消触摸不会修改进度。选关页本身不占用存档字段。

自动走路逐步写入正常撤销历史，读取存档、选关和撤销会清空待走路线。关卡按章节采用四套石墙和地砖配色；箱子归位有短暂微光反馈。
