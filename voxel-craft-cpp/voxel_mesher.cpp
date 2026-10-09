// Camera transform, face culling and per-texture triangle batching.
#include "voxel_mesher.hpp"
#include "../common/voxel_benchmark.h"
#include <pxa/game3d_camera.hpp>
#if VOXEL_PROFILE
#include <pxa/profile_clock.hpp>
#include <cstring>
#endif
#include "voxel_draw_path.hpp"
#include "voxel_lod_colors.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <type_traits>

#ifndef VOXEL_FIXED_ATTRIBUTES
#define VOXEL_FIXED_ATTRIBUTES 0
#endif

namespace voxel {
namespace {

constexpr int kBatchVertices = 384;
std::array<pxa::game::Vertex, kBatchVertices> g_batch{};
std::size_t g_batch_count = 0;
struct BatchStyle {
    std::uint16_t color = 0;
    std::uint8_t slot = 0;
    // 1: solid, 2: affine, 4: cutout. All batches share the depth buffer.
    std::uint8_t flags = 0;
    bool operator==(const BatchStyle&) const = default;
};
BatchStyle g_batch_style{};

/* Face order: +Y, +X, -Z, -Y, -X, +Z. The value selects a row of the
 * 16-level lit palette (row 0 is full brightness, row 15 is black). */
constexpr std::uint8_t kFaceLight[6] = {0, 3, 2, 6, 4, 2};

void flush_batch(pxa::game::Frame& frame, DrawStats& stats) noexcept {
    if (g_batch_count < 3) {
        g_batch_count = 0;
        return;
    }
    const auto vertices = std::span<const pxa::game::Vertex>{g_batch.data(), g_batch_count};
    if (g_batch_style.flags & 1) {
        frame.solid_triangles(vertices, {g_batch_style.color});
    } else {
        // Clipped polygons use triangles in the same depth buffer.
        frame.triangles({g_batch_style.slot}, vertices,
                       {.affine_uv = bool(g_batch_style.flags & 2),
                        .transparent_index0 = bool(g_batch_style.flags & 4),
                        .lit_palette = true});
    }
    ++stats.batches;
    g_batch_count = 0;
}

void push(pxa::game::Frame& frame, DrawStats& stats, BatchStyle style,
          std::span<const pxa::game::Vertex> vertices) noexcept {
    if (style != g_batch_style ||
        g_batch_count + vertices.size() > kBatchVertices)
        flush_batch(frame, stats);
    g_batch_style = style;
    for (const auto& vertex : vertices) {
        if (g_batch_count >= kBatchVertices) break;
        g_batch[g_batch_count++] = vertex;
    }
}

}  // namespace

void Camera::to_camera(float wx, float wy, float wz,
                       pxa::game3d::Vec3& out) const noexcept {
    out=pxa::game3d::CameraBasis::from_pose({x,y,z},yaw,pitch).to_view({wx,wy,wz});
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
#if VOXEL_PROFILE
    const auto geometry_begin=pxa::profile_short_tick();
    std::size_t prepared_used=0;
    auto prepare=[&](BatchStyle style,std::span<const pxa::game::Vertex> vertices,unsigned kind) {
        // A clipped perimeter expands to quad records. Reserve their complete
        // wire size, plus padding, so encoding cannot overwrite unread IR.
        const auto encoded = kind == 5 ? 56 * (vertices.size() == 4 ? 1 : vertices.size()-2) : 0;
        const auto bytes=std::max(12+vertices.size_bytes(),encoded+4);
        if(bytes>budget.staging.size()-prepared_used ||
           frame.bytes_used()+prepared_used+bytes>budget.max_bytes) return false;
        auto* p=budget.staging.data()+prepared_used;
        p[0]=std::byte(kind); p[1]=std::byte(style.flags);p[2]=std::byte(style.slot);
        p[3]=std::byte{};pxa::wire::put16(p+4,static_cast<std::uint16_t>(bytes));
        pxa::wire::put16(p+6,style.color);pxa::wire::put16(p+8,static_cast<std::uint16_t>(vertices.size()));
        pxa::wire::put16(p+10,0);
        // Diagnostic vertices share the final command buffer. Their native
        // padding never escapes: Frame writes the wire's reserved byte as zero.
        std::memcpy(p+12,vertices.data(),vertices.size_bytes());
        prepared_used+=bytes;return true;
    };
#endif
    g_batch_count = 0;
    g_batch_style = {};

    const auto basis=pxa::game3d::CameraBasis::from_pose(
        {camera.x,camera.y,camera.z},camera.yaw,camera.pitch);
    const float max_distance = budget.max_distance;
    const float horizontal = static_cast<float>(budget.width) / (2 * budget.focal);
    const float vertical = static_cast<float>(budget.height) / (2 * budget.focal);
    constexpr float radius = kChunkSize * 0.866026f;
    const float radius_x = radius * std::sqrt(1 + horizontal * horizontal);
    const float radius_y = radius * std::sqrt(1 + vertical * vertical);
    auto to_camera = [&](float wx,float wy,float wz) { return basis.to_view({wx,wy,wz}); };
    const auto& axes = basis.axes;

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
                // Reject a chunk only when its bounding sphere is outside a
                // frustum plane. A centre-only cone test drops visible edges.
                // Far is view-space depth, not radial distance: an extra
                // world-space sphere would discard visible frustum corners.
                const auto center = to_camera(center_x, center_y, center_z);
                if (center.z + radius < 0.25f || center.z - radius > max_distance ||
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

    using MeshVertex = std::conditional_t<VOXEL_FIXED_ATTRIBUTES,
        pxa::game3d::FixedMeshVertex, pxa::game3d::MeshVertex>;
    using Attribute = std::conditional_t<VOXEL_FIXED_ATTRIBUTES, std::int16_t, float>;
    using Light = std::conditional_t<VOXEL_FIXED_ATTRIBUTES, std::uint8_t, float>;
    std::array<MeshVertex, 4> polygon{};
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
            const float plane = static_cast<float>(quad.plane);
            // Cull before constructing any corners or UVs. About half the
            // cached faces face away, so their vertex work is entirely wasted.
            const float face_plane = plane +
                (quad.face == 0 || quad.face == 1 || quad.face == 5 ? 1 : 0);
            const bool front = quad.face == 0 ? camera.y > base_y + face_plane :
                               quad.face == 3 ? camera.y < base_y + face_plane :
                               quad.face == 1 ? camera.x > base_x + face_plane :
                               quad.face == 4 ? camera.x < base_x + face_plane :
                               quad.face == 2 ? camera.z < base_z + face_plane :
                                                camera.z > base_z + face_plane;
            if (!front) { ++stats.faces_culled; continue; }
            const bool horizontal_face = quad.face == 0 || quad.face == 3;
            const bool x_face = quad.face == 1 || quad.face == 4;
            const auto origin = horizontal_face
                ? to_camera(base_x + u0, base_y + face_plane, base_z + v0)
                : x_face ? to_camera(base_x + face_plane, base_y + v0, base_z + u0)
                         : to_camera(base_x + u0, base_y + v0, base_z + face_plane);
            const auto& u_axis = axes[x_face ? 2 : 0];
            const auto& v_axis = axes[horizontal_face ? 2 : 1];
            const float width = quad.width, height = quad.height;
            const pxa::game3d::Vec3 du{u_axis.x * width, u_axis.y * width, u_axis.z * width};
            const pxa::game3d::Vec3 dv{v_axis.x * height, v_axis.y * height, v_axis.z * height};
            // A conservative sphere rejects offscreen faces before building
            // four corners and running the precise six-plane clipper. Its
            // radius bounds the whole rectangle for every camera orientation.
            const float face_radius=(width+height)*0.5f;
            const float center_z=origin.z+(du.z+dv.z)*0.5f;
            const float center_x=origin.x+(du.x+dv.x)*0.5f;
            const float center_y=origin.y+(du.y+dv.y)*0.5f;
            if(center_z+face_radius<0.25f || center_z-face_radius>max_distance ||
               std::fabs(center_x)>(center_z+face_radius)*horizontal+face_radius ||
               std::fabs(center_y)>(center_z+face_radius)*vertical+face_radius) {
                ++stats.faces_culled;continue;
            }
            const auto light = static_cast<Light>(
                budget.lit_palette && !VOXEL_BENCH_SCENE ? kFaceLight[quad.face] : 0);
            constexpr int uv_scale = VOXEL_FIXED_ATTRIBUTES ? 256 : 16;
            const auto bottom_v = static_cast<Attribute>(quad.height * uv_scale);
            const auto right_u = static_cast<Attribute>(quad.width * uv_scale);
            const Attribute top_v = 0;
            polygon = {{
                {origin, 0, bottom_v, light},
                {{origin.x + du.x, origin.y + du.y, origin.z + du.z}, right_u, bottom_v, light},
                {{origin.x + du.x + dv.x, origin.y + du.y + dv.y, origin.z + du.z + dv.z}, right_u, top_v, light},
                {{origin.x + dv.x, origin.y + dv.y, origin.z + dv.z}, 0, top_v, light}}};
#if VOXEL_FIXED_ATTRIBUTES
            auto count = projector.project_polygon_fixed(polygon, projected);
#else
            auto count = projector.project_polygon(polygon, projected);
#endif
            if (!count || *count < 3) { ++stats.faces_culled; continue; }
            if (*count == 4) {
                // Projector bounds screen Q4 coordinates to [0,32752]. A
                // convex perimeter's doubled area fits int32, avoiding AOT
                // int64 multiplication while rejecting wire-rounded slivers.
                const auto& a=projected[0];
                auto area=[&](const auto& b,const auto& c) {
                    return (b.x_q4-a.x_q4)*(c.y_q4-a.y_q4)-
                           (b.y_q4-a.y_q4)*(c.x_q4-a.x_q4);
                };
                if (area(projected[1],projected[2])+area(projected[2],projected[3])==0) {
                    ++stats.faces_culled;continue;
                }
            }
            const auto path = budget.cheap_paths
                ? choose_draw_path(std::span{projected}.first(*count),
                                   std::max(quad.width, quad.height), quad.cutout != 0)
                : DrawPath::perspective;
            const std::uint16_t color = path == DrawPath::solid ? kLodColors[quad.slot * 7 +
                (budget.lit_palette ? kFaceLight[quad.face] : 0)] : 0;
            const BatchStyle style{static_cast<std::uint16_t>(path == DrawPath::solid ? color : 0), quad.slot,
                static_cast<std::uint8_t>((path == DrawPath::solid ? 1 : 0) |
                                         (path == DrawPath::affine ? 2 : 0) |
                                         (quad.cutout ? 4 : 0))};
            // Include pending triangle bytes in the budget before accepting
            // another face; leave the caller's reserved HUD space untouched.
            const std::size_t face_bytes = budget.painter ? 56 * (*count == 4 ? 1 : *count - 2) :
                                          12 + (*count - 2) * 36;
            if (stats.faces_drawn >= budget.max_faces ||
                frame.bytes_used() + g_batch_count * 12 + 12 + face_bytes > budget.max_bytes) {
                stats.exhausted = true;
#if VOXEL_PROFILE
                if(!budget.staging.empty()) goto encode_prepared;
#endif
                flush_batch(frame, stats);
                return stats;
            }
#if VOXEL_PROFILE
            if(!budget.staging.empty()) {
                bool accepted;
                if(budget.painter) accepted=prepare(style,std::span{projected}.first(*count),5);
                else {
                    std::array<pxa::game::Vertex,24> fan{};
                    std::size_t vertices=0;
                    for(std::size_t i=1;i+1<*count;++i) {
                        fan[vertices++]=projected[0];fan[vertices++]=projected[i];fan[vertices++]=projected[i+1];
                    }
                    accepted=prepare(style,std::span{fan}.first(vertices),3);
                }
                if(!accepted) {stats.exhausted=true;goto encode_prepared;}
                ++stats.faces_drawn;stats.solid_faces+=path==DrawPath::solid;
                stats.affine_faces+=path==DrawPath::affine;
                continue;
            }
#endif
            if (budget.painter) {
                flush_batch(frame, stats);
                const auto vertices = std::span{projected}.first(*count);
                if (*count == 4) {
                    const std::array<pxa::game::Vertex,4> quad_vertices{
                        projected[0],projected[1],projected[2],projected[3]};
                    if (path == DrawPath::solid) frame.solid_depth_quad(quad_vertices,{color},true);
                    else frame.textured_quad({quad.slot},quad_vertices,
                        {.affine_uv=path==DrawPath::affine,.painter=true,
                         .transparent_index0=quad.cutout!=0,.lit_palette=true});
                } else if (path == DrawPath::solid) {
                    frame.solid_depth_polygon(vertices, {color});
                } else {
                    frame.textured_depth_polygon({quad.slot}, vertices,
                        {.affine_uv = path == DrawPath::affine,
                         .painter = true,
                         .transparent_index0 = quad.cutout != 0, .lit_palette = true});
                }
                stats.batches += *count == 4 ? 1 : *count - 2;
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
                    if (area != 0) push(frame, stats, style, vertices);
                }
            }
            ++stats.faces_drawn;
            stats.solid_faces += path == DrawPath::solid;
            stats.affine_faces += path == DrawPath::affine;
        }
        if (!mesh->overflow) break;
        skip += mesh->count;
        mesh = &world.chunk_mesh_page(ref.cx, ref.cy, ref.cz, skip);
        }
    }
