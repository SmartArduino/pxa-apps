# PXA Apps

[English](README.en.md)

PXA 的开源应用与共享资源仓库，包括商城、天气、小说阅读器和游戏。系统运行库、SDK 与打包工具在 [pxa-system](https://github.com/SmartArduino/pxa-system)，ESP 固件与板级集成在 [pxa-workspace](https://github.com/SmartArduino/pxa-workspace)。

ABI、IPC 和资源生命周期回归夹具位于 `pxa-system/simulator/desktop/tests/apps/`，通过该仓库的 `tools/package/test_core2_smoke.sh`、`test_core2_ipc_smoke.sh` 和 `test_resource_app.sh` 运行。纯诊断应用、渲染基准和实验游戏原型已移出本仓库；历史验收报告与截图保留。

应用通过 **Core 2.x** 接入；自研 C++ 应用使用 C++26，游戏移植保留上游 C/C++ 实现。应用名称与包 ID 不带实现语言后缀：使用 [`jump-jump-3d`](jump-jump-3d/README.md)、[`pixel-dungeon`](pixel-dungeon/README.md) 和 `voxel-craft`。这些自研应用的旧 C 算法仅在 `common/cpp/tests/reference/c` 中作为性能、像素与玩法对照，不再提供生产应用清单。

[名称与旧存档说明](docs/app-names.zh-CN.md) · [迁移及真机报告](docs/core2-cpp-migration-20261011.zh-CN.md)

## 用独立 DevKit 运行

先[安装 DevKit](https://pxa.doit.am/docs/setup/)，在本仓库的父目录执行：

```sh
git clone https://github.com/SmartArduino/pxa-apps.git
pxa run pxa-apps/pixel-dungeon
pxa run pxa-apps/voxel-craft --profile sensecap-watcher
```

每次关闭模拟器再执行下一条运行命令。内置屏幕配置用于 DPI、圆屏和安全区检查。应用可能引用 `common/`，复制示例时保留共享源码及资源的相对目录结构。

[`weather`](weather/README.md) 使用 IP 定位与 Open-Meteo，不需要 API key。网络和定位可用性取决于实际运行环境。
[`novel-reader`](novel-reader/README.md) 支持可配置 JSON 源、离线 TXT、系统输入、自适应字体、章节导航与自动翻页；测量和验收脚本位于其 `docs/` 与 `tests/`。
[`browser`](browser/README.md) 是 PXA 浏览器，支持 HTTP/HTTPS 轻量阅读、关键词搜索、链接、书签和基础表单，使用最新流式网络 SDK。

## 在固件工作区联调

将本仓库克隆到工作区的 `local/pxa-apps`。下面命令均在 **pxa-workspace 根目录**执行：

```sh
git clone https://github.com/SmartArduino/pxa-apps.git local/pxa-apps
tools/dev.sh sim pixel-dungeon --source-root local/pxa-apps
tools/dev.sh sim voxel-craft --profile esp-mosaico --source-root local/pxa-apps
```

工作区默认自动发现该目录；外部源码可通过 `--source-root` 或被忽略的 `local/apps.toml` 登记。构建输出位于 `local/app-output`，不写进本应用仓库。

## 打包与发布

商城支持一个多目标容器，也支持同一待审版本的分目标包。应用使用不同目标资源时分别构建：

```sh
for target in esp32s3 esp32s31 simulator; do
  tools/app.sh build store --target "$target" --aot-only \
    --output "local/app-output/store-variants/$target"
done
tools/app.sh build store --target wasm \
  --output local/app-output/store-variants/wasm
```

工作区默认使用 `deps/pxa-system/apps/pxa/.dev-signing` 开发夹具。正式发布设置 `PXA_SIGNING_KEY` 为自己的长期密钥，且各目标保持相同 App ID、版本、release sequence 与发布者身份。[商城发布教程](https://pxa.doit.am/docs/store-publishing/)

独立 DevKit 自动在用户数据目录生成开发密钥；SDK 不携带私钥。不要提交密钥、令牌、本机设置或 `.dev-signing`。

## SDK 使用与性能

回调应用通过 `common/cpp/Application.cmake` 使用可选的无状态 `pxa::call`，复用有界协议辅助函数和消息 ID，不分配 Context、协程池或请求表。需要异步任务和声明式 UI 的应用链接完整 C++ SDK。PXA 导入只有 `pxa.core.v2::pxa_call` 与 `pxa_io`，WASI libc 导入按组件声明。

多架构包共享一次加锁的 Wasm 编译，再生成各目标 AOT。`PXA_BUILD_CACHE_DIR` 启用增量构建；`PXA_COMPILER_CACHE=ccache` 可复用相同编译工作，不增加设备内存。缓存键包含容量定义、SDK 与编译器输入，不手工复用布局不同的二进制。

## 回归检查

在固件工作区根目录运行，SDK 与工具取自相邻的 `deps/pxa-system`：

```sh
bash local/pxa-apps/tests/native/run.sh
bash local/pxa-apps/tests/native/reference/run.sh
CC=clang CXX=clang++ bash local/pxa-apps/common/cpp/tests/run.sh -DPXA_C_REFERENCE_BENCH=ON
bash local/pxa-apps/voxel-craft/tests/test.sh
python3 tools/test-app-matrix.py --output local/app-matrix --target all --sdk-examples
```

字体预览和实验快照放在生成目录。相同截图可共用附件，但不同测量、测试配置、运行资源及许可证分别保留。性能报告必须说明工作负载、实际显示帧率、阶段耗时与内存，而不是只比较提交 FPS。

## 触屏与手柄小游戏

以下九款游戏使用 SDK 2.1，以 288×224 逻辑画布等比适配实际屏幕，支持触屏与控制器、校验存档、后台暂停和右上角游戏菜单。默认显示简体中文，在暂停菜单第四行切换 English，语言独立保存。构建时可同时输出模拟器、ESP32-S3 和 ESP32-S31 的签名 AOT 包：

| 应用 | 玩法 |
| --- | --- |
| [2048](game-2048/README.md) | 滑动合并数字、撤回一步、保存棋盘与最高分 |
| [Pocket Chess](pocket-chess/README.md) | 国际象棋、人机与双人模式、合法走子提示及升变 |
| [Pocket Sokoban](pocket-sokoban/README.md) | 34 关推箱子、撤回、步数与推箱次数、解锁进度 |
| [Pet Garden](pet-garden/README.md) | 原创像素宠物、房间与花园、三条进化路线、装饰商店、接莓果和节奏小游戏 |
| [Crisp Arcade](crisp-arcade/README.md) | MIT 原版 CAST N、GRAPPLING H、REFLECTOR、COLOR ROLL，独立最高分 |
| [Marble Islands](marble-islands/README.md) | small3dlib 软件 3D、八座滚珠岛屿、集宝石、定时门、弹柱、刹车和通关奖牌 |
| [Lantern Grove](lantern-grove/README.md) | 原创 CC0 像素森林放置经营，三种资源、四类建筑、八章目标与有上限的离线生产 |
| [Harbor Cafe](harbor-cafe/README.md) | 原创 CC0 海边咖啡馆，接单、调饮、送餐、补货、三条升级路线和每日经营结算 |
| [Tiny Elements](tiny-elements/README.md) | 原创 CC0 粒子沙盒，沙、水、火、蒸汽、植物与熔岩反应，自由绘制与四项挑战 |

六款较新的游戏使用 [Pocket Play](common/pocket-play/README.md)，2048、国际象棋与推箱子使用 [Pocket Game](common/pocket-game/README.md)；复制或开发时保留对应运行时与 [Pocket UI](common/pocket-ui/README.md)。十款游戏均不播放背景音乐，保留操作与事件音效。Lantern Grove、Harbor Cafe 和 Tiny Elements 可在各自帮助页开关音效并保存声音偏好。

[Anarch](anarch/README.md) 使用独立的 160×120 逻辑画布，同样等比适配屏幕和触摸坐标；菜单右上角或控制器 X 切换中英文，帮助按钮或 Y 打开说明。Game Boy 等模拟器中的游戏语言由 ROM 决定。各应用说明包含完整操作、许可证、构建和可复用测试入口。

## 水平仪工具

[水平仪 / Spirit Level](spirit-level/README.md) 提供双轴气泡、边尺水平/垂直测量、锁定、相对归零、已知水平面标定与中英文设置。支持加速度计和可选陀螺仪融合，采用绝对重力基准；无有效传感器时显示等待状态。构建入口为 `tools/app.sh build spirit-level --source-root local/pxa-apps`。

## 计算器工具

[多功能计算器](calculator/README.md) 支持基础与科学计算、DEG/RAD、8条历史、记忆值、八类单位换算，以及比例、折扣、变化幅度和平均分摊。横竖屏按实际比例满屏排布，支持触屏、手柄和Gen4三键。

## 奶蛙小游戏

[奶蛙2048超级版](milkyfrog-2048/README.md)、[奶蛙的奶块雨](milkyfrog-rain/README.md) 和 [奶蛙工厂](milkyfrog-miner/README.md) 为三个独立应用，支持触屏和手柄、原版录音、独立声音开关与存档。文字由 PXA 系统字体绘制。工厂首版提供矿工模式与 47 种稀有图鉴；素材来源、转换与测试入口见[共用层](common/milky-frog/README.md)。

## 归档游戏移植

[68 款独立游戏](common/retro-games/README.md)使用 SDK 2.1，统一等比显示、触摸坐标和生命周期，复用解谜、Arduboy 与 SAF 源码；每款保留独立包 ID 和原授权。清单见 [games.json](common/retro-games/games.json)。已在本仓库提供的 Anarch、2048、国际象棋及 Arcade 中的扫雷、俄罗斯方块、打砖块不重复导入。

## SDL 游戏移植

[死亡修道院](abbaye-des-morts/README.md)、[C-Dogs SDL](cdogs-sdl/README.md)、[Project: Starfighter](starfighter/README.md) 和 [Neverputt](neverputt/README.md) 各自提供独立应用、图标与架构分包，保留上游代码和素材许可。共享 [SDL guest 运行层](common/sdl-games/README.md) 接入手柄、触摸、音频和资源按需读取；SDL、混音器及软件 GL 均在应用中运行。C-Dogs 当前线性内存预算为 28 MiB，需另外预留 Host 内存；模拟器验证不代表真机帧率。

## 游戏机模拟器

每个平台独立安装并保存数据，共用高效的 PXA 前端：

[SNES](snes-emulator/README.md) · [Mega Drive](mega-drive-emulator/README.md) · [Master System](master-system-emulator/README.md) · [Game Gear](game-gear-emulator/README.md) · [PC Engine](pc-engine-emulator/README.md).
