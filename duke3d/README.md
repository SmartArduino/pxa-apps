# Duke Nukem 3D

正式入口为 `duke3d`，包 ID 为 `pxa-duke3d`，版本 0.2.0。包中只携带引擎和少量元数据；
资源通过 Host 的固定 `app-media-root` 读取，通常位于 `/PXA/media/lucinhu/pxa-duke3d/`，
不弹出目录选择器。应用代码和私有存档由 Host 的存储策略管理。

使用 MG24 转换后的 `duke3d.grp`。先用 [导入工具](../common/recrof/tools/import_data.py)
放到 `.game-data/duke3d/files/`，再通过部署工具传入 Host 查询到的目录。
旧 `-sd` 身份的存档、资源迁移见 [公共说明](../common/recrof/README.md#固定-sd-资源与真机性能目标)。

保留已在设备验证的原生配置：Core 2.1 主循环、Audio 0.10 原采样率 PCM、
GameRender 1.2 索引图像转换；128 KiB 引擎栈、16 KiB 回调栈，
不分配 Asyncify 续体；192 页虚拟 Flash 资源缓存、4 页 NVM 只读缓存及分页 NVM，最大线性内存 5 MiB。Clock 优先请求 8ms，不支持时回退 16ms；
32ms 为计算期间的协作预算。原有显示、音频和手柄映射继续使用公共移植层。

构建：

```sh
bash tools/app.sh build duke3d --board doit-gamekit --source-root local/pxa-apps \
  --target esp32s31 --aot-only --output local/duke3d-build
```

历史真机性能记录保留旧包版本和名称，见 `common/recrof/VALIDATION.md` 与工作区
`docs/validation`。本次改名不会自动提高帧率；新版本是否达到 15–20fps 仍以实际关卡测量为准。
上游同步仍使用 `common/recrof/tools/upstream.py`，平台层与 vendor 修改保持原有三方合并方式。

若此前构建过旧回调版本，首次切换请指定新的 `PXA_BUILD_CACHE_DIR`；构建会拒绝执行模型与清单不一致的旧缓存。

桌面使用根据经典游戏标志重绘的透明图标，来源、尺寸与生成记录见 [图标说明](../common/recrof/ICONS.md)。
