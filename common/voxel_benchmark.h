/* Deterministic, allocation-free C/C++ renderer workload. Normal games exclude
 * every benchmark branch when VOXEL_BENCH_SCENE is zero. The field is bounded
 * equally in both implementations, including outside resident C chunks. */
#ifndef PXA_VOXEL_BENCHMARK_H
#define PXA_VOXEL_BENCHMARK_H
#include <stdint.h>
#ifndef VOXEL_BENCH_SCENE
#define VOXEL_BENCH_SCENE 0
#endif
#define VOXEL_BENCH_SEED 0x5ae1u
#define VOXEL_BENCH_FOV 1.221451928f /* 2 atan(0.70) */
#define VOXEL_BENCH_NEAR 0.25f
#define VOXEL_BENCH_FAR 24.0f
#define VOXEL_BENCH_EYE_Y 9.60f
#define VOXEL_BENCH_X (VOXEL_BENCH_SCENE == 2 ? 18.5f : 32.5f)
#define VOXEL_BENCH_Z 18.5f
#define VOXEL_BENCH_YAW (VOXEL_BENCH_SCENE == 2 ? 0.7f : 0.0f)
#define VOXEL_BENCH_PITCH (VOXEL_BENCH_SCENE == 2 ? -0.15f : 0.0f)
static inline uint8_t voxel_benchmark_block(int x, int y, int z) {
    if ((unsigned)x >= 64 || (unsigned)y >= 24 || (unsigned)z >= 64) return 0;
    if (y <= 7) return y == 0 ? 19 : y < 5 ? 3 : y < 7 ? 2 : 1;
    const int tx = ((x + 4) / 8) * 8, tz = ((z + 4) / 8) * 8;
    if (tx < 8 || tx > 56 || tz < 8 || tz > 56) return 0;
    const int top = 11 + (int)(((unsigned)tx * 73856093u ^
        (unsigned)tz * 19349663u ^ VOXEL_BENCH_SEED) & 1u);
    if (x == tx && z == tz && y <= top) return 5;
    const int dx = x - tx, dz = z - tz, dy = y - top;
    return dy >= -1 && dy <= 2 && dx*dx + dz*dz + dy*dy <= 6 ? 6 : 0;
}
#endif
