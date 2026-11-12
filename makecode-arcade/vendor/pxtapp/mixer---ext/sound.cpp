#include "pxt.h"
#include "SoundOutput.h"
#include "melody.h"

namespace music {

static ExtDAC *dac;

// MCA PATCH 7：重置时丢 dac。它指向 VM 堆里的 ExtDAC/WSynthesizer，
// 清理完再被前端线程读就是 use-after-free（真机上是花屏/杂音/崩）。
DLLEXPORT void pxt_audio_shutdown() {
    dac = NULL;
}

DLLEXPORT void pxt_get_audio_samples(int16_t *buf, unsigned numSamples) {
    // 离线（重启间隙）时前端也照样来要样本 —— 直接静音，别碰已释放的混音器
    if (!pxt_vm_online() || !dac) {
        memset(buf, 0, numSamples * 2);
        return;
    }

    target_disable_irq();
    dac->src.fillSamples(buf, numSamples);
    target_enable_irq();

    for (unsigned i = 0; i < numSamples; ++i) {
        // playing at half-volume
        buf[i] = buf[i] << 3;
    }

    //DMESG("samples %d %d %d %d", numSamples, buf[0], buf[20], buf[100]);
}

ExtDAC::ExtDAC(WSynthesizer &data) : src(data) {
    dac = this;
    // 每次（重新）构造都留一行 trace：重启后若没有这一行，说明单例指针没被
    // 丢掉、dac 一直是 NULL —— 那正是"重置后没声音"的判据。
    mcaTrace("[vm] ExtDAC created (audio online)");
}

} // namespace music