#!/usr/bin/env python3
"""Prepare the pinned MIT runtime for the single-threaded PXA guest.

The vendor tree is never modified. Fail on changed patch anchors so upstream
updates require reviewing the port instead of silently losing a change.
"""
import shutil
import sys
import tempfile
import re
from pathlib import Path

source, destination = map(Path, sys.argv[1:])
output = destination
temporary = tempfile.TemporaryDirectory(prefix="pxa-arcade-runtime-")
destination = Path(temporary.name) / "pxtapp"
shutil.copytree(source, destination)


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

# Keep the single-threaded scheduler readable and independently reviewable.
path = destination / 'core---vm/scheduler.cpp'
text = path.read_text()
start = text.index('static void mainRunLoop() {')
end = text.index('// 通知 ID 从函数内 static', start)
text = text[:start] + '#include "scheduler_step.inc"\n\n' + text[end:]
path.write_text(text)
patch('core---vm/scheduler.cpp', '    mainRunLoop();\n    if (mcaRestartDone)',
      '    return; // PXA advances the scheduler from its event loop.\n    if (mcaRestartDone)')
patch('core---vm/scheduler.cpp', 'static void resetRuntimeState() {',
      'void pxa_vm_step() { mainRunLoop(); }\n\nstatic void resetRuntimeState() {\n    pxaNextFiber = NULL;')
patch('core---vm/scheduler.cpp', '// 宿主说"重开这一局"（libretro 的 reset 键 / Restart）',
      'void pxa_vm_cleanup() { resetRuntimeState(); mcaRestartDone = 0; }\n\n// 宿主说"重开这一局"（libretro 的 reset 键 / Restart）')
patch('screen---ext/screen.cpp',
      'const uint64_t mySeq = pxt_vm_frame_request();',
      'const uint64_t mySeq = mcaPubSeq; // PXA already stepped synchronously.')

# JS exceptions resume bytecode state directly. An explicit depth marker
# propagates throws across a nested toString invocation without C longjmp.
patch('pxtbase.h', '#define NUM_TRY_FRAME_REGS 3', '#define NUM_TRY_FRAME_REGS 4')
patch('vm.h', '    jmp_buf loopjmp;',
      '    jmp_buf loopjmp;\n    unsigned execDepth, exceptionDepth;\n    bool exceptionPending;')
patch('core---vm/vm.cpp', '    longjmp(ctx->loopjmp, 1);',
      '    ctx->exceptionDepth = tf->registers[3];\n    ctx->exceptionPending = true;')
patch('core---vm/vm.cpp', '    f->registers[0] = (uintptr_t)ctx->currAction;',
      '    f->registers[3] = ctx->execDepth;\n    f->registers[0] = (uintptr_t)ctx->currAction;')
patch('core---vm/vm.cpp', '    setjmp(ctx->loopjmp);',
      '    const auto depth = ++ctx->execDepth;\n    unsigned quantum = 32768;\n    extern unsigned pxaFrameInstructions;')
patch('core---vm/vm.cpp', '        uint16_t opcode = *ctx->pc++;',
      '        if (depth == 1 && (!quantum-- || !pxaFrameInstructions)) {\n'
      '            ctx->resumePC = ctx->pc; ctx->pc = nullptr; break;\n'
      '        }\n        if (depth > 1 && !pxaFrameInstructions) target_panic(PANIC_BLOCKING_TO_STRING);\n        if (pxaFrameInstructions) --pxaFrameInstructions;\n'
      '        uint16_t opcode = *ctx->pc++;')
patch('core---vm/vm.cpp', '    ctx->img->execLock = 0;\n}',
      '    --ctx->execDepth;\n    ctx->img->execLock = 0;\n}')
# Insert guards before PUSH, so a throw cannot write onto the restored stack.
path = destination / 'core---vm/vm.cpp'
text = path.read_text()
guard = "if (ctx->exceptionPending) { if (ctx->exceptionDepth < depth) break; ctx->exceptionPending = false; continue; }"
text = text.replace('            if (opcode & VM_OPCODE_PUSH_MASK)', '            ' + guard + '\n            if (opcode & VM_OPCODE_PUSH_MASK)')
text = text.replace('            if (opcode & VM_RTCALL_PUSH_MASK)', '            ' + guard + '\n            if (opcode & VM_RTCALL_PUSH_MASK)')
text = text.replace('    auto r = ctx->r0;\n    ctx->pc = prevPC;',
    '    if (ctx->exceptionPending) return TAG_UNDEFINED;\n    auto r = ctx->r0;\n    ctx->pc = prevPC;')
