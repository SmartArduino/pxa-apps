#!/usr/bin/env python3
"""Prepare the pinned MIT runtime for the single-threaded PXA guest.

The vendor tree is never modified. Fail on changed patch anchors so upstream
updates require reviewing the port instead of silently losing a change.
"""
import shutil
import sys
from pathlib import Path

source, destination = map(Path, sys.argv[1:])
shutil.copytree(source, destination, dirs_exist_ok=True)


def patch(name, old, new):
    path = destination / name
    text = path.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"Patch anchor changed: {name}: {old[:70]!r}")
    path.write_text(text.replace(old, new))


# Wasm addresses have no Cortex-M RAM/flash address split. Keep all GC blocks
# in the existing bounded arena; literal objects remain outside that arena.
patch('platform.h', '#if defined(PXT64) || defined(__MINGW32__)',
      '#if defined(PXT64) || defined(__MINGW32__) || defined(PXA_ARCADE)')
patch('core---vm/scheduler.cpp', '#elif !defined(PXT64)',
      '#elif !defined(PXT64) && !defined(PXA_ARCADE)')
path = destination / 'core---vm/scheduler.cpp'
text = path.read_text().replace('#if defined(PXT64) || defined(__MINGW32__)',
    '#if defined(PXT64) || defined(__MINGW32__) || defined(PXA_ARCADE)')
path.write_text(text)

# Native ABIs tolerate an unused argument on these vtable callbacks; Wasm
# validates the exact function signature and otherwise traps during collection.
patch('base/core.cpp', 'static void dtorDoNothing() {}',
      'static void dtorDoNothing(RefObject *) {}')
patch('base/core.cpp', 'void doNothing() {}', 'void doNothing(RefObject *) {}')
patch('pxtbase.h', 'void doNothing();', 'void doNothing(RefObject *);')

path = destination / 'core---vm/scheduler.cpp'
text = path.read_text()
start = text.index('void sleep_core_us(uint64_t us) {')
end = text.index('// MCA v0.2.10：target_yield', start)
text = text[:start] + '''void sleep_core_us(uint64_t us) {
    if (currentFiber) sleep_ms((unsigned)((us + 999) / 1000));
}

''' + text[end:]
path.write_text(text)

# NumberBoxes contains packed 12-byte {vtable, double} records, including a
# padding record. Patch every vtable instead of aborting at the upstream TODO.
patch('core---vm/verify.cpp',
'''                return (state->error = "TODO: NumberBoxes");''',
'''                if ((sect.size - 8) % 12 != 0 && (sect.size - 12) % 12 != 0)
                    return (state->error = "invalid number boxes");
                for (uint32_t pos = 8; pos + 12 <= sect.size; pos += 12) {
                    const VTable *number = &number_vt;
                    memcpy(data + pos, &number, sizeof(number));
                }''')

# Resume one upstream scheduler iteration per frame request. Retain its fiber
# cursor between calls, and forget it before unloading the old image.
patch('core---vm/scheduler.cpp',
      'static void mainRunLoop() {\n    FiberContext *f = NULL;',
      'static FiberContext *pxaNextFiber;\nstatic void mainRunLoop() {\n    auto &f = pxaNextFiber;')
patch('core---vm/scheduler.cpp',
      '    for (;;) {\n        // MCA v0.2.14：panic 后从这里收尾退出。',
      '    for (unsigned once = 0; once < 1; ++once) {\n        // MCA v0.2.14：panic 后从这里收尾退出。')
patch('core---vm/scheduler.cpp', '    mainRunLoop();\n    if (mcaRestartDone)',
      '    return; // PXA calls pxa_vm_step() from its event loop.\n    if (mcaRestartDone)')
patch('core---vm/scheduler.cpp', 'static void resetRuntimeState() {',
      'void pxa_vm_step() { mainRunLoop(); }\nvoid pxa_vm_cleanup();\n\nstatic void resetRuntimeState() {\n    pxaNextFiber = NULL;')
patch('core---vm/scheduler.cpp', '// 宿主说"重开这一局"（libretro 的 reset 键 / Restart）',
      'void pxa_vm_cleanup() { resetRuntimeState(); mcaRestartDone = 0; }\n\n// 宿主说"重开这一局"（libretro 的 reset 键 / Restart）')
patch('screen---ext/screen.cpp',
      'const uint64_t mySeq = pxt_vm_frame_request();',
      'const uint64_t mySeq = mcaPubSeq; // PXA already stepped synchronously.')

# There is no portable setjmp in wasm32-wasip1. Ordinary VM fiber suspension
# already returns normally. Explicit JS throws are rejected in this first port;
# panics trap the guest rather than returning into invalid object accesses.
patch('core---vm/vm.cpp', '    longjmp(ctx->loopjmp, 1);',
      '    target_panic(PANIC_UNHANDLED_EXCEPTION);')
patch('core---vm/scheduler.cpp', 'static int mcaPanicUnwind() {',
      'static int mcaPanicUnwind() {\n#ifdef PXA_ARCADE\n    __builtin_trap();\n#endif')

# Keep native and Wasm allocation ownership in libc; upstream global new/delete
# conflict with the PXA SDK allocator and are unnecessary for VM objects.
patch('core---vm/scheduler.cpp', 'void *operator new(size_t size) {',
      '#ifndef PXA_ARCADE\nvoid *operator new(size_t size) {')
patch('core---vm/scheduler.cpp', 'uint8_t *gcBase;',
      '#endif\nuint8_t *gcBase;')

for filename in ['core---vm/scheduler.cpp', 'core---vm/target.cpp']:
    patch(filename, '#include <signal.h>', '// No POSIX signal handling in the guest.')

# Runtime diagnostics use the existing PXA logger, with no host filesystem IO.
path = destination / 'core---vm/scheduler.cpp'
text = path.read_text()
start = text.index('static FILE *mcaTraceFile;')
end = text.index('// MCA PATCH 7：', start)
text = text[:start] + '''extern "C" void emu_core_log(const char *, ...);
void mcaTrace(const char *format, ...) {
    char line[256]; va_list args; va_start(args, format);
    vsnprintf(line, sizeof(line), format, args); va_end(args);
    emu_core_log("ARCADE %s", line);
}
DLLEXPORT void pxt_vm_trace(const char *msg) { mcaTrace("%s", msg); }
DLLEXPORT const char *pxt_vm_trace_path() { return ""; }

''' + text[end:]
path.write_text(text)
