# Duke3D 固定 SD 资源测试包

复用 `../duke3d` 引擎，代码包不携带 GRP。要求固件和 SDK 实现 FS 0.3 `app-media-root`，由 Host 按已验证身份自动绑定 `/PXA/media/<app-key>`，启动不申请文件权限或弹目录选择器。资源文件名为 `duke3d.grp`，使用共享移植说明中的 MG24 GRP 转换格式。私有存档目前仍在内部旧布局。

从 pxa-projects 工作区构建 GameKit 的 ESP32-S31 包：

```sh
tools/app.sh build duke3d-sd --board doit-gamekit --target esp32s31 \
  --source-root local/pxa-apps --output local/storage-fixed-games --aot-only
```

资源由安装或导入侧按身份预置到固定媒体根，Guest 不选择目标路径。本次设备资源通过开发用 PXADB 预置；通用资源导入界面和整应用 SD 安装仍待实现。工作区设计见 `docs/pxa-storage-layout.zh-CN.md`，实际场景、内存和帧率记录见 `docs/validation/2026-10-18-duke3d-sd.zh-CN.md`。

0.1.7 为待真机复测的 NVM 存储优化版本。私有 NOR 按需写入 `nvm-4194304/000` 至 `511` 的小页文件，每页 8,192 B 数据加 16 B 格式头；只有一份临时发布文件。旧 `state.nvm` 保留为只读迁移来源，新页关闭后使用 FS replace 发布，未变化的 NOR 位不重复写入。重开时有界枚举已有页，不逐个探测空页。最大 512 页，新增常驻索引和路径不足 1 KiB，不增加读取缓存，也不降低 4 MiB 线性内存上限。

新页覆盖优先于迁移来源；旧程序不识别这些覆盖文件，因此不能直接降级后继续写旧 NVM。迁移来源与新页会暂时并存，安装/运行仍需足够空间；当前未实现来源回收，也未完成掉电耐久性验收。遇到损坏页或枚举错误会停止，不把错误当作空白 NOR。

三个 ASan/UBSan 配置覆盖跨页 AND/擦除、无变化跳过、旧 dense/NVM2 迁移、重开、损坏页及发布失败后保留旧页。真实引擎在 Node 协议宿主中首次进入和重开均到达屋顶，使用 17 页共 139,536 B，迁移源 2,064 B，实际线性内存 4,063,232 B；这些不是设备帧率或启动稳定性结论。
