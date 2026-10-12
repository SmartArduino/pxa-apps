# Maze Evil：触摸事件积压修复（2026-10-12）

在 `/dev/ttyACM0` 的 296×240 pai-touch 上复现了持续转向延迟。主要问题是旧输入积压；约 30 FPS 的显示帧率掩盖了它。修复不改变画质、游戏速度、33 ms 更新周期或绘制路径。

## 原因和修改

1. 游戏同时订阅 Canvas 和 ROOT 的 pointer 事件。LVGL 的事件冒泡使同一物理触摸被两个节点上报。最终仅用透明 ROOT 接收输入，删除空 Canvas 及对应特性要求；画面继续由 Surface 绘制。保留 Host 的正常事件冒泡语义。
2. ESP Host 的 8 项 pointer mailbox 只合并紧邻队尾的同一流 MOVE。两个节点、多指输入交错时，队列会先装满旧 MOVE，再以每个物理指针 16 ms 的节奏消费，持续产生陈旧坐标。现在在队尾连续 MOVE 段内按 instance/surface/node/pointer 合并最新坐标和采样时间，保持原队列位置，避免高频流挤掉其他流。
3. 合并不能越过 DOWN/UP/CANCEL 等可靠事件。原来队列满时的回溯替换没有这个边界，可能把新手势坐标放到旧手势中；现在也修正这一情况。
4. 游戏在 UP 中消费最终坐标，再判定是否点按。即使末尾 MOVE 未被采样或被合并，转向总量也正确，短距离拖动不会误开火。CANCEL 只释放对应指针，不触发开火、不采用取消坐标，也不会释放另一根行走手指。相关 native 回归测试覆盖这些情况。

没有新增线程、队列、动态分配或状态成员。mailbox 容量仍为 8，布局上限仍为 464 B。最坏只检查 8 项；高频交错移动不再产生多份待处理位置，减少了事件复制及后续回调工作。单 ROOT 减少一个 LVGL 对象；主要延迟收益来自 Host 修复。释放/取消的内联适配器复用原有 move/up 函数类型，保持 AOT 初始化元数据及函数表大小不变。

## 真机结果

固定地图种子 `0x9E3779B9`、初始相机、148×120 RGB565 三缓冲、2× 显示和原画质。基线及 Host 单独修复各进行 3 轮静止/转向，最终 0.1.6 进行 2 轮，各窗口 4 秒；右半屏注入 48 次往返 MOVE，间隔 65 ms。下表只合并转向窗口，按样本数加权。原始各轮计数、局部内存最低值和 LCD 完成帧间隔见 [measurements.json](input-latency-20261012/measurements.json)。

| 指标 | 原固件 + 0.1.4 | Host 修复 + 原 0.1.4 | Host 修复 + 最终 0.1.6 专用包 |
|---|---:|---:|---:|
| 采样至 Host 完成 Guest 事件派发，平均 | 169.31 ms | 4.98 ms | 4.05 ms |
| 采样至关联帧提交，平均 | 198.54 ms | 26.54 ms | 26.20 ms |
| 采样至关联帧 LCD 完成，平均 | 235.73 ms | 63.31 ms | 62.67 ms |
| 采样至关联帧 LCD 完成，最大 | 281.02 ms | 85.45 ms | 81.10 ms |
| 实际显示 FPS | 30.370 | 30.373 | 30.367 |

Host 修复保持原游戏包时，关联帧 LCD 完成延迟降低约 73%。帧率差异不支持提速或退化结论，游戏仍由 33 ms 定时器限制。没有通过提升移动灵敏度、降低画质或更改时间步来掩盖延迟。

测量边界：使用设备采样时间戳，不把串口 ACK 当输入延迟；`guest` 是 Host 派发阶段完成，不是精确的游戏物理更新埋点。`present/visible` 将最新已派发输入与后续帧关联，不能证明该帧一定消费了这次输入。数据排除 USB 传输和物理触控芯片响应，也不是高速摄影测得的触摸到光子延迟。直方图超过约 94 ms 后进入最后一桶，因此修复前的 `p95` 字段不能作为精确 P95；表中使用均值和实际最大值。

