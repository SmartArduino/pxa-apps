// SPDX-License-Identifier: MIT
#define PXA_POCKET_STUDIO 1
#include "../animation.cpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <vector>

namespace {
uint32_t clock_ms;
int request_status;
}
namespace pocket {
uint32_t millis() { return clock_ms; }
void redraw() {}
void log(const char *) {}
}
namespace studio {
void picture_region(int, int, int, int, const Image &, int, int, int, int, int, bool) {}
}
extern "C" int32_t pxa_call(uint32_t, uint64_t, const uint8_t *, uint32_t) { return request_status; }

int main(int argc, char **argv) {
    using namespace fish::animation;
    assert(argc == 2);
    std::ifstream file(argv[1], std::ios::binary);
    const std::vector<uint8_t> binary((std::istreambuf_iterator<char>(file)), {});
    assert(!binary.empty());
    request_status = PXA_STATUS_BUSY;
    update(true);
    assert(!failed() && !pending && retry_at == 100);
    clock_ms = 99; update(true);
    assert(!pending && retries == 1);
    clock_ms = 100; request_status = 0; update(true);
    assert(pending && !retry_at);
    uint8_t failure[4]; pxa_store_u32(failure, uint32_t(PXA_STATUS_IO_ERROR));
    pxa_event_t completion{};
    completion.service = PXA_ASSETS_SERVICE; completion.opcode = PXA_ASSETS_READ;
    completion.token = pending; completion.payload = failure; completion.payload_size = sizeof(failure);
    assert(event(completion) && !failed() && retries == 2 && retry_at == 300);
    const uint64_t obsolete = completion.token;
    play(fish::art::WAVE);
    assert(!pending && !retry_at && event(completion) && !failed());
    request_status = PXA_STATUS_BUSY;
    update(true);
    for (unsigned i = 0; i < 3; ++i) { clock_ms = retry_at; update(true); }
    assert(failed() && retries == 3 && !pending);
    request_status = 0;
    play(fish::art::IDLE);
    completion.token = obsolete;
    assert(event(completion) && !failed());
    uint32_t random = 12345;
    for (unsigned i = 0; i < 3000; ++i) {
        random = random * 1664525 + 1013904223;
        clock_ms += random % 2000;
        if (i % 71 == 0) play(fish::art::Clip((random >> 16) % fish::art::COUNT));
        update(true);
        if (pending) {
            auto &group = groups[filling];
            std::memcpy(group.data, binary.data() + fish::art::frames[group.first].offset, group.bytes);
            group.ready = true; filling = -1; pending = 0;
        }
        update(true);
        assert(!failed());
    }
    std::puts("Animation retries, cancellation and cache jitter passed");
}
