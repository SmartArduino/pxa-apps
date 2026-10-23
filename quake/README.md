# Quake

商城版随安装提供 LibreQuake 的首个 Lite 关卡，使用允许修改和再分发的自由素材，
包含原项目的许可与署名。它不是 id Software 的原版关卡。资源由 Host 自动绑定
安装时选定卷上的媒体根，不限定 SD 卡，不需要选择目录。代码、资源与私有存档
由系统安装管理器管理。重新生成签名资源包的方法见
[资源接入说明](../common/recrof/resources/README.md)。

0.6.0 使用 MCU PAK 格式 3：模型顶点索引扩大为 10 位，三角形数据偏移扩大为
21 位，模型头使用独立的 `IDPL` 标记。模型最多 1024 个顶点，绘制临时缓冲为
32 KiB，引擎栈为 96 KiB；最大 Guest 线性内存仍固定为 3.5 MiB。旧格式 2 的
PAK 需要用当前转换器重新转换。拥有原作数据的用户仍可自行转换、导入；转换器
与程序必须来自同一个格式版本。打包工具不上传原作资源。

自由关卡包保留原始模型几何和动画，将皮肤按最近邻采样限制到 128×128，
不改变关卡几何；关卡出口重新开始本关。没有附带音乐或外部 QuakeC 程序。
其他 LibreQuake 关卡不能直接加入：仍须检查 BSP 节点、实体、字符串索引和
绘制工作区的边界，避免原 MCU 格式静默截断。

0.5.0 使用 Core 2.1 原生主循环、Audio 0.10 原采样率 PCM 和 GameRender 1.2
INDEX8 显示转换。世界渲染使用 Guest 原引擎，开启逐像素等价的光照系数复用和
按实际尺寸存储的光照网格缓存。大块资源及网格复制使用局部 `memory.copy`，
命令编码使用编译期布局检查；不改变渲染尺寸或采样质量。

光照缓存为 512 个标签加 96 KiB 数据池，设备上共 120 KiB，位于 Guest 线性
内存。地图切换失效，动态光照绕过，数据池耗尽先清空标签再复用。最大线性内存
仍固定为 3.5 MiB。原生主循环不使用 Asyncify
续体缓冲。不能把这些数字当成包含 Host、AOT、显示和音频的整机内存需求。

普通包不读取渲染探针文件。跨材质 Host 世界批次与同步 Host 混音继续默认关闭；
可选 Host 投影计划由固件的 `CONFIG_PXA_GAME_RENDER_PREPARE_KIB` 控制，默认 0。
实验中执行内核更快，但整帧仍慢于 Guest 世界渲染，故不默认增加 96 KiB 工作区。

构建：

```sh
bash tools/app.sh build quake --board doit-gamekit --source-root local/pxa-apps \
  --target esp32s31 --aot-only --output local/quake-build
```

最低目标仍为实际关卡 15 FPS，期望 20 FPS；编译成功或单独内核加速不等于
达到目标，更换素材后的设备帧率需要重新测量。

缓存和原引擎差分检查：

```sh
python local/pxa-apps/common/recrof/tests/run_quake_lightmap_cache.py
python local/pxa-apps/common/recrof/tests/run_quake_projected_spans.py
```

前者运行原生 ASan/UBSan 和实际 WASM bulk 路径。上游更新继续使用已有三方同步
工具，平台缓存代码位于 `quake/port/`；更新后须重新执行差分与实机验收。

正式入口为 `quake`，包 ID 为 `pxa-quake`；仅提供 SD 资源模式。资源导入 `.game-data/quake/files/`，不进入签名包。旧 `-sd` 身份的迁移步骤见 [公共说明](../common/recrof/README.md#固定-sd-资源与真机性能目标)。

桌面使用根据经典游戏标志重绘的透明图标，来源、尺寸与生成记录见 [图标说明](../common/recrof/ICONS.md)。
