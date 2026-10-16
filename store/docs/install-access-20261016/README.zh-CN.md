# Mosaico 商城安装服务开放与确认（2026-10-16）

## 已确认的原因

ESP-Mosaico（480×480，305 DPI，`/dev/ttyACM2`）可以获取应用详情：HTTP 200、
2621 B，也能获取图标与截图。安装按钮刷新票据后，尚未启动下载 worker 就失败。
旧 Host 的 `pxa_esp_store_installer_authorized()` 只接受 `built_in=1` 且 App ID
为 `pxa-store` 的应用；设备商城实际为普通安装的 `builtin=0`。

旧商城的提交辅助函数只返回布尔值，丢失 Host 的真实状态；失败分支固定显示
UNAVAILABLE，因此出现“安装失败，请重试: 无法连接商城”。0.8.7 在同一旧 Host 上
实测记录 `store: install request rejected status=-4`，显示“系统拒绝了安装服务请求”。
这确认了本次错误是安装服务拒绝，不是图片流轮询日志中的 HTTP 超时警告。

## 本轮修改

- 按用户要求，服务 20 向当前运行的已安装、已启用签名应用开放。不依赖预置位置、
  App ID 或发布者。签名认证、配置的 HTTPS 源、票据检查和下载记录的所有权保持由 Host 实施。
- 每次首次安装、更新和离线包安装都需要新的 Host 原生确认。没有静默安装参数，
  不保留上次批准；worker 在请求弹窗前清除批准状态。单独下载只保存经过验证的容器。
- 未确认、拒绝、弹窗无法显示、取消或调用方退出均不会启动安装。已经确认并进入的
  原子安装事务继续安全完成，避免留下损坏版本。
- SDK 增加 `pxa_store_request_package_status()`，保留同步错误；原布尔接口仍可用。
  商城 0.8.7 区分拒绝、忙碌、内存不足、不支持和空间不足，并记录被拒绝的状态码。

## 验证边界

- ESP Host 原生 ASan/UBSan 完整回归通过。访问策略覆盖普通安装、独立名称和第三方
  发布者、身份不匹配、禁用及仅暂存未安装等情况。
- 真实下载 worker 的原生测试检查：拒绝确认、旧批准不能复用、等待确认后取消、
  重复安装每次重新确认、单独下载不安装、离线包拒绝与失败、残包回收。
- Guest SDK 全部 wire 回归通过；安装错误传播分别通过 C 和 C++26 ASan/UBSan 检查。
- 全部 C++ 应用原生 ASan/UBSan 回归通过，包含商城票据、UTF-8、图片生命周期及 DPI 布局。
- 模拟器包管理、系统手势、声明式 UI、两组 reference features 共 5/5 通过。
  桌面模拟器没有 ESP 商城安装服务，不能用这些结果冒充真机下载安装成功。
- ESP32-S31 和模拟器 AOT 商城包构建通过，0.8.7 已安装并重启商城运行验证了真实
  `DENIED=-4` 及改正后的提示。旧 Host 仍拒绝安装，这是预期的前置对照。
- 新 Host 固件构建通过，4,096,640 B，适配现有 4 MiB 应用分区。新固件已烧入本设备；
  普通安装的商城 0.8.8 已能下载真实包并弹出系统原生确认，取消后包清单确认未安装。
  此次没有自动批准安装，用户确认后的商城安装成功路径仍未由本轮真机测试覆盖。

本轮不扩大下载缓冲、worker 栈、队列或 Guest 线性内存上限。新固件相较设备上的
图标修复候选，SRAM data/bss 仍为 39,196/31,320 B，外部 BSS 仍为 36,016 B。
Flash text +1,600 B、rodata −16 B；构建同时包含工作区已有的图片/平台修改，不能把
整个二进制差异归因到安装策略。尚未测量新固件的运行时内存峰值或性能收益。

## 复测与更新

```sh
PXA_TEST_SANITIZERS=1 bash firmware/components/pxa/tests/test_host.sh
bash deps/pxa-system/tools/package/test_guest_sdk.sh
SANITIZE=1 bash local/pxa-apps/tests/native/run.sh
tools/app.sh build store --target simulator,esp32s31 \
  --source-root local/pxa-apps --output local/store-install-check
tools/firmware.sh esp-mosaico build
```

冻结固件及 SHA-256、测试日志、设备原始日志和只写应用分区的烧录脚本位于产品仓库
忽略目录 `local/store-install-mosaico-20261016/`。本轮固件 SHA-256：
`c2ef585a2f0447577dfa914bb69f64a389bcfac054475a643eb6c57f1cd50490`。
复测设备已更新本轮 Host。其他设备也需更新 Host；单独更新商城或重烧此前的图标
固件不能解除旧限制。真机截图见 [商城预览报告](../screenshot-viewer-20261016/README.zh-CN.md)。

先启动该目录的 `flash-firmware.py`，再 BOOT+RESET 进入下载模式；等待完整写入和
摘要校验成功，最后只按 RESET 启动。该脚本不会写分区表、文件系统或用户数据。
