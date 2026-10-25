# Anarch for PXA

Anarch 是 Miloslav Ciz（drummyfish）创作的 CC0 第一人称动作游戏。本移植包含原版十个关卡、图像、文字和六种音效，无需另行下载游戏数据。PXA 版不播放背景音乐，包括旧存档开启音乐的情况。上游源码保持不变，固定版本、来源和许可见 [上游说明](third_party/README.md) 与 [LICENSE](third_party/anarch/LICENSE)。平台层、音频适配、测试和应用图标同样以 CC0-1.0 发布。

需要支持 Core 2.1 原生 `main-loop` 的 PXA 固件。160×120 逻辑画面在屏幕及安全区内按 4:3 等比居中显示，支持非整数缩放及小屏缩小。296×240 显示为 296×222，320×240 为 320×240，480×320 为 426×320。最近邻缩放的 Surface 上限为 480×320；更大的屏幕使用 Host 的 1/2/4 倍能力居中显示。屏幕或安全区变化时重新布局，触摸按同一视口回映。逻辑目标为 30 Hz；实际性能取决于设备。线性内存上限 1 MiB，Wasm 栈 64 KiB。

## 操作

| 手柄按键 | 功能 | 模拟器键盘 |
| --- | --- | --- |
| 方向键 | 移动、转向；菜单选择 | 方向键 |
| A | 开火、确认 | Z |
| B | 配合方向键侧移、调整视角；跳过开场文字 | X |
| X | 跳跃；菜单内切换语言 | A |
| Y | 下一件武器；菜单内打开帮助 | S |
| L / R | 左右侧移 | Q / W |
| SELECT | 配合下方向键打开菜单 | TAB |
| START | 游戏菜单 | ENTER |

默认使用中文，已有语言偏好优先。菜单按 X 或触摸右上角“中文 X / EN X”切换语言；按 Y 或触摸“帮助 Y / Help Y”打开操作说明，A/B/Y 或轻触返回。菜单、选项、教程、HUD 标签、地图提示、故事概述与结算提供中文；英文故事保留上游原文。字体来源见 [字形头文件](../common/pocket-ui/glyphs.hpp)，SIL OFL 许可见 [OFL.txt](../common/pocket-ui/OFL.txt)。

“继续 / Continue”恢复关卡，“地图 / Map”打开地图，“声音 / Sound”切换音效开关，“视角 / Look”切换固定、自由与自动视角。声音菜单保留原存档的四种内部状态，但只按音效位显示“音效 / SFX”或“关闭 / Off”，背景音乐始终关闭。

触屏左侧移动或转向，右侧中部开火或确认，右侧下部跳跃，右上角菜单，底部偏右为 B。首次触摸后显示轮廓提示，使用手柄后隐藏。触摸区域随画面缩放；单触点硬件同一时刻只能操作一个区域。触屏取消、移出按键区及进入后台会释放按键。部分固件直接处理系统返回并退出应用；打开游戏菜单可用 START 或右上角触屏区。

## 音频与存档

保留原版 8 kHz 六种音效，最多混合四路，带直流滤波与限幅。音乐命令作为兼容接口被忽略。优先使用原生 PCM；后端不支持时使用 16 kHz 单声道媒体会话并线性插值。拒绝可选的 `audio.playback` 权限后仍可静音运行。

游戏保存原版 12 字节数据，键为 `anarch.save.v1`，包含设置、解锁进度和关卡记录，不提供任意位置即时存档。语言偏好独立保存在 `anarch.preferences.v1`，格式为 `A L 1 locale`，0 为中文、1 为英文；无效时使用中文。切换语言异步写入并在失败时重试。通过游戏菜单退出时最多等待两秒保存。进入后台或锁屏暂停逻辑、画面和音效，恢复时扣除暂停时间。

## 构建与验证

从工作区根目录运行，按工作区说明准备 WASI SDK、WAMR AOT 编译器及开发签名密钥：

```sh
tools/app.sh build anarch --source-root local/pxa-apps \
  --target esp32s3,esp32s31,simulator --aot-only \
  --output local/artifacts/anarch/<时间戳>/build
```

输出签名包与展开目录。设备空间有限时只选择所需架构；`esp32s3` 为 Xtensa，`esp32s31` 为 RISC-V，`simulator` 为桌面平台。通过 `tools/pxadb/pxadb.py package install` 安装，无需刷写固件。

[前端测试](tests/frontend.py) 检查视口、安全区、触摸、Surface 重建、语言偏好、旧存档兼容及中文字形：

```sh
python3 local/pxa-apps/anarch/tests/frontend.py --sanitize \
  --output local/artifacts/anarch/<时间戳>/frontend
```

[音频测试](tests/audio_test.c) 与 `audio.c` 一起用 C99 编译，支持 `-Wall -Wextra -Werror -fsanitize=address,undefined`，验证六种音效、音量、并发上限、暂停、音乐命令无声及分块一致性。可执行文件、日志与采样数据均放在 `local/artifacts`。签名包运行和生命周期检查见 [模拟器测试](tests/simulator.py)；PCM 检查不代替扬声器听音。