输入轨迹相同，初始场景相同，但旧版可能漏掉紧接 UP 的最后一个 MOVE，终态相机不保证相同。截图中这部分差异来自输入处理；FPS 仅用于检查本次操作测试的帧节奏，没有据此宣称相同固定视角下的渲染性能提升。0.1.6 补齐可靠 UP 坐标后消除了这个不确定的末尾位移。

剩余约 64 ms 包含游戏更新/渲染等待和提交后的显示流水线。还可以进一步分析帧节奏与显示唤醒，但本次未更改显示驱动或增加帧缓冲。

## 内存核对

内存使用局部 `MEMORY START/STOP`，覆盖采样间的瞬时分配，没有使用历史开机最低值冒充本次峰值。窗口包含相同的 2,112 B 性能探针，截图在窗口外执行。

最终采用单 S3 AOT-only 包，保持 O3 / size level 0。测试期间外部流程安装了 Jump Jump，因此不把不同应用目录的全设备堆占用直接相减。以下仅比较相同工作区固件、相同两应用目录下的原 0.1.4 和最终单 ROOT 的 0.1.6，各两个转向窗口：

| 相同环境的全设备堆占用 | 原游戏 0.1.4 | 最终 0.1.6 | 差值 |
|---|---:|---:|---:|
| SRAM 常态快照 | 286,432 B | 286,428 B | −4 B |
| SRAM 局部真实峰值 | 286,596 B | 286,592 B | −4 B |
| PSRAM 常态快照 | 2,081,264 B | 2,080,708 B | −556 B |
| PSRAM 局部真实峰值 | 2,086,588 B | 2,084,300 B | −2,288 B |

稳定占用和窗口真实峰值均未增长。Host 单独修复、原游戏包保持不变时，SRAM 峰值也保持 280,088 B；当时目录仅有一个已安装应用。当前工作区还包含 IME 修改，不能将不同固件间的 PSRAM 差额全部归因于 mailbox。

`measurements.json` 也保留了中间 0.1.5 多目标/单目标测试及新版→原版→新版对照，区分最终包与中间包。中断且未完成的测试窗口不进入最终统计。首次加载全过程峰值尚未做匹配对照，本次内存结论限定为以上运行窗口及不新增缓冲/分配的代码检查。

帧缓冲没有新增：Guest 中映射的三帧为 106,560 B，原有 fallback/mapped 联合存储仍预留 142,080 B。线性内存最大值保持 1 MiB，未改变 Guest 静态缓冲或堆配置。原始启动日志的代码映射为 131,072 B；原/新 S3 AOT 文件为 344,300/344,148 B，最终 text 段为 130,044 B，仍使用 131,072 B 映射；初始化段 207,012 B、函数表 1,040 B 均与原版相同，未增加代码映射或函数元数据需求。该光线投射游戏没有新增 Z-buffer。Host Surface 的 frame/scratch/mailbox 堆分配均为 0（映射像素属于 Guest，不能重复计入 Host）；pointer mailbox 保持原大小。

原始 `surface.peak_total` 是自开机累计高水位，也包含此前 Jump Jump 的 raster 分配，未作为本游戏峰值使用。

## 验证与复现

- 新回归测试先在旧实现上失败，修复后通过 ASan/UBSan：双指各自子/父节点持续交错、流身份隔离、最新时间戳、满队列可靠事件、手势边界和独立移动节奏。
- `tests/input_test.cpp` 在 ASan/UBSan 下通过：省略末尾 MOVE 的拖动仍有完整转向量，重复 UP 不重复开火，普通点按开火一次，取消不误开火，双指释放相互独立。
- `bash firmware/components/pxa/tests/test_host.sh` 全部通过。顺便修正已有 IME 头依赖导致的原生测试编译失败：补 adapter include，删除重复 installer typedef。
- pai-touch 模拟器的 `pxa_lvgl_ui_test`、`pxa_surface_test`、`pxa_ui_test`、`pxsys_lvgl_renderer_test` 四项通过。
- 新签名 AOT 实际在 SDL 模拟器中运行，使用真实 SDL pointer 事件验证标题进入、行走、转向、点按开火和释放后恢复；人工检查截图，转向正确且一次点按只消耗一次弹药。模拟器帧率不用于真机结论。
- 真机各轮探针无溢出、无 Surface 切换；覆盖原版、新版重复启动与停止，最终包文件逐项核对 SHA256。截图确认游戏实际进入并转向。修复前后截图因输入积压造成的相机响应差异不应解释为渲染画质变化。

