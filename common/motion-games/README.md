# 体感游戏共用支持

供 `shake-dice`、`marble-maze`、`fortune-sticks` 和 `mini-racer` 独立应用复用的底层支持。运行时不包含游戏入口、玩法选择器或另一游戏的代码；各应用有独立 ID、权限、图标、资源、界面和持久化格式。

`runtime.hpp` 负责公开的 GameRender、UI输入、时钟、传感器资源和前后台生命周期。Window 0.3、GameRender 1.5、Assets 2.2、Sensor 0.1 与 Core 2.1 为兼容下限。同次启动中拒绝某项权限后不反复弹窗，可继续按键或触屏。首次体感授权在创建绘图Surface之前完成，避免旧Host弹窗恢复冲突。授权等待期间暂停游戏时钟，完成或拒绝授权后重新启动；先完成传感器授权再订阅。后台取消未完成请求并关闭已创建资源。UI指针的逻辑坐标转换为 Surface 像素后才映射到游戏画面。

`motion.hpp` 统一 Gen4 安装轴向、平滑、校准、时效与摇动检测；其他设备可用迷宫的方向及镜像设置。`paint.hpp` 和 `controls.hpp` 提供绘制和可触摸的三按键操作。投影多边形在量化后检查面积，避免侧面的细小点数使整帧失效。

生成某个应用的资源：

```sh
python local/pxa-apps/common/motion-games/tools/generate_assets.py \
  --app local/pxa-apps/shake-dice
```

工具收集应用顶层C++源文件及头文件中的文字，也包含签文等内容头文件，排除生成的字形表；仅打包所需字形及基本数字、标点、速度单位。保留源字体校验值，生成独立图集、原创SVG对应图标，以及包含全部许可声明的单个NOTICE。需要 Pillow、fontTools 和 CairoSVG；设备上没有新增第三方运行时库。

测试产物写入命令指定的、本机忽略的 `local/artifacts/<topic>/<时间戳>/`：

- `tests/test_core.py --output ...`：AddressSanitizer/UBSan、骰子历史与校验、分级地图连通、碰撞、实际沿路径通关、原位置恢复、校准与摇动。
- `tests/build_motion_simulator.py --workspace ... --output ...`：用已有模拟器构建目录链接隔离的测试传感器 Provider，不改动SDK或固件。`GRAVITY_TEST_MOTION` 为兼容现有测试工具保留的环境变量，指定三轴加速度及三轴角速度共六个整数的文件，单位为微米/秒²和微弧度/秒。缺失或无效文件表示读数不可用。
- `tests/test_apps.py --workspace ... --package-root ... --executable ... --output ...`：运行实际签名应用，验证三按键、数量与历史、暂停与校准、断流停球、首关/末关解锁和重启。通关夹具只修改测试实例的规范POSIX存储快照，应用中没有作弊接口。加 `--touch` 验证触摸命中、拖动与多指归属；`--exit` 验证菜单的退出操作真正交给系统关闭；`--apps` 和 `--sizes` 可以缩小范围。
- `tests/test_extra_apps.py` 使用相同参数，验证好运签与3D小赛车的实际签名应用。包含主题、收藏、三页阅读、摇动、按键、体感断流自动暂停、完整快照恢复、三赛道结算和退出；加 `--touch` 验证触屏，加 `--all-signs --apps fortune-sticks --sizes 160x160` 检查全部24支签在最小屏幕完整显示。赛车结算夹具仅修改测试实例的规范存储快照，不增加生产作弊接口。各新应用的 `tests/test_core.py` 提供独立的ASan/UBSan核心测试。

共享代码为 MIT；`licenses/` 保留源许可原文，打包工具将声明合并到每个应用的 NOTICE。

`art/font.json` 中的 `{"storage":"embedded"}` 选择紧凑字形模式（也可显式传 `--embedded-fonts`）。工具逐页选择可逆预测和字面量/回溯编码，并逐字节验证还原结果。生成的覆盖值嵌入应用，初始化时利用已有DrawBuffer和公开GameRender上传一次，不增加解码依赖或每帧开销。原有PXR资源模式保持兼容。`tests/font_codec_test.cpp` 可对指定应用的生成字形做ASan/UBSan校验，验证与原始INDEX8完全相同及畸形数据拒绝。
