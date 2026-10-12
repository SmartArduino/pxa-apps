# PXA 拼音输入法

可独立安装、升级的 C++26 输入法。`main` 页面介绍使用方法，`provider` 是按需启动的独立 Service Component。系统负责焦点、键盘和文字提交，Guest 负责拼音解码；普通应用继续使用 C++ SDK 的 `ui::TextInput`。

空间和 PSRAM 较少的设备可以选择 [轻量版](../pinyin-ime-lite/README.zh-CN.md)：去掉大词库与统计解码器，保留小型拼音单字表和相同键盘。两版通过 `PinyinSession<Engine>` 共用会话、九键、分词、删除、分页与消息编码，使用编译期分派；完整版的词句解码行为保持原样，不因安装轻量版增加常驻开销。

安装后在系统设置的「输入法」中选择「拼音输入法」。需要包含可选 `ime 0.1` 服务的 Host；旧固件不能加载这个服务。关闭键盘或切换应用后释放解码器。引擎不可用时系统自动使用内置键盘，密码始终由系统处理。

## 布局和操作

- 九键三行四列，右列固定删除、分词和回车；长按字母组直接输入数字。全键完整 QWERTY，中文保留分词，英文单次大写、双击锁定。
- 底栏固定「符」「123」「中/EN」「空格」「收起」，常用操作不藏在齿轮里。数字四行、符号两页；当前面板入口改成「返回」。齿轮选择布局和语言。
- 空闲时一行简洁提示；组合输入时上排拼音、下排汉字，两排分别左右滑动。箭头展开候选网格，「键盘」恢复输入。选择拼音不提交正文，读音不重复显示。
- 空格确认首候选或插入空格；长按并左右拖动移动原光标。删除可长按重复。回车先确认组合，再按单行/多行编辑器完成或换行；收起不会提交表单。
- 切换语言/布局确认已有组合和排队文字。移动原光标、切换编辑器或外部改写正文取消旧组合。九键「分词」锁定完整读音；选中半句后保留剩余九键状态。
- 系统设置独立「输入法」页面，保存默认输入法、布局和语言。英文默认全键，可主动选九键多击（650 ms 确认）；密码全键且不能启动中文 Provider。

布局综合 DPI、字体、安全边距及屏幕轮廓，圆屏/圆角屏逐行计算安全宽度，大屏限制宽度。通常控制在屏幕高度 60% 内，极小屏约束冲突时允许提高占比。所有尺寸保留完整 QWERTY，极窄屏更适合默认九键。中文大陆黑体，拉丁字符优先复用固件现有抗锯齿位图字形，不增加动态字形缓存。12 px 固定标签另复用 39 个字形（含中点）的大陆黑体 A8 位图（5,532 B，只读数据），大字号与其他候选仍用产品字体。

已有滚动容器优先显示原编辑器的当前行，关闭时恢复滚动位置；固定或过高的编辑器使用键盘内的当前行辅助区，支持点击及左右箭头移动原光标。它只保留光标附近的有界 UTF-8 窗口，密码始终遮蔽，不建立第二套编辑状态。键盘仍是 Host 的 LVGL 控件，没有额外帧缓冲或字形图集。

键盘先绘制加载状态，再启动签名验证和词典加载。英文由 Host 直接编辑；选择英文或密码时不启动中文 Provider，切换英文会取消加载或卸载已加载词典。验证按阶段/字节预算推进，中文提前输入进入有界队列。关闭后仍释放词典，而不以常驻缓存换取打开速度。

## 构建与测试

在 pxa-projects 根目录执行：

```sh
bash tools/app.sh build pinyin-ime --target simulator --source-root local/pxa-apps
bash tools/app.sh build pinyin-ime --target esp32s3 --aot-only --source-root local/pxa-apps
bash local/pxa-apps/pinyin-ime/tests/run_t9.sh
bash deps/pxa-system/tools/package/test_cpp_ime_app.sh \
  build/simulator/pai-touch/pxsys_ime_test \
  local/app-output/pai-touch/pxa-pinyin-ime \
  local/pxa-apps/.dev-signing/publisher-private.pem
```

集成测试使用签名输入法和普通 C++ TextInput 应用，覆盖中文词句、全键/九键、数字与英文、长按删除、确认、正文重置、背压重试、密码隔离、故障回退与重复加载释放，并渲染 13 种分辨率、DPI 和屏幕形状。Host 协议与生命周期说明见 pxa-system 的 `docs/zh-CN/input-methods.md`。代码审查、九键防误删和动态密码模式回归见 [审查记录](docs/validation/20261013-review.zh-CN.md)。本轮布局、真机配对及限制见 [手机键盘验证](docs/validation/20261012-phone-keyboard.zh-CN.md)。按需生命周期、前台停顿和内存对照见 [生命周期优化记录](docs/validation/20261012-lifecycle.zh-CN.md)。截图、打开时序、峰值内存和限制见 [自适应与快速打开验证](docs/validation/20261012-adaptive.zh-CN.md)；最初版本的测量保留在 [实现验证](docs/validation/20261011.zh-CN.md)。

## 词典与限制

词典来自固定版本 `lvgl_pinyin_ime` 的许可词表，使用其 libgooglepinyin 解码器；来源、版本和移植修改见 [third_party/README.pxa.md](third_party/README.pxa.md)、[NOTICE](NOTICE) 和 [LICENSE](LICENSE)。

```sh
python3 local/pxa-apps/pinyin-ime/tools/rebuild_dictionary.py
```

生成的词典应为 1,068,442 字节，SHA256 为 `179311c55ab9b912a07ef040e5a95af97e704e2248b7b97735f17a3e05d13b67`。它通过组件私有只读文件能力直接读入最终解码表，不额外保留整份原始词典。

当前支持最多 64 字节拼音、24 位九键输入；每次最多传输 12 个候选及 32 个拼音，搜索有工作量上限。引擎输出字形限于 BMP；编辑器本身仍支持 UTF-8。暂未实现用户词频持久化、双拼、模糊音、云候选和手写，不能将这些视为已支持功能。
