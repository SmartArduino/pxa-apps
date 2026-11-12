#ifndef MCA_TICK_H
#define MCA_TICK_H

// MCA v0.2.10：帧节拍原语（跨平台）
//
// 帧驱动虚拟时钟下，VM 线程在「没有可运行 fiber」时要等两件事之一：
//   1) 前端取走一帧（pxt_vm_frame_done → mca_tick_signal）→ 虚拟时间前进
//   2) 短超时兜底（前端暂停/最小化/被调试器停住）
//
// 为什么不用 pthread_cond_timedwait 一把梭：
//   zig 的 mingw 目标链接 winpthreads，在「broadcast 与进入等待竞争」的
//   窗口里实测会**丢唤醒且超时不生效** —— VM 线程永久挂在 target_yield，
//   表现为存档/快进静默失效（mca_trace 里 114 次进入等待、0 次返回）。
//   Linux/Android 的 cond 实现没有这个问题（真机已验证），所以分开实现：
//   Windows 用自动重置事件的 SetEvent/WaitForSingleObject（信号即唤醒、
//   不丢、超时语义由内核保证），POSIX 用 cond + pending 标志（signal 在
//   wait 之前到达时不丢）。
//
// 这两个函数都是线程安全的，signal 从任意线程调用。

#ifdef __cplusplus
extern "C" {
#endif

// 幂等；在 VM 启动前调用一次。未初始化时 signal/wait 退化为安全空转。
void mca_tick_init(void);
void mca_tick_shutdown(void);

// 前端线程：一帧被取走（虚拟时间前进了）
void mca_tick_signal(void);

// VM 线程：等下一帧，最多 ms 毫秒
void mca_tick_wait(int ms);

// 画面发布事件：前端线程等"第 N 帧已发布"，VM 线程发布完通知。
//
// 与节拍事件**必须分开**：两者唤醒的对象不同（节拍唤醒 VM，发布唤醒前端），
// 共用一个事件会让前端消费掉本该给 VM 的那张票，VM 于是漏掉一帧。
//
// 同样不能用 pthread_cond_timedwait：zig-mingw 的 winpthreads 对它直接返回
// EINVAL(22) —— 实测 300 帧里 288 次立即"超时"返回，前端读到的其实是上一帧
// 的内容，同一输入的两次运行就从随机帧开始分叉（画面差几个像素）。
void mca_pub_init(void);
void mca_pub_shutdown(void);
void mca_pub_signal(void);  // VM 线程：一帧已发布
int mca_pub_wait(int ms);   // 前端线程：1=收到通知，0=超时

#ifdef __cplusplus
}
#endif

#endif // MCA_TICK_H
