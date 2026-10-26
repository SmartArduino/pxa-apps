/* SPDX-License-Identifier: MIT.
 * Use QuickJS's existing single-thread WebAssembly configuration locally.
 * Its original source stays byte-identical to the official release.
 */
#define __EMSCRIPTEN__ 1
#include "quickjs/quickjs.c"
