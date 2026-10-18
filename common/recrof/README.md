# recrof 引擎的 PXA Core 2 移植

四个上游仓库对应五个独立应用：`quake`、`quake2`、`duke3d`、`command-conquer` 和 `red-alert`。后两者共享 Vanilla-Conquer 源码。每个应用的 `upstream.json` 固定了移植使用的上游提交。引擎源码和授权文件保留在各自目录中。

## 构建

从 pxa-projects 根目录执行。需要仓库的 PXA SDK、WASI SDK 34、wamrc；首次下载 Binaryen 130：

```sh
local/pxa-apps/common/recrof/tools/setup.sh
tools/app.sh build quake --source-root local/pxa-apps --target esp32s3,x86 --output local/recrof-build/final
```

把 `quake` 替换成其他应用名即可。`--target wasm` 生成便携 WASM 包；`esp32s3,x86` 同时生成 Xtensa 和模拟器 AOT。输出文件为 `pxa-<应用名>.pxa`，使用本工作区的开发发布者密钥签名。设备必须信任对应发布者。

CMake 在链接后使用 Asyncify 将原引擎的阻塞主循环转换成可暂停的 PXA 事件回调。宿主只接收标准 PXA Core 2 导入；Asyncify 导入在构建时消除。SD 变体另外要求支持 FS 0.3 `app-media-root` 的固件；固定映射缓冲也要求宿主正确实现 pinned linear memory 的原地增长。

## 导入资源

引擎源码不包含商业游戏资源。为本地测试导入的数据不会加入 Git。请使用自己有权使用的版本，重新分发应用包前核对资源授权。

```sh
python3 local/pxa-apps/common/recrof/tools/import_data.py quake /path/to/pak0conv.pak
python3 local/pxa-apps/common/recrof/tools/import_data.py quake2 /path/to/Quake2
python3 local/pxa-apps/common/recrof/tools/import_data.py duke3d /path/to/DUKE3DCONV.GRP
python3 local/pxa-apps/common/recrof/tools/import_data.py command-conquer /path/to/CNC-DOS-data
python3 local/pxa-apps/common/recrof/tools/import_data.py red-alert /path/to/RedAlert-data
```

导入后重新构建。只读资源放在签名的 `assets/game` 中，通过 Assets 2 分块读取，不占用应用私有 FS 的 8 MB 配额。设置和存档写入私有 `data` 目录。导入器统一小写文件名，并检查大小写冲突和符号链接。

Quake 必须使用上游 MG24Quake 2.0 转换器生成的 PAK；普通 PC PAK 不能直接使用。转换器源码位于 `quake/tools/MCUPackConverter`，在有 `pak0.pak` 的目录运行转换器会生成 `pak0conv.pak`。目前只接入 `pak0.pak`，未验证完整注册版的其他 PAK。转换器可在 Linux 下编译：

```sh
cc -O2 local/pxa-apps/quake/tools/MCUPackConverter/*.c -lm -o /tmp/pxa-quake-pakconvert
# 在包含原版 pak0.pak 的目录运行 /tmp/pxa-quake-pakconvert
```

Duke3D 必须先转换 GRP：

```sh
python3 -m pip install mido
python3 local/pxa-apps/duke3d/tools/PythonCVT/mcugrpcvt.py DUKE3D.GRP DUKE3DCONV.GRP
```

Quake II 使用标准 `baseq2/pak0.pak`。C&C 可导入 DOS 试玩版的 DEMO.MIX、DEMOL.MIX、SOUNDS.MIX、SPEECH.MIX，或完整零售版资源；DOS/试玩版自动使用低分辨率。Windows Gold 需要自己的 CCLOCAL.MIX 等完整配套资源。Red Alert 使用 REDALERT.MIX 和战役 MAIN.MIX，或完整的展开资源；默认采用 Windows 素材模式并缩放输出。不要把 Windows 的 CCLOCAL.MIX 直接重命名成 DOS 的 LOCAL.MIX。仅有 OpenRA 的最小联机资源集不能作为完整单人战役的验收依据。

## 操作

射击游戏：左半屏拖动控制前后移动和横移；右半屏拖动转向；按钮 MENU 打开菜单，WPN 切武器，USE 使用，JMP 跳跃，FIRE 射击。菜单内用左侧上下拖动选择，FIRE 确认。手柄方向键移动，A 射击/确认，B 跳跃，START 菜单，SELECT 回车，X 使用，Y 切武器。

策略游戏：触摸映射鼠标左键，带右键标志的指针映射右键；右上角 RMB 按钮在上一个游戏光标位置执行右键，便于触屏取消选择。手柄 A/B 分别为左键/右键，START 为 Escape，SELECT 为回车。策略游戏的原有阻塞键盘等待会主动让出执行权，以便接收后续 PXA 输入事件。当前没有联机功能。

## 平台适配与限制

- 软件绘制、RGB565 双缓冲 guest-mapped Surface；以 320×240 为逻辑屏幕，按显示器尺寸缩放。
- PXA Clock 驱动，独立的引擎 C 栈与 Asyncify 续体栈，后台暂停并释放按键。
- 可选 `audio.playback` 权限，PCM 混音后输出 16 kHz 单声道；拒绝权限仍可游戏。实际设备音频尚未验证。
- 标准 stdio/POSIX 调用桥接到私有 FS 与签名资源，虚拟 Flash 使用文件保存 NOR 数据。
- 不使用 ESP32 的 GPIO、SDMMC、DMA、FreeRTOS 或 Flash 直映射驱动。
- 目前清单的线性内存上限：Quake 3 MiB、Quake II 12 MiB、Duke3D 4 MiB、C&C 10 MiB、RA 14 MiB；另需宿主、AOT 和显示内存。不能据此承诺在 8 MiB PSRAM 的设备上均可运行。大型资源包还需要足够的存储空间。
- WAMR 无 Wasm 异常处理，原引擎 setjmp/longjmp 错误恢复改为显示错误并等待重新打开应用。正常主循环不依赖 longjmp。
- `duke3d-sd` 已在 16 MiB PSRAM 的 GameKit / ESP32-S31 上自动读取固定 SD 资源并进入屋顶关卡，完成短时 FPS 与内存测量；其他游戏、音频连续性及长期稳定性仍待真机验收，见 `VALIDATION.md`。

## 测试工具

`tests/host.mjs` 是协议级 Node 22 测试宿主，执行真实引擎并读取真实资源；它不能代替正式 PXA 模拟器。`tests/simulator.py` 启动正式签名包、捕获运行日志和截图，检查异常、空白画面与正常 HOME 退出。`tests/fixture` 验证暂停期间的 C 栈对象、C++ 字符串、虚拟文件和路径解析。资源导入单元测试：

```sh
python3 -m unittest discover -s local/pxa-apps/common/recrof/tests -p 'test_*.py'
```

详细验收状态见同目录 `VALIDATION.md`，源码改动见 `PATCHES.md`，测试资源来源见 `TEST_DATA.md`。

Quake 的门和开关沿用原作自动触发机制，USE 按钮在 Quake 中没有独立动作。

策略游戏的资源版本说明参考上游 [VanillaTD 安装说明](https://github.com/TheAssemblyArmada/Vanilla-Conquer/wiki/Installing-VanillaTD) 和 [VanillaRA 安装说明](https://github.com/TheAssemblyArmada/Vanilla-Conquer/wiki/Installing-VanillaRA)。
