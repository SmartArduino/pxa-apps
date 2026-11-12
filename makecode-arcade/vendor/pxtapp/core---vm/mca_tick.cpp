// MCA v0.2.10：帧节拍原语的平台实现（见 mca_tick.h 的说明）
//
// 本文件只做一件事：把「等下一帧 / 通知帧到达」封装成不会丢唤醒、超时
// 一定生效的两个调用。刻意不依赖 pxtapp 的任何类型，方便单独审查。

#include "mca_tick.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdio.h>

// 自动重置事件：SetEvent 置位（攒一张"有帧"的票），WaitForSingleObject
// 消费并清零。VM 未在等待时的 signal 不会丢，下次 wait 立即返回。
static HANDLE mcaTickEvt = NULL;

void mca_tick_init(void) {
    if (!mcaTickEvt)
        mcaTickEvt = CreateEventA(NULL, FALSE, FALSE, NULL);
}

void mca_tick_shutdown(void) {
    if (mcaTickEvt) {
        CloseHandle(mcaTickEvt);
        mcaTickEvt = NULL;
    }
}

void mca_tick_signal(void) {
    if (mcaTickEvt)
        SetEvent(mcaTickEvt);
}

void mca_tick_wait(int ms) {
    if (!mcaTickEvt) {
        Sleep(1);
        return;
    }
    if (ms < 1)
        ms = 1;
    WaitForSingleObject(mcaTickEvt, (DWORD)ms);
}

// 画面发布事件（前端线程等帧用），独立于节拍事件，见 mca_tick.h
static HANDLE mcaPubEvt = NULL;

void mca_pub_init(void) {
    if (!mcaPubEvt)
        mcaPubEvt = CreateEventA(NULL, FALSE, FALSE, NULL);
}

void mca_pub_shutdown(void) {
    if (mcaPubEvt) {
        CloseHandle(mcaPubEvt);
        mcaPubEvt = NULL;
    }
}

void mca_pub_signal(void) {
    if (mcaPubEvt)
        SetEvent(mcaPubEvt);
}

int mca_pub_wait(int ms) {
    if (!mcaPubEvt) {
        Sleep(1);
        return 0;
    }
    if (ms < 1)
        ms = 1;
    return WaitForSingleObject(mcaPubEvt, (DWORD)ms) == WAIT_OBJECT_0 ? 1 : 0;
}

#else // POSIX（Linux / Android / macOS）

#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

static pthread_mutex_t mcaTickMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t mcaTickCond = PTHREAD_COND_INITIALIZER;
static int mcaTickPending = 0; // signal 早于 wait 到达时保留这一票

void mca_tick_init(void) {
    // 静态初始化即可，无需动作
}

void mca_tick_shutdown(void) {}

void mca_tick_signal(void) {
    pthread_mutex_lock(&mcaTickMutex);
    mcaTickPending = 1;
    pthread_cond_broadcast(&mcaTickCond);
    pthread_mutex_unlock(&mcaTickMutex);
}

void mca_tick_wait(int ms) {
    if (ms < 1)
        ms = 1;

    pthread_mutex_lock(&mcaTickMutex);
    if (!mcaTickPending) {
        struct timeval tv;
        struct timespec dl;
        gettimeofday(&tv, NULL);
        dl.tv_sec = tv.tv_sec + ms / 1000;
        dl.tv_nsec = tv.tv_usec * 1000L + (long)(ms % 1000) * 1000000L;
        if (dl.tv_nsec >= 1000000000L) {
            dl.tv_sec++;
            dl.tv_nsec -= 1000000000L;
        }
        pthread_cond_timedwait(&mcaTickCond, &mcaTickMutex, &dl);
    }
    mcaTickPending = 0;
    pthread_mutex_unlock(&mcaTickMutex);
}

// 画面发布事件（POSIX：cond + 待取票）
static pthread_mutex_t mcaPubMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t mcaPubCond = PTHREAD_COND_INITIALIZER;
static int mcaPubPending = 0;

void mca_pub_init(void) {}

void mca_pub_shutdown(void) {}

void mca_pub_signal(void) {
    pthread_mutex_lock(&mcaPubMutex);
    mcaPubPending = 1;
    pthread_cond_broadcast(&mcaPubCond);
    pthread_mutex_unlock(&mcaPubMutex);
}

int mca_pub_wait(int ms) {
    if (ms < 1)
        ms = 1;
    int got = 0;
    pthread_mutex_lock(&mcaPubMutex);
    if (!mcaPubPending) {
        struct timeval tv;
        struct timespec dl;
        gettimeofday(&tv, NULL);
        dl.tv_sec = tv.tv_sec + ms / 1000;
        dl.tv_nsec = tv.tv_usec * 1000L + (long)(ms % 1000) * 1000000L;
        if (dl.tv_nsec >= 1000000000L) {
            dl.tv_sec++;
            dl.tv_nsec -= 1000000000L;
        }
        pthread_cond_timedwait(&mcaPubCond, &mcaPubMutex, &dl);
    }
    got = mcaPubPending;
    mcaPubPending = 0;
    pthread_mutex_unlock(&mcaPubMutex);
    return got;
}

#endif
