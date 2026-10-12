# Maze Evil / C++ SDK 多点触摸验证（2026-10-12）

双指行走、转向和行走中点按开火已经通过模拟器及 `/dev/ttyACM0` 的 pai-touch 验证。没有改变画质、148×120 RGB565 渲染、296×240 显示、地图种子 `0x9e3779b9` 或 33 ms 游戏周期。最终应用版本为 0.1.7，S3 AOT 与最终双目标发布包中的 S3 AOT 逐字节一致。

## 设计与修复

- **SDK 手指归属**：原来的 `CanvasPointer` 已带 pointer_id 和四种 phase，但应用需要自己维护捕获。现在增加强类型 `PointerPhase`、`action()`、`ended()` 及可选 `ui::PointerCapture`。一个捕获对象 16 B，无堆分配；匹配 surface/node/generation/id，防止其他手指或重建页面误释放。只有匹配的 UP/CANCEL 能释放，CANCEL 不代表点击。开发示例见 SDK 的 `MULTITOUCH.zh-CN.md`。
- **Host 身份稳定**：旧 LVGL 适配器从最新注册的 indev 起编号，新加入指针设备会改变正在按住的手指 ID。改为从最早注册的 pointer indev 起编号，无额外表或分配。交互期间仍要求保留旧 indev；删除较早的 indev 前应取消全部手势，任意设备热插拔的永久 ID 暂不保证。
- **测试必须有两根独立手指**：模拟器增加两个独立 LVGL indev，保留原 SDL 鼠标入口；PXADB 报告真实 max_pointers，正确传递 ID 和 CANCEL，不再把取消伪装成 UP。pai-touch 注入复用现有两个物理 indev，不额外创建设备。其他板卡仍按现有驱动报告一指，没有虚报双指能力。
- **积压与可靠边界**：PXADB 在队尾连续 MOVE 段按手指合并，不跨越 DOWN/UP/CANCEL/SYNC。一个手指释放或取消不干扰另一个；KEY、取消全部、超时及错误清理会释放全部所有权。
- **失败事务**：注入 DOWN 可能已改变驱动状态但尚未等到 LVGL 消费。失败时主动取消这个部分成功的样本，不留下按住状态。冷启动系统首帧较慢曾暴露 100 ms 调试等待超时；改为有界 1 s 等待并增加失败 DOWN 回归。等待仅发生在 PXADB 调试任务，物理输入与游戏线程不等待。
- **游戏双重归属**：重复 DOWN 不能让同一手指跨左右半屏同时取得行走和转向。保持游戏原有紧凑状态，没有强制加入两个新的 SDK 捕获对象。

## 真机数据与边界

每个窗口 4 s，显示 FPS 来自 LCD 完成时间戳。新包双指窗口执行：左指持续前进、右指往返转向、交错 MOVE、取消右指、复用右指点按开火，最后释放左指；两轮之间停止并重启游戏。原始计数、帧时间、局部堆最低值和初始化记录见 [measurements.json](multitouch-20261012/measurements.json)。

| 工作负载 | 显示 FPS（两轮平均） | Guest 派发均值 | 关联帧提交均值 | 关联 LCD 完成均值 |
| --- | ---: | ---: | ---: | ---: |
| 原 0.1.6，单指转向 | 30.376 | 3.898 ms | 25.055 ms | 61.444 ms |
| 新 0.1.7，冷启动后单指转向 | 30.280 | 4.120 ms | 26.459 ms | 62.935 ms |
| 新 0.1.7，双指行走和转向 | 30.420 | 4.718 ms | 22.222 ms | 58.398 ms |
| 新 0.1.7，行走 | 30.457 | 3.589 ms | 19.069 ms | 53.504 ms |
| 新 0.1.7，点按开火 | 30.275 | 6.395 ms | 22.715 ms | 58.909 ms |

这里支持“双指输入可用且未出现明显帧率退化”，不支持“多指让渲染更快”。双指和单指不是相同工作负载，不能直接比较其延迟收益。单指两轮差异也不足以判断回归或提速。游戏仍受 33 ms 周期限制。

`guest` 是 Host 完成事件派发，并非应用物理更新耗时；`present/visible` 把最新已派发输入与后续帧关联，不能证明该帧已经消费输入。测量不包含 USB 或触控芯片响应，不是物理双指触摸到光子的实测。pai-touch 硬件已有双触点采样，本次自动化通过同一对 LVGL 设备注入，未用机器手或高速摄影验证真实双指传感器。

## 内存核对

没有增加 Guest 静态控制状态、帧缓冲、Z-buffer 或命令队列。SDK 捕获是按需使用能力，每实例 16 B；不使用时没有默认缓存。实际 Xtensa 类型布局：pai-touch 硬件对象 936→928 B，内部 TestControlAdapter 36 B 不变；PXADB 所有权掩码复用原有小状态，无堆分配。

