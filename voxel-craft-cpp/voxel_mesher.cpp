// Camera transform, face culling and per-texture triangle batching.
#include "voxel_mesher.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace voxel {
namespace {

constexpr int kBatchVertices = 384;
std::array<pxa::game::Vertex, kBatchVertices> g_batch{};
std::size_t g_batch_count = 0;
int g_batch_slot = -1;
bool g_batch_cutout = false;

/* Face order: +Y, +X, -Z, -Y, -X, +Z. The value selects a row of the
 * 16-level lit palette (row 0 is full brightness, row 15 is black). */
constexpr std::uint8_t kFaceLight[6] = {0, 3, 2, 6, 4, 2};

void flush_batch(pxa::game::Frame& frame, DrawStats& stats) noexcept {
    if (g_batch_count < 3 || g_batch_slot < 0) {
        g_batch_count = 0;
        return;
    }
    pxa::game::PolygonOptions options;
    /* Perspective-correct: merged quads are large enough that affine UVs
     * visibly swim when the camera moves, and the painter_depth kernel
     * interpolates the reciprocal depth anyway. */
    options.affine_uv = false;
    /* Triangle batches are always the depth tested path: the host rejects
     * painter combined with the lit palette on triangles, so a painter-mode
     * frame must not carry the flag into this fallback. */
    options.lit_palette = true;
    options.painter = false;
    options.transparent_index0 = g_batch_cutout;
    frame.triangles(pxa::game::AtlasBinding{
                        static_cast<std::uint8_t>(g_batch_slot)},
                    std::span<const pxa::game::Vertex>{g_batch.data(),
                                                       g_batch_count},
                    options);
    ++stats.batches;
    g_batch_count = 0;
}

void push(pxa::game::Frame& frame, DrawStats& stats, int slot, bool cutout,
          std::span<const pxa::game::Vertex> vertices) noexcept {
    if (slot != g_batch_slot || cutout != g_batch_cutout ||
        g_batch_count + vertices.size() > kBatchVertices)
        flush_batch(frame, stats);
    g_batch_slot = slot;
    g_batch_cutout = cutout;
    for (const auto& vertex : vertices) {
        if (g_batch_count >= kBatchVertices) break;
        g_batch[g_batch_count++] = vertex;
    }
}

}  // namespace

void Camera::to_camera(float wx, float wy, float wz,
                       pxa::game3d::Vec3& out) const noexcept {
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float dx = wx - x;
    const float dy = y - wy; /* world up becomes camera up */
    const float dz = wz - z;
    const float x1 = dx * cy - dz * sy;
    const float z1 = dx * sy + dz * cy;
    out.x = x1;
    out.y = dy * cp - z1 * sp;
    out.z = dy * sp + z1 * cp;
}

void Camera::forward(float& dx, float& dy, float& dz) const noexcept {
    const float cp = std::cos(pitch);
    dx = std::sin(yaw) * cp;
    dy = -std::sin(pitch);
    dz = std::cos(yaw) * cp;
}

void Camera::right(float& dx, float& dz) const noexcept {
    dx = std::cos(yaw);
    dz = -std::sin(yaw);
}

