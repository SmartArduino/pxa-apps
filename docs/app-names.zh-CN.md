# 应用名称

正式应用以功能命名，显示名称、源码目录和包 ID 不附带实现语言。全部继续使用 C++26 和当前 SDK。

| 源码目录 | 显示名称 | 包 ID | 之前的包 ID |
| --- | --- | --- | --- |
| jump-jump-3d | Jump Jump 3D | pxa-jump-jump-3d | pxa-jump-jump-3d-cpp |
| pixel-dungeon | Pixel Dungeon | pxa-pixel-dungeon | pxa-pixel-dungeon-cpp |
| voxel-craft | Voxel Craft | pxa-voxel-craft | pxa-voxel-craft-cpp |

构建、安装示例：

```sh
bash tools/app.sh build jump-jump-3d --target simulator,esp32s3
bash tools/app.sh build pixel-dungeon --target simulator,esp32s3
bash tools/app.sh build voxel-craft --target simulator,esp32s3
python3 tools/pxadb/pxadb.py package install local/app-output/pai-touch/pxa-jump-jump-3d.pxa --port /dev/ttyACM0
```

包 ID 是私有存储、权限和安装记录的身份。新 ID 不会自动覆盖旧的 `*-cpp` 安装，也不会自动读取其存档；这次源码改名没有删除设备上的旧安装及数据。已有用户升级时，应先备份并显式迁移私有文件，再确认新包能加载存档后处理旧安装。不能直接清除旧应用数据，也不能仅重命名已签名的包目录或修改其 manifest。

`pxa-jump-jump-3d` 等 ID 曾被已停用的 C 实现使用。设备若仍安装该旧包，应核对 SDK/ABI 和存档格式，使用备份后的正常安装流程替换。

原生测试中的 C 对照实现仍保存在 `common/cpp/tests/reference/c`，临时对照包使用 `pxa-jump-jump-3d-reference` 和 `pxa-pixel-dungeon-reference`，避免与正式游戏冲突。历史报告、截图和原始日志保留测试当时的身份；通用 SDK 的 `guest-cpp` 目录和 SDK 示例名称不属于这个应用命名调整。

2026-10-12 验证：三个正式包均成功生成 ESP32-S3、x86_64 AOT 和 Wasm，名称与 ID 检查覆盖仓库全部 package.json。Jump/Pixel 原生测试 11/11、Voxel 输入/存档/背包/声音/布局/世界/渲染及 C 对照测试、开发工具测试 13/13 均通过。两个临时 C 对照准备工具使用新路径成功执行。

实际加载签名包的模拟器覆盖 296×240/160 DPI、480×480/305 DPI 和 176×176 圆屏：Jump/Pixel 六项交互通过，Pixel 保存后重启加载通过；Voxel 三种屏幕标题及普通屏进入游戏通过。截图已检查，无语言后缀。[截图](app-names-20261012/screens.png)和[验证记录及包哈希](app-names-20261012/validation.json)保留在本仓库。本次改名不改变游戏循环、渲染、资源内容或存档格式；没有宣称其本身带来性能收益。
