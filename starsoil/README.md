# 星壤物语 Starsoil

默认通过 Window 0.5 请求横屏，并在旋转完成后按实际屏幕尺寸重新布局场景、菜单与触控区域。方屏保持当前方向，不能旋转的设备继续使用原生尺寸；退出或回到桌面后由系统恢复方向。

原创 2D 像素沙盒。探索森林、草甸、沙漠、雪原、雨林与地下晶洞，采矿、建造、种树、合成装备，与村民交易并挑战星核守卫。支持中文和英文、触摸和手柄；穿戴的衣服、护甲、武器和披风会随角色动画显示。

## 开始探索

在世界列表创建世界，可选择尺寸并输入种子；相同尺寸和种子生成相同地形。最多保留八个独立世界。从小屋出发，先拜访向导，再向右探索矿井。采集木材和石头，制作工作台与熔炉；用矿石和煤炼锭，再使用铁砧制作装备。合成手册会显示所需材料和工作设施。

铜镐可以开采金矿，铁镐可以开采晶体。橡子可以种在草地或泥土上。在工作台用木板、石砖或玻璃合成对应背景墙（一份材料得到四面墙），直接手持放置。木锤在工作台制作，铁锤在铁砧制作；长按目标背景墙拆除并回收，镐只拆前景方块。四面墙可在工作台还原一份材料；天然洞壁可清除，但不掉落人工墙。旧版按下放木板仍可使用。床可以设置回家位置，回城药水可返回；死亡也会回家并保留背包。

所有镐和斧都可以挥击面前的敌人，并在采矿、砍树时同时造成伤害；高等级工具伤害更高，剑的攻击更快、范围更远。弓使用箭，星杖消耗魔力。

镐和斧可以隔着前景方块采集工具范围内、等级允许的目标。触摸精确点选、固定选块和智能选块都检查距离和工具等级。智能模式按瞄准方向优先选择近处目标，并在破坏后接续选块。前景方块由镐斧采集，背景墙由锤子独立拆除。

人工木墙、石砖墙或玻璃墙可阻止刷怪，小型封闭建筑也会受到保护；大型房间应铺满背景墙。木门占两格，整体放置、开关和拆除；关闭时会挡住怪物，但苔木精能够推门。门下的地板、墙壁若留有缺口，外面的怪物仍可能进入。

背包和储物箱支持长按约 0.3 秒后拖动：放到空格可移动，放到不同物品上可交换，同类物品自动合并至每格 999，余量保留在原格；放到格子外或取消触摸不会改变物品。单击背包格即更换手持物品，关闭背包后直接使用，无需移入快捷栏。药水和装备可快速双击或点击“使用 / 穿戴”使用；工具和方块双击会拿在手上并返回场景。选用的背包物品在快捷栏右侧单独显示，点击它可回到背包，选择及排列会随世界保存。

直接轻点门、宝箱、储物箱、床、工作台、熔炉、铁砧或 NPC 可进行对应互动，拿着任何镐、斧也一样；距离过远时需先靠近。手持镐斧点按场景时，松手前不足 0.3 秒只互动，按住约 0.3 秒后才开始挥动、采集或拆除；长按松手不会再触发互动，拖动或取消也不会误点家具。右侧使用按钮和手柄 B 保持直接挥动，互动按钮也可随时主动操作家具。

跳跃短按约 3.5 格、长按约 5.5 格，可登上五格高的台阶；云靴仍提供二段跳。普通怪刷新更稀疏：白天地表每 12 秒最多 2 只、夜间每 9 秒最多 3 只、地下每 10 秒最多 4 只，守卫在场时不追加普通怪；出生点与角色、现有敌人保持距离，房屋免刷规则继续生效。

## 操作

普通敌人会按地表生态、深度和昼夜出现。雪原的霜露史莱姆缓慢高跳，沙甲虫蓄力后直线冲刺，夜间灯蛾短暂停顿后冲向玩家并在天亮时离开地表，雨林刺蜂保持距离并瞄准射刺。蓄力时显示金色提示条，刺弹会被实体方块和关闭的门阻挡。四种怪物分别提供凝胶与冰块、沙块与铜矿、丝线与星尘、藤蔓与凝胶。远行指南的生物页列出全部九种敌人的栖息地和行为。

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

## 画面与特效

世界界面采用深蓝底与铜金边框，快捷栏带有选中框和独立数量底色；任务提示压缩为窄条。
木质屋檐、边梁、草根与岩石顶缘增加了材质层次，洞穴背景包含分层岩脊、石笋与晶簇。
静止镜头复用原有 192 KiB 缓存保存背景、地块、背景墙与树木；角色、草尖、波纹和粒子
逐帧绘制。移动、地形编辑与照明变化会重新绘制静态场景，不增加整帧缓冲。

天空和洞穴渐变保留 RGB888 精度，写入 RGB565 时采用固定 4×4 有序抖动，
洞口混合也保留相邻色阶。每行预计算四个颜色后按连续字写入，静止镜头沿用缓存；
不增加帧缓冲，也不使用逐帧随机噪声。

火把带有跳动火舌、余烬与局部暖光；裸露水面与熔岩显示连续波纹。
森林和雨林有飘叶，雪原有落雪，夜间草甸、森林与雨林有萤火虫；
晶矿与传送门闪光，采集、命中、落地和出入水面有短暂粒子反馈。
装饰效果使用独立随机状态，不影响掉落、世界生成与存档。