text = text.replace('                    PUSH(v); // make sure it doesn\'t get collected',
    '                    if (ctx->exceptionPending) return NULL;\n                    PUSH(v); // make sure it doesn\'t get collected')
path.write_text(text)
path = destination / 'pointers.cpp'
text = path.read_text().replace('if (panicCode) { ctx->sp = prevSP; return; }',
    'if (ctx->exceptionPending) return;\n  if (panicCode) { ctx->sp = prevSP; return; }')
text = re.sub(r'(^  .*convertToString\(.*;)$', r'\1\n  if (ctx->exceptionPending) return;', text, flags=re.MULTILINE)
path.write_text(text)
# Hardware panics still stop the guest: returning into a failed cast is unsafe.
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

# Reset all per-cartridge timing and event state, including absolute runtime().
patch('core---vm/scheduler.cpp', '    pxaNextFiber = NULL;',
      '    pxaNextFiber = NULL;\n    g_frameRequests = g_loops = g_virtTimeNum = 0;\n'
      '    mcaPubSeq = 0; mcaNotifyId = 0; mcaResetRequest = 0; mcaMainLoopLive = 0;')

# Resolve the packed 4-bit display directly to the frontend's native format.
path = destination / 'screen---ext/screen.cpp'
with path.open('a') as stream:
    stream.write('''
namespace pxt {
DLLEXPORT void pxt_screen_get_rgb565(uint16_t *pixels, size_t stride) {
    auto display = instWDisplay;
    if (!display || !pxt_vm_online() || stride < 160) return;
    uint16_t palette[16];
    for (unsigned i = 0; i < 16; ++i) {
        uint32_t c = display->currPalette[i];
        palette[i] = ((c >> 8) & 0xf800) | ((c >> 5) & 0x7e0) | ((c >> 3) & 31);
    }
    const auto *source = display->pubBuf;
    for (unsigned x = 0; x < 160; ++x)
        for (unsigned y = 0; y < 120; y += 2) {
            const unsigned pair = *source++;
            pixels[y * stride + x] = palette[pair & 15];
            pixels[(y + 1) * stride + x] = palette[pair >> 4];
        }
}
}
''')

# Singleton objects and screen arrays use libc new in this adapter, not GC.
patch('screen---ext/screen.cpp', '    instWDisplay = NULL;',
    '    if (instWDisplay) { delete[] instWDisplay->screenBuf; delete[] instWDisplay->pubBuf; delete instWDisplay; }\n    instWDisplay = NULL;')
patch('mixer---ext/melody.cpp', '    instWSynthesizer = NULL;',
    '    delete instWSynthesizer;\n    instWSynthesizer = NULL;')

# Report the real VM GC capacity to game libraries.
patch('core---vm/control.cpp', '    return 16 * 1024 * 1024;',
      '    return 1 << PXT_VM_HEAP_ALLOC_BITS;')

# Cartridge source maps are intentionally excluded from the writable VM image.
# Upstream searches 512 bytes beyond dataEnd while reporting a panic.
path = destination / 'core---vm/vm.cpp'
text = path.read_text()
a = text.index('static const uint8_t *find_src_map() {')
b = text.index('static const uint8_t *decode_num', a)
path.write_text(text[:a] + 'static const uint8_t *find_src_map() { return NULL; }\n\n' + text[b:])

# Preserve mtimes so incremental configure does not rebuild every vendor TU.
output.mkdir(parents=True, exist_ok=True)
for file in destination.rglob('*'):
    if not file.is_file(): continue
    target = output / file.relative_to(destination)
    if not target.exists() or target.read_bytes() != file.read_bytes():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file, target)
for file in output.rglob('*'):
    if file.is_file() and not (destination / file.relative_to(output)).exists(): file.unlink()
temporary.cleanup()
