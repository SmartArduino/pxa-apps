# 离线图片诊断

中文 | [English](README.en.md)

这个可选测试包独立检查 SDK/Host 图片解码、资源释放和显示，不依赖 Wi-Fi、网站脚本或浏览器 UI，也不给浏览器增加代码和默认缓冲。动画与按钮针对 296×240 的 pai-touch，属于诊断界面。

使用 Python 和 Pillow 生成自制图片，再用正常工具链构建：

```sh
python browser/tests/image-probe/generate.py
pxa build browser/tests/image-probe --target simulator --output /tmp/image-probe-sim
pxa build browser/tests/image-probe --target esp32s3 --output /tmp/image-probe-s3
```

按所在环境补充发布者签名参数，使用标准安装和启动命令。包名为 `pxa-image-probe`，不要求网络或音频权限，不替换原浏览器和数据。模拟器回归脚本为 `browser/tests/image_probe_smoke.py`，需要指定 `--simulator`、`--package`、`--publisher-key`、`--ime-package`、`--output`。它验证普通图、大图、释放重载、实际彩色像素与动画变化。

`Preparing` 阶段用 4064 B 固定缓冲把包内源文件写入本工具的私有目录。收到 `IMAGE PROBE READY` 后再开始测量。三个按钮分别为：

- **Decode**：把 File 所有权交给现有图片 SDK，Host 直接读取压缩输入；测量阶段没有整份输入经过 Guest 内存。
- **Release**：释放解码图；下一 Canvas 帧移除最后的显示引用。
- **Large**：先准备第二份源文件，再次点击 Decode 检查大图。

共同样本是 600×1200 的基线 JPEG，带合法 APP 元数据；本轮为 229,193 B。它低于旧缓冲的 256 KiB 扩容边界，可以在默认 512 KiB 临时预算内对比。大图是 1199×2533 / 90,591 B，用来验证有界 JPEG 后端超出旧百万像素限制的能力，不与共同样本比较速度。生成摘要保存在 `assets/manifest.json`，不同 Pillow 版本应重新核对大小和 SHA。

两张图均限制为宽 236、高 1024、32768 输出像素、512 KiB 压缩输入。日志中的 `elapsed_us` 是 SDK 请求至完成的墙钟时间，包括文件读取、Host 处理、排队和 Guest 调度，**不是纯 JPEG CPU 时间**。准备文件、截图和首次字体缓存要单独记录。

`MEMORY START/STOP` 界定每轮真实堆窗口；普通 `MEMORY` 和 PXA budget 高水位可能保留历史值。Canvas 最快每 33,333 µs 提交一次，使用固定命令包。可用 `PERF START display` / `PERF STOP` 读取面板完成间隔，用 `SCREENSHOT AFTER_PRESENT` 检查像素变化；软件数据仍需实际屏幕确认。结束后卸载本测试包和数据，包管理器可能保留一个空锁文件。
