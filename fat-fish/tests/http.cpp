// SPDX-License-Identifier: MIT
#include "../dsh_http.hpp"
#include <cassert>

namespace {
int finish_result;
unsigned reads;
void prepare(fish::dsh::Http &http) {
    http.active = true;
    http.handle = UINT64_C(1) << 32;
    http.deadline = 1000;
    http.upload_size = http.upload_offset = 3000;
    http.upload_finished = false;
    reads = 0;
}
}

extern "C" int32_t pxa_call(uint32_t, uint64_t, const uint8_t *, uint32_t) { return 0; }
extern "C" int32_t pxa_io(uint64_t, uint32_t operation, uint8_t *data, uint32_t size) {
    if (operation == PXA_HTTP_IO_FINISH_UPLOAD) return finish_result;
    if (operation == PXA_HTTP_IO_RESPONSE) {
        assert(size >= 16);
        std::memset(data, 0, 16);
        pxa_store_u16(data, 200);
        pxa_store_u16(data + 2, 1);
        pxa_store_u64(data + 8, 2);
        return 16;
    }
    assert(operation == PXA_NET_IO_READ && size >= 2);
    if (reads++) return 0;
    data[0] = '{'; data[1] = '}'; return 2;
}

int main() {
    fish::dsh::Http completed;
    prepare(completed);
    finish_result = PXA_STATUS_BAD_STATE;
    completed.pump(0);
    assert(completed.done && !completed.error && completed.status == 200 && completed.used == 2);

    fish::dsh::Http failed;
    prepare(failed);
    finish_result = PXA_STATUS_IO_ERROR;
    failed.pump(0);
    assert(failed.done && failed.error == PXA_STATUS_IO_ERROR && !failed.status);

    fish::dsh::Http pending;
    prepare(pending);
    finish_result = PXA_STATUS_WOULD_BLOCK;
    pending.pump(0);
    assert(pending.active && !pending.done);
    finish_result = PXA_STATUS_BAD_STATE;
    pending.pump(1);
    assert(pending.done && !pending.error && pending.used == 2);
    std::puts("dsh-pet HTTP completion tests passed");
}
