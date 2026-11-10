# Pocket Studio

为 `pocket-game` 与 `pocket-play` 提供可选的原生分辨率美术与系统字体。
应用在 CMake 为组件设置 `PXA_POCKET_STUDIO=1`，保留原有 288×224 逻辑坐标、输入与存档接口。

- 几何图形直接写入已获取的 surface 输出缓冲区，省去低分辨率中间帧。画面等比适配，保留棋盘格比例。
- 字体通过透明 Canvas 的 sized-text 指令使用 Host 字体，在物理屏幕分辨率渲染；不放大位图字模。文字指令保留顺序遮挡，暂停菜单覆盖旧标签；内容哈希避免重复提交相同文本。
- `studio::label` 提供居中/左右对齐与定宽裁切；较长的放大数字先降字号。图形只允许在 `game::draw()` 期间绘制。
- `studio::round`、`disc`、`card`、`button` 在输出分辨率绘制；`picture` 用只读 RGB565 调色板及索引图，绘制时无需解码器、文件服务或堆分配。
- `studio::round_alpha` 在当前原生帧上混合圆角面板，透明度范围为 0–256，保留裁切与文本遮挡顺序。
- `studio::round_outline` 绘制连续的圆角细描边，与填充共享圆角几何，内部区域保持透明。
- 圆角与安全区根据系统 environment 设置。每边至少保留 `ceil(radius × (1 − 1/√2))` 的内缩，避免棋盘和底部按钮被圆角裁切。
- 暂停菜单入口为逻辑 x240–263/y3–20，语言、继续、重开与保存退出沿用共用运行时。

运行时菜单、字体画布和游戏状态分离；应用仍负责验证存档并选择何时保存。
系统字体由 Host 提供，未引入新的字体库或系统依赖。未启用宏的其他游戏沿用原绘制路径。

`pocket-play` 应用可通过 `PXA_POCKET_CUSTOM_MENU=1` 实现 `game::draw_menu(bool paused, int row, bool exiting)` 与 `game::menu_hit(int x, int y)`，自定义暂停菜单外观及入口按钮的命中区域。暂停面板沿用共享运行时的位置与四行命中区域，语言、重开和保存退出由运行时处理。

`PXA_POCKET_THEME_TEXT=1` 使用 Host 预加载的系统 Caption 字体，排版可通过 `pocket::sp()` 换算系统字体单位。`PXA_POCKET_FAST_START=1` 让首个图形帧先完成提交，随后发布原生文字画布。

场景源图位于各应用的 `assets/scene.png`，生成提示及来源位于同目录 `scene-source.json`。
重新编码：

```sh
python3 local/pxa-apps/common/pocket-studio/prepare_scene.py \
  local/pxa-apps/lantern-grove/assets/scene.png \
  local/pxa-apps/lantern-grove/scene.inc
```

`tests/simulator.py` 使用真实已签名产品模拟器，验证五款游戏在矩形及圆角屏幕上的操作、双语菜单、存档与重启。
运行输出必须指定到工作区 `local/artifacts/<topic>/<timestamp>/`，不要写入源码目录。

`picture_region` 支持场景局部回填及透明角色图集（索引 0 透明）；`raster` 将软件 3D 帧直接缩放到 surface，避免逐像素调用绘图接口。`glow` 使用有界径向混合，直接叠加在原场景上。上述绘图均无需堆分配。

咖啡厅角色图集为 `harbor-cafe/assets/characters.png`，四列两行，每格 64×80，脚部锚点 y=76。生成来源保存在 `characters-source.json`。用 `prepare_sprites.py <输入透明图集> <生产PNG> <characters.inc>` 重新编码。

`tests/drawing.cpp` 对真实原生绘制实现执行缩放、透明索引、局部回填、边缘裁切和光晕混合检查；建议结合 ASan/UBSan 运行，输出可执行文件及日志仍放到 `local/artifacts/`。

`particles.hpp` 提供固定容量的短寿命碎光池，更新与绘制分离。粒子使用独立的随机流，不能改变游戏随机结果或存档；绘制使用不遮掉文字的光晕混合。默认32粒，可按应用缩小容量。
