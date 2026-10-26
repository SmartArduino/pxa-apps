/* SPDX-License-Identifier: MIT.
 * QuickJS 2026-06-04 dtoa.c includes setjmp.h but never uses jmp_buf,
 * setjmp, or longjmp. WASI's header refuses even this unused include unless
 * experimental exception handling is enabled. Keep the upstream C bytes
 * unchanged and supply an empty header for this unused dependency only.
 * Intentionally provides no fake setjmp/longjmp implementation. */
#ifndef PXA_WHATAJONG_UNUSED_SETJMP_H
#define PXA_WHATAJONG_UNUSED_SETJMP_H
#endif