#if VOXEL_PROFILE
encode_prepared:
    if(!budget.staging.empty()) {
        const auto encode_begin=pxa::profile_short_tick();
        stats.geometry_us=pxa::profile_short_us(geometry_begin,encode_begin);
        for(std::size_t offset=0;offset<prepared_used;) {
            const auto* p=budget.staging.data()+offset;
            const bool polygon=p[0]==std::byte{5};
            const BatchStyle style{pxa::wire::get16(p+6),std::to_integer<std::uint8_t>(p[2]),std::to_integer<std::uint8_t>(p[1])};
            const auto vertices=pxa::wire::get16(p+8);
            const auto size=pxa::wire::get16(p+4);
            if(polygon) {
                // Read a record before Frame overwrites it. Encoded quads are
                // four bytes smaller; triangle batches only shrink on merging.
                std::array<pxa::game::Vertex,10> copied;
                std::memcpy(copied.data(),p+12,vertices*sizeof(pxa::game::Vertex));
                offset+=size;
                flush_batch(frame,stats);
                const auto perimeter=std::span{copied}.first(vertices);
                if(vertices==4) {
                    const std::array<pxa::game::Vertex,4> quad_vertices{
                        copied[0],copied[1],copied[2],copied[3]};
                    if(style.flags & 1) frame.solid_depth_quad(quad_vertices,{style.color},true);
                    else frame.textured_quad({style.slot},quad_vertices,
                        {.affine_uv=bool(style.flags & 2),.painter=true,
                         .transparent_index0=bool(style.flags & 4),.lit_palette=true});
                } else if(style.flags & 1) frame.solid_depth_polygon(perimeter,{style.color});
                else frame.textured_depth_polygon({style.slot},perimeter,{.affine_uv=bool(style.flags & 2),
                    .painter=true,.transparent_index0=bool(style.flags & 4),.lit_palette=true});
                stats.batches += vertices == 4 ? 1 : vertices - 2;
            } else {
                // Each source record is fully copied before a batch can flush,
                // so overlapping source/destination storage remains safe.
                std::array<pxa::game::Vertex,24> copied;
                std::memcpy(copied.data(),p+12,vertices*sizeof(pxa::game::Vertex));
                offset+=size;
                for(unsigned i=0;i<vertices;i+=3) {
                    const std::array<pxa::game::Vertex,3> triangle{copied[i],copied[i+1],copied[i+2]};
                    const auto& a=triangle[0];const auto& b=triangle[1];const auto& c=triangle[2];
                    const auto area=std::int64_t(b.x_q4-a.x_q4)*(c.y_q4-a.y_q4)-
                                    std::int64_t(b.y_q4-a.y_q4)*(c.x_q4-a.x_q4);
                    if(area) push(frame,stats,style,triangle);
                }
            }
        }
        flush_batch(frame,stats);
        stats.encode_us=pxa::profile_short_us(encode_begin,pxa::profile_short_tick());
        return stats;
    }
#endif
    flush_batch(frame, stats);
    return stats;
}

void draw_particles(pxa::game::Frame& frame,
                    const pxa::game3d::Projector& projector,
                    const Camera& camera,
                    std::span<const Particle> particles) noexcept {
    if (std::none_of(particles.begin(), particles.end(),
                     [](const Particle& particle) { return particle.life > 0.0f; })) return;
    const auto basis = pxa::game3d::CameraBasis::from_pose(
        {camera.x, camera.y, camera.z}, camera.yaw, camera.pitch);
    constexpr float kHalf = 0.055f;
    std::array<pxa::game3d::MeshVertex, 3> triangle{};
    std::array<pxa::game::Vertex, pxa::game3d::Projector::max_triangle_vertices> projected{};
    std::array<pxa::game::Vertex, 36> batch{};
    std::size_t used = 0;
    std::uint16_t color = 0;
    for (const auto& particle : particles) {
        if (particle.life <= 0.0f) continue;
        const auto center = basis.to_view({particle.x, particle.y, particle.z});
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
