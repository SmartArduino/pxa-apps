#include "pxt.h"
#include "mca_tick.h" // 取帧等待用跨平台事件（pthread_cond_timedwait 在 mingw 上不可用）

// MCA v0.2.10：前端每取一帧就向 VM 要一张"推进一圈"的许可，返回该请求的序号。
// VM 发布画面时带上同样的序号，前端等"发布序号 >= 我的请求序号"再取画面，
// 于是第 N 次取帧恒定拿到第 N 帧 —— 虚拟时钟按帧推进、画面内容与调度无关。
DLLEXPORT uint64_t pxt_vm_frame_request();

namespace pxt {
extern volatile uint64_t mcaPubSeq; // 定义在 scheduler.cpp：已发布的帧序号
class WDisplay {
  public:
    uint32_t currPalette[16];
    bool newPalette;
    uint8_t *screenBuf; // VM 正在写的工作缓冲
    uint8_t *pubBuf;    // 已发布给前端的快照（双缓冲，见 pxt_screen_publish）
    unsigned screenBufSize; // 单块缓冲字节数（0 = 尚未分配）

    int width, height;

    WDisplay();
    void updateLoop();
    void update(Image_ img);
};

SINGLETON(WDisplay);

WDisplay::WDisplay() {
    width = getConfig(CFG_DISPLAY_WIDTH, 160);
    height = getConfig(CFG_DISPLAY_HEIGHT, 128);
    DMESG("init display: %dx%d", width, height);
    const size_t n = (size_t)width * height / 2 + 20;
    // 必须清零：这两块缓冲是"第一帧画面"的来源，new 出来的内容是上次
    // 堆里剩下的垃圾。不清零的话，开机头几帧的像素取决于分配器复用到了
    // 什么 —— 同一输入两次运行的画面从第 1 帧起就不同（存档确定性失效）。
    screenBufSize = (unsigned)n;
    screenBuf = new uint8_t[n];
    pubBuf = new uint8_t[n];
    memset(screenBuf, 0, n);
    memset(pubBuf, 0, n);
    memset(currPalette, 0, sizeof(currPalette));
    newPalette = false;
}

//% expose
int setScreenBrightnessSupported() {
    return 0;
}

//% expose
void setScreenBrightness(int level) {
    // TODO
}

//% expose
void setPalette(Buffer buf) {
    auto display = getWDisplay();
    if (48 != buf->length)
        target_panic(PANIC_SCREEN_ERROR);
    for (int i = 0; i < 16; ++i) {
        uint8_t r = buf->data[i * 3];
        uint8_t g = buf->data[i * 3 + 1];
        uint8_t b = buf->data[i * 3 + 2];
        display->currPalette[i] = (0xff << 24) | (r << 16) | (g << 8) | (b << 0);
    }
    display->newPalette = true;
}

static pthread_mutex_t screenMutex;
static int numGetPixels;

// NX 诊断：把"取帧尺寸对不上"这类只在真机上看得见的事写进运行期日志。
// 黑屏的一个直接可能：WDisplay 的 CFG_DISPLAY_HEIGHT 默认 128，而核心按 120
// 报告 —— 对不上就走下面的蓝屏分支，玩家看到的就是"一片黑"。
#if defined(MCA_NX_DIAG)
extern "C" void mca_nx_runlog(const char *fmt, ...) __attribute__((weak));
#define MCA_SCREEN_LOG(...) do { if (mca_nx_runlog) mca_nx_runlog(__VA_ARGS__); } while (0)
#else
#define MCA_SCREEN_LOG(...) ((void)0)
#endif

static void fill_screen(uint32_t *screen, int width, int height, uint32_t color) {
    const int n = width * height;
    for (int i = 0; i < n; ++i)
        screen[i] = color;
}

// MCA PATCH 7：这个函数在**前端线程**上执行，三条硬规则：
//   1) 绝不 target_panic。VM 版的 target_panic → systemReset() → pthread_exit，
//      在前端线程上等于把整个前端注销掉（真机上就是"重置后挂起"）。
//   2) 先拿锁再读 instWDisplay。shutdown 在锁内把它置空，否则会 use-after-free。
//   3) 等待必须有上限。上游写的是 `pthread_cond_timedwait(&c,&m,&{0,100ms})`，
//      但 timespec 是**绝对**时间，1970 年那一刻早已过去 → 永远返回 EINVAL，
//      实际等于不等待；这里改成真正的 16ms 绝对超时（一帧的预算）。
DLLEXPORT void pxt_screen_get_pixels(int width, int height, uint32_t *screen) {
    // MCA v0.2.10：向前端要帧 = 给 VM 一张"推进一圈"的许可。VM 圈数因此
    // 严格等于前端请求的帧数，虚拟时间随之严格推进（存档确定性的基础）。
    // 返回的序号是本次请求的编号：必须等 VM 发布到**这一号**才取画面。
    // 用序号而不是 bool 标志 —— bool 会被后面几帧冲掉，曾经出现"积压十几帧、
    // 末帧取决于 unload 时 VM 恰好跑到哪里"的非确定性。
    //
    // 请求必须**无条件**发出（连离线时也发）：前端每调一次 retro_run 就是
    // 一帧，若"离线那几帧不计数"，VM 上线后要补跑的圈数就取决于线程启动
    // 快慢（墙钟），帧号与圈号从此错位。无条件请求保证「第 N 次取帧 = 第 N 圈」。
    const uint64_t mySeq = pxt_vm_frame_request();

    if (!pxt_vm_online()) {
        fill_screen(screen, width, height, 0xff000000);
        return;
    }

    // 等 VM 产出**这一帧**（不持锁等，免得与 publish 抢锁）。
    //
    // 取帧必须与 VM 的节拍严格乒乓（一帧一取），这是帧驱动时钟 + 快照确定性
    // 的前提：不等就会读到上一帧，内容和"谁先跑"有关，同一输入两次运行画面
    // 从随机帧起分叉。100ms 兜底（VM 恐慌/卡死时退回旧帧，不让前端挂死）。
    //
    // 这里用 mca_pub_wait 而不是 pthread_cond_timedwait：后者在 zig-mingw 的
    // winpthreads 上直接返回 EINVAL(22)，实测 300 帧里 288 次"立即超时"，
    // 等于没等（见 mca_tick.h）。
    int waitedMs = 0;
    while (mcaPubSeq < mySeq && pxt_vm_online() && waitedMs < 100) {
        if (!mca_pub_wait(10))
            waitedMs += 10;
    }

    pthread_mutex_lock(&screenMutex);

    auto disp = instWDisplay;
    if (!disp || !pxt_vm_online()) {
        pthread_mutex_unlock(&screenMutex);
        fill_screen(screen, width, height, 0xff000000);
        return;
    }
    numGetPixels++;
    if (numGetPixels <= 2)
        MCA_SCREEN_LOG("[screen] real frame #%d %dx%d panic=%d seq=%llu/%llu", numGetPixels,
                       width, height, panicCode, (unsigned long long)mySeq,
                       (unsigned long long)mcaPubSeq);

    if (mcaPubSeq < mySeq) {
        // 兜底路径：VM 没在预算内出帧（恐慌 / 卡顿 / 正在重置）。
        // 读上一帧或黑屏都行，唯一要求是别让前端挂住。
        mcaTrace("[w] giveup seq=%llu pub=%llu", (unsigned long long)mySeq,
                 (unsigned long long)mcaPubSeq);
    }

    if (width != disp->width || height != disp->height) {
        // 尺寸不符是镜像配置问题，蓝屏让玩家/开发者看得见，但不要杀前端
        static int reported = 0;
        if (reported++ < 3)
            MCA_SCREEN_LOG("[screen] size mismatch core=%dx%d disp=%dx%d buf=%u",
                           width, height, disp->width, disp->height, disp->screenBufSize);
        pthread_mutex_unlock(&screenMutex);
        fill_screen(screen, width, height, 0xff0000ff);
        return;
    }

    if (panicCode > 0) {
        int n = width * height;
        uint32_t *p = screen;
        // blue screen
        while (n--)
            *p++ = 0xff0000ff;
    } else {
        auto sp = disp->pubBuf; // 只读已发布的快照（双缓冲，见 publish）
        auto pal = disp->currPalette;
        for (int x = 0; x < width; ++x) {
            uint32_t *p = screen + x;
            for (int y = 0; y < (height >> 1); ++y) {
                uint8_t v = *sp++;
                *p = pal[v & 0xf];
                p += width;
                *p = pal[v >> 4];
                p += width;
            }
        }
    }
    // 这里不再回置"帧已取走"标志、也不再广播：前端与 VM 靠序号配对，
    // VM 侧不再等前端消费（双缓冲，见 WDisplay::update）。
    pthread_mutex_unlock(&screenMutex);
}

// MCA PATCH 7：重置时丢掉屏幕的静态状态。必须在 gcReset() 之前调用 ——
// instWDisplay / screenBuf 都活在 VM 堆上，清理完就是野指针。
DLLEXPORT void pxt_screen_shutdown() {
    pthread_mutex_lock(&screenMutex);
    instWDisplay = NULL;
    numGetPixels = 0;
    pthread_mutex_unlock(&screenMutex);
    mca_pub_signal(); // 叫醒可能正在等取帧的前端线程（它会看到"离线"退回黑屏）
}

void WDisplay::update(Image_ img) {
    if (!img)
        return;

    if (img->bpp() != 4 || img->width() != width || img->height() != height)
        target_panic(PANIC_SCREEN_ERROR);

    pthread_mutex_lock(&screenMutex);
    // MCA v0.2.10：这里不再等前端取帧。
    //
    // 上游（以及 PATCH 7 的超时版）在这里等"上一帧被取走"，是因为写的就是
    // 前端要读的那块缓冲 —— 不排空就会覆盖未消费的帧，而等待本身又引入
    // 真实时间依赖（超时丢弃 = 丢帧数随调度变化）。
    // 现在改成双缓冲：本函数只写 screenBuf（VM 侧工作缓冲），发布时由
    // pxt_screen_publish 拷进 pubBuf 再交给前端。前端读 pubBuf、VM 写
    // screenBuf，两者永不重叠，于是这里不需要任何等待。
    if (!pxt_vm_online()) {
        pthread_mutex_unlock(&screenMutex);
        return;
    }
    memcpy(screenBuf, img->pix(), img->pixLength());
    pthread_mutex_unlock(&screenMutex);

    if (newPalette) {
        newPalette = false;
    }
}

// MCA v0.2.10：主循环每圈调一次，把当前画面缓冲发布给前端（一圈一帧）。
// 前端"先要帧、后取帧"：第 N 次要帧对应第 N 次发布，序号在同一把锁内写成，
// 所以前端看到序号就保证缓冲已就绪；前端读 buf 期间持锁，发布无法插入。
DLLEXPORT void pxt_screen_publish(unsigned long long seq) {
    pthread_mutex_lock(&screenMutex);
    auto disp = instWDisplay;
    if (!disp || !pxt_vm_online()) {
        pthread_mutex_unlock(&screenMutex);
        return;
    }
    // 双缓冲：把工作缓冲拷进发布缓冲，前端只读 pubBuf。
    // 这样"前端取帧"与"VM 继续画下一帧"不共享任何字节。
    memcpy(disp->pubBuf, disp->screenBuf, (size_t)disp->width * disp->height / 2);
    mcaPubSeq = (uint64_t)seq; // 序号与缓冲在同一把锁内就绪，前端看到序号即可放心读
    pthread_mutex_unlock(&screenMutex);
    // 锁外唤醒前端线程：它拿到锁后会重新确认序号，不存在"读到半帧"的窗口
    mca_pub_signal();
}

//% expose
void updateScreen(Image_ img) {
    getWDisplay()->update(img);
}

//% expose
void updateStats(String msg) {
    DMESG("stats: %s", msg->getUTF8Data());
}
} // namespace pxt