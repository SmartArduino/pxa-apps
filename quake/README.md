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

玩家、犬类、门及部分怪物的受伤回调沿用生成代码的无参数签名，其他受伤回调
接收攻击者和伤害值。`port/callbacks.c` 按真实签名分发，避免 WebAssembly
间接调用因强转不同函数类型而退出。修改或同步 QuakeC 生成代码后，运行
`common/recrof/tests/test_quake_pain_callbacks.py --output <本地产物目录>`，
检查全部已注册回调的签名覆盖情况。

掌机固定布局：方向键前后左右移动，X/B 抬头与低头，Y/A 左右转动视角，
R 开火、L 跳跃、SELECT 使用、START 打开菜单。移植层在游戏内按下这些按键时设置
对应引擎绑定，因此旧存档配置或素材中缺少 `MOUSE1`、逗号和句号绑定时，
实体键和屏幕按钮仍能工作。菜单中的方向选择、确认和返回不变。更改输入映射后，
运行 `common/recrof/tests/test_quake_controls.py --output <本地产物目录>`，
验证手柄状态经过真实按键绑定后生成的引擎命令及松键行为。

0.5.0 使用 Core 2.1 原生主循环、Audio 0.10 原采样率 PCM 和 GameRender 1.2
INDEX8 显示转换。世界渲染使用 Guest 原引擎，开启逐像素等价的光照系数复用和
按实际尺寸存储的光照网格缓存。大块资源及网格复制使用局部 `memory.copy`，
命令编码使用编译期布局检查；不改变渲染尺寸或采样质量。

光照系数复用路径中，二次幂尺寸贴图的连续重复采样可直接复用上一个像素。
非二次幂贴图仍保留带小数坐标的倒数采样；坐标转换先转成无符号数再左移，
避免负数左移的未定义行为。此优化不增加纹理缓存空间。

光照缓存为 512 个标签加 96 KiB 数据池，设备上共 120 KiB，位于 Guest 线性
内存。地图切换失效，动态光照绕过，数据池耗尽先清空标签再复用。最大线性内存
仍固定为 3.5 MiB。原生主循环不使用 Asyncify
续体缓冲。不能把这些数字当成包含 Host、AOT、显示和音频的整机内存需求。

普通包不读取渲染探针文件，使用应用内缓存世界渲染和 Host 显示转换。
投影、深度、纹理采样和光照始终留在应用内。Host 没有世界采样接口或
投影工作区；同步 Host 混音实验默认关闭。

天空在应用内用原始双层贴图合成，保留原调色板和动画节奏。固定 48 KiB
保存双层素材与当前帧，避免逐帧读取预生成的 2 MiB 天空动画，也不占用
墙面纹理的双缓冲。纹理预取跳过天空表面；预取地址不匹配时，所有平台
都重新加载实际纹理，避免抬头或路口出现黑白错纹。

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
python local/pxa-apps/common/recrof/tests/run_quake_lit_span.py
python local/pxa-apps/common/recrof/tests/run_quake_texture_cache.py
python local/pxa-apps/common/recrof/tests/run_quake_sky_cache.py
python local/pxa-apps/common/recrof/tests/run_host_indexed_blit.py
```

前者运行原生 ASan/UBSan 和实际 WASM bulk 路径。上游更新继续使用已有三方同步
工具，平台缓存代码位于 `quake/port/`；更新后须重新执行差分与实机验收。
纹理测试执行实际预取状态机，覆盖天空切换、错误预取、mip 和缓冲边界；
天空测试与当前 PAK 转换器逐像素比较，均包含原生检查和实际 WASM 执行。

正式入口为 `quake`，包 ID 为 `pxa-quake`；仅提供 SD 资源模式。资源导入 `.game-data/quake/files/`，不进入签名包。旧 `-sd` 身份的迁移步骤见 [公共说明](../common/recrof/README.md#固定-sd-资源与真机性能目标)。

桌面使用根据经典游戏标志重绘的透明图标，来源、尺寸与生成记录见 [图标说明](../common/recrof/ICONS.md)。
