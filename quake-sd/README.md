# Quake SD

资源从固定的 `/PXA/media/<AppKey>/` 自动绑定，无需选择目录。资源名称与转换步骤见
[公共说明](../common/recrof/TEST_DATA.md)。代码与私有存档由系统安装管理器管理。

0.4.24 使用 Core 2.1 原生主循环、Audio 0.10 原采样率 PCM 和 GameRender 1.2
INDEX8 显示转换。世界渲染使用 Guest 原引擎，开启逐像素等价的光照系数复用和
按实际尺寸存储的光照网格缓存。大块资源及网格复制使用局部 `memory.copy`，
命令编码使用编译期布局检查；不改变渲染尺寸或采样质量。

光照缓存为 512 个标签加 96 KiB 数据池，设备上共 120 KiB，位于 Guest 线性
内存。地图切换失效，动态光照绕过，数据池耗尽先清空标签再复用。最大线性内存
仍固定为 3.5 MiB。引擎栈 64 KiB，链接栈预算 80 KiB；原生主循环不使用 Asyncify
续体缓冲。不能把这些数字当成包含 Host、AOT、显示和音频的整机内存需求。

普通包不读取渲染探针文件。跨材质 Host 世界批次与同步 Host 混音继续默认关闭；
可选 Host 投影计划由固件的 `CONFIG_PXA_GAME_RENDER_PREPARE_KIB` 控制，默认 0。
实验中执行内核更快，但整帧仍慢于 Guest 世界渲染，故不默认增加 96 KiB 工作区。

构建：

```sh
bash tools/app.sh build quake-sd --board doit-gamekit --source-root local/pxa-apps \
  --target esp32s31 --aot-only --output local/quake-build
```

最低目标仍为实际关卡 15 FPS，期望 20 FPS。完整实机窗口、包摘要、输入与内存结果
记录在主工作区 `docs/validation/2026-10-21-quake-host-prepare.zh-CN.md`；编译成功或
单独内核加速都不等于达到目标。

缓存和原引擎差分检查：

```sh
python local/pxa-apps/common/recrof/tests/run_quake_lightmap_cache.py
python local/pxa-apps/common/recrof/tests/run_quake_projected_spans.py
```

前者运行原生 ASan/UBSan 和实际 WASM bulk 路径。上游更新继续使用已有三方同步
工具，平台缓存代码位于 `quake/port/`；更新后须重新执行差分与实机验收。
