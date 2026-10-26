# Project: Starfighter

独立 PXA 应用，使用 SDK 2.1 和共享 SDL guest 运行层。原版图像、音乐及代码许可保留在 `vendor/` 与 `assets/licenses/`；第三方资源的许可也保留在 `media/` 中。源代码版本和修改见 `vendor/UPSTREAM.json`。

方向键移动，A 辅助射击/确认，B/X 主射击，Start 暂停，L 返回，Select/R 切换武器。保留原版手柄按键语义。

保留原版存档和设置，写入应用私有目录。世界逻辑保留 640×480，RGB565 画面按长宽比缩放到屏幕。

画面按 4:3 保持比例适配屏幕，允许黑边；触摸悬浮按钮在手柄输入后隐藏。所有游戏资源通过签名资源包按需读取，不需要将整个包加载进内存。音频使用 PXA PCM，并兼容只提供 session API 的 Host。

在 PXA 固件工作区根目录构建，每次仅选一个架构：

```sh
tools/app.sh build starfighter --target simulator --aot-only
tools/app.sh build starfighter --target esp32s3 --aot-only
tools/app.sh build starfighter --target esp32s31 --aot-only
```

构建需要该工作区的 SDK 资源编译器支持 `kind: archive`。不同架构应分别输出、安装或上传；桌面模拟器性能不代表设备帧率。