DrawStats draw_world(pxa::game::Frame& frame,
                     const pxa::game3d::Projector& projector,
                     const World& world, const Camera& camera,
                     const DrawBudget& budget) noexcept {
    DrawStats stats;
    g_batch_count = 0;
    g_batch_slot = -1;
    g_batch_cutout = false;

    const float cyaw = std::cos(camera.yaw);
    const float syaw = std::sin(camera.yaw);
    const float cpitch = std::cos(camera.pitch);
    const float spitch = std::sin(camera.pitch);
    const float max_distance = budget.max_distance;
    const float horizontal = static_cast<float>(budget.width) / (2 * budget.focal);
    const float vertical = static_cast<float>(budget.height) / (2 * budget.focal);
    constexpr float radius = kChunkSize * 0.866026f;
    const float radius_x = radius * std::sqrt(1 + horizontal * horizontal);
    const float radius_y = radius * std::sqrt(1 + vertical * vertical);
    auto to_camera = [&](float wx, float wy, float wz) {
        const float dx = wx - camera.x;
        const float dy = camera.y - wy;
        const float dz = wz - camera.z;
        const float x1 = dx * cyaw - dz * syaw;
        const float z1 = dx * syaw + dz * cyaw;
        return pxa::game3d::Vec3{x1, dy * cpitch - z1 * spitch,
                                   dy * spitch + z1 * cpitch};
    };

    /* Front-to-back chunks reduce overdraw. Every terrain primitive uses the
     * same depth buffer, including clipped faces and palette-lighting tiers. */
    struct ChunkRef {
        std::int16_t cx = 0;
        std::int16_t cy = 0;
        std::int16_t cz = 0;
        float distance = 0.0f;
    };
    constexpr int kMaxChunks = kChunksX * kChunksY * kChunksZ;
    std::array<ChunkRef, kMaxChunks> chunks{};
    int chunk_count = 0;
    for (int cx = 0; cx < kChunksX; ++cx) {
        for (int cy = 0; cy < kChunksY; ++cy) {
            for (int cz = 0; cz < kChunksZ; ++cz) {
                const float center_x =
                    static_cast<float>(cx * kChunkSize) + kChunkSize * 0.5f;
                const float center_y =
                    static_cast<float>(cy * kChunkSize) + kChunkSize * 0.5f;
                const float center_z =
                    static_cast<float>(cz * kChunkSize) + kChunkSize * 0.5f;
                const float dx = center_x - camera.x;
                const float dy = center_y - camera.y;
                const float dz = center_z - camera.z;
                const float reach = max_distance + kChunkSize * 0.87f;
                if (dx * dx + dy * dy + dz * dz > reach * reach) continue;
                // Reject a chunk only when its bounding sphere is outside a
                // frustum plane. A centre-only cone test drops visible edges.
                const auto center = to_camera(center_x, center_y, center_z);
                if (center.z + radius < 0.25f ||
                    std::fabs(center.x) - center.z * horizontal > radius_x ||
                    std::fabs(center.y) - center.z * vertical > radius_y) continue;
                auto& ref = chunks[chunk_count++];
                ref.cx = static_cast<std::int16_t>(cx);
                ref.cy = static_cast<std::int16_t>(cy);
                ref.cz = static_cast<std::int16_t>(cz);
                ref.distance = dx * dx + dy * dy + dz * dz;
            }
        }
    }
    std::sort(chunks.begin(), chunks.begin() + chunk_count,
              [](const ChunkRef& a, const ChunkRef& b) {
                  return a.distance < b.distance;
              });

    std::array<pxa::game3d::MeshVertex, 4> polygon{};
    std::array<pxa::game::Vertex, pxa::game3d::Projector::max_polygon_vertices> projected{};
    for (int index = 0; index < chunk_count; ++index) {
        const auto& ref = chunks[index];
        const float base_x = static_cast<float>(ref.cx * kChunkSize);
        const float base_y = static_cast<float>(ref.cy * kChunkSize);
        const float base_z = static_cast<float>(ref.cz * kChunkSize);
        const ChunkMesh* mesh = &world.chunk_mesh(ref.cx, ref.cy, ref.cz);
        std::uint32_t skip = 0;
        for (;;) {
        for (std::uint16_t q = 0; q < mesh->count; ++q) {
            const MeshQuad& quad = mesh->quads[q];
            const float u0 = static_cast<float>(quad.u);
            const float v0 = static_cast<float>(quad.v);
            const float u1 = u0 + static_cast<float>(quad.width);
            const float v1 = v0 + static_cast<float>(quad.height);
            const float plane = static_cast<float>(quad.plane);
            /* Corner positions in world space plus their texture coordinates. */
            float wx[4] = {};
            float wy[4] = {};
            float wz[4] = {};
            float tu[4] = {};
            float tv[4] = {};
            const float us[4] = {u0, u1, u1, u0};
            const float vs[4] = {v0, v0, v1, v1};
            for (int corner = 0; corner < 4; ++corner) {
                switch (quad.face) {
                    case 0:
                        wx[corner] = base_x + us[corner];
                        wy[corner] = base_y + plane + 1.0f;
                        wz[corner] = base_z + vs[corner];
                        tu[corner] = (us[corner] - u0) * 16.0f;
                        tv[corner] = (vs[corner] - v0) * 16.0f;
                        break;
                    case 3:
                        wx[corner] = base_x + us[corner];
                        wy[corner] = base_y + plane;
                        wz[corner] = base_z + vs[corner];
                        tu[corner] = (us[corner] - u0) * 16.0f;
                        tv[corner] = (vs[corner] - v0) * 16.0f;
                        break;
                    case 1:
                    case 4:
                        wx[corner] = base_x + plane + (quad.face == 1 ? 1.0f : 0.0f);
                        wy[corner] = base_y + vs[corner];
                        wz[corner] = base_z + us[corner];
                        tu[corner] = (us[corner] - u0) * 16.0f;
                        /* side textures hang from the top of the block */
                        tv[corner] = (v1 - vs[corner]) * 16.0f;
                        break;
                    default:
                        wx[corner] = base_x + us[corner];
                        wy[corner] = base_y + vs[corner];
                        wz[corner] = base_z + plane + (quad.face == 5 ? 1.0f : 0.0f);
                        tu[corner] = (us[corner] - u0) * 16.0f;
                        tv[corner] = (v1 - vs[corner]) * 16.0f;
                        break;
                }
            }
            // A block face is only visible from its outward half-space.
            const float face_x = wx[0], face_y = wy[0], face_z = wz[0];
            const bool front = quad.face == 0 ? camera.y > face_y :
                               quad.face == 3 ? camera.y < face_y :
                               quad.face == 1 ? camera.x > face_x :
                               quad.face == 4 ? camera.x < face_x :
                               quad.face == 2 ? camera.z < face_z : camera.z > face_z;
            if (!front) { ++stats.faces_culled; continue; }
            for (int corner = 0; corner < 4; ++corner) {
                polygon[corner] = {to_camera(wx[corner], wy[corner], wz[corner]),
                                   tu[corner], tv[corner],
                                   static_cast<float>(budget.lit_palette ? kFaceLight[quad.face] : 0)};
            }
            auto count = projector.project_polygon(polygon, projected);
            if (!count || *count < 3) { ++stats.faces_culled; continue; }
            // Include pending triangle bytes in the budget before accepting
            // another face; leave the caller's reserved HUD space untouched.
            const std::size_t face_bytes = *count == 4 && budget.painter ? 56 :
                                          12 + (*count - 2) * 36;
            if (stats.faces_drawn >= budget.max_faces ||
                frame.bytes_used() + g_batch_count * 12 + 12 + face_bytes > budget.max_bytes) {
                stats.exhausted = true;
                flush_batch(frame, stats);
                return stats;
            }
            if (*count == 4 && budget.painter) {
                flush_batch(frame, stats);
                std::array<pxa::game::Vertex, 4> vertices{};
                std::copy_n(projected.begin(), 4, vertices.begin());
                frame.textured_quad({quad.slot}, vertices,
                    {.affine_uv = false, .painter = true,
                     .transparent_index0 = quad.cutout != 0, .lit_palette = true});
                ++stats.batches;
            } else {
                // project_polygon returns a perimeter; triangulate it once.
                for (std::size_t i = 1; i + 1 < *count; ++i) {
                    const std::array<pxa::game::Vertex, 3> vertices{
                        projected[0], projected[i], projected[i + 1]};
                    const auto& a = vertices[0];
                    const auto& b = vertices[1];
                    const auto& c = vertices[2];
                    const auto area = static_cast<std::int64_t>(b.x_q4 - a.x_q4) * (c.y_q4 - a.y_q4) -
                                      static_cast<std::int64_t>(b.y_q4 - a.y_q4) * (c.x_q4 - a.x_q4);
                    if (area != 0) push(frame, stats, quad.slot, quad.cutout != 0, vertices);
                }
            }
            ++stats.faces_drawn;
        }
        if (!mesh->overflow) break;
        skip += mesh->count;
        mesh = &world.chunk_mesh_page(ref.cx, ref.cy, ref.cz, skip);
        }
    }
    flush_batch(frame, stats);
    return stats;
}

