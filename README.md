# PXA Apps

[English](README.en.md)

PXA 的开源应用与共享资源仓库，包括商城、天气、小说阅读器和游戏。系统运行库、SDK 与打包工具在 [pxa-system](https://github.com/SmartArduino/pxa-system)，ESP 固件与板级集成在 [pxa-workspace](https://github.com/SmartArduino/pxa-workspace)。

ABI、IPC 和资源生命周期回归夹具位于 `pxa-system/simulator/desktop/tests/apps/`，通过该仓库的 `tools/package/test_core2_smoke.sh`、`test_core2_ipc_smoke.sh` 和 `test_resource_app.sh` 运行。纯诊断应用、渲染基准和实验游戏原型已移出本仓库；历史验收报告与截图保留。

所有持续维护的应用使用 **C++26、Core 2.0**。应用名称与包 ID 不再带实现语言后缀：使用 [`jump-jump-3d`](jump-jump-3d/README.md)、[`pixel-dungeon`](pixel-dungeon/README.md) 和 `voxel-craft`。旧 C 算法仅在 `common/cpp/tests/reference/c` 中作为性能、像素与玩法对照，不再提供生产应用清单。

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

## 游戏机模拟器

每个平台独立安装并保存数据，共用高效的 PXA 前端：

[SNES](snes-emulator/README.md) · [Mega Drive](mega-drive-emulator/README.md) · [Master System](master-system-emulator/README.md) · [Game Gear](game-gear-emulator/README.md) · [PC Engine](pc-engine-emulator/README.md).
