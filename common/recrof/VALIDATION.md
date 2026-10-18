# 验收记录（2026-10-18）

验收分为早期桌面移植和后续设备验证，以下桌面结果不代表最新优化版本已全部通过设备验收。

GameKit / ESP32-S31 的 `duke3d-sd` 已自动读取固定 SD 资源并进入 Hollywood Holocaust 屋顶关卡，验证转向和射击，全程未选择目录，最大线性内存为 4 MiB。0.1.6 在加载超时中选择等待后，独立 `PERF FPS` 的 15 个屋顶样本为 10.8–11.9 FPS，中位数 11.9；启动稳定性仍未通过。早先 INFO 采样带入 LittleFS 容量扫描干扰，不能据此量化优化收益。包及私有数据目前仍在内部，不能称为整应用 SD 安装。工作区的详细报告位于 `docs/validation/2026-10-18-duke3d-sd.zh-CN.md`。其余游戏、设备音频连续性、存档回读及长期运行仍待验收。

资源导入器的 6 个测试通过，覆盖大小写冲突、缺失主资源、Quake II 安装目录、RA 展开 MIX、TD DOS demo 和符号链接。

0.1.7 的 SD 变体改用按需 NVM 页文件，避免反复随机改写同一个大文件；其它变体保留 NVM2 格式，但跳过未变化的 NOR 写入。默认、分页及 48 页缓存三个 ASan/UBSan 配置通过；旧格式迁移、失败发布和损坏页均有回归。新 Node 协议宿主支持固定只读媒体根、真实目录枚举及 FS replace，首次进入和重开均观察到屋顶场景；设备 AOT 已构建，真机复测仍待设备空闲。详见 `duke3d-sd/README.md`，不能将协议宿主帧数作为设备 FPS。

`tests/fixture` 在正式 PXA 产品模拟器的 WASM 运行时通过：C 栈哨兵、长短 C++ 字符串、带虚方法的文件对象在多次异步操作后保持完整；私有文件写入和读回一致。模拟器日志包含 `FIXTURE PASS`。两个 C 栈都位于链接器声明的辅助栈范围内，兼容 WAMR 的边界检查。

Node 协议宿主已经运行五个真实引擎并进入实际关卡：Quake shareware、Quake II 3.14 demo、Duke3D 1.3D shareware、Tiberian Dawn DOS demo、Red Alert 95 demo。后台暂停/恢复检查已在 Quake II、Duke3D 上通过。该宿主仅为协议与游戏逻辑测试，不代替正式产品模拟器。

正式产品模拟器已经通过 Quake、Quake II 的 AOT 启动、资源读取和画面检查；Quake 测试使用实际触摸 MENU/FIRE 进入新游戏。Duke3D 的签名 AOT 包也通过启动和触摸输入检查；C&C 的签名 AOT 包已通过触摸开始游戏、选择难度、进入任务和 RMB 操作检查。红警也已在正式产品模拟器中进入试玩任务，并检查触摸与 RMB。五个引擎的原生 AOT 任务画面全部通过。ESP32-S3 构建产物以最终安装包目录中的清单为准，未在 ESP32 上执行。

五个最终签名包均通过正常 HOME 退出检查，模拟器退出码为 0，清理后的日志无 Guest exception。记录位于 `local/recrof-test/{quake,duke3d,command-conquer}-shutdown-home`、`quake2-shutdown-home-final` 和 `ra-shutdown-home`。验收脚本在正常退出后才写入通过结果。

早期桌面测试发现模拟器的强制 SIGTERM 路径会先删除 LVGL 触摸设备，再由 `pxsys_pxadb_control_stop` 重复删除调试接口创建的设备，导致宿主异常退出。GDB 栈记录在 `local/recrof-test/ra-stop-gdb/runtime.log`。该阶段未修改 SDK 的退出实现，验收使用 PXA 的正常 HOME 退出流程；后续设备阶段另有 FS 和 WAMR pinned memory 的 SDK 改动。

完整注册版、资料片、多人网络、设备音频和存档回读尚未验收。C&C/RA 的 DOS 与 Windows 素材不能随意混用；OpenRA 最小联机资源集不作为单人任务的验收资源。

测试资源及原始归档的校验和见 `TEST_DATA.md`。截图和宿主日志保存在工作区 `local/recrof-test`，最终包保存在 `local/recrof-build/final`。
