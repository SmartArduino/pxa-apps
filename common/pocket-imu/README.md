# Pocket IMU transport

`service.hpp` 是回调应用可复用的加速度计/陀螺仪传输状态机，不负责测量零点或游戏控制。`Service<Device>` 用实际SDK消息发现三轴、正确单位的传感器，先完成可选权限，再开始约50毫秒订阅；陀螺仪不可用时降级到加速度计。请求提交、关闭背压、取消、迟到资源回包和权限撤销均有界处理。暂停、后台与退出必须传入 `foreground=false`，恢复再连接。

Device约定：`status`枚举提供Unavailable/Connecting/Permission/Streaming/Denied/Error，另提供 `now`、`retry`、`wanted`、`gyro`。`tilt`提供`suspend()`、`acceleration(values,timestamp,arrival)`、`gyroscope(values,timestamp)`、`gyro_time`和`calibrated`初始化状态。后一字段只用于一次就绪日志；算法可以保留绝对重力，也可以为游戏显式校准，传输层不定义测量基准。

通过 Pocket Play 的 `PXA_POCKET_SERVICES` 钩子接入。原生资源/权限测试位于 `marble-islands/tests/motion-service.cpp`；水平仪的测量算法和UI测试独立位于 `spirit-level/tests`。日志、测试二进制与模拟器产物只写入`local/artifacts/`。
