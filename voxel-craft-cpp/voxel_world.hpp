// Voxel world: block table, terrain generation and ray casting.
#pragma once

#include <array>
#include <cstdint>

namespace voxel {

constexpr int kWorldX = 64;
constexpr int kWorldY = 24;
constexpr int kWorldZ = 64;
constexpr int kSeaLevel = 7;

enum BlockId : std::uint8_t {
    kAir = 0,
    kGrass,
    kDirt,
    kStone,
    kSand,
    kWood,
    kLeaves,
    kWater,
    kPlank,
    kBrick,
    kGlass,
    kCobble,
    kTable,
    kSnow,
    kGravel,
    kCactus,
    kBush,
    kFlower,
    kWool,
    kBedrock,
    kBlockCount
};

/* Face kinds of the shared 16x16 block texture set. */
enum FaceKind : int { kFaceTop = 0, kFaceSide = 1, kFaceBottom = 2 };

struct BlockInfo {
    const char* name;
    /* Asset slot indices (b00..b44) for top, side and bottom faces. */
    std::uint8_t texture[3];
    bool solid;   /* blocks movement */
    bool opaque;  /* hides the neighbouring face */
    bool cutout;  /* texture index 0 stays transparent (leaves, glass, plants) */
};

const BlockInfo& block_info(std::uint8_t id) noexcept;

/* Slots used by the hotbar, in cycle order. */
extern const std::uint8_t kHotbarBlocks[];
extern const int kHotbarCount;

/* One merged, coplanar, single-material rectangle of faces. Greedy meshing
 * turns a flat 8x8 chunk face into one quad instead of 64. */
struct MeshQuad {
    std::uint8_t face = 0;     /* 0 +Y, 1 +X, 2 -Z, 3 -Y, 4 -X, 5 +Z */
    std::uint8_t plane = 0;    /* coordinate along the face normal */
    std::uint8_t u = 0;        /* in-plane origin */
    std::uint8_t v = 0;
    std::uint8_t width = 1;    /* extents along u and v */
    std::uint8_t height = 1;
    std::uint8_t slot = 0;     /* block texture slot */
    std::uint8_t cutout = 0;
};

constexpr int kChunkSize = 8;
constexpr int kChunksX = (kWorldX + kChunkSize - 1) / kChunkSize;
constexpr int kChunksY = (kWorldY + kChunkSize - 1) / kChunkSize;
constexpr int kChunksZ = (kWorldZ + kChunkSize - 1) / kChunkSize;
/* Per-chunk quad budget: 192 chunks * 8 bytes must fit the Guest heap. */
constexpr int kMaxQuadsPerChunk = 384;

struct ChunkMesh {
    std::array<MeshQuad, kMaxQuadsPerChunk> quads{};
    std::uint16_t count = 0;
    bool built = false;
    bool overflow = false;
};

class World {
public:
    void generate(std::uint32_t seed) noexcept;

    std::uint8_t at(int x, int y, int z) const noexcept;
    void set(int x, int y, int z, std::uint8_t id) noexcept;

    bool in_bounds(int x, int y, int z) const noexcept {
        return x >= 0 && x < kWorldX && y >= 0 && y < kWorldY && z >= 0 &&
               z < kWorldZ;
    }
    bool solid(int x, int y, int z) const noexcept;
    bool opaque(int x, int y, int z) const noexcept;
    int highest_block(int x, int z) const noexcept;

    /* Chunk mesh cache: rebuilt lazily after edits. */
    const ChunkMesh& chunk_mesh(int cx, int cy, int cz) const noexcept;
    // Reuse the same cache for overflow pages; the previous borrowed page is
    // invalidated. A later chunk_mesh() restores the first page lazily.
    const ChunkMesh& chunk_mesh_page(int cx, int cy, int cz,
                                     std::uint32_t skip) const noexcept;
    using QuadVisitor = bool (*)(void*, const MeshQuad&) noexcept;
    // Overflow is streamed through the same mesher, without growing the cache.
    // Returning false from the visitor stops traversal at the frame budget.
    template<class Visitor>
    bool visit_chunk(int cx, int cy, int cz, Visitor& visitor) const noexcept {
        if (cx < 0 || cy < 0 || cz < 0 ||
            cx >= kChunksX || cy >= kChunksY || cz >= kChunksZ) return false;
        const auto& mesh = chunk_mesh(cx, cy, cz);
        if (mesh.overflow)
            return generate_quads(cx, cy, cz, &visitor,
                [](void* context, const MeshQuad& quad) noexcept {
                    return (*static_cast<Visitor*>(context))(quad);
                });
        for (std::uint16_t i = 0; i < mesh.count; ++i)
            if (!visitor(mesh.quads[i])) return false;
        return true;
    }
    void mark_dirty(int x, int y, int z) noexcept;

private:
    void build_chunk(ChunkMesh& mesh, int cx, int cy, int cz) const noexcept;
    bool generate_quads(int cx, int cy, int cz, void*, QuadVisitor) const noexcept;
    bool face_visible(int x, int y, int z, int face) const noexcept;

    std::array<std::uint8_t, kWorldX * kWorldY * kWorldZ> blocks_{};
    mutable std::array<ChunkMesh, kChunksX * kChunksY * kChunksZ> meshes_{};
};

struct RayHit {
    bool hit = false;
    int x = 0;
    int y = 0;
    int z = 0;
    int nx = 0;
    int ny = 0;
    int nz = 0;
    float distance = 0.0f;
};

/* Voxel DDA along a normalised direction; returns the first solid block. */
RayHit raycast(const World& world, float ox, float oy, float oz, float dx,
               float dy, float dz, float max_distance) noexcept;

}  // namespace voxel
