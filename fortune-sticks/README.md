# 好运签

独立PXA应用，包ID `pxa-fortune-sticks`，无网络和SD卡依赖。

- 24支原创鼓励签，覆盖学业、友情、勇气、心愿、生活和发现，或选择随心抽取。不会连续抽到同一支签。内容用于轻松娱乐和日常鼓励。
- 点击签筒、确认“求一签”或轻摇设备，都可触发摇筒和出签动画。摇动检测使用加速度，角速度影响摇筒动画幅度；菜单和记录页释放传感器。
- 签文、签意、小行动支持按键翻页；长竖屏的签文页同时展示全部内容，并提供较大的细读文字。最小160×160屏幕也不需要触摸滚动才能读完。
- 收藏喜欢的签，保存最近8次求签、主题、摇动开关和收藏；启动可恢复最近的结果。切换主题不改写历史记录。动画进入后台时只落定并记录一次。
- 三按键用↑↓选择、确认执行，结果页默认选中“再求一签”；触屏可直接点击按钮和签筒。

在 `pxa-projects` 构建：

```sh
tools/app.sh build fortune-sticks --board doit-gamekit-1 --target esp32s3 --aot-only \
  --source-root "$PWD/local/pxa-apps" --output "$PWD/local/app-output/fortune-sticks"
```

模拟器改为 `--target simulator`。资源生成、传感器约定和真实应用测试见[共用支持](../common/motion-games/README.md)；`tests/test_core.py --output ...` 验证抽签与持久化。代码、原创签文和图标为MIT，字体为SIL OFL，完整声明在安装包的 `assets/NOTICE.txt`。

本应用的 `art/font.json` 选择内嵌字形：资源生成工具会无损打包全部灰度覆盖值，启动时通过公开GameRender上传字形页，后续逐帧不解码；原始字形页保存在 `resources/` 供校验，不重复装进设备。构建使用LTO与WASI提供的整数格式化库；运行时接口与SDK兼容下限不变。
