# Standalone SAF source and platform notice

Original SAF, microTD, Chess, smallchesslib 0.7d, embedded bitmap fonts/images/sprites and original callbacks are by drummyfish, CC0 1.0. Full license is vendor/original/LICENSE. Fixed ESPboy-edu mirror commit: ac59afbc9a906a022f1161828e9c9a32c9b3814a. All five originals are retained byte-for-byte.

MIT adapter files have their full LICENSE.MIT. New platform/header branch implements only documented SAF_FE callbacks; original Chess ESPboy preset, MAX_AI_LEVEL5, record128 and game/library bytes are retained. The ESPboy watchdog callback is a hardware no-op. No Arduino or PC frontend is executed.

microTD has two transparent safety corrections: actual16-column aura geometry and bounded descending spawn range after original clamps. Exact diff: utd-safety.patch. The original failures and independent native comparisons are retained in research/utd-feasibility-20261008T134814Z-886ed2c7. These are not replacement rules.

Profile is silent, with32 session cache bytes for microTD and no disk persistence. Chess originally disables saving. Original0..5 synchronous Chess AI can delay rendering; original simplified chess semantics and128-ply records are retained, not represented as complete FIDE implementation. Original END auto-replay remains active. No hardware verification is claimed.

This basis is independent of previously delivered SAF minigames and all51 prior packages. Entry1/3 here are adapter identities for standalone microTD/Chess, not legacy MINE/2048 dispatcher selections.
