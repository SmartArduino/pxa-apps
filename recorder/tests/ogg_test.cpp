// SPDX-License-Identifier: MIT
#include "../ogg.hpp"
#include <algorithm>
#include <cassert>

int main() {
    using namespace recorder;
    std::array<std::byte, ogg::max_page> output{};
    std::array<std::byte, ogg::max_packet + 1> packet{};
    for (const size_t count : {size_t(1), size_t(254), size_t(255), size_t(256), size_t(1275)}) {
        std::fill(packet.begin(), packet.end(), std::byte{0x5a});
        const auto size = ogg::page(output, std::span(packet).first(count), 17, 31, UINT64_C(0x100000960));
        assert(size == 27 + count / 255 + 1 + count);
        assert(ogg::header(output));
        assert(ogg::get32(output.data() + 14) == 17);
        assert(ogg::get32(output.data() + 18) == 31);
        assert(ogg::granule(output.data()) == UINT64_C(0x100000960));
        size_t payload = 0;
        for (unsigned i = 0; i < unsigned(output[26]); ++i) payload += unsigned(output[27 + i]);
        assert(payload == count);
        assert(output[26 + unsigned(output[26])] != std::byte{255});
        assert(ogg::crc(std::span(output).first(size)) == ogg::get32(output.data() + 22));
        output[size - 1] ^= std::byte{1};
        assert(ogg::crc(std::span(output).first(size)) != ogg::get32(output.data() + 22));
    }
    const auto eos_size = ogg::page(output, {}, 17, 32, 48000, 4);
    assert(eos_size == 27 && output[26] == std::byte{} && output[5] == std::byte{4});
    assert(ogg::page(output, packet, 0, 0, 0) == 0);
    assert(ogg::page(std::span(output).first(10), ogg::head, 0, 0, 0) == 0);
    assert(ogg::page(output, ogg::head, 17, 0, 0, 2));
    assert(std::equal(ogg::head.begin(), ogg::head.end(), output.begin() + 28));
}
