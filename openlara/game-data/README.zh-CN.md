# 原版资源入口

把自己持有的原版 PC《古墓丽影 1》资源放在这里：

- `DATA/`：完整原版的 21 个 `.PHD` 文件，保持原始文件名。
- 或直接放入 `GAME.GOG`、`.ISO`、`.BIN` 光盘镜像之一。
- 可选 `audio/`：原版编号 MP3/OGG/WAV/FLAC 配乐。

随后从 workspace 根目录执行：

```sh
python3 local/pxa-apps/openlara/tools/prepare-data.py --upstream local/upstream-openlara
./tools/dev.sh sim --board doit-gamekit openlara
```

准备脚本会自动识别这里的资源；不完整或损坏时报告缺失项并保留已安装资源。没有原版文件时继续使用固定散列的公开 City 演示数据。商业游戏二进制被本目录的 `.gitignore` 排除。

原版资源可来自 [GOG 原版合集](https://www.gog.com/en/game/tomb_raider_123) 或已持有的 Steam/光盘安装。[OpenLara 上游](https://github.com/XProger/OpenLara) 提供引擎与演示；[TRX 官方安装说明](https://lostartefacts.dev/trx/install_guide/) 同样要求主线资源来自原版安装，公开直接下载项是演示和部分扩展包。

真机也可以直接复制 `DATA` 文件夹到 OpenLara 的 SD 媒体目录，选择暂停菜单的 **Import game data**，按 A 扫描。识别大小写文件名，扫描后回到标题，New Game 从已安装的首个主线关开始。

默认发布者在 SD 卡中的固定目录如下；电脑部署脚本通过 Host 查询实际挂载点，不需要选择设备目录：

```text
/PXA/media/lucinhu/pxa-openlara/DATA/
```

这里只放 `.PHD`；已有 `tracks.ad4`、`title.scr`、`title.pal` 等基础媒体保留在上一级。原始 `GAME.GOG` 需先用上面的电脑端脚本提取，不在真机上解压。外部 FMV 视频尚未接入。

电脑端准备结果统一保存在 `.game-data/openlara/files/`，不再放进 `assets-simulator`，模拟器与设备均走外部媒体根。
