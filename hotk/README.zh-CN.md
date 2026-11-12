# hotk

三国题材文字策略重实现，48 城、255 将、5 剧本；不是 GBA 原版引擎。

适配 PXA 0.7 / Core SDK 2.1。构建使用工作区 SDK、主循环组件和开发者签名：

```sh
tools/app.sh build hotk --board doit-gamekit-1 --target simulator --aot-only
tools/app.sh build hotk --board doit-gamekit-1 --target esp32s31 --aot-only
```

应用源码位于本目录，公共适配层复用 `../common/`，依赖源码随仓库保存，无需原开发机的绝对路径、预编译库或旧 `.pxa` CLI。运行日志和截图保存在工作区 `local/artifacts/`。

游戏状态只保存在本次运行的内存中，退出后重新开始。

文字与数值数据来自 Oliver714/hotk-gba-demake，不含原版 GBA 的精灵、音乐或头像。上游代码的 GPL-3.0-only 不覆盖所有游戏数据；其资源来源声明将数据及生成表列为混合来源。当前保留私人开发用途，待确认所用数据的再分发授权后再上架。
