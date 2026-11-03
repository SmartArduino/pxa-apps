# Digger

Digger Remastered 原引擎，VGA 软件渲染，单人静音模式。

适配 PXA 0.7 / Core SDK 2.1。构建使用工作区 SDK、主循环组件和开发者签名：

```sh
tools/app.sh build digger --board doit-gamekit-1 --target simulator --aot-only
tools/app.sh build digger --board doit-gamekit-1 --target esp32s31 --aot-only
```

构建还需要 Binaryen `wasm-opt`（已使用版本 133），需加入 `PATH`。

应用源码位于本目录，公共适配层复用 `../common/`，依赖源码随仓库保存，无需原开发机的绝对路径、预编译库或旧 `.pxa` CLI。运行日志和截图保存在工作区 `local/artifacts/`。

游戏状态只保存在本次运行的内存中，退出后重新开始。

各上游版权和许可保持原样；适配层许可不覆盖引擎、字体、游戏数据或美术资源。素材仅按用户提供的私人学习用途使用，未发布到商城。

原生 raster 回归：`python3 local/pxa-apps/digger/tests/raster.py --output <本地产物目录>`，需要本机 SDL2 开发库和 `pkg-config`。Enter/触屏 START 开局，方向键移动，A 开火，B 暂停；触屏 NEW 重新开始。

为避免嵌入式堆碎片导致 `memory.grow` 在游戏初始化中失败，线性内存一次性固定预留 6 MiB；渲染仍使用 GameRender。该配置面向至少 16 MiB PSRAM 的设备。
