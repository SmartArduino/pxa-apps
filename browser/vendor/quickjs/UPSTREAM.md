# QuickJS

Pinned official release: https://bellard.org/quickjs/quickjs-2026-06-04.tar.xz
MIT license; see LICENSE. Only the engine is linked; no quickjs-libc, shell, OS or file APIs.

WASI portability changes are deliberately local:
- Disable CONFIG_ATOMICS for __wasi__: this Guest is single threaded.
- Treat the default allocator usable-size fallback as unknown on WASI.
  The browser uses its own size-tracking allocator, not this fallback.
- Remove dtoa.c's unused setjmp.h include. It contains no setjmp/longjmp calls.

Keep these guards when updating upstream. The browser owns limits, scheduling,
DOM and Web API policy outside vendor code.

Memory-limit regression fix: keep a temporary reference to build_backtrace's
error object. A nested OOM can replace and free current_exception while this
function is still adding its stack property. The bounded-allocation regression
reproduced a use-after-free without this patch. Also free its temporary buffer
on its filename-allocation error exits. This patch is covered by script_test.cpp.

Archive SHA-256: b376e839b322978313d929fd20663b11ba58b75df5a46c126dd19ea2fa70ad2a.

Use a correctly typed void-pointer adapter for the two DynBuf allocator
callbacks. Casting js_realloc_rt directly called an incompatible C function
type; the full-engine UBSan run reports it.

WASI uses the same switch dispatcher as upstream Emscripten builds. Native
computed-goto dispatch has no direct Wasm equivalent; the switch path is
benchmarked with the PXA AOT compiler. Native test builds keep upstream dispatch.

The private bridge bytecode strips debug/source data at build time. Website
compilation uses JS_STRIP_SOURCE to avoid retaining downloaded function source;
Function.prototype.toString consequently cannot reproduce those function bodies.

Small-stack Guest adaptation:
- The binary expression parser uses nine fixed records for precedence levels
  0–8. This removes both empty-level descent and recursive RHS parsing while
  preserving operator emission, associativity and private-in grammar. Records
  occupy 180 bytes on Wasm32's existing linear C stack; pending atom references
  are released on every failure path. A small entry parses the first unary
  operand before calling the operator tail, so ordinary nested literals do not
  retain the full kernel frame. Fixed-expression bytecode remains identical to
  the original parser. Browser bridge bytecode changes only for its intentional
  inline-event preparation. Original-page execution does not ship site helpers.
- Empty unary/logical levels use small entries; only actual operators retain
  their guarded emission kernels. Primary expressions and member/call suffixes
  use separate kernels. Blocks, returns, if and var statements do not retain
  the loop/switch/try parser frame; labels and lexical declaration checks keep
  the upstream scope rules. Explicit noinline on the kernels is necessary:
  full LTO otherwise folds them back together and defeats the stack separation.
- Bytecode keeps the common dispatch loop; cold opcode kernels (references,
  classes, construction, iteration, arithmetic, with and eval) are out of line.
  Their state is an explicit stack/PC/result tuple with no heap cache. This
  reduces the retained FAST interpreter frame from 1184 to 632 bytes in the
  pinned desktop build; the module's prepared-code allocation grows, so this
  is stack headroom rather than a claim of lower total resident memory.
- WASI nesting is a bounded resource policy for a 16 KiB operand stack:
  bytecode costs one shared unit and has a separate 12-layer ceiling; ordinary
  native callbacks cost four units, and bound/call/apply forwarding costs two,
  with a shared limit of 28. Native classification runs only on native calls.
  Retained accessors and constructors cost two units with independent ceilings
  of five and six; call/apply/bind forwarding has a four-entry ceiling.
  Object-to-primitive reentry keeps its four-unit/24-unit guard and releases
  owned input on rejection. Primitive inputs bypass its bookkeeping. Nested
  eval costs six units because its compiler/interpreter entries remain live;
  top-level C/API compilation does not pay that reentry cost.
  Native depth uses uint16_t, other bounded counters uint8_t, fitting the
  existing JSRuntime padding (768 bytes in the pinned Wasm32 build).
- Parser/JSON operations use a shared weighted budget of 184, including
  twelve units per active bytecode layer. The weights live at the retained
  syntax/kernel entry, not every empty grammar level. Mixed syntax requires
  its own checks: logical tails cost six, constructor primary parsing eight,
  destructuring twelve, templates/classes ten and function bodies five/eight
  depending on the retained entry. The budget and bytecode ceilings are
  application policy, not ECMAScript limits or exact stack-byte accounting.
  Repeat actual Wasm recovery tests when changing compiler/LTO, frame layout,
  parser structure or any weight; native sanitizer stack sizes are different.
  The bookkeeping helper remains out of line to avoid inflating each parser's
  WAMR local/constant frame. No runtime or page-state object grows.
