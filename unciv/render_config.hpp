// SPDX-License-Identifier: MIT
#pragma once
// Preserve the uncached renderer for comparisons and oversized-sprite fallback.
#if !defined(UNCIV_HOST_BLIT) && !defined(UNCIV_REFERENCE_BLIT)
#ifndef UNCIV_SCANLINE_BLIT
#define UNCIV_SCANLINE_BLIT 1
#endif
#endif