在 pxa-projects 工作目录中：

```sh
bash tools/app.sh build maze-evil --board pai-touch --target esp32s3 --aot-only
python3 tools/pxadb/pxadb.py package install \
  local/app-output/pai-touch/pxa-maze-evil.pxa --port /dev/ttyACM0 --yes
# 确认没有其他程序占用串口，先停止前台应用。
python3 local/pxa-apps/maze-evil/tests/measure_input_latency.py \
  --port /dev/ttyACM0 --output /tmp/maze-input-measurement

python3 local/pxa-apps/maze-evil/tests/verify_simulator.py \
  --simulator build/simulator/pai-touch/pxsys_product_simulator \
  --package local/maze-input-latency-20261012/root-app-output/pxa-maze-evil \
  --publisher-key local/pxa-apps/.dev-signing/publisher-public.der \
  --output /tmp/maze-simulator-check
```

基准脚本要求开始时应用都已停止，结束时释放注入指针、关闭探针、停止自己启动的游戏；不修改设置或存档。复现模拟器时需先构建包含 simulator 目标的包。

只更新应用分区固件，未擦除文件系统、NVS 或分区表。原 Maze 包和可访问数据已备份并校验 SHA256；旧冒号形式目录被当前 FS API 拒绝，原样保留。测试期间外部流程安装的 Jump Jump 也保留。备份含私人数据，留在 Git 忽略目录，不随报告发布。

画面：[修复前](input-latency-20261012/before-playing.jpg)、[Host 修复后](input-latency-20261012/host-only-playing.jpg)、[最终设备包](input-latency-20261012/final-release-playing.jpg)、[模拟器转向](input-latency-20261012/simulator-turned.png)、[模拟器开火后](input-latency-20261012/simulator-fired.png)。

## 组合操作补测

设备实际安装的 0.1.6 包另做三次启动（上面的对照表仍为 0.1.5），每次进入游戏后依次测行走、转向和点按开火，合计九个 4 秒窗口。每次行走拖动保持 1.5 秒，转向保持上述 48 次 MOVE，开火为四次 100 ms 点按、间隔 650 ms。三个启动均通过，输入释放后没有残留移动；四次点按将弹药从 24 减至 20。探针无溢出或 Surface 切换，测试结束停止游戏并释放指针、探针，不改设置和存档。

| 操作 | 显示 FPS | Guest 派发均值 | 关联 LCD 完成均值 | LCD 完成最大值 |
| --- | ---: | ---: | ---: | ---: |
| 行走 | 30.302 | 4.17 ms | 62.45 ms | 78.76 ms |
| 转向 | 30.278 | 4.38 ms | 62.81 ms | 80.42 ms |
| 点按开火 | 30.397 | 8.71 ms | 63.69 ms | 88.43 ms |

测量边界与上文相同：关联 LCD 完成不是物理触摸到光子，也不证明每次射击动画的确切首帧。开火仍在短点按释放后触发。额外安装应用及初始化使整机内存基线不同；原始局部峰值保留在[组合操作数据](input-latency-20261012/gameplay-interactions.json)，不用于与最初目录做内存差额结论。检查了[行走后](input-latency-20261012/repeat-0-walking.jpg)、[开火后](input-latency-20261012/repeat-0-firing.jpg)和[第三次启动开火后](input-latency-20261012/repeat-2-firing.jpg)截图。截图读取在窗口外；最后的连续截图触发限流，基准脚本已增加仅对截图的限流重试，修复后整套补测通过。
