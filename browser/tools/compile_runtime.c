// Build-time tool only. Bytecode is private to this pinned QuickJS version.
#include "quickjs.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f || fseek(f, 0, SEEK_END)) return 2;
    long size = ftell(f); rewind(f);
    if (size <= 0 || size > 32768) return 2;
    char *source = malloc((size_t)size + 1);
    if (!source || fread(source, 1, (size_t)size, f) != (size_t)size) return 2;
    fclose(f); source[size] = 0;
    JSRuntime *rt = JS_NewRuntime(); JS_SetStripInfo(rt, argc==4 ? JS_STRIP_SOURCE : JS_STRIP_DEBUG); JSContext *ctx = JS_NewContextRaw(rt);
    JS_AddIntrinsicBaseObjects(ctx); JS_AddIntrinsicEval(ctx); JS_AddIntrinsicRegExpCompiler(ctx);
    JSValue fun = JS_Eval(ctx, source, (size_t)size, argc==4?argv[3]:"pxa-browser-runtime.js", JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    free(source);
    if (JS_IsException(fun)) { JSValue e=JS_GetException(ctx); const char *text=JS_ToCString(ctx,e); fprintf(stderr,"%s\n",text); return 1; }
    size_t count = 0;
    uint8_t *bytes = JS_WriteObject(ctx, &count, fun, JS_WRITE_OBJ_BYTECODE);
    if (!bytes) return 1;
    f = fopen(argv[2], "wb");
    if (!f || fwrite("PXAQJS1\n", 1, 8, f) != 8 || fwrite(bytes, 1, count, f) != count || fclose(f)) return 2;
    printf("runtime bytecode: %zu bytes\n", count + 8);
    js_free(ctx, bytes); JS_FreeValue(ctx, fun); JS_FreeContext(ctx); JS_FreeRuntime(rt);
    return 0;
}
