# 0.1.4 部分音效丢失与恢复

用户描述正常游玩后部分声音消失、其他音效仍然存在。本次没有取得那一次故障发生时的日志，因此不能证明当次只由以下某一个问题造成，也不能据此判定 USB 供电正常或异常。代码检查和故障注入确认了两处独立的软件问题。

验证分为完整工作区与独立提交快照两组。设备、模拟器及原生十三项记录来自包含此前平台、运动参数和 Clock 0.2 改动的完整工作区。音频修复最初单独提交，并将当时暂存内容导出到独立目录，使用已提交的 SDK 验证：SDK 音频原生检查、九项接口编译诊断，以及应用原生十项（含三项音频故障回归）全部通过，见[独立快照检查](audio-recovery/commit-snapshot-tests.log)。相关平台、运动参数和 Clock 0.2 改动现已分别纳入提交；复现最终 0.1.4 应使用这些完整改动，下方包哈希标识实际安装和测量的产物。

## 原因与修改

| 问题 | 修改与收益 | 内存变化 | 验证 |
|---|---|---|---|
| 0.1.3 遇到暂时配额或资源不足，超过三次重试后将正常素材加入永久失败集合 | 共享 `AudioBank` 区分临时错误和素材错误；临时失败持续退避，饱和计数，不再永久静音；缺失文件仍抑制重复请求 | 缓存仍八槽、同时一个加载 Task；重试字段使用现有对齐空间 | 连续八次配额/资源失败后蓄力声恢复；缺失文件只请求一次 |
| 离开音乐盒时长素材仍在异步加载，加载完成后再次缓存，重新钉住 430400 B 资源 | `release(id)` 同时释放现有句柄、标记该资源的在途请求作废；迟到的句柄直接关闭；自然结束及替换也释放音乐盒素材 | 不增加第二个 Task、队列或缓存；减少不再需要的长素材驻留 | 先停止、后完成加载，断言关闭句柄且不播放；随后两个蓄力声道正常 |
| 音乐流错误后应用直接清除播放请求 | 对临时错误按 200/400/800 ms 最多三次重试；就绪后重置；旧实例事件不影响新实例 | 复用声道截止时间和结构对齐空间 | I/O 错误退避后重开；忽略旧实例结束事件；正常单次结束不重播 |

音效加载的退避按未命中缓存的 `prepare()` 调用次数计算，为 4、8、16、32、64、128，封顶 128；缓存命中照常播放。共享退避避免切换素材 ID 绕过限流，每轮连续临时失败只记录一次警告。音乐流使用单调时间计时，和素材加载的退避不同。

长素材仍使用现有 Host 资源缓存；本轮不更换素材、不改音调、音量、采样率、跳跃参数或背景音乐规则。开场 ICON 原本是不循环的约 2.8 秒音乐；自然播放结束是正常行为，不将其改成常驻 BGM。音乐盒、商店、井盖的停留奖励音效是独立声道。

## 检查与复现

新增 `common/cpp/tests/reference/jump_audio_recovery_test.cpp`，运行真实 C++ SDK 的 Context、协程请求分发和游戏音频控制器，只替换 Host 请求回复。修复前两个问题分别触发「有效素材不能被加入永久失败集合」和「停止后不应缓存长素材」断言，见 [修复前输出](audio-recovery/before-pressure.txt)、[迟到完成输出](audio-recovery/before-late.txt)。

修复后的原生 13 项检查通过，ASan/UBSan 四项相关检查通过。完整运行排除了此前已记录、与本轮无关的 `pixel_reference_equivalence`；不宣称这个既有失败已经修复。Jump Jump 和共享音频组件的 Pixel Dungeon 均通过真实签名 AOT 模拟器在 296×240/160 DPI 圆角屏、480×480/305 DPI 圆角屏的操作检查；Pixel Dungeon 同时覆盖开始、背包、设置、退出与进程重启后读取存档。检查原始输出及摘要保存在 [audio-recovery](audio-recovery)。

最终签名诊断 AOT 包在每次素材读取额外延迟 20 ms 的模拟器中完成十八跳，最终得分 386。Korvo 安装生产 0.1.4（release sequence 5）后也完成十八跳、最终得分 386，覆盖第十二跳音乐盒、第十七跳商店及其后的蓄力。本轮只安装游戏包，没有更新固件、清除存档或修改音量等设备设置。

