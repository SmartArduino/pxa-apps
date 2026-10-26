# 归档游戏运行层

68 款独立应用使用 PXA SDK 2.1 / Core 2、当前 SDK 的 `pxa_call` / `pxa_io` 和 GameRender。应用清单见 [games.json](games.json)。游戏规则和素材保留各自来源与许可，共享源码只存一份：Simon Tatham 解谜 39 款、Arduboy 19 款、SAF 4 款，以及 Brogue、Blockout、Meritous、OpenGolf、Sopwith、WhatAjong 六款独立移植。

同一游戏不重复建立应用。Anarch、2048、国际象棋分别使用已有的 `anarch`、`game-2048`、`pocket-chess`；扫雷、俄罗斯方块、打砖块使用 `arcade` 的现有入口。不同作品仍保留独立包 ID。

## 显示与输入

游戏保持原逻辑分辨率，以整数坐标等比适配实际屏幕和系统安全区，保留黑边。显示与触摸共用一套变换，包含屏幕密度；黑边中的点击会取消当前触摸。窗口尺寸变化会重建渲染上下文，多个连续变化合并处理。上下文使用实际屏幕尺寸、双缓冲和有界索引色纹理上传；渲染忙时保留最新画面重试。

解谜游戏：A 新局，已有棋局时 B 或 Start 继续；游戏中方向键移动后端光标，A/B 为两种选择，X 撤回，Select 重做，Start 返回游戏菜单。数字键盘类使用 L/R 选择下方高亮按键、Y 输入。触摸保留原来的棋盘、数字键盘和操作按钮。

Arduboy 和 SAF：暂停时 A/Start 继续，运行时 Start 暂停；方向键与 A/B 对应原游戏按键。各游戏的额外按键仍遵循界面标签。暂停、后台和控制器断开会释放按键。

主要使用触摸的独立移植提供手柄指针：方向键移动，按住 R 精细移动，A 点击或拖动，B/Start 返回游戏菜单。触摸后隐藏指针。Brogue 使用自己的方向、确认和物品操作映射。

## 声音与存档范围

迁移保留各归档适配器已有的能力。Arduboy/SAF 当前保留声音事件和会话内 EEPROM/保存状态，未接入持久化存储及 Host 音频输出；退出后这些会话数据不保留。Meritous 的归档音频适配器为静音，Brogue 和 Meritous 的内存文件系统同样不应视为持久存档。需要这些能力时，应另行接入 `storage` / `fs` / `audio`，并增加恢复与异常处理测试。

Brogue、Meritous、OpenGolf、WhatAjong 的内存和计算需求明显高于小型游戏；构建成功不代表适合所有 ESP 板。先在桌面模拟器验证，再在目标板测量实际资源与帧率。

## 构建与维护

每个应用根目录有独立 `package.json` 和 `CMakeLists.txt`，可按仓库说明构建模拟器、ESP32-S3、ESP32-S31 或 Wasm 包。应用依赖本目录，复制应用时保留 `common/retro-games` 的相对位置。Brogue 与 Meritous 另需 `wasm-opt`，原适配器使用 Binaryen 108 的 Asyncify 转换；构建所需的游戏源码已随仓库提供。

各应用的 `assets/icon.svg` 是按游戏玩法绘制的独立矢量图标，`assets/icon.png` 用于桌面和商城；图标只由 Host 加载，不进入游戏帧循环。安装 CairoSVG 后运行 `python3 common/retro-games/icons.py` 可重新生成全部图标。图标的 MIT 许可独立于游戏引擎许可，见各应用的 `assets/icon-license.txt`。

`store.json` 保存对应应用的商城名称、分类、玩法、操作、功能范围和源码下载地址。商城截图通过真实产品模拟器获取；`tests/retro-games/simulator.py --store-media` 会在启动前关闭性能计数并隐藏导航条。上传前仍需逐张检查原始截图，截图与运行记录只存放在工作区忽略的本地产物目录。

商城分别提供 ESP32-S3、ESP32-S31、Linux x86_64 三种架构的独立 AOT 安装包，每个包只包含对应架构的执行文件。构建商城版本时分别使用 `tools/app.sh build <应用目录> --target esp32s3`、`--target esp32s31` 和 `--target simulator`，输出到不同目录；不要将三种 AOT 合并到同一个商城下载包。它们共享同一应用 ID、版本、发布序列和作者签名身份。

日常构建关闭逐帧状态和内存转储；诊断构建可将 CMake 选项 `PXA_RETRO_TRACE` 设为 `ON`。此选项会产生大量日志，不应用于性能测量。正式运行仅保留必要的错误和少量交互日志。

仓库检查入口为 `tests/retro-games/check.py`，默认检查清单和原生显示/传输测试，`--target all` 另外构建完整清单的多架构包。`tests/retro-games/simulator.py` 使用真实 product simulator，检查启动、手柄输入、共享菜单的暂停/继续和正常退出；通过 `--simulator`、`--packages`、`--publisher-key`、`--output` 指定工具和独立验证目录。正式运行使用 AOT；Brogue、Solo、WhatAjong 等较深的调用链会超过桌面默认 Wasm 解释栈的容量，不应把解释模式作为交付运行方式。

上游固定版本、原始文件校验和与许可分别保存在各应用及共享引擎的 `upstream-lock.json`、`source-lock.json`、`LICENSE`、`COPYING` 和 `NOTICE` 文件中。不要把 MIT、CC0、GPL、AGPL 等不同授权合并成单一许可。修改平台适配器时保留上游原始文件和已有补丁；新增验证产物放在固件工作区忽略的 `local/artifacts/`。

归档保留的 `platform-lock.json` 描述原移植补丁及其历史文件校验，不作为 SDK 2.1 前端的完整校验清单。当前前端、共享运行层和构建文件以本仓库源码为准；上游原始源码的固定版本和校验记录继续保留。
