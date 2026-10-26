# C-Dogs SDL

独立 PXA 应用，使用 SDK 2.1 和共享 SDL guest 运行层。原版图像、音乐及代码许可保留在 `vendor/` 与 `assets/licenses/`；第三方资源的许可也保留在 `media/` 中。源代码版本和修改见 `vendor/UPSTREAM.json`。

方向键移动菜单/角色，A 确认/射击，B 返回/切换武器，R 投掷手雷，Select 地图，Start 暂停。选人时按 A 加入，再移动到 Done 确认。

保留原版战役进度、自动保存、分数和配置。当前 PXA 版本提供离线单机；网络/局域网加入和战役 ZIP 编辑不可用。音效保留稳定的句柄，使用 2 MiB 解码缓存，正在播放的声音不会被逐出；背景音乐只打开当前曲目。精灵纹理按原始像素分配建立缓存，避免临时 Pic 副本逐帧生成纹理。原版逻辑使用 70 Hz 上限，不能将它当作 LCD 刷新率。

画面按 4:3 保持比例适配屏幕，允许黑边；触摸悬浮按钮在手柄输入后隐藏。所有游戏资源通过签名资源包按需读取，不需要将整个包加载进内存。音频使用 PXA PCM，并兼容只提供 session API 的 Host。

在 PXA 固件工作区根目录构建，每次仅选一个架构：

```sh
tools/app.sh build cdogs-sdl --target simulator --aot-only
tools/app.sh build cdogs-sdl --target esp32s3 --aot-only
tools/app.sh build cdogs-sdl --target esp32s31 --aot-only
```

该应用保留较完整的原版资源，当前线性内存预算为 28 MiB；目标设备需要足够的可用内存，生成 S3 包不代表小内存 S3 板卡能够运行。

构建需要该工作区的 SDK 资源编译器支持 `kind: archive`。不同架构应分别输出、安装或上传；桌面模拟器性能不代表设备帧率。
