// MIT. Included inside the vendored scheduler namespace to use its fiber ABI.
// Advance time once, then give each runnable fiber a bounded turn. A busy
// script retains its bytecode PC and cannot monopolize the PXA event callback.
static FiberContext *pxaNextFiber;
extern bool pxaResetRequested;
unsigned pxaFrameInstructions;
static void mainRunLoop() {
    mcaMainLoopLive = 1;
    if (!mca_frame_requested()) return;
    ++g_loops;
    g_virtTimeNum += MCA_FRAME_STEP;
    pxaFrameInstructions = 131072;
    wakeFibers();
    auto f = pxaNextFiber ? pxaNextFiber : allFibers;
    const auto now = current_time_ms();
    for (unsigned dispatched = 0; f && dispatched < 128 && pxaFrameInstructions; ++dispatched) {
        if (f->wakeTime && (uint64_t)now >= f->wakeTime) f->wakeTime = 0;
        if (!f->wakeTime && !f->waitSource) {
            if (currentFiber != f) {
                saveStack();
                currentFiber = f;
                restoreStack();
            }
            f->pc = f->resumePC;
            f->resumePC = nullptr;
            if (f->wakeFn) {
                auto fn = f->wakeFn;
                f->wakeFn = nullptr;
                f->r0 = fn(f->wakeFnArg);
            }
            if (!f->wakeTime && !f->waitSource) exec_loop(f);
            if (panicCode || pxaResetRequested) return;
            wakeFibers();
            auto next = f->next;
            if (!f->resumePC) {
                if (f->foreverPC) {
                    f->resumePC = f->foreverPC;
                    f->wakeTime = now + 20;
                    for (int i = 0; i < 5; ++i)
                        if (*--f->sp == TAG_STACK_BOTTOM) break;
                    if (*f->sp != TAG_STACK_BOTTOM) target_panic(PANIC_INVALID_IMAGE);
                } else {
                    auto binding = f->handlerBinding;
                    if (binding) {
                        if (auto event = binding->pending) {
                            binding->pending = event->next;
                            setupThread(binding->action, fromInt(event->value), binding);
                            xfree(event);
                            next = f->next;
                        } else binding->flags &= ~HANDLER_RUNNING;
                    }
                    disposeFiber(f);
                }
            }
            f = next;
        } else f = f->next;
    }
    pxaNextFiber = f;
    pxt_screen_publish(g_loops);
}
