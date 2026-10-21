# 游戏桌面图标

前五个正式应用的 `package.json` 均声明 `icon: assets/icon.png`。这些是按用户要求美化的重绘版本，并非未经修改的官方原图。保留各游戏的标志辨识度：Quake 单尖徽记、Quake II 双尖徽记、Duke 金色辐射标志、C&C 的 GDI/Nod 双徽章和 Red Alert 的红金徽章。

| 应用 | 参考原图 | PNG 大小 |
| --- | --- | ---: |
| Quake | [id Software / WinQuake/quake.ico](https://github.com/id-Software/Quake/blob/bf4ac424ce754894ac8f1dae6a3981954bc9852d/WinQuake/quake.ico) | 8,375 B |
| Quake II | [id Software / win32/q2.ico](https://github.com/id-Software/Quake-2/blob/372afde46e7defc9dd2d719a1732b8ace1fa096e/win32/q2.ico) | 8,816 B |
| Duke Nukem 3D | [recrof / Game/duke_icon.png](https://github.com/recrof/Duke3D-ESP32S3/blob/9053c219c1dd87898fa5988ebbb68c951fbbe9fa/Duke3D-ESP32S3/Game/duke_icon.png) | 17,276 B |
| Command & Conquer | [EA / CONQUER.ICO](https://github.com/electronicarts/CnC_Tiberian_Dawn/blob/e0d372dc582c053a699114942b984dad0457f9b3/CONQUER.ICO) | 15,721 B |
| Red Alert | [EA / CODE/REDALERT.ICO](https://github.com/electronicarts/CnC_Red_Alert/blob/0dc09bb1d6188d1ec79ab4765b22a48b75ffee32/CODE/REDALERT.ICO) | 14,262 B |

使用内置 `image_gen` 分别重绘，保留透明背景；最终仅缩放为 96×96 RGBA PNG，未使用代码重新绘制徽记。图标与许可证随应用包签名，游戏地图、音效等数据仍只从固定 SD 资源根读取。Host 在桌面和启动页加载图标；不增加游戏 Guest 的线性内存、常驻任务或渲染通道。单张 RGBA 像素数据为 36 KiB；解码器、缓存及图片变换的实际开销另计。

完整生成提示词、原图链接、原图与成品 SHA-256、转换方式在 [icons.json](icons.json)。保留上游许可文本于各应用的 `assets/icon-license.txt`。游戏名称与标志仍归原权利方所有。原始参考和重绘大图的本地审阅副本位于工作区 `build/game-unify-20261021/icon-sources/` 与 `icon-artwork/`，不随 `.pxa` 分发。

后续引擎同步只更新 `vendor/` 与已声明的上游映射，不覆盖这些桌面图标。若需要更新图标，修改资产并更新 `icons.json` 的哈希和提示词，再重新签名打包；不要将重绘图标标注为官方原图。


OpenLara 使用独立的经典《古墓丽影》低多边形画风：劳拉、青绿色背心、双枪及古墓石墙背景，96×96 不透明 PNG（22,329 B）。参考 [OpenLara 原图](https://github.com/XProger/OpenLara/blob/3b268ca4bf986c380f44a26ffd9218bf6d376249/src/platform/osx/Assets.xcassets/AppIcon.appiconset/icon512x512.png)，最终采用的提示词和哈希同样记录于 `icons.json`；金属徽章候选未采用。

六个应用名称使用各自的 `i18n/messages.yaml` 与中文 metadata catalog。`zh`、`zh-CN`、`zh-Hans` 提供简体名称，`zh-TW`、`zh-Hant` 提供繁体名称；其他语言回退到包的英文名称。无需 Guest 翻译表或新增服务，App ID、签名身份、存档和媒体目录与语言无关。
