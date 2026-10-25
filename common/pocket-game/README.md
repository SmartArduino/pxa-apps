# Pocket Game 运行时

为 2048、Pocket Chess 和 Pocket Sokoban 提供 PXA SDK 2.1 的窗口、像素绘图、输入、生命周期和异步存档。实现与原创 5×7 字体以 CC0-1.0 提供；每款游戏的核心、关卡和其他资源仍按各自许可分发，完整声明保留在各自 `assets/` 中。

## 接入

游戏实现 [runtime.hpp](runtime.hpp) 中全部 `game::` 钩子，并将 [runtime.cpp](runtime.cpp) 加入主循环组件。`init()` 先于 `load()`；游戏只在 `draw()` 内绘图，状态改变后调用 `redraw()`，需持久化时调用 `save()`。`serialize()` 必须检查给定容量，`load()` 必须在完整验证长度、版本、字段范围和校验后再替换状态。

逻辑画布固定为 288×224 RGB565，显示 surface 按屏幕等比 fit，用最近邻采样适配非整数比例和小屏。296×240 得到原点 (0,5)、296×230；320×240 得到 (6,0)、308×240；480×320 得到 (34,0)、411×320；240×240 得到 (0,27)、240×186。大屏利用 Host 的 1/2/4 倍显示，guest surface 上限 480×320。环境尺寸变化会取消输入并串行重建 surface。`Pocket: viewport canvas=288x224 origin=X,Y size=WxH display=WxH` 记录最终物理像素，`Pocket: ready` 日志继续保留。

触摸先逆 Host 整数缩放，再逆 guest 采样；黑边起始触摸被忽略。捕获首个有效指针，保留实际指针 ID，允许捕获后的越界释放；进入菜单或后台会取消当前手势。START 和右上角菜单属于运行时。继续、重开、退出三个按钮的逻辑位置仍为 x56..231、y72..99 / 102..129 / 132..159；第四行 y162..189 切换中英文并保持暂停。方向键首按后延迟 330 ms，再以 110 ms 重复；其他按键只分发按下边沿。

`tick()` 在前台未暂停时调用，间隔至少 20 ms，单次 elapsed 上限 100 ms；每次计算应有明确工作上限。进入后台后停止游戏计算和绘图，但继续排空存储完成事件；回前台重画当前局面。显示队列忙时保留已获取或暂存的帧，并在提交后继续处理期间新增的绘制请求。

存档键为 `<app_id>.save.v1`。`save()` 合并 200 ms 内的写入，以不可变缓冲区保留正在提交的数据；失败后按 1 秒间隔重试最新状态。退出时冻结游戏输入，等待已提交及最新状态写入，最长 2 秒，然后向 Host 交还返回事件。存档有效载荷上限为 3900 字节，为 SDK 的 4096 字节控制消息保留键名和协议头空间。事件接收缓冲区必须使用 `PXA_MAX_CONTROL_BYTES`，较大 capacity 也会违反 SDK 导入约束。

语言默认中文，使用 `pocket::tr("EN", "中文")` 显式翻译。UTF-8 绘制与测宽支持 5×7 ASCII、10×10 CJK，字符步进分别为 6/10，行距 10，均乘 scale；未知字形显示方框。语言以独立 `<app_id>.prefs.v1` 键异步保存为 4 字节 `P L 1 locale`（中文 0、英文 1）。存取错误每秒重试，后到的读取不会覆盖用户新选择，正常退出同时等待语言写入；游戏存档格式保持原样。切换日志为 `Pocket: language zh/en`。

显示内存为一个 129024 字节逻辑画布加两个最大 480×320 RGB565 输出缓冲区 614400 字节，共 743424 字节；输出以实际 frame_bytes 连续排列，基址 64 字节对齐。CJK 字体子集表约 16 KiB，衍生自 Noto Sans CJK SC 并按 SIL OFL-1.1 分发；每个游戏的 assets 必须携带 `OFL-NotoSansCJK.txt` 和 `FONT-NOTICE.txt`。源码、原创 ASCII 字体保留 CC0-1.0。字体再生、许可同步和可复用原生测试见 [Pocket UI](../pocket-ui/README.md)。

## 构建

以下命令从固件工作区根目录运行，把 `game-2048` 替换为 `pocket-chess` 或 `pocket-sokoban` 即可。缓存、临时目录和输出分别隔离，适用于同时构建多款游戏。

```sh
app=game-2048
artifact_root="$PWD/local/artifacts/mini-games/$(date +%Y%m%d-%H%M%S)/$app"
mkdir -p "$artifact_root/cache" "$artifact_root/tmp"
PXA_BUILD_CACHE_DIR="$artifact_root/cache" TMPDIR="$artifact_root/tmp" \
  tools/app.sh build "$app" --source-root local/pxa-apps \
  --target esp32s31,simulator,esp32s3 --aot-only \
  --output "$artifact_root/package"
```

应用使用最大 1 MiB 线性内存、64 KiB 栈，不申请音频或网络权限。签名配置使用工作区既有的 PXA 发布者设置。打包器只复制声明的资源，完整许可证应放入各款游戏的 `assets/`。

## 安装

存储紧张的 ESP32-S3 可以只构建 `esp32s3` 目标。设备 `INFO` 声明 `package_directory_adopt=1` 时，可安装构建产出的签名展开目录，安装器校验后直接接管目录，减少压缩包解包时的额外空间占用。继续使用上面的 `app` 和 `artifact_root`：

```sh
PXA_BUILD_CACHE_DIR="$artifact_root/cache" TMPDIR="$artifact_root/tmp" \
  tools/app.sh build "$app" --source-root local/pxa-apps \
  --target esp32s3 --aot-only --output "$artifact_root/device-package"
python3 tools/pxadb/pxadb.py package install \
  "$artifact_root/device-package/pxa-$app" --port /dev/ttyACM1
```

替换已有安装时，命令会按工具既有规则提示确认；应用私有存档由 Host 保留。串口应替换为目标设备的实际端口。

## 集成测试

[模拟器测试](tests/simulator.py) 使用真实产品模拟器的 Unix 控制接口，验证控制器、触摸、撤销或走棋、菜单、正常退出及跨进程存档恢复。需要 Python 3 和 Pillow。`--game` 接受 `2048`、`chess`、`sokoban`；`--package` 是构建产出的已签名展开目录。

```sh
python3 local/pxa-apps/common/pocket-game/tests/simulator.py \
  --simulator /absolute/path/to/pxsys_product_simulator \
  --package /absolute/path/to/expanded-package \
  --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der \
  --game 2048 --output "$artifact_root/simulator"
```

[设备测试](tests/device.py) 面向 296×240 的 pai-touch ESP32-S3，使用工作区 `tools/pxadb` 与 Pillow。先安装待测包并保证没有其他应用正在运行，再传入实际串口与游戏名称。测试会重开所选游戏当前盘、实际操作触屏、读取并验证设备私有 KV 存档、等待自动锁屏、验证锁屏期间零 raster/present、解锁后恢复同一盘，再从菜单退出并重新启动核对画面与存档。运行前确认测试机允许重开所选游戏。

```sh
python3 local/pxa-apps/common/pocket-game/tests/device.py \
  --port /dev/ttyACM1 --game 2048 --output "$artifact_root/device"
```

推箱子通关测试从第一教学关开始，需要尚未游玩的测试存档。设备测试不卸载或删除其他应用；运行生成的日志、截图和 JSON 必须输出到被 Git 忽略的 `local/artifacts/`。底层规则和损坏存档测试位于各游戏的 `tests/` 中。
