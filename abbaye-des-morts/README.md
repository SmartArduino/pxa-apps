# 死亡修道院 / L’Abbaye des Morts

独立 PXA 应用，使用 SDK 2.1 和共享 SDL guest 运行层。原版图像、音乐及代码许可保留在 `vendor/` 与 `assets/licenses/`；第三方资源的许可也保留在 `media/` 中。源代码版本和修改见 `vendor/UPSTREAM.json`。

方向键移动，A/Start 确认，A 跳跃，Select 返回，X 查看说明，Y 切换原版图像风格。标题页 R 清除进度并开始新游戏。

检查点、剩余生命和世界状态发生变化时原子保存；重新启动后自动继续。保存包含版本、长度和 CRC 校验，损坏文件不会覆盖新游戏状态。

画面按 4:3 保持比例适配屏幕，允许黑边；触摸悬浮按钮在手柄输入后隐藏。所有游戏资源通过签名资源包按需读取，不需要将整个包加载进内存。音频使用 PXA PCM，并兼容只提供 session API 的 Host。

在 PXA 固件工作区根目录构建，每次仅选一个架构：

```sh
tools/app.sh build abbaye-des-morts --target simulator --aot-only
tools/app.sh build abbaye-des-morts --target esp32s3 --aot-only
tools/app.sh build abbaye-des-morts --target esp32s31 --aot-only
```

构建需要该工作区的 SDK 资源编译器支持 `kind: archive`。不同架构应分别输出、安装或上传；桌面模拟器性能不代表设备帧率。
