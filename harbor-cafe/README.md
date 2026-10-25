# Harbor Cafe

原创 CC0 海边像素咖啡馆，`pxa-harbor-cafe` 1.1.1，release_sequence 3。288×224 画布里有木质吧台、咖啡机、三张桌椅、带订单气泡的顾客、店员、窗外海浪、帆船、海鸟与灯塔。咖啡机、露台和花艺升级直接改变店内画面。所有图像均由本应用代码或图标生成器原创绘制，不依赖网络素材。代码、图像与说明以 CC0-1.0 发布，见 LICENSE 与 NOTICE。

默认使用中文界面，可在公共暂停菜单切换英语；配方、订单、经营日结、升级、指南及正在显示的提示同步切换。不播放背景音乐；接单、制作、完成、上菜、进货、升级、日结及错误保留简短原创 CC0 音效。指南内“音效”控制提示音，沿用原有 v1 存档的 sound 字段；暂停、后台和退出由运行时静音。拒绝音频权限或关闭声音仍可正常经营。

## 第一笔生意

开店约 0.6 秒后第一张桌会出现 ESP（浓缩咖啡）气泡。点它选择桌位，再点中间的 ACCEPT 接单；同一按钮变成 MAKE，再点一次消耗原料制作。等待吧台进度完成，托盘出现 READY 后点 SERVE，把成品交给所选桌。首次 ESPRESSO 在耐心尚足时获得 10G，其中配方成本 2G，毛利 8G。界面中的 G 表示金币。音频权限被拒绝时仍能正常经营。

三张桌各自保留订单和耐心条。绿色气泡表示已接单，金色框表示当前桌。接单自动选择对应配方，左右 DRINK 可手动改变配方。成品只交给相符的已接订单；做错时选择正确桌，或按 B 清空托盘。丢弃不退还原料。耐心耗尽的顾客离开，不罚现金；继续服务新顾客即可恢复。

## 配方与经营

| 配方 | 原料 | 成本 | 基础收入 |
|---|---|---:|---:|
| ESPRESSO | 1 豆 | 2G | 9G |
| LATTE | 1 豆、1 奶 | 3G | 14G |
| BERRY SODA | 2 莓果 | 2G | 10G |

耐心超过一半时多付 1G 小费；每级花艺再加 1G。库存依次为豆、奶、莓果。Pantry/SUPPLY 每次进货 4 单位，分别花费 8G、4G、4G。制作时立即扣库存并记当日原料成本，销售时才入账，进货支出不重复计入日结利润。

初始 30G，库存 8/6/6。每个营业日严格为 150 秒前台游戏时间；运行时菜单暂停或后台不会推进。第 1～5 日目标依次 3/4/5/6/7 单，之后每天 7 单；达标日结得 8G 与一颗星。每日租金 10G，余额不足 10G 时归零，不产生额外债务。日结 NET 为销售减已使用原料成本减租金；目标奖励、进货与升级体现于现金，不混入经营净利润。未完成的订单在收店时离开，正在制作或托盘成品会丢弃，其成本已记账。

缺豆且无法经营时，Pantry 最后一项 RECOVERY CRATE 可在豆库存小于 2 时领取：+2 豆、+1 奶、+2 莓果，形成最多 8G 新赊账，每天一次。总债上限 1000G，到上限时超出的部分为紧急救济，仍能领取，避免长期缺货死锁。后续销售先偿还赊账，余款入现金；当前欠款显示在日结。重开当天保留已用补给次数，防止无限领取；次日恢复领取资格。

## 升级与长期目标

三条独立路线每条 0～3 级，费用为 20G、48G、88G：

- MACHINE：制作由 2.4 秒降至 1.9 / 1.4 / 0.9 秒，机器增加塔身、压力表与金属装饰。
- TERRACE：每级增加顾客 6 秒耐心，并依次增加遮阳棚、舒适座椅、海蓝露台栏杆。
- FLOWERS：每级每单小费加 1G，并依次增加盆花、墙上招牌与灯串。

