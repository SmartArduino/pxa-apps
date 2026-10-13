#pragma once
#include "jump3d_font.hpp"
#include "jump3d_palette.hpp"
#include <array>
#include <span>

namespace jump {
// Resource uploads and command submission copy synchronously into Host-owned
// storage. Their Guest scratch lifetimes do not overlap. The palette occupies
// the upper half while procedural textures use the lower half; font uploads
// and frame encoding may then reuse the entire storage.
class FrameMemory {
public:
    static constexpr std::size_t draw_capacity = 16384;
    static constexpr std::size_t upload_capacity = J3_FONT_MAX_ATLAS_BYTES + 20;
    static constexpr std::size_t palette_offset = draw_capacity / 2;
    static_assert(upload_capacity >= draw_capacity + J3_PALETTE_ENTRIES * 2);
    std::span<std::uint8_t> upload() noexcept {
        return {reinterpret_cast<std::uint8_t*>(words_.data()), upload_capacity};
    }
    std::span<std::uint8_t> draw() noexcept { return upload().first(draw_capacity); }
    std::span<std::uint16_t> palette() noexcept {
        return {words_.data() + palette_offset, J3_PALETTE_ENTRIES};
    }
private:
    // Use live uint16_t objects for the palette; uint8_t access is permitted
    // access to their object representation and needs no aliasing exception.
    alignas(4) std::array<std::uint16_t, (upload_capacity + 1) / 2> words_{};
};
static_assert(sizeof(FrameMemory) == FrameMemory::upload_capacity);
}
