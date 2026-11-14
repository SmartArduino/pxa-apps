# Digger

Digger Remastered 原引擎，VGA 软件渲染，单人静音模式。

适配 PXA 0.7 / Core SDK 2.1。构建使用工作区 SDK、主循环组件和开发者签名：

```sh
tools/app.sh build digger --board doit-gamekit-1 --target simulator --aot-only
tools/app.sh build digger --board doit-gamekit-1 --target esp32s31 --aot-only
```

构建还需要 Binaryen `wasm-opt`（已使用版本 133），需加入 `PATH`。CMake 显式关闭 WASI Clang 的自动 `wasm-opt` 阶段，再按固定顺序执行 Asyncify 和局部变量合并；所用 Clang 需支持 `--no-wasm-opt`。这样可保留 Asyncify 排除名单依赖的函数名，并控制 Host 执行栈用量。

应用源码位于本目录，公共适配层复用 `../common/`，依赖源码随仓库保存，无需原开发机的绝对路径、预编译库或旧 `.pxa` CLI。运行日志和截图保存在工作区 `local/artifacts/`。

游戏状态只保存在本次运行的内存中，退出后重新开始。

本移植使用 Digger Remastered 源码及其随附的 VGA/标题数据，按上游 GNU GPL 第 2 版和逐文件 BSD 等许可交付；不包含官网另列的原版 DOS 程序或原版 Windmill 源码。发行时须保留 `assets/NOTICE.txt`、各许可证及完整对应源码，见 [源码交付说明](SOURCES.md)。适配层 MIT 不替代上游许可。

原生 raster 回归：`python3 local/pxa-apps/digger/tests/raster.py --output <本地产物目录>`，需要本机 SDL2 开发库和 `pkg-config`。Enter/触屏 START 开局，方向键移动，A 开火，B 暂停；触屏 NEW 重新开始。

为避免嵌入式堆碎片导致 `memory.grow` 在游戏初始化中失败，线性内存一次性固定预留 6 MiB；渲染仍使用 GameRender。该配置面向至少 16 MiB PSRAM 的设备。

应用图标为独立绘制的铲子与宝石几何图形，许可见 `assets/ICON-LICENSE.txt`；它不改变游戏引擎及原素材的许可范围。
