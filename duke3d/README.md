# Duke Nukem 3D

版本 0.3.0 随安装提供完整的官方第一章试玩原包、未修改的 GRP/RTS 运行文件与
原始分发条款。资源由系统安装到所选存储卷并提供只读 `app-media-root`，不限定
SD 卡。程序、资源和私有存档均由系统管理，不需要用户选择目录或转换资源。

手柄方向键移动和平移，X/B 抬头与低头，Y/A 左右转向，R 开火、L 跳跃。
按住 SELECT 后按 R 切换下一把武器，按 L 切换上一把；每次按下只切换一次，
松开后可再次切换。SELECT 单按在松开时执行“使用”，组合键不触发使用、开火或跳跃；
先松开 SELECT 时，肩键也须松开后才恢复普通动作。START 打开菜单，菜单里 A 确认、B 返回。
触屏右上角 WPN 切换下一把武器。只能选择当前已拥有且符合引擎切换条件的武器。
切武器和 SELECT 单按的短指令保留到实际游戏更新处理后再松开，避免画面轮询
先消耗掉短按；后台、菜单切换和控制器断开时取消尚未处理的指令。

引擎直接解析原始 VOC：8 位 PCM 原位读取，16 位 PCM 按小块转换，跨音频块的
读取只保存至多 8 段的索引。索引存入已有的关卡缓存。多轨 MIDI 用各轨游标稳定
合并到 64 KiB 缓冲，不分配事件列表或完整输入副本，不产生磁盘转换缓存。
已转换的 WAV/单轨 MIDI GRP 仍可读取。资源再分发条件来自原试玩协议，不能
把引擎 GPL 当成完整商业游戏素材的授权。可重现打包方法见
[资源接入说明](../common/recrof/resources/README.md)。

保留已在设备验证的原生配置：Core 2.1 主循环、Audio 0.10 原采样率 PCM、
GameRender 1.2 索引图像转换；128 KiB 引擎栈、16 KiB 回调栈，
不分配 Asyncify 续体；192 页虚拟 Flash 资源缓存、4 页 NVM 只读缓存及分页 NVM，最大线性内存 5.5 MiB。首次前台 Clock 事件启动引擎后关闭周期时钟，
主循环通过事件端口处理输入、文件完成通知与等待截止时间；32ms 为计算期间的协作预算。
原有显示、音频和手柄映射继续使用公共移植层。

临时预览、图块指针表以及死亡后重试用的内部检查点使用 224 KiB 会话内存，
随关卡初始化生成，退出后释放。这样可减少进入关卡和死亡后重试时的 SD 写入。
用户存档槽与设置继续使用原来的私有 NVM 地址、文件格式和 NOR 擦写规则。
范围跨界、标记地址、擦除、重启保留及持久化隔离的 ASan/UBSan 检查：

```sh
python3 local/pxa-apps/duke3d/tests/run_transient.py \
  --output local/artifacts/duke-transient/$(date +%Y%m%d-%H%M%S)
```

构建：

```sh
bash tools/app.sh build duke3d --board doit-gamekit --source-root local/pxa-apps \
  --target esp32s31 --aot-only --output local/duke3d-build
```

新素材和音频读取路径的设备帧率需重新测量；模拟器帧率不能代替实机结果。
上游同步仍使用 `common/recrof/tools/upstream.py`，平台层与 vendor 修改保持原有三方合并方式。

若此前构建过旧回调版本，首次切换请指定新的 `PXA_BUILD_CACHE_DIR`；构建会拒绝执行模型与清单不一致的旧缓存。

桌面使用根据经典游戏标志重绘的透明图标，来源、尺寸与生成记录见 [图标说明](../common/recrof/ICONS.md)。