void draw_particles(pxa::game::Frame& frame,
                    const pxa::game3d::Projector& projector,
                    const Camera& camera,
                    std::span<const Particle> particles) noexcept {
    if (particles.empty()) return;
    constexpr float kHalf = 0.055f;
    std::array<pxa::game3d::MeshVertex, 3> triangle{};
    std::array<pxa::game::Vertex, pxa::game3d::Projector::max_triangle_vertices> projected{};
    std::array<pxa::game::Vertex, 36> batch{};
    std::size_t used = 0;
    std::uint16_t color = 0;
    for (const auto& particle : particles) {
        if (particle.life <= 0.0f) continue;
        pxa::game3d::Vec3 center{};
        camera.to_camera(particle.x, particle.y, particle.z, center);
        if (center.z < 0.3f) continue;
        if (particle.color != color && used >= 3) {
            frame.solid_triangles(
                std::span<const pxa::game::Vertex>{batch.data(), used}, {color});
            used = 0;
        }
        color = particle.color;
        const pxa::game3d::MeshVertex quad[4] = {
            {{center.x - kHalf, center.y - kHalf, center.z}, 0, 0, 0},
            {{center.x + kHalf, center.y - kHalf, center.z}, 0, 0, 0},
            {{center.x + kHalf, center.y + kHalf, center.z}, 0, 0, 0},
            {{center.x - kHalf, center.y + kHalf, center.z}, 0, 0, 0}};
        const int order[2][3] = {{0, 1, 2}, {0, 2, 3}};
        for (const auto& tri : order) {
            triangle[0] = quad[tri[0]];
            triangle[1] = quad[tri[1]];
            triangle[2] = quad[tri[2]];
            auto count = projector.project_triangle(
                triangle, projected, pxa::game3d::FrontFace::both);
            if (!count || *count < 3) continue;
            // project_triangle already emits triples. Flush a full batch so
            // debris is neither duplicated nor silently dropped.
            for (std::size_t i = 0; i + 2 < *count; i += 3) {
                if (used + 3 > batch.size()) {
                    frame.solid_triangles(std::span<const pxa::game::Vertex>{batch.data(), used}, {color});
                    used = 0;
                }
                const auto& a = projected[i];
                const auto& b = projected[i + 1];
                const auto& cc = projected[i + 2];
                const auto area = static_cast<std::int64_t>(b.x_q4 - a.x_q4) *
                                      (cc.y_q4 - a.y_q4) -
                                  static_cast<std::int64_t>(b.y_q4 - a.y_q4) *
                                      (cc.x_q4 - a.x_q4);
                if (area == 0) continue;
                batch[used++] = a;
                batch[used++] = b;
                batch[used++] = cc;
            }
        }
    }
    if (used >= 3)
        frame.solid_triangles(
            std::span<const pxa::game::Vertex>{batch.data(), used}, {color});
}

}  // namespace voxel
