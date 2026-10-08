// Block table, value-noise terrain generation and voxel ray casting.
#include "voxel_world.hpp"

#include <cmath>
#include <cstring>

namespace voxel {
namespace {

/* b00..b44 are ordered as (block - 1) * 3 + face for blocks 1..15. */
constexpr int kFaceDirW[6][3] = {{0, 1, 0},  {1, 0, 0},  {0, 0, -1},
                                  {0, -1, 0}, {-1, 0, 0}, {0, 0, 1}};

constexpr std::uint8_t slot(int block, int face) noexcept {
    return static_cast<std::uint8_t>((block - 1) * 3 + face);
}


constexpr BlockInfo kBlocks[kBlockCount] = {
    {"air", {0, 0, 0}, false, false, false},
    {"grass", {slot(1, 0), slot(1, 1), slot(1, 2)}, true, true, false},
    {"dirt", {slot(2, 0), slot(2, 1), slot(2, 2)}, true, true, false},
    {"stone", {slot(3, 0), slot(3, 1), slot(3, 2)}, true, true, false},
    {"sand", {slot(4, 0), slot(4, 1), slot(4, 2)}, true, true, false},
    {"wood", {slot(5, 0), slot(5, 1), slot(5, 2)}, true, true, false},
    {"leaves", {slot(6, 0), slot(6, 1), slot(6, 2)}, true, false, true},
    {"water", {slot(7, 0), slot(7, 1), slot(7, 2)}, false, false, true},
    {"plank", {slot(8, 0), slot(8, 1), slot(8, 2)}, true, true, false},
    {"brick", {slot(9, 0), slot(9, 1), slot(9, 2)}, true, true, false},
    {"glass", {slot(10, 0), slot(10, 1), slot(10, 2)}, true, false, true},
    {"cobble", {slot(11, 0), slot(11, 1), slot(11, 2)}, true, true, false},
    {"table", {slot(12, 0), slot(12, 1), slot(12, 2)}, true, true, false},
    {"snow", {slot(13, 0), slot(13, 1), slot(13, 2)}, true, true, false},
    {"gravel", {slot(14, 0), slot(14, 1), slot(14, 2)}, true, true, false},
    {"cactus", {slot(15, 0), slot(15, 1), slot(15, 2)}, true, true, false},
    {"bush", {slot(6, 0), slot(6, 1), slot(6, 2)}, false, false, true},
    {"flower", {slot(6, 0), slot(6, 1), slot(6, 2)}, false, false, true},
    {"wool", {slot(13, 0), slot(13, 1), slot(13, 2)}, true, true, false},
    {"bedrock", {slot(3, 0), slot(3, 1), slot(3, 2)}, true, true, false},
};

std::uint32_t hash_u32(std::uint32_t value) noexcept {
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

float hash_unit(int x, int z, std::uint32_t seed) noexcept {
    const std::uint32_t h = hash_u32(static_cast<std::uint32_t>(x) * 0x1000193U ^
                                     static_cast<std::uint32_t>(z) * 0x9e3779b9U ^
                                     seed);
    return static_cast<float>(h & 0xffffU) / 65535.0f;
}

float smooth(float t) noexcept { return t * t * (3.0f - 2.0f * t); }

/* Bilinear value noise in [0, 1]. */
float value_noise(float x, float z, std::uint32_t seed) noexcept {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const float tx = smooth(x - static_cast<float>(x0));
    const float tz = smooth(z - static_cast<float>(z0));
    const float a = hash_unit(x0, z0, seed);
    const float b = hash_unit(x0 + 1, z0, seed);
    const float c = hash_unit(x0, z0 + 1, seed);
    const float d = hash_unit(x0 + 1, z0 + 1, seed);
    const float top = a + (b - a) * tx;
    const float bottom = c + (d - c) * tx;
    return top + (bottom - top) * tz;
}

bool likely_tree(int x, int z, std::uint32_t seed) noexcept {
    return hash_unit(x * 7 + 3, z * 13 + 5, seed ^ 0x5bf03635U) > 0.94f;
}

}  // namespace

const BlockInfo& block_info(std::uint8_t id) noexcept {
    return kBlocks[id < kBlockCount ? id : 0];
}

const std::uint8_t kHotbarBlocks[] = {
    kGrass, kDirt, kStone, kSand, kWood, kLeaves, kPlank, kBrick, kGlass,
    kCobble,
};
const int kHotbarCount = static_cast<int>(sizeof(kHotbarBlocks) /
                                          sizeof(kHotbarBlocks[0]));

void World::generate(std::uint32_t seed) noexcept {
    blocks_.fill(kAir);
    for (int x = 0; x < kWorldX; ++x) {
        for (int z = 0; z < kWorldZ; ++z) {
            const float fx = static_cast<float>(x);
            const float fz = static_cast<float>(z);
            const float hills = value_noise(fx * 0.075f, fz * 0.075f, seed);
            const float detail = value_noise(fx * 0.21f, fz * 0.21f, seed + 991U);
            const float biome = value_noise(fx * 0.035f, fz * 0.035f, seed + 77U);
            int height = 6 + static_cast<int>(hills * 7.0f + detail * 2.0f);
            if (height < 4) height = 4;
            if (height > kWorldY - 6) height = kWorldY - 6;

            const bool desert = biome < 0.36f;
            const bool snowy = biome > 0.72f;
            std::uint8_t surface = kGrass;
            std::uint8_t filler = kDirt;
            if (desert) {
                surface = kSand;
                filler = kSand;
            } else if (snowy) {
                surface = kSnow;
            } else if (height <= kSeaLevel + 1) {
                surface = kSand;
            }

            for (int y = 0; y <= height; ++y) {
                std::uint8_t id = kStone;
                if (y == 0)
                    id = kBedrock;
                else if (y == height)
                    id = surface;
                else if (y >= height - 3)
                    id = filler;
                else if (y > height - 6 && hash_unit(x + y, z - y, seed) >
                                              0.82f)
                    id = kGravel;
                set(x, y, z, id);
            }
            for (int y = height + 1; y <= kSeaLevel; ++y) set(x, y, z, kWater);
        }
    }

    // Finish all terrain before vegetation: a later column must not overwrite
    // a crown that crossed into it. Recover the ground height from its surface
    // material so this pass needs no extra height map or world-sized storage.
    for (int x = 0; x < kWorldX; ++x) {
        for (int z = 0; z < kWorldZ; ++z) {
            int height = 0;
            for (int y = kWorldY - 1; y >= 0; --y) {
                const auto id = at(x, y, z);
                if (id == kGrass || id == kSnow || id == kSand) {
                    height = y;
                    break;
                }
            }
            const float biome = value_noise(static_cast<float>(x) * 0.035f,
                                            static_cast<float>(z) * 0.035f,
                                            seed + 77U);
            const bool desert = biome < 0.36f;
            const bool snowy = biome > 0.72f;

            if (!desert && !snowy && height > kSeaLevel && likely_tree(x, z, seed) &&
                x > 2 && x < kWorldX - 3 && z > 2 && z < kWorldZ - 3) {
                const int trunk = 3 + static_cast<int>(hash_unit(x, z, seed ^ 21U) *
                                                       2.0f);
                const int top = height + trunk;
                for (int y = height + 1; y <= top && y < kWorldY - 2; ++y)
                    set(x, y, z, kWood);
                for (int dx = -2; dx <= 2; ++dx) {
                    for (int dz = -2; dz <= 2; ++dz) {
                        for (int dy = -2; dy <= 1; ++dy) {
                            const int lx = x + dx;
                            const int ly = top + dy;
                            const int lz = z + dz;
                            if (!in_bounds(lx, ly, lz)) continue;
                            if (dx * dx + dz * dz + dy * dy > 6) continue;
                            if (at(lx, ly, lz) != kAir) continue;
                            set(lx, ly, lz, kLeaves);
                        }
                    }
                }
            }
            if (desert && height > kSeaLevel && likely_tree(x, z, seed ^ 0x33U) &&
                x > 1 && x < kWorldX - 2 && z > 1 && z < kWorldZ - 2) {
                const int trunk = 2 + static_cast<int>(hash_unit(x, z, seed) * 2.0f);
                for (int y = height + 1; y <= height + trunk; ++y)
                    set(x, y, z, kCactus);
            }
            if (!desert && !snowy && height > kSeaLevel &&
                at(x, height, z) == kGrass) {
                const float scatter = hash_unit(x * 3 + 1, z * 5 + 2, seed ^ 0x99U);
                if (scatter > 0.93f && in_bounds(x, height + 1, z) &&
                    at(x, height + 1, z) == kAir)
                    set(x, height + 1, z,
                        scatter > 0.965f ? kFlower : kBush);
            }
        }
    }
}

std::uint8_t World::at(int x, int y, int z) const noexcept {
    if (!in_bounds(x, y, z)) return kAir;
    return blocks_[static_cast<std::size_t>((y * kWorldZ + z) * kWorldX + x)];
}

void World::set(int x, int y, int z, std::uint8_t id) noexcept {
    if (!in_bounds(x, y, z)) return;
    const auto index = static_cast<std::size_t>((y * kWorldZ + z) * kWorldX + x);
    if (blocks_[index] == id) return;
    blocks_[index] = id;
    mark_dirty(x, y, z);
}

bool World::face_visible(int x, int y, int z, int face) const noexcept {
    const std::uint8_t id = at(x, y, z);
    if (id == kAir) return false;
    const auto& info = block_info(id);
    if (!info.solid && !info.cutout) return false;
    const std::uint8_t neighbor =
        at(x + kFaceDirW[face][0], y + kFaceDirW[face][1],
           z + kFaceDirW[face][2]);
    if (block_info(neighbor).opaque) return false;
    if (neighbor == id && (id == kWater || info.cutout)) return false;
    return true;
}

void World::mark_dirty(int x, int y, int z) noexcept {
    const int cx = x / kChunkSize;
    const int cy = y / kChunkSize;
    const int cz = z / kChunkSize;
    /* A face on a chunk border depends on the neighbouring chunk too. */
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                const int nx = cx + dx;
                const int ny = cy + dy;
                const int nz = cz + dz;
                if (nx < 0 || nx >= kChunksX || ny < 0 || ny >= kChunksY ||
                    nz < 0 || nz >= kChunksZ)
                    continue;
                meshes_[static_cast<std::size_t>((ny * kChunksZ + nz) * kChunksX + nx)]
                    .built = false;
            }
        }
    }
}

