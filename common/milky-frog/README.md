# 奶蛙小游戏共用层

为 `milkyfrog-2048`、`milkyfrog-rain` 和 `milkyfrog-miner` 提供原版素材的离线转换、响应式布局、高清图片与录音播放。窗口、触屏与手柄、暂停、双语菜单和校验存档继续使用 Pocket Game。各游戏是独立安装包，存档与声音设置分别保存。

录音转换为 16 kHz 单声道 S16，通过 Assets 异步准备，Host 负责播放和混音；Guest 不复制完整 PCM。缓存最多六个录音句柄，录音总量限制为 280 KiB，按需查询大小并回收闲置录音，同一时间只准备一个文件。三条音效轨分别供笑声、碰撞与特殊事件使用。初始预热第一段笑声，尚未准备的事件最多等待 500 ms，过时事件不补播。暂停、退出、静音清理声音；工厂收钩后让当前一轮笑声播完。背景音乐使用包内 Ogg 流，音效和背景音乐可以分别关闭。

文字使用系统字体的 sized-text 能力，由 Host 缓存所需字号。透明 Canvas 将文字和高清图片按显示器实际像素绘制，图片不经过低分辨率游戏画布的放大。PNG 保留 RGBA 透明度，按显示尺寸选择 32、64、128 像素或最多 56,000 像素的原图派生版本；图鉴的小图和连续笑脸帧合并为有界图集。只加载当前画面需要的图片，新画面提交后再释放旧句柄，图片总量限制为 224 KiB，与录音共同适应默认 512 KiB Assets 缓存。图片未就绪时使用包内的小型 RGB565 加 4-bit alpha 后备图。

布局按屏幕宽高、安全区和圆角重排。横屏 2048 使用棋盘与侧栏，方屏、小屏和竖屏改用上下布局；紧凑方屏把分数收进标题栏，保留棋盘空间。首页、设置、图鉴分页及暂停面板共用自适应尺寸。游戏背景画布最长边为 480 像素、总像素不超过 480×320，触摸坐标始终对应当前布局。每款应用声明 2 MiB 固定线性内存、64 KiB 栈；Host 录音缓存、图片、音乐解码和显示内存另计。

返回与翻页使用商城同款系统图标（U+F053 / U+F054），暂停使用 U+F04C，统一通过 Canvas 的 `ICON` 字体角色调用 Host 自带图标字体，由系统按 DPI 选择字形。返回按钮沿用商城的透明底色及按压反馈。应用不打包图标字体，锁定标记仍由 Canvas 绘制；图标与文字共用透明覆盖层和遮挡规则，触摸区域保持独立。

承载文字、图片及图标的 Canvas 必须声明 `PXA_UI_COMPOSITION_ALPHA_OVERLAY`，以便设备把它合成到游戏 Surface 上。透明背景颜色本身不等于透明叠加层；遗漏合成标记会使设备的 Surface 显示路径只输出底图。

## 构建与验证

从 PXA workspace 根目录执行：

```sh
tools/app.sh build milkyfrog-2048 --source-root local/pxa-apps \
  --target simulator,esp32s3,esp32s31 --aot-only \
  --output local/artifacts/milkyfrog-games/BUILD/packages/2048
```

其余两款替换为 `milkyfrog-rain`、`milkyfrog-miner`。本机可使用 `tools/dev.sh sim milkyfrog-2048 --source-root local/pxa-apps` 启动，真机通过 PXADB 安装对应 `.pxa`。不要将模拟器的运行速度当作 ESP32 的帧率。

商城按架构发布独立包。工厂的 ESP32-S3 变体使用小屏资源：静态图片最多 128 像素、稀有图鉴使用 64 像素图集，连续动画和全部录音不变；ESP32-S31、桌面和 WASM 保留完整高清资源。用下列命令生成独立源码副本，原始素材不变，输出目录必须尚不存在：

```sh
python3 local/pxa-apps/common/milky-frog/prepare_small_screen.py \
  --output local/artifacts/milkyfrog-games/BUILD/small-source
tools/app.sh build milkyfrog-miner \
  --source-root local/artifacts/milkyfrog-games/BUILD/small-source \
  --target esp32s3 --aot-only \
  --output local/artifacts/milkyfrog-games/BUILD/small-package
```

资源服务最低版本固定为 Assets 2.5；当前功能不依赖后续版本的大图解码能力。各应用的 `store.json` 保存商城文案，`store/cover.png` 为宣传封面，旁边的 `cover.json` 保存生成提示词、参考图及哈希。封面不打入应用资源包。商城截图必须直接来自实际 PXA 模拟器，关闭性能浮层和系统导航后保存原图。

```sh
bash local/pxa-apps/common/milky-frog/tests/run.sh \
  local/artifacts/milkyfrog-games/BUILD/native
python3 local/pxa-apps/common/milky-frog/tests/simulator.py \
  --simulator /absolute/path/to/pxsys_product_simulator \
  --packages local/artifacts/milkyfrog-games/BUILD/packages \
  --output local/artifacts/milkyfrog-games/BUILD/simulator-tests
```

主机测试覆盖合并与 4096 上限、碰撞与五连击、关卡生成与收钩、图鉴、存档校验和取消手势。模拟器测试使用真实签名包、触屏和手柄，录制实际 Host 混音，检查试听、静音、暂停与重新启动后的存档。截图和运行结果只写入传入的本地产物目录。

## 素材与许可

`prepare_assets.py` 需要 Pillow 和 ffmpeg。已有文件从各应用的 `art/` 复用；缺失时按明确的原站 URL 下载。它生成 `sprites.hpp`、`heroes.hpp`、包内图片、音频及 `SOURCES.json`，记录来源、原文件 SHA-256 与转换方式。工厂的三档笑声在构建前变调并补偿速度，不在设备上运行 AudioWorklet 或变调库。

启动图标通过 `export_icon()` 等比缩放并居中到 96×96 方形，禁止直接把非方形素材拉伸到正方形。奶块雨使用 `art/icon-rain-square.png` 的方形头像构图，参考原奶蛙生成，工具、完整提示词和哈希记录在同名 JSON 中；`SOURCES.json` 同时记录图标来源及最终导出文件。

PXA 新增适配代码使用 MIT，Pocket Game 与其原创 ASCII 字体为 CC0，三款应用全部通过 UI Canvas 调用 Host 系统字体，包内不含字形子集、字体文件或字体图集。原游戏和第三方形象、图片、录音的权利分别保留，参见每款应用 `assets/THIRD-PARTY.txt`。`upstream/` 保留适配时核对的游戏规则与音效脚本；`factory-LICENSE` 只适用于明确采用 MIT 的 frog-miner 项目，不扩展为其他站点或第三方形象、录音的许可。
