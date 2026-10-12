# Source and changes

Copied from `VanillaNahida/lvgl_pinyin_ime` commit
`037169998150a768ff43863fc44d563222d4b801` (2026-10-11), Apache-2.0:

- `src/core/ime_engine_port.cpp` → `core/ime_engine_port.cpp`
- `include/lvgl_pinyin_ime/ime_engine.h` → `include/lvgl_pinyin_ime/ime_engine.h`
- `src/core/ime_t9.{c,h}`, `ime_trie.{c,h}` → `core/`
- `src/generated/ime_t9_syllables.h` → `generated/`
- `src/third_party/libgooglepinyin` → `third_party/libgooglepinyin`
- The generated dictionary from the upstream licensed word list → `assets/dict_pinyin.dat`.

The decoder upstream revision is recorded in `libgooglepinyin/UPSTREAM_REVISION.txt`.
`libgooglepinyin/UPSTREAM.md` is retained as historical upstream documentation;
its “unchanged except P0” claim is outdated: the source already includes P9
explicit-width dictionary serialization. We retain that portability patch.

PXA changes:

- `core/ime_t9.h`: 24 digits, 32 spelling results, 64 bytes per reading; no large
  default candidate cache. `core/ime_t9.c`: bounded DFS (256 nodes per pass),
  and partial-tail segmentation after a complete first syllable.
- `libgooglepinyin/src/userdict.cpp`: include the PXA single-owner mutex shim;
  calls to ambient POSIX file mutation are redirected to unsupported stubs in
  Wasm. Native builds preserve their original POSIX calls.
- Force-include `port/guest_stdio.h` in decoder sources only. Runtime dictionary
  reads/seek use the Host-provided private file handle; no filesystem preopen,
  network access or raw model copy in the Guest.
- `port/single_owner_mutex.h`: no-op mutex on the serialized single-owner Wasm
  instance, native pthread elsewhere. It is not safe for a future concurrent
  decoder instance unless this shim is replaced.
- Only 15 runtime decoder files are linked. Offline builder sources remain for
  traceability; model generation uses a clean, pinned upstream checkout.
- `provider.cpp`, `session.hpp`, `main.cpp`, `port/`, CMake/package files and tests
  are the PXA integration, replacing upstream editor pointers/global UI session.

All original per-file notices are retained. See root `LICENSE` and `NOTICE`.
