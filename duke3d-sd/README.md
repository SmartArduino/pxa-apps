# Duke3D 固定 SD 资源测试包

复用 `../duke3d` 引擎，代码包不携带 GRP。要求固件和 SDK 实现 FS 0.3 `app-media-root`，由 Host 按已验证身份自动绑定 `/PXA/media/<app-key>`，启动不申请文件权限或弹目录选择器。资源文件名为 `duke3d.grp`，使用共享移植说明中的 MG24 GRP 转换格式。私有存档目前仍在内部旧布局。

从 pxa-projects 工作区构建 GameKit 的 ESP32-S31 包：

```sh
tools/app.sh build duke3d-sd --board doit-gamekit --target esp32s31 \
  --source-root local/pxa-apps --output local/storage-fixed-games --aot-only
```

资源由安装或导入侧按身份预置到固定媒体根，Guest 不选择目标路径。本次设备资源通过开发用 PXADB 预置；通用资源导入界面和整应用 SD 安装仍待实现。工作区设计见 `docs/pxa-storage-layout.zh-CN.md`，实际场景、内存和帧率记录见 `docs/validation/2026-10-18-duke3d-sd.zh-CN.md`。
