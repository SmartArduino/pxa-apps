// MIT. Synchronous host ABI for the pinned MakeCode runtime.
#include "pxt.h"
#include "mca_tick.h"
#include <stdarg.h>
extern "C" void emu_core_log(const char *, ...);
namespace pxt {
VMImage *vmImg;
static uint32_t imageHash, imageBytes;
uint32_t mca_image_hash() { return imageHash; }
uint32_t mca_image_bytes() { return imageBytes; }
void pxa_image_identity(const uint8_t *data, unsigned size) {
    imageBytes = size; imageHash = 2166136261u;
    for (unsigned i = 0; i < size; ++i) imageHash = (imageHash ^ data[i]) * 16777619u;
}
void vmStartFromUser(const char *) { target_panic(PANIC_INVALID_IMAGE); }
void dumpDmesg() {}
}
namespace vmcache {
RefCollection *list() { return Array_::mk(); }
void run(String) { target_panic(PANIC_INVALID_IMAGE); }
void del(String) {}
}
const char *vm_settings_dir() { return "/unavailable"; }
extern "C" {
int pxt_vm_is_vm_thread() { return 1; }
int pxt_vm_running() { return pxt_vm_online(); }
void pxt_vm_request_reset() {}
const char *pxt_vm_get_data_directory() { return "/unavailable"; }
void mca_tick_init() {}
void mca_tick_shutdown() {}
void mca_tick_signal() {}
void mca_tick_wait(int) {}
void mca_pub_init() {}
void mca_pub_shutdown() {}
void mca_pub_signal() {}
int mca_pub_wait(int) { return 0; }
void vdmesg(const char *format, va_list args) {
    char line[256]; vsnprintf(line, sizeof(line), format, args); emu_core_log("ARCADE %s", line);
}
void dmesg(const char *format, ...) {
    va_list args; va_start(args, format); vdmesg(format, args); va_end(args);
}
}
