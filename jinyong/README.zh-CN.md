# Jin Yong S60 Core

jinyong-legend S60 原始 C/Lua 5.1 引擎，软件 SDL 兼容层，静音模式；与 kys-cpp 是不同引擎。

适配 PXA 0.7 / Core SDK 2.1。构建使用工作区 SDK、主循环组件和开发者签名：

```sh
tools/app.sh build jinyong --board esp32s31-korvo-1 --target simulator --aot-only
tools/app.sh build jinyong --board esp32s31-korvo-1 --target esp32s31 --aot-only
```

构建还需要 Binaryen `wasm-opt`（已使用版本 133），需加入 `PATH`。

构建阶段还使用主机 C 编译器和 Python 3，将随包 Lua 5.1 脚本编译为 wasm32 字节码，保留原文件名、行号和局部变量调试信息。原始资源包不变，派生脚本包在构建时自动生成；设备启动时直接加载字节码，避免在有限的原生任务栈上解析整份脚本。

PNG 也在构建阶段按原尺寸无损解码到派生像素包，保留 RGB/RGBA 色值和透明度。设备只分配最终软件画面并流式读取像素，避免在 5 MiB 线性内存中同时保存 PNG 解压缓冲和完整画面。原始 PNG 仍保留在原始资源包中；派生包自动生成，不纳入 Git。

运行包使用不超过 512 KiB 的小资源分片，大文件独立存放，逻辑文件名与原数据不变。原始合并包作为构建输入保留，不重复装进运行包。`assets-esp32s3/`、`assets-esp32s31/` 和 `assets-simulator/` 是自动生成的目标资源目录，不用于手工维护素材。启动菜单和属性确认后释放标题图缓存，再加载游戏场景。

原引擎中的 Host 调用先挂起协程，再由事件调度入口提交。文件关闭和 LittleFS 刷新等 Host 工作因此不再叠加 Lua/SDL 的原生调用栈；原生栈及线性栈检查均保留。

应用源码位于本目录，公共适配层复用 `../common/`，依赖源码随仓库保存，无需原开发机的绝对路径、预编译库或旧 `.pxa` CLI。运行日志和截图保存在工作区 `local/artifacts/`。

大型游戏素材不纳入 Git；首次构建前运行 `python3 local/pxa-apps/jinyong/prepare-assets.py /path/to/pxa-jinyong-private-study.zip`，校验并恢复 `assets/` 的原始资源包。私有 FS 服务保存压缩增量、可写文件与存档覆盖层，不依赖设备的小容量 KV 服务。面向至少 16 MiB PSRAM 的设备，固定预留 5 MiB 线性内存以避免启动后的内存增长碎片。

各上游版权和许可保持原样；适配层许可不覆盖引擎、字体、游戏数据或美术资源。素材仅按用户提供的私人学习用途使用，未发布到商城。

触屏下方提供方向、ENTER、SPACE、ESC、Y/N 与 TAB。手柄方向键移动，A 确认，B/Start 菜单，X/Y 对应 Y/N，L 对应空格，Select 对应 Tab。字体模块限于随包 TrueType 字体所需的驱动和 rasterizer；协程 C 栈 128 KiB、Host 栈 64 KiB、Asyncify 缓冲 64 KiB，栈界限仍由运行时检查；主地图采用上游的局部行列缓存模式，精灵缓存上限为 256，保留原资源与规则，通过缓存淘汰控制内存。

文件层的独立主机回归：`python3 local/pxa-apps/jinyong/tests/filesystem.py --output <本地产物目录>`；覆盖原始资源读取、随机访问、写入、重命名、错误与冷重绑。该程序使用主机文件作对照，并覆盖 2 KiB 值、40 个块、短 I/O、背压与原子替换失败；不替代实际设备的私有 FS 验证。

图片像素、透明度、所有权和有界内存回归：`python3 local/pxa-apps/jinyong/tests/png_memory.py --output <本地产物目录>`，使用 ASan/UBSan 检查 PNG 和派生像素读取。

`SOURCE-LOCK.json` 保留原开发包的来源快照，记录路径沿用原包结构；当前编译输入以 `CMakeLists.txt` 和 `game.cmake` 为准。素材恢复使用本目录的 `prepare-assets.py`。

压缩分组和事务日志保存为 `jy-files-v1/` 下的私有文件，每个块经暂存文件加原子替换提交；原版资源仍按需读取，无需把 4.9 MiB 场景缓存整份复制到 RAM。旧 KV 格式不再用于新存档。保存压缩使用有界 64 KiB zlib 工作区，避免在世界状态驻留时再申请默认的大块压缩堆。
