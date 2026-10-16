# 轻量拼音输入法

面向 Flash 和 PSRAM 较少设备的可安装 C++26 Provider。去掉完整版的整词词库、统计解码器、用户词库和模型初始化，保留拼音单字输入、九键/全键、候选翻页和分词。安装后在系统设置 → 输入法中选择「轻量拼音输入法」。普通应用仍使用 `ui::TextInput`，无需修改。

拼音转成汉字仍需要读音映射；这里使用约 **19 KiB 的不可变单字表**，而非约 1 MiB 的整词词典。表中有 **6,749 个不同汉字、7,292 个读音条目、410 个音节**，按原始单字频率排列候选。字表和检索不建立运行时词库或动态缓存；表本身计入 Guest 线性内存，不能把它说成不占 RAM。完整覆盖范围是随包提交的 [字表](data/pinyin.tsv)，不保证所有国标汉字或生僻字均可输入。

## 操作与取舍

- 与 [完整版](../pinyin-ime/README.zh-CN.md) 共用 Host 手机键盘和设置。圆屏、圆角、安全边距、DPI、字体、数字/符号、英文及密码处理沿用现有设计，没有另一套键盘 UI。
- 输入 `ni` 选择「你」；`nihao` 或 `ni'hao` 先选「你」，再从保留的 `hao` 选择「好」。九键选读音后同样逐字选字；选完首字恢复剩余九键状态。
- 不含智能整词/整句候选、联想或学习。无分隔的连续拼音按最长完整音节前缀逐字处理，不做词义消歧；例如 `xian` 和 `xi'an` 不同，歧义处使用「分词」。也接受 `lv/nv/lve/nve` 写法。
- 未完成的拼音可以提供读音前缀候选。未知拼音沿用共享流程的原文提交行为。候选每帧最多 12 个，向前翻页没有固定词数上限；拼音最多 64 字节，九键最多 24 位。
- 中文按需启动，关闭、英文或密码模式释放 Provider。没有常驻解码器、新线程、帧缓冲或字形图集。字体仍由设备提供，缺失的字形不会因安装本 Provider 自动补齐。

`PinyinSession<Engine>` 负责统一的会话、九键、删除、逐字尾句、分页及消息编码；`CharacterEngine` 的查询状态为 72 字节，以二分查找定位静态音节表和连续候选范围。任意页的单个候选直接索引，不再反复扫描音节。完整版使用空的 Google 引擎适配器。使用 C++26 concept 与模板静态分派，不增加虚函数、重复 UI 或新的 ABI。共享编排层仍可使用长度有界的 `std::string`；并非宣称整个 SDK 或 Host 没有动态分配。

## 构建、安装与验证

在 pxa-projects 根目录执行：

```sh
# 小设备优先单架构 AOT-only；不能在其他架构或 Wasm-only Host 上运行。
bash tools/app.sh build pinyin-ime-lite --target esp32s3 --aot-only \
  --source-root local/pxa-apps --output local/app-output/pai-touch/pinyin-ime-lite
python3 tools/pxadb/pxadb.py package install \
  local/app-output/pai-touch/pinyin-ime-lite/pxa-pinyin-ime-lite.pxa --port /dev/ttyACM1

# 需要可移植回退时使用混合包；模拟器与 Wasm 分别构建/验证。
bash tools/app.sh build pinyin-ime-lite --target simulator --source-root local/pxa-apps
bash local/pxa-apps/pinyin-ime-lite/tests/run.sh
bash deps/pxa-system/tools/package/test_cpp_ime_app.sh \
  build/simulator/pai-touch/pxsys_ime_test /path/to/signed/pxa-pinyin-ime-lite \
  /path/to/publisher-private.pem --single-character-provider
```

需要支持可选 `ime 0.1`、Core 2.0 的 Host。包默认携带 AOT 与 Wasm；`--aot-only` 删除回退文件以节省存储和验证时间，不跳过签名校验。源码复用相邻 `pinyin-ime` 的会话和 T9，构建时需要两份源码，设备只需安装轻量包，不会隐式依赖或安装完整版。

原生测试逐项读取全部候选，检查读音分割、多音字、别名、容量边界、消息编码/编辑事务、九键尾句恢复和 10,000 个压力输入，并使用 ASan/UBSan。模拟器沿用两版相同的 13 种配置和生命周期检查；轻量版按逐字输入验收，不冒充完整版整词能力。异常包检查改为破坏/删除/链接代码资产，保留签名和完整清单验证。

真机复现使用共享 `pinyin-ime/tests/ime-heartbeat`，先构建并临时安装该 fixture，保持前台空闲：

```sh
python3 local/pxa-apps/pinyin-ime/tests/measure_lifecycle.py \
  --port /dev/ttyACM1 --phone-layout --provider pxa-pinyin-ime-lite \
  --reopens 3 --output /tmp/ime-lite-memory
python3 local/pxa-apps/pinyin-ime-lite/tests/verify_device.py \
  --port /dev/ttyACM1 --output /tmp/ime-lite-input
```

脚本核对 pai-touch 序列号和 296×240 屏幕，恢复输入法选择、停止 fixture；不负责安装/卸载。核验实际提交「你好」，截图需要人工检查。测量窗口内不截图，`memory_scope=local` 表示本次窗口峰值；`boot` 阶段只取稳定占用，不拿历史最低空闲当本次峰值。

20261013 的 ESP32-S3 AOT-only 包为 **203,122 B**，相同目标的完整版为 **1,231,146 B**；当时模拟器 Provider 线性内存为 **131,072 B**，完整版 **1,245,184 B**。这是历史线性内存分项，代码映射、WAMR 元数据、Host 键盘和字体另占内存；无 PSRAM 设备尚未验证。当时 pai-touch 候选就绪约 **1.09 s**，键盘先显示，签名认证和 AOT 激活仍有同步停顿。历史实测见 [验证报告](docs/20261013-validation.zh-CN.md)；当前 SDK 下的受控前后性能、AOT/Wasm 内存和测试见 [20261015 两版 Provider 优化](../pinyin-ime/docs/validation/20261015-provider-optimization.zh-CN.md)，不同构建和工作负载的数据不可直接比较。

## 字表来源

[NOTICE](NOTICE) / [LICENSE](LICENSE) 记录 Apache-2.0 来源。字表派生自固定 `lvgl_pinyin_ime` 提交 `037169998150a768ff43863fc44d563222d4b801` 的许可词表，只取可由 GB2312 编码的 BMP 汉字单字及读音，去重后按原频率排序。源码保存有序 TSV，构建无需网络或原始 3.5 MB 词表。

```sh
python3 local/pxa-apps/pinyin-ime-lite/tools/generate_table.py --check
# 重取时校验固定版本；--check 验证两个输出，不写入。
python3 local/pxa-apps/pinyin-ime-lite/tools/generate_table.py \
  --reference /path/to/pinned/lvgl_pinyin_ime --check
```
