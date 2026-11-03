# kys-cpp for PXA (work in progress)

基于 scarsty/kys-cpp `f93be8a7cddeade5279a75fef25354fe76e7af46`（2026-10-29）。
本目录是独立 PXA Guest 应用，不向固件、系统或 Host 链接游戏及 SDL 依赖。
当前完整引擎尚未满足 16 MiB PSRAM 的 GameKit 1 启动预算，需要继续缩减代码及内存占用；
现阶段不能作为该设备上的可玩版本交付。

目前在移植和验证阶段，不能将构建成功、资源提示页或底层自检视为完整游戏已测试通过。
原版剧情、地图、图片、字体、初始存档和音乐不在上游代码仓库中，本应用不附带这些游戏资源。

在固件工作区根目录构建：

```sh
tools/app.sh build kys-cpp --board doit-gamekit-1 --target esp32s31 --aot-only
tools/app.sh build kys-cpp --board doit-gamekit-1 --target simulator --aot-only
```

`vendor/src` 保留离线游戏逻辑、Cifa 剧情接口、四种战斗模式、纸片地图及存档代码。
SDL3 的引擎边界适配到现有 PXA SDL2 Guest 运行层；渲染为 320×240，UI 使用原版
1280×720 逻辑坐标，场景及主 UI 纹理为 320×180，线性内存固定为 8 MiB。窗口保持原始画面比例。此配置尚待完整资源实测，
尤其是场景总内存、贴图缓存与实时战斗帧率。

当前不启用上游可选联机对战和 Windows smallpot 视频播放器；未使用的 YAML UI
导入器和 Cifa 字节码工具源码保留但不链接。Cifa 直接执行器用于游戏剧情。

## 游戏资源

传入实际 `game/`，不是外层发行目录。工具不改源文件，并拒绝覆盖已有输出：

```sh
python3 local/pxa-apps/kys-cpp/tools/prepare_resources.py /path/to/game /path/to/prepared-game
```

准备过程将文件名统一为小写，将散装或 ZIP 内 WebP 转为 PNG，并将散装贴图组打包为 ZIP；其他文件内容保持原样。
需要 Pillow。MIDI 需要原游戏 `music/mid.sf2`、`fluidsynth` 和 `ffmpeg`，在电脑上
渲染为 16 kHz Ogg；设备按需播放 Ogg/MP3/WAV，不在设备上运行 MIDI 合成器。
转换不会把旧版 `r0.grp` 转成当前引擎需要的 SQLite 初始存档；旧资源需先用上游工具转换。

旧式目录安装可用 PXADB 原始命令 `PACKAGE paths <publisher-root>:pxa-kys-cpp` 查询
`media` 路径。整应用安装到 SD 的卷内安装不支持该调试命令；应按已核实的发布者目录
确认 `PXA/media/<publisher-directory>/pxa-kys-cpp`，不要只按显示名称猜目录。
将 prepared-game 的**内容**放到这个媒体根。不要额外套一层 game 目录。
Guest 以只读媒体根读取游戏文件，修改和存档写入应用私有目录。
资源至少需要 `config/kysmod.ini`、`font/chinese.ttf` 和 `save/0.db` 或 `save/0.zip`，
以及该配置匹配的地图、贴图和剧情。仅有这三个文件不代表资源完整。

缺少必要资源时显示资源提示页。方向/操作键和触摸会更新输入指示块；A 执行
Cifa（含文件执行）、SQLite 中文持久化、ZIP 往返、GBK/Big5 转码、几何渲染与 PCM 提交检查；
测试工具准备外部夹具后还会验证 SD 随机读取，B 返回。日志明确区分
底层检查和游戏验收。检查只使用 `kys-port-check.*`，不打开游戏存档。

## 验证工具

运行资源准备器测试，所有临时夹具写到指定的本地产物目录：

```sh
python3 local/pxa-apps/kys-cpp/tests/resources_test.py \
  --work-dir local/artifacts/kys-port/<timestamp>/resource-tests
```

`tests/run_simulator_checks.py` 运行实际 PXA 产品模拟器，并严格要求全部底层检查通过。
`tests/run_platform_checks.py` 接入明确指定的 PXADB 设备，准备媒体夹具，注入手柄/触摸输入、
检查返回并保存原始截图；卷内安装需传入经核实的 `--media` 路径。
这些程序不执行完整剧情或战斗验收，也不验证物理按键扫描和扬声器听感。

## 许可与来源

来源版本和改动文件见 `vendor/UPSTREAM.json`。上游 LICENSE 和 readme 原样保留在
`assets/licenses/`；readme 对金庸武侠题材游戏另有禁止牟利条款，不能仅按 GitHub 的
BSD 标签解释授权。Cifa 子模块没有单独 LICENSE，本地保留其原始说明，不另行推定许可。

共享 PXA SDL Guest bridge 为 GPL-3.0-or-later；SDL2/SDL_mixer、stb 和 SQLite 保留
各自声明。当前仅作本机开发与验证，未完成上游附加条款与共享 bridge 的组合分发许可核实，
不要将这个开发包上传商城或用于商业发行。