- Parenthesis lookahead recognizes possible arrow-parameter starts before
  scanning a whole expression. Its numeric tokens validate spelling, suffixes
  and separators without allocating/converting discarded Number/BigInt values.
  Normal parsing and ToNumber/parseInt/parseFloat retain numeric conversion;
  conversion itself is an out-of-line kernel. Numeric and arrow-grammar
  regressions are in tests/language_cases.js.
- Parser/JSON guards unwind before constructing the stack-overflow exception.
  A cleanup guard restores nesting on every return and creates a fresh ordinary
  InternalError when the guarded operation exits. A linked guard stack detects
  intervening native/bytecode calls and context changes, so an inner parse error
  is thrown at its own JS boundary and can be caught inside a getter/reviver. This avoids a large
  allocator/error stack at the exhaustion point; errors retain their prototype,
  identity and message instead of sharing a mutable cached object. JSON failure
  paths retain their original value ownership and parse-record initialization.
- The lexer charges its work to the existing interrupt counter (roughly one
  callback per 157 tokens), including speculative parenthesis lookahead. The
  compiler also polls before variable resolution, label optimization and stack
  analysis. On WASI, an interrupt is recorded in the active recursion guard and
  turned into an uncatchable error after that operation unwinds. A one-shot
  cancellation must not be swallowed by speculative parsing. This reuses the
  guard's failure slot and adds no runtime fields. Cancellation/recovery tests
  cover every callback position with persistent and one-shot handlers, including
  partially compiled closures, captured names and direct eval. Individual
  compiler passes/native operations remain synchronous: these checks are not a
  strict time bound or resumable compiler.
- Coalesce/conditional and thin expression wrappers use upstream force_inline;
  logical parsing skips absent && levels. Short-circuit emission is unchanged.
- Native/bound calls enter a small JS_CallInternal dispatcher; only bytecode
  calls allocate the large interpreter frame. Polling, flags, receiver,
  arguments, generators and exception paths retain upstream behavior.
- WASI stack-overflow and interrupt Errors have an empty stack property: printf/backtrace
  construction at an exhausted stack caused native-task traps. The message and
  InternalError prototype are preserved. The linear-stack limit is separate
  from WAMR's operand stack and the Host task stack.

Generic array-copy optimization:
- Prove an ordinary indexed-data array/prototype chain before omitting long
  hole-to-hole tails. Indexed accessors/prototypes, exotic objects, uint32
  boundaries and unsafe overlap directions fall back. Retained operations use
  upstream Set/Delete semantics, including partial writes before TypeError.
  No allocation or cache; the prototype-depth cutoff only selects fallback.
- Force cancellation checks every 128 generic / 256 dense copied entries.
  Tests cover dense forward/backward and array-like cancellation/recovery in
  native sanitizers and actual 16KiB WAMR. Other native loops remain synchronous.
- Page now explicitly registers Proxy. Eleven borrowed-argument exotic property
  entries charge four native units against the existing 28-unit ceiling;
  proxy call/construct paths already use their call/constructor guards.
  Strong recursion tests require InternalError/stack overflow and actual trap
  entry, so missing Proxy cannot pass as a protected recursive call.

Verification when rebasing:
- `PXA_SCRIPT_TEST_WASI_MODEL=1 PXA_SCRIPT_TEST_INTERPRETED=1 tests/test_script.sh`
  exercises WASI branches and the switch dispatcher under native sanitizers.
  Native sanitizer frame sizes differ from the actual interpreter.
- `tests/parser_smoke.py --runtime` runs generated deep expressions, blocks, functions,
  arrays, objects, classes, templates, destructuring and JSON in real PXA Wasm,
  requiring an ordinary JS error, continued script execution and no Guest trap.
  It also covers recursive accessors, callbacks, proxies, coercion and iterators,
  plus mixed parser nesting, constructor/accessor/eval combinations, call/apply
  reentry, numeric/arrow grammar and fresh-error recovery.
- Keep the mixed-precedence fixture in script_smoke.py and the native tests.
- Compare bytecode for fixed expressions and the trusted runtime module; run
  pinned upstream test_language.js, test_closure.js and test_loop.js. The native
  engine retains the original grammar and dispatch outside WASI-specific guards.
