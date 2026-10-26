# Neverputt

独立 PXA 应用，使用 SDK 2.1 和共享 SDL guest 运行层。原版图像、音乐及代码许可保留在 `vendor/` 与 `assets/licenses/`；第三方资源的许可也保留在 `media/` 中。源代码版本和修改见 `vendor/UPSTREAM.json`。

A 确认/击球，B 返回，Start 暂停；击球前左右调整方向，上下调整力度，X 精细调整。触摸可点击菜单、拖动调整方向和力度。

保留原版设置和球场选择；计分使用原版当前比赛逻辑。未新增中途退出比赛的即时存档。提供 9 组原版球场及 basic ball。3D 场景以 160×120 渲染并原地放大，菜单/文字保持 320×240，关闭阴影、反射和 HMD。

画面按 4:3 保持比例适配屏幕，允许黑边；触摸悬浮按钮在手柄输入后隐藏。所有游戏资源通过签名资源包按需读取，不需要将整个包加载进内存。音频使用 PXA PCM，并兼容只提供 session API 的 Host。

在 PXA 固件工作区根目录构建，每次仅选一个架构：

```sh
tools/app.sh build neverputt --target simulator --aot-only
tools/app.sh build neverputt --target esp32s3 --aot-only
tools/app.sh build neverputt --target esp32s31 --aot-only
```

构建需要该工作区的 SDK 资源编译器支持 `kind: archive`。不同架构应分别输出、安装或上传；桌面模拟器性能不代表设备帧率。
