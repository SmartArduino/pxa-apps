#include "pxt.h"
#include <pthread.h>

// MCA v0.2.14：这把"中断屏蔽"锁必须是**递归锁**，而且必须显式初始化。
//
// 它是 melody / sound 里保护 waiting 队列的锁，两端（VM 线程的
// queuePlayInstructions / stopPlaying、前端线程的 pxt_get_audio_samples）都抢。
// 关键坑：持锁的代码里**也会触发 panic**（xmalloc 失败、GC oops…），而 VM 版的
// target_panic → systemReset() → resetRuntimeState() → stopPlaying() 又要
// target_disable_irq() —— 同一个线程再锁一次非递归锁 = 自己等自己，永久挂死。
// 真机日志的表现就是清理序列停在
//     [vm] cleanup 1/6 gcFreeze
//     [vm] cleanup 2/6 reset callbacks        ← 再没有下文
// （2/6 跑的就是 melody 注册的 stopPlaying）。
//
// 原来那句 `static pthread_mutex_t irqMutex;` 还是**未初始化**的互斥量：零值在
// glibc/bionic 上碰巧等于 PTHREAD_MUTEX_INITIALIZER，但不是所有实现都这样。
// 这里改成 pthread_once + PTHREAD_MUTEX_RECURSIVE，两件事一起解决。
namespace pxt {

#ifdef PTHREAD_MUTEX_RECURSIVE
#define MCA_IRQ_MUTEX_TYPE PTHREAD_MUTEX_RECURSIVE
#else
#define MCA_IRQ_MUTEX_TYPE PTHREAD_MUTEX_RECURSIVE_NP
#endif

static pthread_mutex_t irqMutex;
static pthread_once_t irqOnce = PTHREAD_ONCE_INIT;

static void irqMutexInit() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, MCA_IRQ_MUTEX_TYPE);
    pthread_mutex_init(&irqMutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

void target_disable_irq() {
    pthread_once(&irqOnce, irqMutexInit);
    pthread_mutex_lock(&irqMutex);
}
void target_enable_irq() {
    pthread_mutex_unlock(&irqMutex);
}

} // namespace pxt