/* Greedy meshing: one rectangle per run of coplanar, same-material faces.
 * The mask holds slot + 1 so that 0 means "no face". */
void World::build_chunk(ChunkMesh& mesh, int cx, int cy, int cz) const noexcept {
    mesh.count = 0;
    mesh.overflow = false;
    generate_quads(cx, cy, cz, &mesh, [](void* context, const MeshQuad& quad) noexcept {
        auto& cached = *static_cast<ChunkMesh*>(context);
        if (cached.count < cached.quads.size()) cached.quads[cached.count++] = quad;
        else cached.overflow = true;
        return true;
    });
    mesh.built = true;
}

bool World::generate_quads(int cx, int cy, int cz, void* context,
                           QuadVisitor emit) const noexcept {
    const int base_x = cx * kChunkSize;
    const int base_y = cy * kChunkSize;
    const int base_z = cz * kChunkSize;
    std::array<std::uint16_t, kChunkSize * kChunkSize> mask{};
    for (int face = 0; face < 6; ++face) {
        /* plane axis: the face normal; u/v are the two in-plane axes. */
        const int axis = face == 0 || face == 3   ? 1
                         : face == 1 || face == 4 ? 0
                                                  : 2;
        for (int plane = 0; plane < kChunkSize; ++plane) {
            mask.fill(0);
            for (int v = 0; v < kChunkSize; ++v) {
                for (int u = 0; u < kChunkSize; ++u) {
                    int lx = base_x;
                    int ly = base_y;
                    int lz = base_z;
                    if (axis == 0)
                        lx += plane;
                    else if (axis == 1)
                        ly += plane;
                    else
                        lz += plane;
                    if (face == 1 || face == 4) {
                        lz += u;
                        ly += v;
                    } else if (face == 2 || face == 5) {
                        lx += u;
                        ly += v;
                    } else {
                        lx += u;
                        lz += v;
                    }
                    if (!in_bounds(lx, ly, lz)) continue;
                    if (!face_visible(lx, ly, lz, face)) continue;
                    const std::uint8_t id = at(lx, ly, lz);
                    const auto& info = block_info(id);
                    const int slot = info.texture[face == 0   ? kFaceTop
                                                  : face == 3 ? kFaceBottom
                                                              : kFaceSide];
                    mask[v * kChunkSize + u] =
                        static_cast<std::uint16_t>(1 + slot +
                                                   (info.cutout ? 128 : 0));
                }
            }
            for (int v = 0; v < kChunkSize; ++v) {
                for (int u = 0; u < kChunkSize;) {
                    const std::uint16_t value = mask[v * kChunkSize + u];
                    if (value == 0) {
                        ++u;
                        continue;
                    }
                    // Perspective-correct projection allows a full chunk to
                    // merge without stretching its repeated texture.
                    constexpr int kMaxMerge = kChunkSize;
                    int width = 1;
                    while (u + width < kChunkSize && width < kMaxMerge &&
                           mask[v * kChunkSize + u + width] == value)
                        ++width;
                    int height = 1;
                    while (v + height < kChunkSize && height < kMaxMerge) {
                        bool row = true;
                        for (int k = 0; k < width; ++k) {
                            if (mask[(v + height) * kChunkSize + u + k] != value) {
                                row = false;
                                break;
                            }
                        }
                        if (!row) break;
                        ++height;
                    }
                    {
                        MeshQuad quad;
                        quad.face = static_cast<std::uint8_t>(face);
                        quad.plane = static_cast<std::uint8_t>(plane);
                        quad.u = static_cast<std::uint8_t>(u);
                        quad.v = static_cast<std::uint8_t>(v);
                        quad.width = static_cast<std::uint8_t>(width);
                        quad.height = static_cast<std::uint8_t>(height);
                        quad.slot = static_cast<std::uint8_t>((value - 1) & 0x3f);
                        quad.cutout = (value & 128) ? 1u : 0u;
                        if (!emit(context, quad)) return false;
                    }
                    for (int dv = 0; dv < height; ++dv)
                        for (int du = 0; du < width; ++du)
                            mask[(v + dv) * kChunkSize + u + du] = 0;
                    u += width;
                }
            }
        }
    }
    return true;
}

