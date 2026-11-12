#include "pxt.h"
#include "mca_tick.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <stdarg.h>
#include <setjmp.h> // v0.3.2：panic 回弹点（mcaPanicUnwind）

#ifndef PXT_ESP32
// __MINGW32__ is defined on both mingw32 and mingw64
#ifdef __MINGW32__
#include <windows.h>
#elif !defined(PXT64)
// 只有 32 位目标才用 mmap 分配 GC 堆（见 gcAllocBlock）；64 位走 malloc，
// 而裸机工具链（devkitA64/newlib，Switch）没有 sys/mman.h——无条件包含会
// 让整个文件编不过。
#include <sys/mman.h>
#endif
#endif

#define HANDLER_RUNNING 0x0001

// should this be something like CXX11 or whatever?
#define THROW throw()
#define THREAD_DBG(...)

static uint8_t in_xmalloc_panic;

void *xmalloc(size_t sz) {
    // calloc 语义（清零）：malloc 的旧内容会让"读到未初始化字段"的结果随
    // 进程堆历史变化，同一输入的两次运行行为可能不同。分配量都在 fiber /
    // 事件 / 栈副本这个量级，清零开销可忽略。
    auto r = calloc(1, sz);
    if (r == NULL) {
        DMESG("failed to allocate %d bytes", sz);
        if (in_xmalloc_panic) {
            target_panic(PANIC_GC_OOM);
        } else {
            in_xmalloc_panic = 1;
            soft_panic(PANIC_GC_OOM); // this can happen on esp32 etc; shouldn't on linux/ios etc
        }
    }
    return r;
}

void *operator new(size_t size) {
    return xmalloc(size);
}
void *operator new[](size_t size) {
    return xmalloc(size);
}

void operator delete(void *p)THROW {
    xfree(p);
}
void operator delete[](void *p) THROW {
    xfree(p);
}

uint8_t *gcBase;