场景按约 30 FPS 调度，实际帧率取决于设备负载；暗处的静态贴图复用
约 16 KiB 有界明暗缓存，保持原始颜色、透明度和像素边缘。地形光源特效每帧最多
12 处，环境粒子按可见世界区段生成，不需要额外全屏光照缓冲。
移动、拖动和缓存频繁未命中时使用直接绘制；反复复用的完整贴图才建立明暗缓存。
部分效果参考 [PixelFX](https://github.com/patch-gg/pixelfx)，许可见
`art/PIXELFX-LICENSE.txt`，应用资源中也附带相同许可。

工具武器采用木、铜、铁和晶体的独立材质与轮廓，药瓶、矿物、护符和云靴按原生 16 像素重绘；
人物握点保持原坐标。标题与应用图标共用 32 像素晶体草土徽记，菜单页签使用 8 像素图标。
素材由 `tools/equipment.py` 和 `tools/branding.py` 生成，参考来源记于 `art/equipment-source.json`。

方块边缘由各材质贴图绘制，不叠加通用岩石亮边或重复屋顶装饰。裸土保持土色，
石层使用中性岩灰，木板使用宽木纹。物品数量共用 `quantity.hpp` 的 4×7 像素字形，
仅绘制数字与一像素阴影，透明背景保留底下的物品图标；快捷栏、背包、箱子及拖动预览保持一致。

## 沙与液体

沙块失去支撑后下落，并沿空隙斜向堆积；落入水或熔岩时与液体交换位置。
水向下流动并向附近低处扩散，熔岩速度较慢。采用整格体积模型，不会凭空增加液体。
沙块不会落入角色身体，家具与边界不会被流体覆盖；平台允许液体穿过。
挖开容器出口或放置一格液体后，周围材料会继续更新；旧世界载入后也会开始沉降。
沙块本身不会产生水；已有水被沙子挤开后仍保留在世界中。沙面和沙漠浅层洞穴使用
暖金、砂岩色，与蓝色水体区分；相连液体只在露出的顶部绘制水面亮边。
沙漠石层使用暖色砂岩贴图，仍按石块碰撞、开采并掉落石料。

物理调度使用可休眠的 16×16 区块，附近优先、远处轮转。每个 50 ms 步长最多检查
四个区块并完成 64 次移动；熔岩每三个步长更新一次。调度状态不写入世界文件，
读取时从现有地块重新建立，保持原 SSR1/SSZ1 存档兼容。

## 保存与迁移

游戏使用 PXA 应用私有文件系统保存世界，每八秒检查待保存状态；连续操作时合并写盘，松手约 0.6 秒后提交，持续操作最长约 30 秒触发保存。暂停、退出和切换世界立即保存。世界文件采用 SSZ1 无损压缩封装，内部仍为带校验和的 SSR1；读取兼容原始 SSR1，后续保存自动压缩。写入通过临时文件和原子替换提交；升级应用时请保留应用数据。需要备份时使用 PXA/PXADB 的应用数据备份功能，迁移整个应用私有目录，不要只复制世界列表。

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

实体按键或手柄产生输入后，场景中的悬浮摇杆、跳跃、使用和交谈按钮收起；松开按键、暂停与恢复会保留此模式，重新触摸屏幕或断开手柄时恢复触控按钮。

支持 FS 0.6 `async-atomic-write` 的 Host 使用原生后台存档：逐块等待完成，关闭成功后原子替换，提交完成前保留旧档。旧 Host 自动回退到兼容保存流程；普通加载读取仍为同步。实体按键/手柄操作时隐藏悬浮控件，松键和暂停恢复保留该模式，触屏按下后恢复悬浮控件。

大于 64 KiB 的存档将校验、压缩分析、重叠移位和编码分到多帧执行，每帧处理约 64 KiB，复用原有快照缓冲，输出与原 SSZ1 编码逐字节一致。暂停、退出或切换世界时继续推进当前快照并等待提交，期间不会改用新世界的数据。

渲染使用对齐的 RGB565 成对填色、连续行复制和最近邻像素批处理；洞穴渐变合并同色背景段后计算颜色。输出保留原有坐标、RGB565 舍入、裁剪和像素风格，不增加整帧缓存。

支持 GameRender 1.6 `rgb565-batches` 的 Host 可原生执行背景色块批次和场景最近邻缩放，输出继续使用原有 Surface。背景按原顺序合并相邻同色色块；洞穴过渡需要读取已有像素时先完成批次，再继续本地混色。地块、角色和 UI 保留现有绘制路径。旧 Host 或临时忙碌时自动软件回退；软件缩放坐标表随尺寸变化更新，固定分辨率时跨帧复用。

静态场景缓存按视角、地表高度、昼夜配色、太阳、云层和可见地块邻域更新，晶洞闪光在缓存画面上逐帧绘制。连续移动、夜空频繁动画及超过容量的布局使用直接绘制，缓存容量为 192 KiB，应用线性内存上限为 3 MiB。修改背景或静态场景规则时，需要同步检查 `background_cache.hpp` 与 `static_scene_cache.hpp` 的画面键，并运行逐像素对照测试。

建造时会显示半透明预览：绿色可放置，红色表示阻挡、缺少支撑或超出距离；门显示完整两格范围。手持不同背景墙可直接替换并回收旧墙，同种墙不重复扣料。手持墙壁轻点家具仍可互动，按住则在家具后方补墙。合成页底部可筛选全部、建造、工具、装备和补给，并可叠加“可制作”筛选；手柄在列表第一项按上进入分类，左右切换、下返回配方。

怪物动画使用每种生物共用的 16 色调色板和 8 位索引像素，绘制时先计算调色板光照，再逐行写入画面。飞行摆动使用固定采样表，远离玩家的普通怪物会回收。敌方弹药使用独立的 8 发容量池，并在复活时清空。
