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
- WASI tracks shared nesting units. Native/interpreter calls cost two units
  (entry limit 24); bytecode calls also have an independent eight-layer limit.
  Parser assignment costs four, unary/binary kernels three, primary/postfix
  entries and suffixes three, complex statements six, returns/if three,
  var statements four and blocks two. Function/class/destructuring cost four,
  templates three, parentheses two, logical tails three and active conditional
  branches one. Empty operator levels retain no operator guard.
  JSON parsing costs three; reviver/stringify traversal four. The shared
  recursive-entry budget is 88, including eight additional units per active
  bytecode layer. Weights are calibrated against WASI SDK 34 FAST-interpreter
  frames, roughly 128 bytes per unit; they are conservative resource policy,
  not exact byte measurements or ECMAScript limits. Always repeat actual Wasm
  tests when changing optimization, compiler or parser structure.
  The bookkeeping helper is deliberately out of line: copying it into every
  parser entry needlessly inflates WAMR local/constant frames.
- Parser/JSON guards unwind before constructing the stack-overflow exception.
  A cleanup guard restores nesting on every return and creates a fresh ordinary
  InternalError when the guarded operation exits. A linked guard stack detects
  intervening native/bytecode calls and context changes, so an inner parse error
  is thrown at its own JS boundary and can be caught inside a getter/reviver. This avoids a large
  allocator/error stack at the exhaustion point; errors retain their prototype,
  identity and message instead of sharing a mutable cached object. JSON failure
  paths retain their original value ownership and parse-record initialization.
- Coalesce/conditional and thin expression wrappers use upstream force_inline;
  logical parsing skips absent && levels. Short-circuit emission is unchanged.
- Native/bound calls enter a small JS_CallInternal dispatcher; only bytecode
  calls allocate the large interpreter frame. Polling, flags, receiver,
  arguments, generators and exception paths retain upstream behavior.
- A WASI stack-overflow Error has an empty stack property: printf/backtrace
  construction at an exhausted stack caused native-task traps. The message and
  InternalError prototype are preserved. The linear-stack limit is separate
  from WAMR's operand stack and the Host task stack.

Verification when rebasing:
- `PXA_SCRIPT_TEST_WASI_MODEL=1 PXA_SCRIPT_TEST_INTERPRETED=1 tests/test_script.sh`
  exercises WASI branches and the switch dispatcher under native sanitizers.
  Native sanitizer frame sizes differ from the actual interpreter.
- `tests/parser_smoke.py` runs generated deep expressions, blocks, functions,
  arrays, objects, classes, templates, destructuring and JSON in real PXA Wasm,
  requiring an ordinary JS error, continued script execution and no Guest trap.
- Keep the mixed-precedence fixture in script_smoke.py and the native tests.
- Compare bytecode for fixed expressions and the trusted runtime module; run
  pinned upstream test_language.js, test_closure.js and test_loop.js. The native
  engine retains the original grammar and dispatch outside WASI-specific guards.