namespace pxt {

// MCA PATCH 5（本项目）：宿主请求重启（libretro 的 reset 键）。
//
// 官方宿主是"单游戏单进程"，VM 没有 stop 接口；但 systemReset() 本身就是
// 一套完整的重启收尾（清 fiber / handler 绑定 / GC，然后 pthread_exit）。
// 它只能在 VM 线程里跑，所以宿主线程只置标志，由 mainRunLoop 自行退出去
// 交给 initRuntime 末尾的 systemReset()。
//   mcaResetRequest: 宿主线程写、VM 线程读（单字节标志，volatile 足够）
//   mcaVmExited    : VM 线程**真正退出前**（multiStart 末尾）写，宿主读后清零。
//                    不能在清理中途就置位 —— 那时线程还在跑，宿主若立刻
//                    pthread_join 只能等，真机上表现为"重置后整个前端卡死"。
//   mcaRestartDone : 清理已做完，initRuntime 跳过 systemReset()
volatile int mcaResetRequest = 0;
volatile int mcaVmExited = 0;
// MCA v0.2.10：主循环是否已经在"等前端要帧"的闸门上。
// 宿主用它判断"VM 是否已完成 initRuntime 并且随时能出一帧" ——
// 装载同步化（vmload.cpp 的 spinThread）等到它才返回，这样前端第 1 帧
// 就能立刻拿到第 1 圈的画面，帧号与圈号从第一帧起严格对齐。
volatile int mcaMainLoopLive = 0;
static volatile int mcaRestartDone = 0;

// MCA v0.3.2（本项目）：panic 回弹点 —— panic 之后**不能 return**。
//
// 上游 pxt 的约定是 target_panic()/soft_panic() 不返回（ESP32 上就是
// abort()），VM 里的 C++ 代码全部按这个约定写，例如：
//     if (!isPointer(obj)) failedCast(obj);      // 内部 soft_panic()
//     auto vt = getVTable((RefObject *)obj);     // obj 是 undefined(0)，直接解引用
// v0.2.14 为了"panic 后干净重启"把 panic 改成"置标志就返回、由主循环收尾"，
// 于是这些调用方会带着非法值继续往下跑：实测 Strawberry Slam 在
// asRefCollection() 上踩 PANIC 980（把 undefined 当数组），返回后数组操作码
// 拿 NULL 解引用，整个进程 0xC0000005 挂掉 —— RetroArch 跟着一起死。
//
// 所以 panic 之后必须离开当前栈。用 setjmp/longjmp 弹回 mainRunLoop 的帧，
// 那里本来就有 `if (panicCode)` 分支做 cleanup + 正常线程退出
// （mcaVmExited 置位、前端据此重启）。跨过的 C++ 帧不含需要析构的对象
// （运行库按 -fno-exceptions 编译），VM 内部本来也用同一套机制
// （restoreVMExceptionState）。
// 注意：mingw-w64 的 longjmp 在 x86_64 上走 SEH（RtlUnwindEx），需要
// .pdata 才能逐帧回退 —— Windows 目标因此打开了 -funwind-tables
//（见 tools/build_target.sh；不开的话 longjmp 自己就 0xC0000005 崩，
// 连带 VM 的 try/catch 也是 STATUS_BAD_STACK）。
static jmp_buf mcaPanicJmp;
static volatile int mcaPanicJmpArmed;

// 弹回主循环。返回 0 = 当前没有可用的回弹点（不在主循环里，例如 initRuntime
// 阶段），调用方沿用旧逻辑。
static int mcaPanicUnwind() {
    if (!mcaPanicJmpArmed)
        return 0;
    mcaPanicJmpArmed = 0; // 只能用一次
    longjmp(mcaPanicJmp, 1);
    return 0; // 不会到这里
}

// 离开主循环前必须撤掉回弹点：longjmp 到一个已经返回的栈帧是未定义行为。
static void mcaLeaveMainLoop() {
    mcaPanicJmpArmed = 0;
    mcaMainLoopLive = 0;
}

// MCA PATCH 6（本项目）：重启路径埋点。
//
// 这套重启是"VM 线程自己清理干净再退出、宿主再起新线程"，涉及两次线程切换，
// 真机上卡住时前端只会看到一张静止的画面，日志也没有 —— 所以每一步都写一行
// trace（默认落到存档目录的 mca_trace.log，失败退回 stderr），卡在哪一步一眼可见。
// 只写十几行、每次 flush，对性能没有影响。
static FILE *mcaTraceFile;
static int mcaTraceTried;
static char mcaTracePathBuf[512] = "";

// 依次尝试几个位置：存档目录 → 当前目录 → /tmp（掌机上一定可写）。
// 同时**每个进程启动都截断重建**，避免把上次的旧内容当成这次的结果。
static void mcaTraceOpen() {
    if (mcaTraceTried) return;
    mcaTraceTried = 1;

    const char *dir = pxt_vm_get_data_directory();
    if (dir && *dir) {
        snprintf(mcaTracePathBuf, sizeof(mcaTracePathBuf), "%s/mca_trace.log", dir);
        mcaTraceFile = fopen(mcaTracePathBuf, "w");
    }
    if (!mcaTraceFile) {
        snprintf(mcaTracePathBuf, sizeof(mcaTracePathBuf), "mca_trace.log");
        mcaTraceFile = fopen(mcaTracePathBuf, "w");
    }
#if !defined(_WIN32)
    if (!mcaTraceFile) {
        snprintf(mcaTracePathBuf, sizeof(mcaTracePathBuf), "/tmp/mca_trace.log");
        mcaTraceFile = fopen(mcaTracePathBuf, "w");
    }
#endif
    if (!mcaTraceFile) {
        snprintf(mcaTracePathBuf, sizeof(mcaTracePathBuf), "(no file, stderr only)");
        mcaTraceFile = stderr;
        return;
    }
    fprintf(mcaTraceFile, "# mca trace -> %s\n", mcaTracePathBuf);
    fflush(mcaTraceFile);
    dmesg("mca trace: %s", mcaTracePathBuf);
}

// trace 只写文件（互斥），**不写 stderr**。
//
// 早先版本同时写 stderr 想让日志进 RetroArch 自己的输出。问题是高频 trace
// （例如排查时每圈主循环一行）会把 stderr 管道/终端写满：管道没人读、或
// Windows 控制台逐字符输出，都会让**调用线程阻塞在 write 上** —— VM 线程
// 卡在 trace 里，表现出来像是"暂停请求永远读不到"。日志路径已经在启动时
// 用 dmesg() 报过一次，用户照着找文件即可。
static pthread_mutex_t mcaTraceMutex = PTHREAD_MUTEX_INITIALIZER;

void mcaTrace(const char *fmt, ...) {
    va_list ap;
    // 开文件必须在锁内：mcaTraceTried 不是原子的，VM 线程和前端线程同时第一次
    // 写 trace 时两边都会 fopen(...,"w")，后开的把先写的内容截掉（实测桌面
    // 上 "[vm] ..." 那几行经常整个消失）。开文件只发生一次，开销无所谓。
    pthread_mutex_lock(&mcaTraceMutex);
    mcaTraceOpen();
    if (mcaTraceFile) {
        va_start(ap, fmt);
        vfprintf(mcaTraceFile, fmt, ap);
        fputc('\n', mcaTraceFile);
        fflush(mcaTraceFile);
        va_end(ap);
    } else {
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        fputc('\n', stderr);
        fflush(stderr);
        va_end(ap);
    }
    pthread_mutex_unlock(&mcaTraceMutex);
}

DLLEXPORT void pxt_vm_trace(const char *msg) { mcaTrace("[host] %s", msg); }

DLLEXPORT const char *pxt_vm_trace_path() {
    mcaTraceOpen();
    return mcaTracePathBuf;
}

// MCA PATCH 7：「VM 在线」标志。
//
// 前端线程每帧都会来拉画面/音频。重启间隙里 VM 的堆、单例、混音器都被释放了，
// 此时拉取必须变成"黑屏 + 静音"的纯本地操作，绝不能碰 VM 状态。
// 这个标志由 VM 线程自己维护：清理第一步置 0，新镜像装载成功后置 1。
static volatile int pxtVmLive = 1;
DLLEXPORT int pxt_vm_online() { return pxtVmLive; }
DLLEXPORT void pxt_vm_set_online(int online) { pxtVmLive = online ? 1 : 0; }

FiberContext *allFibers;
FiberContext *currentFiber;
static pthread_mutex_t eventMutex;
static pthread_cond_t newEventBroadcast;

static struct Event *eventHead, *eventTail;

struct Event {
    struct Event *next;
    int source;
    int value;
};

Event lastEvent;

Event *mkEvent(int source, int value) {
    auto res = new Event();
    memset(res, 0, sizeof(Event));
    res->source = source;
    res->value = value;
    return res;
}

volatile int panicCode;
extern "C" void drawPanic(int code);

void schedule() {
    auto f = currentFiber;
    if (!f->wakeTime && !f->waitSource)
        oops(55);
    f->resumePC = f->pc;
    f->pc = NULL; // this will break the exec_loop()
}

void dmesg_flush();

static void panic_core(int error_code) {
    int prevErr = errno;

    panicCode = error_code;

    drawPanic(error_code);

    // MCA v0.2.14：panic 必须进 trace 并带上线程身份。
    // 此前 panic 只走 dmesg（进前端日志），mca_trace.log 里是空白 —— 于是
    // "加载存档后卡死、日志停在 cleanup 2/6" 这类问题无法判断到底是哪条
    // 线程、哪个 panic 码。DMESG 那两行保持原样（前端日志里还要看）。
    mcaTrace("[vm] PANIC %d on %s thread (errno=%d %s)", error_code,
             pxt_vm_is_vm_thread() ? "vm" : "frontend", prevErr, strerror(prevErr));

    DMESG("PANIC %d", error_code % 1000);
    DMESG("errno=%d %s", prevErr, strerror(prevErr));

    dmesg_flush();
}

extern "C" void target_panic(int error_code) {
    panic_core(error_code);

#if defined(PXT_ESP32)
    // sleep_core_us(5 * 1000 * 1000);
    abort();
#elif defined(PXT_VM)
    // MCA v0.2.14：panic 之后走哪条路，取决于"当前在谁身上、跑到哪一步"。
    //
    // (1) 前端线程（retro_run / 拉音频）：**绝不能跑清理**。
    //     systemReset() 末端是 pthread_exit()，在前端线程上等于把 RetroArch 的
    //     render 线程注销掉（UI 永久卡住）；而且此刻它可能正握着 irqMutex /
    //     screenMutex，清理再进去抢就是自死锁 —— 真机日志精确停在
    //     "[vm] cleanup 2/6 reset callbacks"，那里调的 stopPlaying() 第一件事
    //     就是 target_disable_irq()。
    //     做法：只登记 panic 码，重启交给 VM 线程。
    //
    // (2) VM 线程、主循环已在跑：**也不当场 systemReset**。
    //     panic 基本都发生在 fiber 里，从那儿 pthread_exit 有两个坑：
    //       * Mingw 上直接崩（0xC0000005，PATCH 5 踩过这个坑）；
    //       * 它绕过 multiStart() 的收尾，mcaVmExited 永远不置位 → 前端以为
    //         VM 还活着、永不重启 → 真机表现"画面冻在最后一帧，进程不崩不退"。
    //     做法：panicCode 已经置位，exec_loop 里的 `if (panicCode) break;`
    //     会立刻结束当前 fiber，控制权回到主循环顶；那里看到 panicCode 就走
    //     hostRestartCleanup() + return，线程从 multiStart 正常收尾退出。
    //
    // (3) VM 线程、主循环还没起来（initRuntime 阶段就失败）：没有更干净的
    //     返回点，只能当场清理退出。
    if (!pxt_vm_is_vm_thread()) {
        mcaTrace("[vm] panic on frontend thread -> defer reset to vm thread");
        if (pxt_vm_running())
            pxt_vm_request_reset();
        // 不 pthread_exit、不 while(1)：调用方拿到控制权后自行返回。
    } else if (mcaMainLoopLive) {
        mcaTrace("[vm] panic in vm thread -> unwind to main loop (no pthread_exit)");
        // v0.3.2：**必须离开当前栈**，不能 return —— 调用方会拿非法值继续跑
        // （见 mcaPanicUnwind 上面的说明）。
        if (!mcaPanicUnwind())
            systemReset();
    } else {
        systemReset();
    }
#else
    while (1)
        sleep_core_us(10000);
#endif
}

DLLEXPORT int pxt_get_panic_code() {
    return panicCode;
}

void ets_log_dmesg();
void soft_panic(int errorCode) {
    if (errorCode >= 999)
        errorCode = 999;
    if (errorCode <= 0)
        errorCode = 1;
    vm_stack_trace();
    panic_core(1000 + errorCode);
#if defined(PXT_ESP32)
    ets_log_dmesg();
    sleep_core_us(4000000);
    abort();
#else
    // 与 target_panic 同一条规矩（见那里 (1)(2)(3) 的说明）。
    if (!pxt_vm_is_vm_thread()) {
        mcaTrace("[vm] soft_panic on frontend thread -> defer reset to vm thread");
        if (pxt_vm_running())
            pxt_vm_request_reset();
    } else if (mcaMainLoopLive) {
        mcaTrace("[vm] soft_panic in vm thread -> unwind to main loop (no pthread_exit)");
        // 同 target_panic（1)(2)(3)；v0.3.2 起此处不再返回给调用方。
        if (!mcaPanicUnwind())
            systemReset();
    } else {
        systemReset();
    }
#endif
}

void sleep_core_us(uint64_t us) {
#ifdef PXT_ESP32
    uint64_t endp = esp_timer_get_time() + us;
    while (esp_timer_get_time() < endp)
        ;
#else
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    while (nanosleep(&ts, &ts))
        ;
#endif
}

// MCA v0.2.10：target_yield 等「下一帧被前端取走」的信号，而不是睡固定
// 时长（Windows 上 nanosleep 粒度实际 6-15ms，墙钟外推时钟下会把游戏压到
// 6 倍慢）。帧驱动时钟下虚拟时间只在前端取帧时前进，这里只要在两件事之
// 一时醒来：帧到了 / 兜底超时到了。具体等待由 mca_tick.cpp 按平台实现
// （Windows 用 WinAPI 事件 —— winpthreads 的 cond 在竞争下会丢唤醒且超时
// 不生效，实测把 VM 线程永久挂在 target_yield）。
void target_yield() {
#ifdef PXT_ESP32
    vTaskDelay(1);
#else
    mca_tick_wait(2);
#endif
}

void sleep_ms(uint32_t ms) {
    currentFiber->wakeTime = current_time_ms() + ms;
    schedule();
}

void sleep_us(uint64_t us) {
    if (us > 20000) {
        sleep_ms((uint32_t)(us / 1000));
    } else {
        sleep_core_us(us);
    }
}

#ifndef PXT_ESP32
// MCA PATCH 10：虚拟时钟（快进 / 慢放 / 存档确定性的基础）。
//
// 游戏的节拍全部挂在 current_time_ms() 上：forever 每 20ms 一个 tick、
// pause(ms) 等 wakeTime —— 都是"现在几点"。墙钟不能用作游戏时间：
// 前端以高于 60fps 的频率调 retro_run 时，墙钟不变，游戏不会加速；
// 而且同一帧号对应的墙钟时刻每次都不同，同一输入的两次运行画面无从对齐。
//
// v0.2.10 起的模型是**前端请求驱动、一圈一帧**：
//   · 前端每请求一帧 → g_frameRequests++（并发 mca_tick_signal 唤醒 VM）
//   · VM 主循环每圈先等一张许可，然后 g_virtTimeNum += 一帧、发布一帧
// 于是 VM 的圈数严格等于前端帧数，同一帧号恒定对应同一虚拟时刻，
// 同一输入序列的运行行为完全可复现。
//
// 快进 / 慢放不需要倍率：前端多取帧游戏就快（虚拟时间按帧推进），
// 前端少取帧游戏就慢，这是 libretro 的固有语义（RetroArch 的 fast-forward
// 就是提高 retro_run 频率）。缩放时间步长的做法不可取 —— 会让一层循环里
// 跳过多个 20ms 周期，forever 丢 tick。
static volatile uint64_t g_frameRequests = 0; // 前端线程写：请求了几帧
static uint64_t g_loops = 0;                  // VM 线程写：已推进了几圈
static uint64_t g_virtTimeNum = 0;            // VM 线程本地推进，单位 1/3 µs
// 已发布的帧序号（= 发出去的第几帧）。前端按序号配对，不靠 bool 标志。
// screen.cpp 的发布函数在同一个锁内写它，保证"序号可见 ⇒ 缓冲已就绪"。
volatile uint64_t mcaPubSeq = 0;

// 前端每请求一帧调一次（screen.cpp 的 get_pixels 开头），返回本次请求的序号。
// 前端必须等到 pxt_vm_pub_seq() >= 该序号才取画面 —— 这样"请求 N 拿到第 N 帧"，
// 不会因为 bool 标志被后续帧覆盖而错配（实测会积压 10+ 帧，末帧取决于
// unload 时 VM 恰好跑到哪里，同一输入两次运行结果不同）。
DLLEXPORT uint64_t pxt_vm_frame_request() {
    g_frameRequests++;
    mca_tick_signal();
    return g_frameRequests;
}

DLLEXPORT uint64_t pxt_vm_pub_seq() {
    return mcaPubSeq;
}

// VM 线程：还有没有未处理的帧请求
static int mca_frame_requested() {
    return g_frameRequests > g_loops;
}

// 一帧的虚拟时间：1/60 秒 = 16666.7 µs，单位是 1/3 µs → 50000
#define MCA_FRAME_STEP 50000u

uint64_t current_time_us() {
    return g_virtTimeNum / 3;
}
#endif

int current_time_ms() {
    return (int)(current_time_us() / 1000);
}

void disposeFiber(FiberContext *t) {
    if (allFibers == t) {
        allFibers = t->next;
    } else {
        for (auto tt = allFibers; tt; tt = tt->next) {
            if (tt->next == t) {
                tt->next = t->next;
                break;
            }
        }
    }

    // DMESG("free: %p %p", t, t->stackCopy);

    xfree(t->stackCopy);
    xfree(t);

    if (currentFiber == t)
        currentFiber = NULL;
}

#define INITIAL_STACK_COPY_SIZE 16

FiberContext *setupThread(Action a, TValue arg = 0, HandlerBinding *hb = NULL) {
#if 0
    int numThreads = 0;
    for (auto p = allFibers; p; p = p->next)
        numThreads++;
    DMESG("setup thread: %p #%d", a, numThreads);
    //if (numThreads > 10)
    //    abort();
#endif
    auto t = (FiberContext *)xmalloc(sizeof(FiberContext));
    memset(t, 0, sizeof(*t));
    if (!vmImg->stackBase) {
        vmImg->stackBase = (TValue *)xmalloc(VM_STACK_SIZE * sizeof(TValue));
        vmImg->stackTop = vmImg->stackBase + VM_STACK_SIZE;
        vmImg->stackLimit = vmImg->stackBase + VM_MAX_FUNCTION_STACK + 5;
    }
    t->stackCopy = (TValue *)xmalloc(sizeof(TValue) * INITIAL_STACK_COPY_SIZE);
    t->stackCopySize = INITIAL_STACK_COPY_SIZE;
    t->sp = vmImg->stackTop - 8;

    // DMESG("thr: %p %p", t, t->stackCopy);

    auto ptr = t->stackCopy;
    *ptr++ = TAG_STACK_BOTTOM;
    *ptr++ = 0;
    *ptr++ = arg;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = (TValue)0xf00df00df00df00d;

    t->handlerBinding = hb;
    auto ra = (RefAction *)a;
    // we only pass 1 argument, but can in fact handle up to 4
    if (ra->numArgs > 2)
        target_panic(PANIC_INVALID_IMAGE);
    t->currAction = ra;
    t->resumePC = actionPC(ra);

    t->img = vmImg;
    t->imgbase = (uint16_t *)vmImg->dataStart;

    // add at the end
    if (allFibers)
        for (auto p = allFibers; p; p = p->next) {
            if (!p->next) {
                p->next = t;
                break;
            }
        }
    else
        allFibers = t;

    return t;
}

void runInParallel(Action a) {
    setupThread(a);
}

void runForever(Action a) {
    auto f = setupThread(a);
    f->foreverPC = f->resumePC;
}

void waitForEvent(int source, int value) {
    currentFiber->waitSource = source;
    currentFiber->waitValue = value;
    schedule();
}

FiberContext *suspendFiber() {
    currentFiber->waitSource = PXT_WAIT_SOURCE_PROMISE;
    schedule();
    return currentFiber;
}

void resumeFiberWithFn(FiberContext *ctx, fiber_resume_t fn, void *arg) {
    if (ctx->waitSource != PXT_WAIT_SOURCE_PROMISE)
        oops(52);
    ctx->waitSource = 0;
    ctx->wakeFn = fn;
    ctx->wakeFnArg = arg;
}

void resumeFiber(FiberContext *ctx, TValue v) {
    if (ctx->waitSource != PXT_WAIT_SOURCE_PROMISE)
        oops(52);
    ctx->waitSource = 0;
    ctx->r0 = v;
}

static void startHandler(HandlerBinding *hb, Event &e) {
    if (!hb)
        return;
    lastEvent = e; // this is quite racy
    if (hb->flags & HANDLER_RUNNING) {
        auto tmp = mkEvent(e.source, e.value);
        if (hb->pending == NULL) {
            hb->pending = tmp;
        } else {
            int numev = 0;
            auto p = hb->pending;
            for (; p->next; p = p->next)
                numev++;
            if (numev >= 10) {
                xfree(tmp);
                return;
            }
            p->next = tmp;
        }
    } else {
        hb->flags |= HANDLER_RUNNING;
        setupThread(hb->action, fromInt(e.value), hb);
    }
}

static void dispatchEvent(Event &e) {
    startHandler(findBinding(e.source, e.value), e);
    startHandler(findBinding(e.source, DEVICE_EVT_ANY), e);
}

static void wakeFibers() {
    for (;;) {
        pthread_mutex_lock(&eventMutex);
        if (eventHead == NULL) {
            pthread_mutex_unlock(&eventMutex);
            return;
        }
        Event *ev = eventHead;
        eventHead = ev->next;
        if (eventHead == NULL)
            eventTail = NULL;
        pthread_mutex_unlock(&eventMutex);

        for (auto thr = allFibers; thr; thr = thr->next) {
            if (thr->waitSource == 0)
                continue;
            if (thr->waitValue != ev->value && thr->waitValue != DEVICE_EVT_ANY)
                continue;
            if (thr->waitSource == ev->source) {
                thr->waitSource = 0;
            } else if (thr->waitSource == DEVICE_ID_NOTIFY && ev->source == DEVICE_ID_NOTIFY_ONE) {
                thr->waitSource = 0;
                break; // do not wake up any other threads
            }
        }

        dispatchEvent(*ev);
        delete ev;
    }
}

static void saveStack() {
    auto f = currentFiber;
    if (!f)
        return;
    int sizeNeeded = vmImg->stackTop - f->sp;
    // DMESG("save %d %p", sizeNeeded, f);
    if (!f->stackCopy || sizeNeeded > f->stackCopySize) {
        xfree(f->stackCopy);
        f->stackCopySize = sizeNeeded + 10;
        f->stackCopy = (TValue *)xmalloc(f->stackCopySize * sizeof(TValue));
        // DMESG(" -> %p", f->stackCopy);
    }
    memcpy(f->stackCopy, f->sp, sizeNeeded * sizeof(TValue));
}

static void restoreStack() {
    auto f = currentFiber;
    memcpy(f->sp, f->stackCopy, (uint8_t *)vmImg->stackTop - (uint8_t *)f->sp);
}

static void hostRestartCleanup();

static void mainRunLoop() {
    FiberContext *f = NULL;
    mcaMainLoopLive = 1;
    // v0.3.2：装好 panic 回弹点。从这一刻起，VM 线程里任何更深的 panic
    // （target_panic/soft_panic 的第 (2) 条分支）都会 longjmp 回到这里，
    // 由循环顶的 `if (panicCode)` 分支统一收尾，而不是 return 回调用方
    // 让它继续解引用非法值（见 mcaPanicUnwind 上面的说明）。
    if (setjmp(mcaPanicJmp) == 0) {
        mcaPanicJmpArmed = 1;
    } else {
        mcaTrace("[vm] panic %d unwound to main loop frame", panicCode);
        // 局部变量 f 在 longjmp 后不可信，这里不再使用；下面的 panic 分支直接返回。
    }
    for (;;) {
        // MCA v0.2.14：panic 后从这里收尾退出。
        //
        // panic 发生在 fiber 里时，target_panic / soft_panic 只置标志就返回
        //（见那里的 (2)），exec_loop 的 `if (panicCode) break;` 会把控制权送回
        // 这里。走 hostRestartCleanup() 之后**正常 return**，线程从 multiStart
        // 收尾退出（mcaVmExited=1），前端据此自动重启 —— 而不是 pthread_exit
        //（Mingw 上崩、Linux 上会让前端以为 VM 还活着）。
        if (panicCode) {
            mcaTrace("[vm] panic %d seen at main loop -> cleanup and exit", panicCode);
            hostRestartCleanup();
            mcaLeaveMainLoop();
            return;
        }
        // MCA PATCH 5：宿主请求重启 → 退出主循环，交给 initRuntime 末尾的
        // systemReset()（释放 fiber/GC 后 pthread_exit），宿主随后重新 start。
        if (mcaResetRequest) {
            mcaResetRequest = 0;
            mcaTrace("[vm] reset request seen");
            // 时钟对账：loops 应等于前端已请求帧数，virtMs = loops/60 秒。
            // 对不上就是帧闸门/序号配对出了问题（确定性回归的第一线索）。
            mcaTrace("[vm] clock: loops=%llu requests=%llu virtMs=%llu",
                     (unsigned long long)g_loops, (unsigned long long)g_frameRequests,
                     (unsigned long long)(g_virtTimeNum / 3000));
            hostRestartCleanup();
            mcaTrace("[vm] cleanup returned, leaving main loop");
            mcaLeaveMainLoop();
            return;   // initRuntime 看到 mcaRestartDone 就不再 systemReset()
        }
        // 一圈一帧：等前端请求下一帧再推进。
        // 没有这个同步，VM 的圈数会随调度在「前端帧数」附近漂移，虚拟时间
        // 跟着漂，同一输入的两次运行画面就分岔。
        if (!mca_frame_requested()) {
            mca_tick_wait(5);
            continue;
        }
        g_loops++;
        g_virtTimeNum += MCA_FRAME_STEP;
        // 发布一帧给前端（内容 = 上一圈逻辑执行完后的画面缓冲）。
        // 放在圈顶而不是圈尾：圈内有多处 continue，放圈尾会漏帧，帧数就对不上了。
        pxt_screen_publish(g_loops); // 拷缓冲 + 置序号 + 广播（同一把锁）
        // MCA v0.2.14：panic 之后不能在这里 return。
        // 直接 return 会回到 initRuntime 末尾的 systemReset()（pthread_exit），
        // 那条路在 Mingw 上崩、在 Linux 上绕过 multiStart 的收尾。改成 continue，
        // 让循环顶那个 panic 分支统一做 hostRestartCleanup() + 正常返回。
        if (panicCode)
            continue;
        wakeFibers();
        auto now = current_time_ms();
        auto fromBeg = false;
        if (!f) {
            f = allFibers;
            fromBeg = true;
        }
        while (f) {
            if (f->wakeTime && now >= (int)f->wakeTime)
                f->wakeTime = 0;
            if (!f->wakeTime && !f->waitSource)
                break;
            f = f->next;
        }
        if (f) {
            if (currentFiber != f) {
                saveStack();
                currentFiber = f;
                restoreStack();
            }
            f->pc = f->resumePC;
            f->resumePC = NULL;
            if (f->wakeFn) {
                auto fn = f->wakeFn;
                f->wakeFn = NULL;
                f->r0 = fn(f->wakeFnArg);
                if (f->wakeTime || f->waitSource)
                    continue; // we got suspended again
            }
            exec_loop(f);
            // panic 后不能在这里 `return`（会落到 initRuntime 末尾的
            // systemReset() → pthread_exit），也不能单纯 `break`（那会跳出
            // 外层 for，同样从 mainRunLoop 返回）。就在这里把收尾做掉，
            // 然后 return 到 initRuntime —— 它看到 mcaRestartDone 就不再
            // systemReset()，线程从 multiStart 正常退出（mcaVmExited=1）。
            if (panicCode) {
                mcaTrace("[vm] panic %d after exec_loop -> cleanup and exit", panicCode);
                f = NULL;
                hostRestartCleanup();
                mcaLeaveMainLoop();
                return;
            }
            auto n = f->next;
            if (f->resumePC == NULL) {
                if (f->foreverPC) {
                    f->resumePC = f->foreverPC;
                    f->wakeTime = current_time_ms() + 20;
                    // restore stack, as setupThread() does it
                    for (int i = 0; i < 5; ++i) {
                        if (*--f->sp == TAG_STACK_BOTTOM)
                            break;
                    }
                    if (*f->sp != TAG_STACK_BOTTOM)
                        target_panic(PANIC_INVALID_IMAGE);
                } else {
                    auto hb = f->handlerBinding;
                    if (hb) {
                        auto pev = hb->pending;
                        if (pev) {
                            hb->pending = pev->next;
                            setupThread(hb->action, fromInt(pev->value), hb);
                            xfree(pev);
                        } else {
                            f->handlerBinding->flags &= ~HANDLER_RUNNING;
                        }
                    }
                    disposeFiber(f);
                }
            }
            f = n;
        } else if (fromBeg) {
            target_yield();
        }
    }
}

// 通知 ID 从函数内 static 提到文件级（systemReset 时不随 fiber 清理，
// 保证一局之内不撞号）
static volatile int mcaNotifyId;

int allocateNotifyEvent() {
    return ++mcaNotifyId;
}

void raiseEvent(int id, int event) {
    auto e = mkEvent(id, event);
    pthread_mutex_lock(&eventMutex);
    if (eventTail == NULL) {
        if (eventHead != NULL)
            oops(51);
        eventHead = eventTail = e;
    } else {
        eventTail->next = e;
        eventTail = e;
    }
    pthread_cond_broadcast(&newEventBroadcast);
    pthread_mutex_unlock(&eventMutex);
}

DLLEXPORT void pxt_raise_event(int id, int event) {
    raiseEvent(id, event);
}

void registerWithDal(int id, int event, Action a, int flags) {
    // TODO support flags
    setBinding(id, event, a);
}

uint32_t afterProgramPage() {
    return 0;
}

char **initialArgv;

void screen_init();
void initKeys();
void target_startup();

void initRuntime() {
    current_time_ms();
    mca_tick_init(); // 帧节拍原语（幂等；重启路径上重复调用无害）
    mca_pub_init();  // 画面发布事件（前端等帧用，同样幂等）
    target_startup();

    setupThread((TValue)vmImg->entryPoint);

    target_init();
    screen_init();
    initKeys();

    DMESG("start main loop");

    mcaTrace("[vm] main loop entered");
    mainRunLoop();
    if (mcaRestartDone) {
        // 宿主重启：清理已在 hostRestartCleanup() 里完成，
        // 这里正常 return，让 VM 线程干净退出（宿主的 pthread_join 才安全）
        mcaRestartDone = 0;
        mcaTrace("[vm] initRuntime return (restart path)");
        return;
    }
    mcaTrace("[vm] main loop ended without restart -> systemReset");
    systemReset();
}

// MCA PATCH 5：GC 分配游标从函数内 static 提到文件级，好让 systemReset()
// 在重启时把它绕回堆首。64 位下 VM 堆固定 1MB（PXT_VM_HEAP_ALLOC_BITS=20），
// 不回绕的话重开几次游戏就 soft_panic(20)。
static uint8_t *gcAllocPtr = (uint8_t *)GC_BASE;

void *gcAllocBlock(size_t sz) {
#ifdef PXT_ESP32
    void *r = xmalloc(sz);
#else
    uint8_t *&currPtr = gcAllocPtr;
    sz = (sz + GC_PAGE_SIZE - 1) & ~(GC_PAGE_SIZE - 1);
#if defined(PXT64) || defined(__MINGW32__)
    if (!gcBase) {
        gcBase = (uint8_t *)xmalloc(1 << PXT_VM_HEAP_ALLOC_BITS);
        // 堆清零：malloc 的旧内容会让"读到未初始化字段"的行为随进程堆
        // 历史变化，同一输入的两次运行结果可能不同。1MB 一次 memset
        // 只在开局付一次。
        memset(gcBase, 0, (size_t)1 << PXT_VM_HEAP_ALLOC_BITS);
        currPtr = gcBase;
    }
    void *r = currPtr;
    if ((uint8_t *)currPtr - gcBase > (1 << PXT_VM_HEAP_ALLOC_BITS) - (int)sz)
        soft_panic(20);
#else
    void *r = mmap(currPtr, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (r == MAP_FAILED) {
        DMESG("mmap %p failed; err=%d", currPtr, errno);
        target_panic(PANIC_INTERNAL_ERROR);
    }
#endif
    currPtr = (uint8_t *)r + sz;
#endif

    if (isReadOnly((TValue)r)) {
        DMESG("mmap returned read-only address: %p", r);
        target_panic(PANIC_INTERNAL_ERROR);
    }
    return r;
}

void gcProcessStacks(int flags) {
    int cnt = 0;
    for (auto f = allFibers; f; f = f->next) {
        TValue *end, *ptr;
        if (f == currentFiber) {
            end = vmImg->stackTop;
            ptr = f->sp;
        } else {
            end = f->stackCopy + (vmImg->stackTop - f->sp);
            ptr = f->stackCopy;
        }
        gcProcess((TValue)f->currAction);
        gcProcess((TValue)f->r0);
        if (flags & 2)
            DMESG("RS%d:%p/%d", cnt++, ptr, end - ptr);
        // VLOG("mark: %p - %p", ptr, end);
        while (ptr < end) {
            gcProcess(*ptr++);
        }
    }
}

#define MAX_RESET_FN 32
static reset_fn_t resetFunctions[MAX_RESET_FN];

void registerResetFunction(reset_fn_t fn) {
    for (int i = 0; i < MAX_RESET_FN; ++i) {
        if (!resetFunctions[i]) {
            resetFunctions[i] = fn;
            return;
        }
    }

    target_panic(PANIC_INTERNAL_ERROR);
}

// MCA PATCH 5（本项目）：VM 的"回到初始状态"收尾序列。
// 官方 systemReset() 把这段内联在里面，宿主侧重启要复用它但**不能** pthread_exit
//（Windows/mingw 下从 VM 线程 pthread_exit 会在 fiber 栈已错位时崩，
// 实测 0xC0000005；改成让线程正常 return 即可安全 join），所以抽出来共用。
static void resetRuntimeState() {
    // MCA v0.2.14：先报"谁在清理"。清理只该发生在 VM 线程上；如果这行写成
    // frontend，说明有人从前端线程走了 systemReset（那会 pthread_exit 掉前端
    // 线程，并可能自死锁在 irqMutex 上）。
    mcaTrace("[vm] cleanup begin on %s thread (panic=%d)",
             pxt_vm_is_vm_thread() ? "vm" : "frontend", panicCode);

    // MCA PATCH 7：**第一步**就让前端侧的输出通道失效。
    // 从这里到新一局 gcStartup 之间，前端线程仍然每帧来拉画面/音频；它一旦
    // 碰到已经被释放的 instWDisplay / dac / WSynthesizer 就是 use-after-free，
    // 而 pxt_screen_get_pixels 里的 target_panic 还会在**前端线程**上
    // pthread_exit —— 表现就是"按重置后前端挂起、不崩也不退"。
    mcaTrace("[vm] cleanup 0/7 output offline");
    pxt_vm_set_online(0);   // 前端从这一行起拿黑屏+静音，不再碰 VM 状态
    pxt_screen_shutdown();  // 丢掉 instWDisplay

    mcaTrace("[vm] cleanup 1/6 gcFreeze");
    gcFreeze();

    // MCA v0.2.14：每个回调单独留痕。
    //
    // 真机上出现过"日志停在这一步不动"（cleanup 2/6 之后什么都没有）。
    // 这一步跑的是各模块注册的 reset 回调（v0.2.13 只有 melody 的
    // stopPlaying），出问题的话必须能看出是哪个回调、卡在它前面还是后面。
    mcaTrace("[vm] cleanup 2/6 reset callbacks");
    for (int i = 0; i < MAX_RESET_FN; ++i) {
        auto fn = resetFunctions[i];
        if (!fn)
            continue;
        mcaTrace("[vm]   reset fn #%d %p enter", i, (void *)fn);
        fn();
        mcaTrace("[vm]   reset fn #%d %p done", i, (void *)fn);
    }
    mcaTrace("[vm] cleanup 2/6 done");

    // PATCH 9：单例的 static 指针活过 GC 重置，必须丢 —— 否则重启后
    // getWSynthesizer() 返回上一局的野指针（不重新构造 → dac 永远 NULL →
    // 没声音；地址被复用时更会类型错乱写坏内存）。
    //
    // 顺序很重要：**必须放在 reset 回调之后**。回调里 melody 的 stopPlaying()
    // 会调 getWSynthesizer()，若此时单例已被清空，它会当场 new 一个新的到
    // 即将被 gcReset 掉的旧堆里 —— 等于又留下一个野指针（实测就是这么复现的：
    // trace 里 ExtDAC 在 cleanup 0/7 之后又被创建了一次）。
    mcaTrace("[vm] cleanup 2.5/6 drop singletons (music/audio)");
    pxt_audio_shutdown();   // dac = NULL（指向 VM 堆）
    pxt_music_shutdown();   // instWSynthesizer = NULL

    mcaTrace("[vm] cleanup 3/6 coreReset");
    coreReset(); // clears handler bindings

    mcaTrace("[vm] cleanup 4/6 dispose fibers");
    currentFiber = NULL;
    while (allFibers) {
        disposeFiber(allFibers);
    }

    // this will consume all events, but won't dispatch anything, since all listener maps are empty
    mcaTrace("[vm] cleanup 5/6 wakeFibers");
    wakeFibers();

    // mark all GC memory as free
    mcaTrace("[vm] cleanup 6/6 gcReset");
    gcReset();

    // GC 分配游标绕回堆首 —— 与第一次启动时的状态一致（64 位下 VM 堆固定
    // 1MB，不回绕的话重开几次游戏就 soft_panic(20)）
#if defined(PXT64) || defined(__MINGW32__)
    if (gcBase)
        gcAllocPtr = gcBase;
#else
    gcAllocPtr = (uint8_t *)GC_BASE;
#endif
    // 清空 reset 回调槽：registerResetFunction() 在槽满时会 target_panic，
    // 而 systemReset 只调回调不清表，重启时重复注册就会 panic
    for (int i = 0; i < MAX_RESET_FN; ++i)
        resetFunctions[i] = NULL;

    // MCA PATCH 6：块清单也要一起忘掉。切块游标已经绕回堆首，若 firstBlock 里
    // 还留着上一局的块，新块会落在同一地址 → linkFreeBlock() 自环 → sweep() 死循环
    //（真机上就是"重置后画面冻住"）。清空后新一局从堆首重新切块 = 冷启动。
    gcForgetAllBlocks();
    // 兜底：万一还有静态指针在清理途中被重建（比如某个 reset 回调里 new 了
    // 单例），这里再丢一次。此时堆已作废，任何残留指针都是野的。
    pxt_screen_shutdown();
    pxt_audio_shutdown();
    pxt_music_shutdown();
    mcaTrace("[vm] cleanup done (blocks forgotten)");
}

// 宿主说"重开这一局"（libretro 的 reset 键 / Restart）
// 清理过程本身若再触发 panic（比如 GC_CHECK / oops），会经 target_panic →
// systemReset() 递归回到这里。真机上这条路是无限递归 = 画面冻住不动。
// 用一个标志挡住重入：已经在清理就只把线程退掉，不再跑第二遍。
static volatile int mcaResetting;

static void hostRestartCleanup() {
    dmesg("HOST RESET");
    mcaTrace("[vm] HOST RESET begin");
    if (!panicCode)
        panicCode = -1;
    mcaResetting = 1;
    resetRuntimeState();
    mcaResetting = 0;
    mcaTrace("[vm] HOST RESET end");
    // mcaVmExited 由 multiStart() 在线程真正返回前设置（PATCH 6）：
    // 在这里置位会让宿主的 pthread_join 提前去等一个还在清理的线程。
    mcaRestartDone = 1;
}

// MCA v0.2.14：VM 线程自己退出的统一出口。
//
// 为什么不能只写 pthread_exit()：pthread_exit **不走** multiStart() 的 return
// 路径，那些收尾语句（mcaMainLoopLive=0 / offline / mcaVmExited=1）一行都不会
// 执行。于是宿主永远认为 VM 还活着（pxt_vm_running() == true），前端也就永远
// 不会走重启分支 —— 真机上的表现正是"清理日志停在中途，画面冻在最后一帧，
// 进程不崩也不退出"。凡是线程要自己死掉，都必须先把"我死了"写清楚。
static void vmThreadExitNow(const char *why) {
    mcaTrace("[vm] %s -> thread exit (marked)", why);
    mcaLeaveMainLoop();
    pxt_vm_set_online(0);
    mcaVmExited = 1; // 宿主（vm_poll_restart）据此 join 并用原始镜像重新启动
    pthread_exit(NULL);
}

void systemReset() {
#ifdef PXT_ESP32
    esp_restart();
#else
    if (!panicCode)
        panicCode = -1;

    dmesg("TARGET RESET");

    if (mcaResetting) {
        // 清理中途又 panic：直接让线程退出，避免递归清理打死循环
        vmThreadExitNow("systemReset re-entry during cleanup");
    }

    mcaResetting = 1;
    resetRuntimeState();
    mcaResetting = 0;
    mcaTrace("[vm] systemReset cleanup done");

    vmThreadExitNow("after cleanup");
#endif
}


} // namespace pxt
