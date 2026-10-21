# Quake II

资源自动绑定到 Host 分配的固定 SD 媒体根（通常是 `/PXA/media/lucinhu/pxa-quake2/`），无需选择目录。资源名称和转换步骤见 [公共说明](../common/recrof/TEST_DATA.md)。引擎代码与私有存档由 Host 的应用存储策略管理，不假设有内部存储分区。

真机最低目标为实际关卡 15fps；构建成功不代表通过真机验收。测试结果记录在工作区 `docs/validation`。Clock 0.3 协商 8ms，旧 Host 回退 16ms；保持原始画质和音频。

正式入口为 `quake2`，包 ID 为 `pxa-quake2`；仅提供 SD 资源模式。资源导入 `.game-data/quake2/files/`，不进入签名包。旧 `-sd` 身份的迁移步骤见 [公共说明](../common/recrof/README.md#固定-sd-资源与真机性能目标)。

桌面使用根据经典游戏标志重绘的透明图标，来源、尺寸与生成记录见 [图标说明](../common/recrof/ICONS.md)。
