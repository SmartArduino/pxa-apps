# Voxel Craft C++

使用独立 PXA C++26 SDK 的体素沙盒。纹理和调色板在初始化时逐个加载、绑定并释放临时句柄，Renderer 持有绑定引用；稳定帧仅提交复用的 DrawList。

触摸左侧控制移动，右侧拖动转向；侧栏控制挖掘、放置、飞行和上下移动。热栏选择方块，多指按 pointer_id 独立管理；后台取消持续动作。

渲染从眼睛位置投影，保持 Q4 坐标和 Q8 深度。近面与视锥裁剪保留 UV；四边形和裁剪三角形共享深度，叶片透明 texel 不写遮挡深度。所有 45 个方块材质与 HUD 使用正确资源；绿色仙人掌没有树冠。

默认保留完整透视纹理。显式启用 `VOXEL_CHEAP_PATHS=1` 后，面按投影大小和深度变化选择绘制路径：不透明面宽或高小于 4 像素时用
对应材质/光照的平均 RGB565 色；纹理误差不超过半个 texel 时用仿射 UV；
其余保留透视 UV。树叶等 cutout 始终保留纹理和透明孔洞。四边形使用
扫描线深度路径，裁剪后的多边形用有界退化四边形扇形，所有路径共用深度缓冲。
目前 pai-touch 树林斜视的近似路径没有实测收益，默认关闭，不能宣称比完整纹理更快。
颜色表由实际资源生成，`tools/generate_lod_colors.py --check` 检查同步。
完整画面和 HUD 请求直接扫描输出；输入 Canvas 没有可见 UI 内容。
Host 遇到系统覆盖层时仍保留合成回退，不绕过遮挡或 UI 检查。

区块可见性与精确裁剪共用视空间远面，避免径向距离误剔除边缘的树冠和树干。
地图先完成地形再生成植被，草丛不能替换树干底部。保留 8 格区块，
每块固定 192 个 quad；32 个回归种子的最大值为 173，World 从 688896 降至
393984 字节。复杂编辑超容量时分页复用同一缓存，避免漏面。视距根据实际
帧间隔与 Host 光栅化成本调整，且裁剪远面同步使用该视距，低质量仍保留深度。
模拟步长保持 16 ms，最大更新次数显式设为 8，使约 10 FPS 时移动和重力
仍按实际时间推进；后台恢复重置模拟时钟。

构建：

```sh
bash tools/app.sh build voxel-craft-cpp --target all
bash local/pxa-apps/voxel-craft-cpp/tests/test.sh
```

主机测试检查多指输入、后台复位、32 个地图种子的树干/树冠、所有区块的可见面、超容量分页与编辑重建，
以及绘制路径边界、相同可见面数、树叶 cutout、DrawList 容量和零帧内堆分配。
`SANITIZE=1 bash local/pxa-apps/voxel-craft-cpp/tests/test.sh` 加跑 ASan/UBSan。
`PXA_APP_DEFINES=VOXEL_PROFILE=1` 可启用 Guest 绘图计时；默认不增加帧内 WASI 计时调用。

可重复的绘制对照用 `PXA_APP_DEFINES=VOXEL_BENCH_DISTANCE=24` 固定出生相机、
地图种子、视距和光照；`VOXEL_CHEAP_PATHS=0` 可关闭三档绘制。测量包关闭运动与
自动视距调整，正式包不定义 `VOXEL_BENCH_DISTANCE`，保留正常触摸和运动。

严格 C/C++ 对照使用 `VOXEL_BENCH_SCENE=1`（正视）或 `2`（斜视），C 版另加
`VOXEL_AUTOPLAY=1`。公共 fixture 在 `../common/voxel_benchmark.h`，固定相同的
98304 个方块、相机、FOV、24 格远面、中性光照和完整纹理，关闭 HUD/近似绘制/
自适应与模拟。编译定义用逗号分隔。`tests/benchmark_test.cpp` 对照全部方块和
缓存容量；C 原版 Z 面 UV 交换已修复后才允许参与画质一致的对照。
在固定场景另加 `VOXEL_CHEAP_PATHS=1,VOXEL_BENCH_CHEAP=1` 可对照绘制近似的画面与收益；
该结果应独立标注，不能与完整纹理结果混作语言性能比较。

`VOXEL_PROFILE=1` 将几何准备与编码放在同一命令缓冲的两个阶段，只在边界
计时，120 帧后输出均值。它仍有观察者成本，最终显示 FPS 必须使用不含该宏
的包；Guest 提交墙钟时间包含 Host 抢占，不等于纯编码 CPU 时间。

2026-10-08 pai-touch `/dev/ttyACM2` 三轮显示帧率约 7.42 FPS，Host 光栅化约 59.5 ms；会随场景、视距和操作变化。数据与限制见 workspace `local/host-interface-20261008/REPORT.zh-CN.md`，不能当作与 C 版等价画面的语言性能比较。

2026-10-09 等画质对照（296×240、固定相机和 24 格视空间远面、完整透视纹理、相同材质）：
S1 的 LCD 完成 FPS 为旧 C++ 10.439 → 本版 13.901，C 为 12.587；
S2 为旧 C++ 8.914 → 本版 12.107，C 为 12.526。各三次独立采样，
性能提高约 33%/36%，S2 比 C 低约 3.3%。生产 Guest 线性内存 13→9 页，
World 减少 294912 B，既有帧/深度/命令缓冲不增加。完整计量、真实峰值、
截图、诊断的观察者影响及限制见 workspace `docs/performance/pxa-3d-20261009.zh-CN.md`；
原始报告和可复现包在 `local/pxa-3d-goal-20261009/`。
