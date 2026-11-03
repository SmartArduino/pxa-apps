# 星壤物语 Starsoil

原创 2D 像素沙盒。探索森林、草甸、沙漠、雪原、雨林与地下晶洞，采矿、建造、种树、合成装备，与村民交易并挑战星核守卫。支持中文和英文、触摸和手柄；穿戴的衣服、护甲、武器和披风会随角色动画显示。

## 开始探索

在世界列表创建世界，可选择尺寸并输入种子；相同尺寸和种子生成相同地形。最多保留八个独立世界。从小屋出发，先拜访向导，再向右探索矿井。采集木材和石头，制作工作台与熔炉；用矿石和煤炼锭，再使用铁砧制作装备。合成手册会显示所需材料和工作设施。

铜镐可以开采金矿，铁镐可以开采晶体。橡子可以种在草地或泥土上。按住向下键放置木板可建造背景墙。床可以设置回家位置，回城药水可返回；死亡也会回家并保留背包。

所有镐和斧都可以挥击面前的敌人，并在采矿、砍树时同时造成伤害；高等级工具伤害更高，剑的攻击更快、范围更远。弓使用箭，星杖消耗魔力。

人工木板或玻璃背景墙可阻止刷怪，小型封闭建筑也会受到保护；大型房间应铺满背景墙。木门占两格，整体放置、开关和拆除；关闭时会挡住怪物，但苔木精能够推门。门下的地板、墙壁若留有缺口，外面的怪物仍可能进入。

背包和储物箱支持长按约 0.3 秒后拖动：放到空格可移动，放到不同物品上可交换，同类物品自动合并至每格 999，余量保留在原格；放到格子外或取消触摸不会改变物品。单击背包格即更换手持物品，关闭背包后直接使用，无需移入快捷栏。药水和装备可快速双击或点击“使用 / 穿戴”使用；工具和方块双击会拿在手上并返回场景。选用的背包物品在快捷栏右侧单独显示，点击它可回到背包，选择及排列会随世界保存。

直接轻点门、宝箱、储物箱、床、工作台、熔炉、铁砧或 NPC 可进行对应互动，拿着任何镐、斧也一样；距离过远时需先靠近。手持镐斧点按场景时，松手前不足 0.3 秒只互动，按住约 0.3 秒后才开始挥动、采集或拆除；长按松手不会再触发互动，拖动或取消也不会误点家具。右侧使用按钮和手柄 B 保持直接挥动，互动按钮也可随时主动操作家具。

跳跃短按约 3.5 格、长按约 5.5 格，可登上五格高的台阶；云靴仍提供二段跳。普通怪刷新更稀疏：白天地表每 12 秒最多 2 只、夜间每 9 秒最多 3 只、地下每 10 秒最多 4 只，守卫在场时不追加普通怪；出生点与角色、现有敌人保持距离，房屋免刷规则继续生效。

## 操作

默认开启智能选块：手柄和悬浮使用按钮沿操作方向寻找可达目标，青色选框表示当前可用。镐会跳过空格和无法开采的矿物，斧头优先树干；按住使用会保留当前进度，完成后接续下一块。默认不挖脚下地板，按住下方向可向下开采，上下配合左右可处理斜向地形。建造会检查支撑、人物占位、两格门与树苗土壤，持续铺桥保持同一层，重复背景墙不扣材料。暂停菜单的“智能选块”可切换到原来的固定选块；模式和语言一起保存，兼容旧语言设置。直接点按或拖动场景始终使用精确目标。

| 功能 | 触摸 | 手柄 |
|---|---|---|
| 移动、瞄准 | 悬浮摇杆；点按或拖动场景选择目标 | 方向键；上下调整瞄准 |
| 跳跃 | 跳跃按钮 | A |
| 挖掘、砍树、攻击、放置 | 长按使用 | 长按 B |
| 背包、合成 | 右上角背包、合成 | X、Y |
| 切换手持物品 | 点击快捷栏 | L、R |
| 家具、村民、宝箱 | 直接轻点，手持镐斧也可互动；或用互动按钮 | SELECT |
| 暂停 | 菜单按钮 | START |

在背包里双击装备，或选中后按 A 穿戴。衣服、护甲和披风会更新角色外观；再次操作可以卸下。弓需要箭，星杖消耗可恢复的魔力；云靴提供二段跳，辉光项链照亮周围。按住向下可落到平台下方。水会减缓下落，熔岩会造成伤害。

村民提供引导与交易。探索地下宝箱、阅读远行指南中的生物页，并在祭坛制作星核，随后互动召唤守卫。

## 保存与迁移

游戏使用 PXA 应用私有文件系统保存世界，每八秒自动保存，暂停菜单也会保存。世界文件采用 SSZ1 无损压缩封装，内部仍为带校验和的 SSR1；读取兼容原始 SSR1，后续保存自动压缩。写入通过临时文件和原子替换提交；升级应用时请保留应用数据。需要备份时使用 PXA/PXADB 的应用数据备份功能，迁移整个应用私有目录，不要只复制世界列表。

读取到损坏的世界文件时会保留原文件并阻止覆盖，界面显示错误。先备份数据，再检查存储或恢复自己的备份；不要用反复保存来尝试修复。游戏运行只依赖本地存储，音频播放为可选权限。

## 构建

从 PXA workspace 根目录执行：

```sh
tools/app.sh build starsoil --source-root local/pxa-apps --target all
```

SDK、编译器和签名设置遵循 workspace 的应用开发教程。`tests/` 包含核心玩法、世界存储、字体排版和真实模拟器操作验证程序；运行输出应放入 workspace 忽略的 `local/artifacts/`。

Inventory sorting preserves the hotbar and held slot. Storage offers matching quick-stack, take-all and sorting; transfers and pickups keep overflow in the source. The crafting header filters recipes that can be made here and reports missing materials, stations or result space. Jumping accepts a 100 ms edge grace and 120 ms landing buffer without automatic bouncing. Recall searches nearby safe space if home is blocked or covered in lava. Saplings also grow on edited ground, with bounded background work and full crown clearance.

Touch controls use independent pointer captures: a floating left joystick and translucent jump/use/interact buttons. Push the stick up to jump or down to drop through platforms; two-contact devices can move and jump with one finger while using with the other. Menus retain a single drag owner. Pause, background, resize, and page changes cancel or block held gestures until release. The world fills the area previously used by the bottom action strip.

Bows rotate with their actual aim, including the limbs, string, arrow and both hands. Shoulder anchors stay attached; two-bone arms retain their lengths and sleeves while aiming. The support forearm crosses in front of the torso when needed. Release/recovery follows the shot cooldown, and empty bows show no nocked arrow. Airborne characters use separate ascent, apex and descent poses with matching worn equipment; tool actions remain independent of leg motion.

World sizes include Huge (1024 × 256); existing worlds retain their dimensions. Immutable saves are written in steps of at most 4 KiB with an incremental checksum and atomic replacement. The loader validates before changing the live world and does not allocate a second full map. `tests/simulator_test.py --performance-only --fixture-tool <tool> --package <package> --output <local/artifacts/path>` measures one headless profile; add `--performance-world huge` for the larger map. These counters measure simulator submissions, not physical LCD FPS.
