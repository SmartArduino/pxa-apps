# 三国逐城

原创 PXA 单人三国策略游戏。新建战役后直接选择刘备、曹操或孙权，默认从赤壁前夕起兵，也可以在同一屏换到另两个历史时期。在 64 季内统一 32 城。各时期的初始城池、驻军、可用武将和夺城悬赏不同。

| 剧本 | 开局局势 |
| --- | --- |
| 208 年 · 赤壁前夕 | 曹军已入荆州，刘备驻夏口，孙权以柴桑为据。 |
| 214 年 · 益州攻守 | 刘备进入益州，刘璋守成都，曹军与孙权分守淮南与江东。 |
| 219 年 · 襄樊相持 | 汉中易手，刘备军与曹军在襄樊对峙，江东可从长江进兵。 |

城池和人物以汉末史料为背景。游戏从相应时点展开架空战役，驻军、道路连通与悬赏按玩法配置。舆图使用古城近似坐标及现代地貌。秣陵、许都随纪年显示后来的建业、许昌。

## 怎样玩

- **治城**　选己方城，点「城池内政」可征募、屯田、筑城、兴市、安民和巡访。民心影响征募与季收入。任命太守可加快民心回升、补充守军并增加收入，也会占用一名可领军武将。每座城可巡访一次，收益可能是金、粮、军械或历练。
- **行军**　点己方出发城，再点相邻目标。再点一次己方目标可把它设为新的出发城。出征可选先锋、偏师或主力，并任命领军者。急行立即在地图上移动。扎营围城等下一季抵达，降低会战中有效城防。最多可同时保有三路部队，回到「军议 → 军情」可查看悬赏、军列和撤营。
- **领军**　每方八名武将，另可招降一名敌方客将。武将按历练提升至五级，连续作战会积累疲劳，换将或等季末休整。军械可从巡访、夺城和悬赏获得。青锋剑、山文甲、赤绒鞍和行军策分别改善输出、减伤、粮耗和初始军令。名册中的「军械」也能花金操练。
- **会战**　枪克骑、骑克弓、弓克枪。山地利枪、水乡利弓、平原利骑。看敌令再换阵，并在合适时机用将技。「号令」里可下达强攻、固守或迂回，战术持续两合，水乡无法迂回。打过第一合，可消耗军令发起单挑。突刺克佯攻，佯攻破格挡，格挡挡突刺。三合结果会改变双方军心。胜负用独立题字呈现，青铜窗框和体力条由透明图块拼接。
- **俘将与外交**　会战胜利后有机会俘获敌方武将，可放归安民、索赎得金，或花金招为客将。军议还可通商买粮、招集边军和议和三季，对方城外有己方军列时可先撤兵再议和。

触屏点选或拖动地图。手柄用方向键选择，A 确认，B 返回或暂停。经营、出兵、季节推进和战后处置会保存进度。升级可读取十城与三十二城旧档。离开未结束的会战后，继续战役会回到会战之前的存档节点。

## 运行与打包

在 PXA 固件工作区根目录运行。

    tools/dev.sh sim han-banners --board doit-gamekit-2 --source-root local/pxa-apps
    tools/app.sh build han-banners --target all --board doit-gamekit-2 --source-root local/pxa-apps

多架构包包含 Wasm、桌面 AOT 及 ESP32-S31、ESP32-S3、ESP32-P4 AOT。应用需要包中声明版本的 PXA GameRender 和 Assets 服务。

## 资料与资源

- 玩法参考宇峻奥汀的《三国群英传》内政、战略地图、会战循环，以及《霸王的大陆》的武将培养、内政与单挑。场景、规则和数据为本作设计。 [《三国群英传》官方介绍](https://www.uj.com.tw/uj/Games/GamesContent.aspx?p=34)
- 开局参考《三国志》与《华阳国志》。 [《先主传》](https://zh.wikisource.org/wiki/三國志/卷32) · [《吴主传》](https://zh.wikisource.org/wiki/三國志/卷47) · [《华阳国志》卷六](https://ctext.org/wiki.pl?chapter=224311&if=gb&remap=gb)
- [原始图片、透明逐帧图和生成提示词](art/README.md)。使用 Pillow 重跑 tools/pack_art.py 可重建游戏图集。
- 小屏使用整数倍 Z Labs Pixel 字形。可用视口达到约 557 × 383 像素时，标题、正文和战斗数字会按实际像素字号栅格化 Han Banners Sans，并缓存字形。该字体是思源黑体 CN Medium 的改名子集，可用 tools/subset_sans.py 传入上游字体文件重建。 [像素字许可](assets/ZLabs-Pixel-OFL.txt) · [黑体许可](assets/Han-Banners-Sans-OFL.txt) · [黑体来源](tools/fonts/sans.json)
- 舆图来自 Natural Earth II 1:50m 与 Natural Earth 1:10m 河川线。向 tools/build_terrain.py 传入 NE2_50M_SR_W.zip 和 ne_10m_rivers_lake_centerlines.zip 可重建底图。 [底图](https://www.naturalearthdata.com/downloads/50m-raster-data/50m-natural-earth-2/) · [水系](https://www.naturalearthdata.com/downloads/10m-physical-vectors/) · [公有领域条款](https://www.naturalearthdata.com/about/terms-of-use/)
