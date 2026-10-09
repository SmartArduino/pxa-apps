# 游戏交互与内存复现

从 pxa-projects 根目录运行，使用仓库锁定的工具链和签名开发密钥。正式应用没有
`VOXEL_VALIDATE` 或 `VOXEL_PLAYTEST_SCENE`；测试地图只在独立 `pxa-voxel-craft-cpp-ui2`
身份下运行。原应用的世界、设置和背包不会被测试脚本写入。

```sh
SANITIZE=1 bash local/pxa-apps/voxel-craft-cpp/tests/test.sh
python local/pxa-apps/voxel-craft-cpp/tests/build_playtest.py --target esp32s3 --output local/voxel-playtest
python tools/pxadb/pxadb.py package install local/voxel-playtest/artifacts/pxa-voxel-craft-cpp.pxa --yes --port /dev/ttyACM2
python local/pxa-apps/voxel-craft-cpp/tests/device_playtest.py --device --port /dev/ttyACM2 --output local/voxel-playtest/screenshots
python local/pxa-apps/voxel-craft-cpp/tests/device_playtest.py --device --resume --port /dev/ttyACM2 --output local/voxel-playtest/restart
```

串口号可能变化，先核对 `hello` 的板型和芯片标识。本轮 pai-touch 实际使用
`/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_A4:CB:8F:D5:D4:F0-if00`，
对应 `/dev/ttyACM0`；连接中的另一块开发板不参与测试。同一串口一次只运行一个程序。

完整流程要求测试身份没有已有存档（断言三个槽位 generation=1）；重复新测试时只清理
这个测试身份，随后运行脚本。清数据不等于撤销系统权限；首次授权验证可用新的测试身份。`--resume` 依赖上一轮生成的存档，不能先清数据。

```sh
python tools/pxadb/pxadb.py package clear-data pxa-voxel-craft-cpp-ui2 --port /dev/ttyACM2
python local/pxa-apps/voxel-craft-cpp/tests/device_playtest.py --device --local-heap --no-screenshots --port /dev/ttyACM2 --output local/voxel-playtest/peak
```

`--local-heap` 在实际启动之前执行 MEMORY START，在取消触摸后、停止应用前执行
MEMORY STOP；不截屏的流程还覆盖矿物耗时、进度取消、实际数量、放置、物品移动、
随身配方、工作台操作和三个存档槽。带截图的运行与干净峰值运行分开。

授权对话框的生命周期也单独测量。使用没有授予过音频权限的新测试身份；暂停在弹窗中
停止应用不能留下 Surface、mailbox 或挂起的权限对象。拒绝权限后仍须看到游戏 ready。
清数据只清理应用私有数据，不撤销系统授权；拒绝后能否再次请求由 Host 权限策略决定。
首次允许测试应确认日志确实出现该身份的权限弹窗，不能把已有授权的启动当成首次授权。

```sh
python local/pxa-apps/voxel-craft-cpp/tests/build_playtest.py --target esp32s3 --font-size 10 --identity pxa-voxel-craft-cpp-ui3 --output local/voxel-permission
python tools/pxadb/pxadb.py package install local/voxel-permission/artifacts/pxa-voxel-craft-cpp.pxa --yes --port /dev/ttyACM2
python local/pxa-apps/voxel-craft-cpp/tests/device_lifecycle.py --port /dev/ttyACM2 --app pxa-voxel-craft-cpp-ui3 --cycles 3 --stop-pending --output local/voxel-permission/pending
python local/pxa-apps/voxel-craft-cpp/tests/device_lifecycle.py --port /dev/ttyACM2 --app pxa-voxel-craft-cpp-ui3 --cycles 1 --output local/voxel-permission/allowed
```

正式包的三次启动内存使用已授权音频，测量期间不截屏、不启用 PERF。`--cold-first`
只是标记，必须先真实重启；新游戏只在内存中创建，不保存，不改变原设置。

```sh
python tools/pxadb/pxadb.py reboot --port /dev/ttyACM2
python local/pxa-apps/voxel-craft-cpp/tests/device_memory.py --port /dev/ttyACM2 --cold-first --output local/voxel-production-memory
```

模拟器使用 `--target simulator` 的测试包和独立 state-root / PXADB socket，避免覆盖
日常模拟器的包和数据。`device_playtest.py --port unix:/path/to/socket --log /path/to/runner.log`
（不传 `--device`）驱动已启动的 runner；第二次重新启动 runner 后加 `--resume`。
启动参数及多分辨率截图见 [本轮报告](../docs/performance/voxel-gameplay-20261009.zh-CN.md)。

收尾仅停止、清理和卸载本次使用的 `pxa-voxel-craft-cpp-ui2/ui3` 测试身份，保留正式应用及其他应用。

工具的独立地图和触屏流程：

```sh
python local/pxa-apps/voxel-craft-cpp/tests/build_playtest.py --target esp32s3 --tools --font-size 10 --output local/voxel-tools
python tools/pxadb/pxadb.py package install local/voxel-tools/artifacts/pxa-voxel-craft-cpp.pxa --yes --port /dev/ttyACM2
python local/pxa-apps/voxel-craft-cpp/tests/device_playtest.py --device --tools --port /dev/ttyACM2 --output local/voxel-tools/play
python local/pxa-apps/voxel-craft-cpp/tests/device_playtest.py --device --tools --resume --expected local/voxel-tools/play/report.json --port /dev/ttyACM2 --output local/voxel-tools/restart
```

`--tools` 仅给隔离地图提供有限的测试材料；正式新游戏仍是空背包。
脚本逐格摆放六种工具的真实 3×3 配方，检查错误工具不加速、每种工具的耗时、
数量和耐久，并保存、重启验证。存档为 v3，原生测试还检查 v1/v2 迁移与损坏拒绝。
`--font-size 10` 只在 pai-touch 的固定 160 DPI 测试包中去掉不会加载的两档字体，
避免在保留四个正式应用时触及安装空间的保守准入门槛；正式包始终包含三档字体。

首次授权可构建 `--identity pxa-voxel-craft-cpp-ui3`，用 `device_lifecycle.py --app`
指定相同身份。权限请求日志表示“已排队”，脚本等待实际弹窗激活后才点击。
日志突发丢包时重读保留记录；MEMORY 响应期间暂停日志推送，避免缺失 DATA 帧。
测量原始区域最小剩余量，不使用历史累计 `peak_total` 代替本次运行峰值。

材质音效预览可独立生成；文件是审听资料，不打包到游戏：

```sh
clang++ -std=c++2c -O3 local/pxa-apps/voxel-craft-cpp/tests/sound_test.cpp -o /tmp/voxel-sound
/tmp/voxel-sound local/material-sounds.wav
```

顺序为树叶、泥土、砂砾、雪、木、石、玻璃、羊毛；每种依次为打击、破坏、放置。
测试检查八种波形不同、PCM 单包边界、首尾零采样和无削波；主观听感仍由实际听音判断。
Host 状态栏缓存的实 LVGL 回归由 `SANITIZE=1 bash tools/test-system-overlay-cache.sh` 运行，
模拟启动器 → 游戏首帧之前隐藏系统栏，检查旧快照释放及正常系统浮层的保留。
