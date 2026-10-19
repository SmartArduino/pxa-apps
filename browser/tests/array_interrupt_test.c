#include "quickjs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct Budget { unsigned calls; int stop; };
static int interrupt(JSRuntime *runtime, void *opaque)
{
    (void)runtime;
    struct Budget *budget = opaque;
    ++budget->calls;
    return budget->stop;
}

static void evaluate(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "setup.js", 0);
    assert(!JS_IsException(value));
    JS_FreeValue(ctx, value);
}

int main(void)
{
    const char *setups[] = {
        "var a=Array(4096).fill(1);",
        "var a=Array(4096).fill(1);",
        "var a={length:4096,4095:1};",
    };
    const char *copies[] = {
        "a.copyWithin(0,1)", "a.copyWithin(1,0)",
        "Array.prototype.copyWithin.call(a,0,1)",
    };
    for (unsigned i=0; i<sizeof(setups)/sizeof(*setups); ++i) {
        JSRuntime *runtime = JS_NewRuntime(); assert(runtime);
        JS_SetMemoryLimit(runtime, 4*1024*1024);
        JS_SetMaxStackSize(runtime, sizeof(void*)==4 ? 16384 : 2*1024*1024);
        JSContext *ctx = JS_NewContext(runtime); assert(ctx);
        evaluate(ctx, setups[i]);
        JSValue code = JS_Eval(ctx, copies[i], strlen(copies[i]), "copy.js",
                               JS_EVAL_FLAG_COMPILE_ONLY);
        assert(!JS_IsException(code));
        /* Warm the VM's ordinary interrupt counter. Cancellation must then be
           observed inside the native copy loop, without 10,000 VM polls. */
        struct Budget budget = {0, 0};
        JS_SetInterruptHandler(runtime, interrupt, &budget);
        evaluate(ctx, "for(var j=0;j<15000;++j){}");
        assert(budget.calls);
        budget.calls=0; budget.stop=1;
        JSValue value = JS_EvalFunction(ctx, code);
        assert(JS_IsException(value) && budget.calls==1);
        JSValue error = JS_GetException(ctx);
        const char *message = JS_ToCString(ctx, error);
        assert(message && strstr(message, "interrupted"));
        JS_FreeCString(ctx, message); JS_FreeValue(ctx, error);
        JS_FreeValue(ctx, value);
        JS_SetUncatchableException(ctx, 0);
        JS_SetInterruptHandler(runtime, NULL, NULL);
        evaluate(ctx, "if(1+2!==3)throw Error('recovery');a=null;");
        JS_FreeContext(ctx); JS_FreeRuntime(runtime);
    }
    puts("native array-copy cancellation and same-runtime recovery PASS");
}
