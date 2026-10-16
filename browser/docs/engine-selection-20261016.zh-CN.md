# 小型 JavaScript 引擎验证（2026-10-16）

当前仍使用 QuickJS。本轮完成了 JerryScript 的独立 Guest 原型及 MicroQuickJS 兼容性检查，没有替换已登录的真机浏览器，也没有修改 Host。

结论是：**本轮 JerryScript 原型没有实现减小 ESP32-S3 AOT 的目标，暂不作为生产替换。** 这不是断言 JerryScript 在所有平台都比 QuickJS 大。MicroQuickJS 值得用于更受限的脚本环境，但直接替换会改变已有网页兼容性。

## 为什么不能直接比较引擎宣传体积

浏览器交付的是 Wasm 经 PXA 编译器生成的 Xtensa AOT。原生 ARM ROM、Linux CLI、压缩安装包、Wasm、AOT 映射和运行时堆是不同口径。

JerryScript 解析器使用 `setjmp/longjmp`。WASI SDK 34 的实现需要 Wasm 异常处理扩展；当前配套 ESP32-S3 AOT 编译器对最小验证程序报告 `invalid section id`。本轮遵守 Guest-only 要求，没有增加 Host 导入或启用新的运行时扩展。

另一条路是在构建时通过 Binaryen Asyncify 将恢复流程转换成普通 Wasm，调度循环也放在 Guest C 内。已证明转换后的程序只导入 WASI，不再依赖 `asyncify.*` 或桌面 JavaScript 胶水，但需要额外代码、恢复缓冲和控制流处理。

## 独立探针的体积

两者均使用 WASI SDK 34、引擎 `Oz`、full LTO，以及同一个 PXA `wamrc` 的 `--target=xtensa --cpu=esp32s3 --opt-level=3 --size-level=0 --stack-bounds-checks=1`。JerryScript 使用裁剪 profile，关闭 BigInt、Container、TypedArray、DataView、Proxy、WeakRef、SharedArrayBuffer、Atomics 和模块系统，保留解析器、JSON、Date、RegExp、Promise。

| 文件口径 | QuickJS 探针 | JerryScript 探针 |
| --- | ---: | ---: |
| 转换前 Wasm（剥离调试及名称段） | 517,224 B | 374,068 B |
| 可由现有环境执行的 Wasm（同上） | 517,224 B | 625,350 B |
| ESP32-S3 AOT | 1,617,464 B | 2,191,140 B |

更正：最初记录的 961,545 B / 1,037,474 B Wasm 含工具链带入的 DWARF，不适合用于发布体积对比。现已用 SDK `llvm-strip --strip-all` 统一剥离，并验证所有非 custom section 字节不变。剥离后 QuickJS 通用用例 10/10 通过。[剥离明细](engine-selection-20261016/stripped-wasm.json)及[回归](engine-selection-20261016/candidate-quickjs-stripped.json)保留原始及新摘要。SDK Release 默认采用 `--strip-debug`，会保留名称等少量信息，不能与 `--strip-all` 混为一谈。AOT 已确认不包含 custom/调试 section，其体积不因本次 Wasm 口径更正而直接扣减。

JerryScript 原型的 AOT 多 **573,676 B（35.5%）**。源文件、配置和产物的 SHA-256 见 [artifacts.json](engine-selection-20261016/artifacts.json)。第一次关闭 LTO 的完整 JerryScript 探针更大，不能拿它与 full LTO 的 QuickJS 做配对结论。

**这是两个独立探针的结果，不是完整浏览器包的配对基准。** 探针覆盖的 C API、引擎特性和保护机制不完全相同；尚未计入 JerryScript 的浏览器绑定、可信快照支持、C++ 回调恢复、完整超时及原生栈保护。QuickJS 探针没有读取正式浏览器的可信字节码。不能把本表的数值直接从浏览器包中相减，也不能据此推算 FPS 或最终内存收益。

## QuickJS 探针的 AOT 为什么有约 1.6 MB

对实际 AOT 文件按 WAMR section 格式解析，所有 section、头及对齐合计与文件大小逐字节相符，原始数据见 [quickjs-aot-sections.json](engine-selection-20261016/quickjs-aot-sections.json)。

| 内容 | 大小 | 占文件 |
| --- | ---: | ---: |
| 本机代码区 | 1,336,212 B | 82.61% |
| 重定位信息 | 170,764 B | 10.56% |
| 初始化数据 | 92,780 B | 5.74% |
| 函数元数据 | 17,568 B | 1.09% |
| 目标信息、导出、section 头及对齐 | 140 B | 0.01% |

