# 游戏桌面图标

应用的 `package.json` 声明 `icon: assets/icon.png`。Quake、Quake II 和 Duke 保留按用户要求美化的徽记重绘版本；C&C 与 Red Alert 使用原创坦克图形，不使用 EA 游戏画面、阵营徽记或标志来推广移植。

| 应用 | 参考原图 | PNG 大小 |
| --- | --- | ---: |
| Quake | [id Software / WinQuake/quake.ico](https://github.com/id-Software/Quake/blob/bf4ac424ce754894ac8f1dae6a3981954bc9852d/WinQuake/quake.ico) | 8,375 B |
| Quake II | [id Software / win32/q2.ico](https://github.com/id-Software/Quake-2/blob/372afde46e7defc9dd2d719a1732b8ace1fa096e/win32/q2.ico) | 8,816 B |
| Duke Nukem 3D | [recrof / Game/duke_icon.png](https://github.com/recrof/Duke3D-ESP32S3/blob/9053c219c1dd87898fa5988ebbb68c951fbbe9fa/Duke3D-ESP32S3/Game/duke_icon.png) | 17,276 B |
| Command & Conquer | 原创 `command-conquer/assets/icon.svg`，CC0-1.0 | 14,764 B |
| Red Alert | 原创 `red-alert/assets/icon.svg`，CC0-1.0 | 14,210 B |

Quake、Quake II 和 Duke 使用内置 `image_gen` 重绘为 96×96 透明 PNG。C&C 和 Red Alert 的可编辑源为 SVG，通过 CairoSVG 渲染为 256×256 PNG，使用统一的圆角坦克图形及不同配色。图标与许可证随应用包签名，Host 在桌面和启动页加载，不增加游戏 Guest 的线性内存、常驻任务或渲染通道。解码器、Host 缓存及图片变换的开销另计。

资产来源、成品 SHA-256 和转换方式在 [icons.json](icons.json)。许可记录于各应用的 `assets/icon-license.txt`。游戏名称与原游戏标志仍归原权利方所有；本地审阅产物不随应用分发。

后续引擎同步只更新 `vendor/` 与已声明的上游映射，不覆盖这些桌面图标。若需要更新图标，修改资产并更新 `icons.json` 的哈希和提示词，再重新签名打包；不要将重绘图标标注为官方原图。

DOOM 使用原版第一章试玩 IWAD 中的 `M_DOOM` 图形，保留原像素与调色板，并置于原创圆角背景中。`doom/assets/wordmark.png` 保存原图形，`icon.svg` 保存可编辑布局；相关数据来源与许可独立于引擎 GPL，见该目录 `icon-license.txt`。


OpenLara 使用独立的经典《古墓丽影》低多边形画风：劳拉、青绿色背心、双枪及古墓石墙背景，96×96 不透明 PNG（22,329 B）。参考 [OpenLara 原图](https://github.com/XProger/OpenLara/blob/3b268ca4bf986c380f44a26ffd9218bf6d376249/src/platform/osx/Assets.xcassets/AppIcon.appiconset/icon512x512.png)，最终采用的提示词和哈希同样记录于 `icons.json`；金属徽章候选未采用。

六个应用名称使用各自的 `i18n/messages.yaml` 与中文 metadata catalog。`zh`、`zh-CN`、`zh-Hans` 提供简体名称，`zh-TW`、`zh-Hant` 提供繁体名称；其他语言回退到包的英文名称。无需 Guest 翻译表或新增服务，App ID、签名身份、存档和媒体目录与语言无关。
