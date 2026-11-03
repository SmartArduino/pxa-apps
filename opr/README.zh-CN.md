# Open Dungeon

OPR 原始六模块地牢规则，三职业、五层与最终 Boss，PXA 自绘前端。

适配 PXA 0.7 / Core SDK 2.1。构建使用工作区 SDK、主循环组件和开发者签名：

```sh
tools/app.sh build opr --board doit-gamekit-1 --target simulator --aot-only
tools/app.sh build opr --board doit-gamekit-1 --target esp32s31 --aot-only
```

应用源码位于本目录，公共适配层复用 `../common/`，依赖源码随仓库保存，无需原开发机的绝对路径、预编译库或旧 `.pxa` CLI。运行日志和截图保存在工作区 `local/artifacts/`。

游戏状态只保存在本次运行的内存中，退出后重新开始。

各上游版权和许可保持原样；适配层许可不覆盖引擎、字体、游戏数据或美术资源。素材仅按用户提供的私人学习用途使用，未发布到商城。

原生规则回归与完整战役：`python3 local/pxa-apps/opr/tests/run.py --output <本地产物目录>`。