本机代码区长度含 4 B literal-size 字段，本文件 literal pool 长度为 0。文件没有 custom section，不是大量调试段造成的体积。

这里包含完整的网络 JS 源码解析/编译能力、VM、对象和数组等基础内建，以及本探针主动注册的 Date、JSON、RegExp、Promise；还包括探针文件读取、打印和 WASI libc 的可达代码，不是纯 QuickJS 静态库。`1+2` 只是运行的输入，不会令处理任意 JS 的引擎在链接时自动变成一个加法函数。

引擎 C 到 Wasm 已使用 Oz 和 full LTO，后续 Wasm 到 Xtensa 的 AOT 阶段使用 O3、size-level 0，并启用原生栈检查。该阶段可以重新内联、展开并生成沙箱访问所需指令；不能把原生 x86 的最小 QuickJS 体积或第一阶段 Oz 当作最终 Xtensa AOT 体积保证。各项检查及优化选项的独立体积成本尚未做消融测量。

## 正确性与退出边界

可复用检查器为 `tests/engine_candidate.py`。它只向 Node WASI 暴露生成的测试目录；每条用例都要求探针随后在新 realm 成功执行 `1+2`。外部进程超时用来发现引擎内部无法中断的工作，并不构成浏览器内的保护措施。

| 检查 | QuickJS 探针 | JerryScript 原型 |
| --- | --- | --- |
| 算术、现代语法、属性描述符、中文及四字节字符 JSON | 通过 | 通过 |
| 普通正则 | 通过 | 通过 |
| Promise 构造及 `then` 存在性 | 通过 | 通过 |
| 语法错误后重建 realm | 通过 | 通过 |
| 无限循环中断后重建 realm | 通过 | 通过 |
| 堆耗尽后重建 realm | JS 异常，通过 | 捕获 fatal 10，通过 |
| 灾难性正则 `/(a+)+$/` | 中断，通过 | 超过 3 秒，外部终止 |
| 从源码构建并执行完整 DOM 桥接 | 被既有 WASI 解析深度保护拒绝 | 通过 |

Promise 项没有验证微任务顺序。JerryScript 的 fatal 恢复目前只覆盖 C 探针，不代表可安全跳过实际浏览器 C++ 回调中的析构和资源释放。普通 VM halt 回调不会自动覆盖所有原生 RegExp 匹配工作。

QuickJS 拒绝完整桥接源码是已知的原生栈保护取舍：正式浏览器使用构建时预编译的可信桥接文件，并已在真机通过。现有浏览器 `PXA_SCRIPT_TEST_WASI_MODEL=1 tests/test_script.sh` 的 ASan/UBSan 回归本轮再次通过。JerryScript 尚未进行 PXA 模拟器或真机浏览器集成验证，不能据其源码解析通过推断 12 KiB 设备任务栈安全。

最初 full LTO 原型曾崩溃：执行入口被内联进不应进行 Asyncify 转换的恢复调度边界。固定入口 `noinline` 后，上表 JerryScript 用例和 AOT 编译通过。另发现 `GC_MARK_LIMIT=0` 下桥接执行异常，改为 4 后通过；本轮不采用 0。

原始结果：[通用 JerryScript](engine-selection-20261016/candidate-jerry-core.json)、[通用 QuickJS](engine-selection-20261016/candidate-quickjs-core.json)、[含桥接 JerryScript](engine-selection-20261016/candidate-jerry.json)、[含桥接 QuickJS](engine-selection-20261016/candidate-quickjs.json)。后两份为额外的源码桥接诊断，不是生产浏览器验收分数。

## 内存口径

JerryScript 探针使用 external context，一开始分配 **512 KiB 堆加 context**；其 `heap_used` / `heap_peak` 只表示该池内分配，不含闲置池容量、Asyncify 恢复缓冲、输入源码、线性 C 栈和 AOT。

例如桥接的约 26 KiB 已用堆不能与 QuickJS 按需分配的约 130 KiB 初始化活跃堆直接比较，声称省下约 100 KiB。JerryScript 原型的恢复缓冲也未计入其引擎堆统计。本轮没有证明整机 SRAM、PSRAM 或真实峰值下降。

