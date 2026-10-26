/* SPDX-License-Identifier: GPL-2.0-or-later
 * SDL Sopwith includes setjmp.h, but the pinned core calls neither function
 * and uses no jmp_buf. Do not enable unsupported WebAssembly EH for this
 * unused header. Any future real use must fail compilation for review.
 */
#ifndef SOPWITH_PXA_UNUSED_SETJMP_H
#define SOPWITH_PXA_UNUSED_SETJMP_H
#define setjmp(...) SOPWITH_PXA_SETJMP_IS_UNSUPPORTED
#define longjmp(...) SOPWITH_PXA_LONGJMP_IS_UNSUPPORTED
#endif
