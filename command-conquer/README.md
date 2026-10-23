EA has not endorsed and does not support this product.

# Command & Conquer

商城版一次安装程序及独立签名的试玩资源。依据 [EA C&C 模组规则](https://www.ea.com/games/command-and-conquer/news/modding-faq)，仅免费、非商业分发，不含原版音乐；保留关卡、画面、语音及音效。资源自动绑定到 Host 分配的应用媒体根，系统管理安装卷，可用 SD 卡或其他可安装存储卷，无需选择目录，不依赖内部存储分区。资源构建入口见 [资源说明](../common/recrof/resources/README.md)。

真机最低目标为实际关卡 15fps；构建成功不代表通过真机验收。验证产物仅保存在工作区 `local/artifacts/`。Clock 0.3 协商 8ms，旧 Host 回退 16ms。

正式入口为 `command-conquer`，包 ID 为 `pxa-command-conquer`。开发资源可导入 `.game-data/command-conquer/files/`；该本地目录不会自动上传。旧 `-sd` 身份的迁移步骤见 [公共说明](../common/recrof/README.md#固定-sd-资源与真机性能目标)。

桌面图标使用原创坦克 SVG，不使用 EA 游戏图形或阵营徽记；可编辑源及许可保存在 `assets/`。

Vanilla-Conquer 的选曲逻辑已适配没有音乐文件的资源包：空播放列表停止选曲，随机模式只有一首歌时允许重复，顺序模式最多遍历一次列表。音效和语音仍可正常使用，避免原先不断寻找下一首歌导致的卡死。
