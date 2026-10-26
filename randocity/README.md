# Rando City

Original MIT procedural motorcycle city: free play, edge race and crazy courier with complete source.

PXA SDK 2.1 / Core 2 移植，保留归档中的游戏实现与许可证。使用[共享运行层](../common/retro-games/README.md)的显示适配、生命周期和诊断配置。

在固件工作区根目录构建：

```sh
tools/app.sh build randocity --target simulator,esp32s3,esp32s31 --aot-only \
  --output "local/artifacts/retro-build/$(date +%Y%m%d-%H%M%S)/randocity"
```

操作、声音与存档范围见[运行层说明](../common/retro-games/README.md)。游戏来源固定在本目录的 `upstream-lock.json`、`source-lock.json` 或共享引擎的来源记录中；各 `LICENSE`、`COPYING` 和 `NOTICE` 文件分别保留原授权。构建不下载游戏源码。