JerryScript 官方提供 system allocator 等配置，可以研究为按需分配实现，但还需补齐 32 位配置、总预算、错误后的释放以及 C++ 边界。目前没有把它接入浏览器的 256 KiB～4 MiB 设置。现有 QuickJS 的设置仍为按需分配上限，无需预留对应容量。

## MicroQuickJS 的取舍

固定检查版本 `6d4d7eb6b99011d5aab6e88a9b99e8fd4d86c6e9`，本地原生 CLI 已构建。官方明确定位为接近 ES5 的严格子集；约 100 KiB ROM 是 ARM Thumb-2 示例口径，不是 PXA AOT 实测。

直接运行现有 `browser-runtime.js`，在第 5 行 `const` 处语法失败；探测 `Object.defineProperty` 存在，`Object.defineProperties` 和 `Promise` 不存在。因此不能称为等价替换。需要改写自己的桥接、补充 Web API，并明确接受第三方网页语言兼容性下降；只转译应用自己的脚本不能解决下载网页的语法。

## AOT UI 与解释执行的 QuickJS 组件

用户提出保持 UI 为 AOT、将 QuickJS 单独放进 `.wasm` 解释执行。这比继续改 JerryScript 更值得做完整原型：可以保留现有 JavaScript 兼容性，同时降低脚本代码的磁盘体积。

当前代码已有基础：

- pai-touch 的 `sdkconfig.defaults` 同时启用 `CONFIG_WAMR_ENABLE_AOT` 和 `CONFIG_WAMR_ENABLE_INTERP`。
- SDK 的 `tools/pxa_tools/build.py` 和包清单支持每个组件分别选择 `artifact: aot/wasm/both`。
- `pxa/ipc.hpp`、Host IPC broker 和 endpoint resolver 支持异步消息及按需启动服务组件。适合 `main.aot` 加 `script.wasm`，不需要让 Guest 再嵌套一个 Wasm 解释器。
- 当前导入策略不是任意动态链接；不能只把 C 链接库改名为 `.wasm`。应走已有组件和 IPC，而非假定可以直接调用另一组件的 C API 或共享 JS 指针。

同一个 QuickJS 独立探针为剥离调试及名称后的 Wasm **517,224 B**、S3 AOT **1,617,464 B**，Wasm 文件少约 **68.0%**。此前 961,545 B / 40.6% 的比较包含 Wasm 调试段，已更正。这只是探针未压缩文件，不是完整包、安装后总量或内存的收益结论。

建议职责分配：UI 组件保留显示、滚动、触摸、网络和 Cookie；脚本组件持有 VM、脚本 DOM、定时器和 Promise。传递页面批次、输入事件、网络结果和渲染结果，不远程调用每一次属性读写。现有 IPC 每次 payload 上限为 **1024 B**，源码和渲染批次需分块、背压、版本号及取消旧页面；不能堆积整页的多个副本。静态页不触发脚本服务，活跃脚本停止后先释放 VM。组件代码何时真正卸载仍须验证现有生命周期，不能仅因 JS 堆释放便宣称模块内存已归还。

有三个必须测量的限制：

1. **解释执行更慢。** QuickJS 自身是 JS 字节码解释器，外层 C 引擎改用 Wasm 解释执行后，脚本、桥接和原生内建都增加调度成本。可信 JS 预编译可省解析，不能消除引擎执行成本。
2. **组件隔离不等于并行。** 当前 ESP Host 主要在同一 runtime task 调用组件回调。异步 IPC 只能避免 UI 主动同步等待，不能让较长的脚本回调自动与 UI 并行；仍需要短任务、限额和取消。QuickJS 的普通 interrupt 是中止，不是任意位置可恢复的协程。
3. **RAM 不保证下降。** 当前 `pxa_wamr_engine.c` 对普通 Wasm 保留源文件缓冲；AOT 则已有流式读取及可释放输入缓冲的路径。解释器还需模块/执行结构、操作数栈；fast interpreter 可能产生预处理代码。第二组件另有线性内存、C 栈、libc 和队列。JS 堆与 DOM 的逻辑需求不会因为改变外层执行方式自动缩小。

因此应先对照现有单 AOT 与混合组件，分别测试静态页首次打开、脚本服务冷启动、登录/轮询、DOM 更新、拖动中的输入延迟、源码/消息复制、稳定和本次运行的内存峰值。当前尚未实施该拆分，不宣称已经降低内存或保证滚动帧率。

## Wasm 是否可以不完整驻留 PSRAM