const ChunkMesh& World::chunk_mesh(int cx, int cy, int cz) const noexcept {
    static const ChunkMesh empty{};
    if (cx < 0 || cx >= kChunksX || cy < 0 || cy >= kChunksY || cz < 0 ||
        cz >= kChunksZ)
        return empty;
    auto& mesh =
        meshes_[static_cast<std::size_t>((cy * kChunksZ + cz) * kChunksX + cx)];
    if (!mesh.built) build_chunk(mesh, cx, cy, cz);
    return mesh;
}

const ChunkMesh& World::chunk_mesh_page(int cx, int cy, int cz,
                                       std::uint32_t skip) const noexcept {
    if (!skip) return chunk_mesh(cx, cy, cz);
    if (cx < 0 || cy < 0 || cz < 0 || cx >= kChunksX || cy >= kChunksY || cz >= kChunksZ) {
        return chunk_mesh(-1, -1, -1);
    }
    auto& mesh = meshes_[static_cast<std::size_t>((cy * kChunksZ + cz) * kChunksX + cx)];
    mesh.count = 0;
    mesh.overflow = false;
    mesh.built = false;
    struct Page {ChunkMesh& mesh; std::uint32_t skip;};
    Page page{mesh, skip};
    generate_quads(cx, cy, cz, &page, [](void* context, const MeshQuad& quad) noexcept {
        auto& page = *static_cast<Page*>(context);
        if (page.skip) {--page.skip; return true;}
        if (page.mesh.count == page.mesh.quads.size()) {
            page.mesh.overflow = true;
            return false;
        }
        page.mesh.quads[page.mesh.count++] = quad;
        return true;
    });
    return mesh;
}