Guest AOT text 段 130,044→130,248 B，代码映射仍为 131,072 B；初始化段 207,012 B、函数表 1,040 B 不变。线性内存上限仍为 1 MiB；三帧映射像素仍为 106,560 B，fallback/mapped 联合存储仍为 142,080 B。运行时 PXA budget 为 internal 28,956 B / external 16,627 B，两版本相同；Host frame/scratch/mailbox 均为 0，性能探针同为 2,112 B。

整机堆必须另看，不能仅凭上述布局宣称完全不增长。采用 MEMORY START/STOP 的本轮局部最低值，截图均在窗口外；首次应用加载与稳态分开记录：

| 单指转向对照的整机占用 | 原 0.1.6 | 新 0.1.7 冷启动复测 |
| --- | ---: | ---: |
| SRAM 常态 | 279,936–279,972 B | 279,832 B |
| SRAM 窗口真实峰值 | 280,100 B | 279,996 B |
| PSRAM 常态 | 2,062,960 B | 2,063,692 B |
| PSRAM 窗口真实峰值 | 2,068,216 B | 2,067,272 B |
| 首次加载 SRAM 峰值 | 279,840 B | 279,812 B |
| 首次加载 PSRAM 峰值 | 2,784,924 B | 2,784,260 B |

冷启动 PSRAM 常态仍有 **+732 B** 差额，来源尚未确认；不能宣称整机稳定内存完全不增长。初始化和单指窗口峰值未增长，PXA 分类预算与映射没有变化。顺序安装未重启曾出现约 +1.4 KB PSRAM 差额，因此未将该轮作为最终对照。双指窗口 PSRAM 峰值约 2,069,404 B，属于不同负载；重复启动整套测试还记录到更高初始化峰值，原始数据保留，不把它与单次冷启动混为一谈。

基线与最终固件之间还有调试失败清理/等待修复，工作区也存在并行的 IME/音频任务。尝试同固件旧包→新包冷启动复核时，设备已被另一任务刷写占用，安装在获取串口锁时失败，没有部署旧包。严格匹配环境的 +732 B 归因仍待复测；最终报告没有将其他任务的固件或数据混入本轮结果。

## 验证与复现

- SDK 原生测试全套通过；两个捕获的 constexpr 测试覆盖稀疏 ID、跨区、不同节点/代次、UP/CANCEL、复用和后台 reset；文档示例通过 C++26 编译。
- 游戏输入、PXADB 状态/队列及部分失败 DOWN 清理均通过 ASan/UBSan。模拟器代理 16 项测试通过。
- 实际 LVGL 两 indev 测试通过：第一指按住期间注册第二个设备，第一指 ID 不漂移；交错移动、第二指取消/复用和删除较新设备均正确。UI/Surface/renderer 四项 CTest 与 Host 原生检查通过。
- 最终双目标 0.1.7 包在模拟器完成单指和双指回归，人工检查截图：取消右指后仍行走且弹药不减少；有效点按后弹药减少一次；全部释放后控制标记消失。真机两轮四种操作通过，无探针溢出或 Surface 切换。
- 测试结束自己的游戏、指针、性能探针均已清理；未修改设备设置。已有应用保留。可访问的两份 Voxel 存档在测试前后核对 SHA256；私人备份与启动日志不随报告发布。

```sh
bash tools/test-pxadb-input.sh
bash deps/pxa-system/tools/package/test_guest_cpp.sh
python3 -m unittest tools/test_simulator_pxadb.py
bash tools/app.sh build maze-evil --board pai-touch \
  --target esp32s3,simulator --aot-only --output /tmp/maze-multitouch-package
python3 local/pxa-apps/maze-evil/tests/verify_simulator.py \
  --simulator build/simulator/pai-touch/pxsys_product_simulator \
  --package /tmp/maze-multitouch-package/pxa-maze-evil \
  --publisher-key local/pxa-apps/.dev-signing/publisher-public.der \
  --output /tmp/maze-multitouch-simulator
# 设备需安装上述 S3 包和支持双指注入的固件，且无其他任务占用串口。
python3 local/pxa-apps/maze-evil/tests/measure_input_latency.py \
  --port /dev/ttyACM0 --output /tmp/maze-multitouch-device \
  --modes standing multitouch walking firing --repeat 2 --restart-each-repeat
```

画面：[真机双指测试后](multitouch-20261012/device-multitouch.jpg)、[模拟器同时走看](multitouch-20261012/two-finger-walk-look.png)、[取消右指](multitouch-20261012/two-finger-look-cancelled.png)、[行走中开火](multitouch-20261012/two-finger-walk-fire.png)、[全部释放](multitouch-20261012/two-finger-all-released.png)。
