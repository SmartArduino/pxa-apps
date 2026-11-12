# richman

原创棋盘和规则的大富翁类回合制游戏；不包含《大富翁4》原版素材。

适配 PXA 0.7 / Core SDK 2.1。构建使用工作区 SDK、主循环组件和开发者签名：

```sh
tools/app.sh build richman --board doit-gamekit-1 --target simulator --aot-only
tools/app.sh build richman --board doit-gamekit-1 --target esp32s31 --aot-only
```

应用源码位于本目录，公共适配层复用 `../common/`，依赖源码随仓库保存，无需原开发机的绝对路径、预编译库或旧 `.pxa` CLI。运行日志和截图保存在工作区 `local/artifacts/`。

游戏状态只保存在本次运行的内存中，退出后重新开始。

本实现使用原创棋盘、地名与规则，不含《大富翁4》的原版资源。图标为本应用原创 SVG；商城截图由真实 PXA 模拟器生成。