bool World::solid(int x, int y, int z) const noexcept {
    return block_info(at(x, y, z)).solid;
}

bool World::opaque(int x, int y, int z) const noexcept {
    return block_info(at(x, y, z)).opaque;
}

int World::highest_block(int x, int z) const noexcept {
    for (int y = kWorldY - 1; y >= 0; --y) {
        const std::uint8_t id = at(x, y, z);
        if (id != kAir && block_info(id).solid) return y;
    }
    return 0;
}

RayHit raycast(const World& world, float ox, float oy, float oz, float dx,
               float dy, float dz, float max_distance) noexcept {
    RayHit hit;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (length <= 0.0001f) return hit;
    dx /= length;
    dy /= length;
    dz /= length;

    int x = static_cast<int>(std::floor(ox));
    int y = static_cast<int>(std::floor(oy));
    int z = static_cast<int>(std::floor(oz));
    const int step_x = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    const int step_y = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    const int step_z = dz > 0 ? 1 : (dz < 0 ? -1 : 0);
    const float inf = 1.0e30f;
    const float t_delta_x = step_x ? std::fabs(1.0f / dx) : inf;
    const float t_delta_y = step_y ? std::fabs(1.0f / dy) : inf;
    const float t_delta_z = step_z ? std::fabs(1.0f / dz) : inf;
    float t_max_x = step_x ? (step_x > 0 ? (std::floor(ox) + 1.0f - ox)
                                         : (ox - std::floor(ox)))
                           : inf;
    t_max_x = step_x ? t_max_x * t_delta_x : inf;
    float t_max_y = step_y ? (step_y > 0 ? (std::floor(oy) + 1.0f - oy)
                                         : (oy - std::floor(oy)))
                           : inf;
    t_max_y = step_y ? t_max_y * t_delta_y : inf;
    float t_max_z = step_z ? (step_z > 0 ? (std::floor(oz) + 1.0f - oz)
                                         : (oz - std::floor(oz)))
                           : inf;
    t_max_z = step_z ? t_max_z * t_delta_z : inf;

    int normal_x = 0;
    int normal_y = 0;
    int normal_z = 0;
    float travelled = 0.0f;
    for (int guard = 0; guard < 512; ++guard) {
        const std::uint8_t id = world.at(x, y, z);
        if (id != kAir && block_info(id).solid) {
            hit.hit = true;
            hit.x = x;
            hit.y = y;
            hit.z = z;
            hit.nx = normal_x;
            hit.ny = normal_y;
            hit.nz = normal_z;
            hit.distance = travelled;
            return hit;
        }
        if (t_max_x < t_max_y && t_max_x < t_max_z) {
            travelled = t_max_x;
            t_max_x += t_delta_x;
            x += step_x;
            normal_x = -step_x;
            normal_y = 0;
            normal_z = 0;
        } else if (t_max_y < t_max_z) {
            travelled = t_max_y;
            t_max_y += t_delta_y;
            y += step_y;
            normal_x = 0;
            normal_y = -step_y;
            normal_z = 0;
        } else {
            travelled = t_max_z;
            t_max_z += t_delta_z;
            z += step_z;
            normal_x = 0;
            normal_y = 0;
            normal_z = -step_z;
        }
        if (travelled > max_distance) break;
    }
    return hit;
}

}  // namespace voxel