先完成三个营业日并把三条路线升到二级，再冲三级与连续达标星数。现金、库存、赊账、累计服务、星数、升级、天数及当天完整经营现场都会持久保存。运行时 RESTART 重开当天时间与订单，保留现金、库存与成长；同一天已经结算过时不会重复扣租金或发目标奖励。

## 控制

所有触摸控件完整位于逻辑 `y < 212`，左侧按钮有效中心在 `x >= 28`；右上角运行时菜单与顶部 FPS 区保留。

| 操作 | 触屏 | 手柄 | 产品模拟器键 |
|---|---|---|---|
| 选择桌位 | 点桌子或 TABLE 左右 | 上/下 | UP / DOWN |
| 接单、制作、上菜、次日 | 中间主按钮 | A | Z |
| 更换配方 | DRINK 左右 | 左/右 | LEFT / RIGHT |
| 清托盘、返回 | 点托盘杯子；页面 BACK | B | X |
| 进货 | SUPPLY | X | A |
| 升级 | GROW | Y | S |
| 指南 | Pantry / Grow 里的 GUIDE | L | Q |
| 音效开关 | 指南的“音效” | 指南中 A | Z |
| 暂停/继续/重开/退出 | 右上角菜单 | START | ENTER |

触屏玩家清托盘的操作：点托盘中的杯子。取消的触摸与移出按钮后的释放不会触发购买或接单。暂停、后台及退出由冻结的共享运行时负责清理输入并保存；菜单的 CONTINUE 不会误触游戏。

## 存档与实现

`core.cpp` 为有界纯状态机；`main.cpp` 仅在 draw 绘图。每 tick 至多推进 100ms、检查三个顾客；景物 10Hz 重绘。显式 160 字节 HCFE v1 小端格式，包含长度、版本、保留零字段、FNV-1a 校验和完整范围/状态关系验证，拒绝坏档时保持原状态。游戏交易即请求保存，运行时合并 200ms 内请求；12 秒自动保存，菜单与正常退出也提交快照。最多 1MiB 固定线性内存、64KiB 栈，非 Asyncify main-loop。

## 构建与测试

在工作区根目录设置自己独占的忽略产物目录，所有缓存和 TMPDIR 放在其中：

```sh
artifact="$PWD/local/artifacts/pocket-worlds/<timestamp>/harbor-cafe"
mkdir -p "$artifact/native" "$artifact/cache" "$artifact/tmp"
TMPDIR="$artifact/tmp" g++ -std=c++17 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  local/pxa-apps/harbor-cafe/{core,main}.cpp \
  local/pxa-apps/harbor-cafe/tests/{runtime_stub,native}.cpp \
  -o "$artifact/native/harbor-native"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  "$artifact/native/harbor-native" "$artifact/native"
PXA_BUILD_CACHE_DIR="$artifact/cache" TMPDIR="$artifact/tmp" tools/app.sh build \
  harbor-cafe --source-root local/pxa-apps --target simulator,esp32s31,esp32s3 \
  --aot-only --output "$artifact/package"
python3 local/pxa-apps/harbor-cafe/tests/integration.py \
  --simulator /path/to/pxsys_product_simulator --package /path/to/expanded-package \
  --publisher-key /path/to/publisher-public.der --output "$artifact/simulator"
```

原生覆盖五天真实经营、三配方、库存/现金守恒、全路线升级、失败补给与偿债、错误配方恢复、暂停边界可保存状态、随机/截断/校验正确但字段越界坏档、真实游戏钩子、触摸 CANCEL 和绘图边界。真实模拟器测试须使用新的输出目录建立隔离 profile，五个 150 秒营业日加启动与生命周期检查约需 13 分钟，以实际触屏和手柄交替完成多日经营，从宿主 KV 独立解码经营进度；验证暂停不计时、重开保留成长、正常退出与新进程恢复，不注入预置存档。
