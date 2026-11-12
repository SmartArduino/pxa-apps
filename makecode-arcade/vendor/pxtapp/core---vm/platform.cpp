#include "pxt.h"

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace pxt {

static void initRandomSeed() {
    // MCA PATCH 10：种子必须**确定**。
    //
    // 上游用 /dev/urandom（Windows 上原实现还会对 fd=-1 read() 直接崩进程），
    // 本项目早先改成 GetTickCount64 + 栈地址 —— 但那样每次启动的 Math.random
    // 序列都不同：同一输入的两次运行第 1 帧就分叉，rewind / 存档 / 确定性
    // 回归全部失效（实测两次跑 20 帧的末帧哈希都不同）。
    //
    // 改用镜像自带的 hexHash：同一卡带每次启动同一序列（可复现），不同卡带
    // 之间仍然不同。局内的随机状态另外进快照，恢复后序列照原样继续。
    unsigned seed = 0xC0DA1;
    if (vmImg && vmImg->infoHeader) {
        uint64_t h = vmImg->infoHeader->hexHash;
        seed ^= (unsigned)h ^ (unsigned)(h >> 32);
    }
    seedRandom(seed);
}

void sendSerial(const char *data, int len) {
    /*
    if (!serial) {
        serial = new codal::_mbed::Serial(USBTX, NC);
        serial->baud(9600);
    }
    serial->send((uint8_t*)data, len);
    */
}

extern "C" void drawPanic(int code)
{
    // TODO
}


extern "C" void target_init()
{
    initRandomSeed();
}

// MCA PATCH 10：给 vmload.cpp 用的显式播种入口。
//
// target_init() 在 initRuntime 里才调用，而 settings 模块的 seedAddRandom
// （v0.2.10 起为 no-op）与游戏启动代码都跑在它后面 —— 顺序上没问题。
// 但 vmStartCore 在装载完镜像后立刻显式种一次，保证"随机序列 = f(镜像哈希)"
// 不依赖 target_init 的调用时序，重启/换游戏后也一定重新播种。
void seedRandomFromImage()
{
    initRandomSeed();
}

void updateScreen(Image_ img);

void screen_init() {
    updateScreen(NULL);
}

}

