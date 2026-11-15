# 遗迹疾奔 · Relic Run

PXA 第三人称 3D 无限跑酷。探险者穿越丛林石桥、落日神庙和幽蓝古墓，躲避倒木、石块、低门与断桥，并按路口提示转弯。每 400 米切换环境；速度随距离逐渐增加。金币计入得分，护盾抵挡一次碰撞，磁铁在限时内吸取金币。最佳得分、累计金币、画质与声音设置保存在应用数据中。

触屏：左右滑动变道，上滑或轻触跳跃，下滑滑铲。路口提示出现后靠近路口，再向指定方向滑动。右上角暂停，暂停菜单可开关声音或返回主菜单。标题页可打开操作指南和切换精美／流畅画质。

手柄：方向键变道，↑／A 跳跃，↓／B 滑铲，START 暂停／继续。标题页 A 开始、B 操作指南、X 切换画质；暂停页 A／START 继续、X 开关声音、B 返回主菜单。失败后 A 重来。

在固件工作区根目录运行：

```sh
tools/dev.sh sim temple-runner --source-root local/pxa-apps
```

构建模拟器和 ESP32-S3 包：

```sh
tools/app.sh build temple-runner --target simulator,esp32s3 \
  --source-root local/pxa-apps \
  --output "local/artifacts/temple-runner/$(date +%Y%m%d-%H%M%S)"
```

游戏以 60 Hz 固定步长模拟，目标呈现周期 33 ms。使用 PXA GameRender 批量绘制、近裁剪、可见面剔除、有限绘制距离与预计算的模型截面；无逐帧堆分配或软件深度缓冲。渲染尺寸按屏幕使用整数倍缩放，画质选项减少植被、装饰和金币远景数量。实际真机帧率仍需在所用固件、设备和 LCD 上测量。

逻辑与生成关卡检查：

```sh
local/pxa-apps/temple-runner/tests/check.sh
```

`tests/touch.json` 和 `tests/controller.json` 供 PXA `verify --scenario` 使用。验证输出请指定工作区的 `local/artifacts/`；商城截图须使用 `--store-screenshots` 并检查原图。

美术为代码生成的原创几何模型，音乐由 `tools/build_music.py` 合成，不包含第三方游戏模型、音频或品牌资源。`tools/build_font.py` 使用系统安装的 Noto CJK 生成字形，许可与版权见 `licenses/`。图标由 `tools/build_icon.py` 从原创 SVG 构建。

字体生成后会同步更新随包的 `assets/THIRD_PARTY_LICENSES.txt`，包含应用、共享 SDK 及 Noto 字体的完整声明。仅更新声明时可运行 `python3 tools/build_notices.py`；`licenses/` 中的原始声明也随源码保留。