当前不能直接按文件小块边读边执行。PXA 在 Wasm 路径完整分配并读取 artifact，WAMR loader 将函数 `code` 指向常驻缓冲；classic 解释器用 `frame_ip` 直接取指，fast 解释器生成自己的预处理指令。部分 classic 加载优化还会改写原始 opcode，因此只把缓冲换成只读 Flash 指针也不可行。

需要区分三条方案：

- **加载时分块读取**：去掉完整源文件副本，执行需要的指令仍常驻。类似已有 AOT 流式装载的目标，可降低装载峰值，但不是运行时分页。
- **按函数或代码页缓存**：首次执行时读取，缓存不足时淘汰。需要修改 loader 和取指/调用/返回位置表示，处理活跃调用、分支、跨块 LEB、验证及指针失效。QuickJS 探针的 code section 为 438,078 B、1098 个函数，最大函数体 29,959 B；小缓存是否够用取决于同时活跃的函数和命中率，不能仅按文件总量估算。频繁 Flash 读取可能引入输入延迟，尚未实现或测试。
- **从 Flash 映射读取代码**：ESP32-S3 可把主 Flash 映射为只读地址，由硬件缓存按需取数据，无需完整 PSRAM 代码副本。需要只读指令支持、适合映射的稳定代码存储和完整生命周期。LittleFS 的普通文件不保证物理连续；把文件系统分区 mmap 并不等于获得一个连续文件指针。

第三条值得优先评估，但属于 Host/WAMR/安装存储适配，不是浏览器 Guest 开关。现有 Guest 无法接管 Host 解释器取指。即使代码不驻留 PSRAM，JS 堆、线性内存、执行栈、DOM 和必要元数据仍需要 RAM。

WAMR 官方的 XIP 文档主要讨论 AOT；当前 PXA 明确拒绝该格式，不能把它当作已实现的 Wasm Flash 解释执行。可另外研究 AOT XIP，但还要验证 Xtensa 重定位、Flash 映射和平台支持。

参考：[ESP32-S3 Flash 映射](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/spi_flash/index.html#memory-mapping-api)、[WAMR XIP](https://github.com/bytecodealliance/wasm-micro-runtime/blob/main/doc/xip.md)。本节均为代码检查与设计判断，未修改 Host 或设备。

## 复现及后续门槛

本地原型目录为工作区 `local/browser-validation/jerry-20261016`，包含独立 upstream 检出、`port/`、`web.profile`、五处 profile 编译条件修复的 `web-profile.patch`、编译日志和产物。未将实验异常恢复层链接进浏览器。

执行已构建的探针：

```sh
python3 local/pxa-apps/browser/tests/engine_candidate.py \
  --wasm local/browser-validation/jerry-20261016/jerry-web-lto-safe.wasm \
  --output /tmp/jerry-candidate.json
python3 local/pxa-apps/browser/tests/engine_candidate.py \
  --wasm local/browser-validation/jerry-20261016/quickjs-probe-bounded.wasm \
  --output /tmp/quickjs-candidate.json
```

需要支持 WASI preview1 的 Node；可通过 `--node` 指定路径。JerryScript 当前应因正则截止失败返回非零。`--bridge` 额外启用完整桥接源码诊断。进程耗时包含 Node 启动，检查器故意不输出可误认为引擎执行时间的性能指标。

替换默认引擎前仍须满足：完整浏览器相同功能的安装包/AOT 对照；真实稳定和本次运行峰值不增长；超时、OOM、解析深度、原生栈和 C++ 回调释放可靠；设置与可信快照保持正确；在模拟器和同固件设备验证交互及滚动。也可继续研究更小的 Guest 异常实现，但不能通过关闭网络脚本解析器或隐藏兼容性下降获得“等价优化”。

官方资料：[JerryScript 配置](https://jerryscript.net/configuration/)、[固定版本解析器 setjmp 实现](https://github.com/jerryscript-project/jerryscript/blob/b7069350c2e52e7dc721dfb75f067147bd79b39b/jerry-core/parser/js/common.h)、[Asyncify 原理](https://kripken.github.io/blog/wasm/2019/07/16/asyncify.html)、[纯 Wasm setjmp 概念示例](https://github.com/kripken/talks/blob/991fb1e4b6d7e4b0ea6b3e462d5643f11d422771/jmp.c)、[MicroQuickJS 语言与 API 说明](https://github.com/bellard/mquickjs/blob/6d4d7eb6b99011d5aab6e88a9b99e8fd4d86c6e9/README.md)。