真机这次实际捕获一次 `scale_intro.s16` 的临时配额不足（`error=-8`），不是只在模拟回复中制造失败。后续单独蓄力时，两个连续 DMA 样本的非零块计数增加 100，写入错误仍为零，确认该声道组恢复了非静音输出，见[恢复后的设备记录](audio-recovery/charge-after-pressure.json)和[蓄力截图](audio-recovery/device-charge-after-pressure.jpg)。十八跳的十四个 DMA 采样均为零写入错误，没有永久素材失败或音乐流错误日志，见[真机摘要](audio-recovery/device-summary.json)。DMA 证明数字音频输出到 I2S 的状态，不替代扬声器声学检测。

从 workspace 根目录运行：

```sh
PXA_CPP_GAME_TEST_BUILD="$PWD/local/jump-audio-tests" \
  bash local/pxa-apps/common/cpp/tests/run.sh \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
ctest --test-dir local/jump-audio-tests --output-on-failure -R jump_audio

# 独立目录构建诊断包，不在生产包上打开诊断输出。
PXA_APP_DEFINES=J3_TRACE_TIMING=1 \
PXA_BUILD_CACHE_DIR="$PWD/local/jump-audio-trace-cache" \
  bash tools/app.sh build jump-jump-3d --target simulator --aot-only \
  --source-root local/pxa-apps --output local/jump-audio-trace

# 先启动独立的模拟器 PXADB 服务；按服务地址覆盖 endpoint/control。
TZ=CST-8 PXA_ASSET_READ_DELAY_US=20000 \
  python3 local/pxa-apps/jump-jump-3d/tools/reference/simulator_platforms.py \
  --package local/jump-audio-trace/pxa-jump-jump-3d \
  --output local/jump-audio-slow-assets --utc-offset-seconds 28800

python3 local/pxa-apps/jump-jump-3d/tools/reference/device_audio_replay.py \
  local/jump-audio-device \
  --reference-log local/jump-audio-slow-assets/simulator.log --jumps 18
```

真机重放工具使用 Korvo `/dev/ttyUSB0`、2000000 波特率，重新启动当前游戏但不清除存档，按同一地图种子和目标间距执行十八次跳跃。生产包没有开启 `J3_TRACE_TIMING`。真机和模拟器的落点、分数不要求逐像素一致，不把这两种工作负载拿来比较 FPS。自动检查能证明请求、资源和设备写入状态，不能替代声学录音或主观听音。

## 内存与限制

原生完整应用状态仍 43560 B，`AudioBank` 仍 552 B，渲染状态仍 9520 B；Guest 线性内存仍初始 131072 B、上限 1048576 B。三帧缓冲 2304000 B、命令队列 49152 B、深度缓冲 0 B，不提高 Host 音频资源预算。本次改善在于释放不再使用的资源及恢复能力，不声称提高显示 FPS。

恢复逻辑使代码映射增长：S31 AOT +4932 B，S3 +4544 B，桌面 +6348 B，见 [包哈希](audio-recovery/package-hashes.json)。因此不能将「固定数据不增大」写成「总内存完全不变」。真机内存日志中的最低空闲和预算峰值是开机累计值，包含此前会话、安装和截图，不是本次运行的真实峰值，不能用于宣称峰值下降。

| 最终包的采样 | 开局 READY | 十八跳后 READY |
|---|---:|---:|
| SRAM 空闲 | 59071 B | 59035 B |
| PSRAM 空闲 | 10368496 B | 10201832 B |
| 游戏预算内部占用 | 44924 B | 44924 B |
| 游戏预算外部占用 | 196938 B | 360218 B |
| 其中音频缓存 | 138304 B | 301584 B |
| 预算拒绝 / 分配失败 | 0 / 0 | 0 / 0 |

缓存会随实际访问的素材变化；上表是同一次运行中不同场景的占用，不能解释为修复增加了 163280 B 内存。应用临时配额错误与预算拒绝计数的统计位置不同，后者为零不等于整个流程未遇到临时资源压力。原始 SRAM 最低空闲 15076 B、PSRAM 最低空闲 7648484 B、游戏预算内部/外部峰值 61348/558692 B 均为本次开机累计；单次真实峰值仍未取得，见[开局](audio-recovery/device-ready-memory.txt)与[操作后](audio-recovery/device-final-memory.txt)采样。
