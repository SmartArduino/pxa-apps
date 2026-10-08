# Voxel Craft C++

使用独立 PXA C++26 SDK 的体素沙盒。纹理和调色板在初始化时逐个加载、绑定并释放临时句柄，Renderer 持有绑定引用；稳定帧仅提交复用的 DrawList。

触摸左侧控制移动，右侧拖动转向；侧栏控制挖掘、放置、飞行和上下移动。热栏选择方块，多指按 pointer_id 独立管理；后台取消持续动作。

渲染从眼睛位置投影，保持 Q4 坐标和 Q8 深度。近面与视锥裁剪保留 UV；四边形和裁剪三角形共享深度，叶片透明 texel 不写遮挡深度。所有 45 个方块材质与 HUD 使用正确资源；绿色仙人掌没有树冠。

地图先完成地形再生成植被，草丛不能替换树干底部。每区块固定 384 个 quad；复杂编辑超容量时分页复用同一缓存，避免漏面。视距根据实际帧间隔与 Host 光栅化成本调整，低质量仍保留深度。

构建：

```sh
bash tools/app.sh build voxel-craft-cpp --target all
bash local/pxa-apps/voxel-craft-cpp/tests/test.sh
```

主机测试检查多指输入、后台复位、32 个地图种子的树干/树冠、所有区块的可见面、超容量分页与编辑重建。`PXA_APP_DEFINES=VOXEL_PROFILE=1` 可启用 Guest 绘图计时；默认不增加帧内 WASI 计时调用。

2026-10-08 pai-touch `/dev/ttyACM2` 三轮显示帧率约 7.42 FPS，Host 光栅化约 59.5 ms；会随场景、视距和操作变化。数据与限制见 workspace `local/host-interface-20261008/REPORT.zh-CN.md`，不能当作与 C 版等价画面的语言性能比较。
