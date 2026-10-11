#include "../draw.hpp"
#include <cassert>

static unsigned attempts;
extern "C" std::int32_t pxa_call(std::uint32_t, std::uint64_t,
                                 const std::uint8_t*, std::uint32_t) { return 0; }
extern "C" std::int32_t pxa_io(std::uint64_t handle, std::uint32_t operation,
                               std::uint8_t* data, std::uint32_t size) {
    assert(handle == 77 && operation == 0x101);
    const auto* bytes = reinterpret_cast<const std::byte*>(data);
    ++attempts;
    assert(pxa::wire::get64(bytes + 20) == (attempts <= 2 ? 1u : 2u));
    assert(pxa::wire::get16(bytes + 36) == (attempts == 1 ? 0xf800 : 0x001f));
    return attempts == 1 ? static_cast<int>(pxa::Error::would_block) : static_cast<int>(size);
}

int main() {
    pxa::Transport transport;
    transport.phase(pxa::Phase::event);
    pxa::game::Renderer renderer(transport, 77, 0);
    std::array<std::uint8_t, 128> bytes{};
    arcade::DrawList list;
    list.begin(renderer, bytes.data(), bytes.size());
    arcade::clear(&list, 0xf800);
    assert(arcade::submit(renderer, &list) == static_cast<int>(pxa::Error::would_block));
    // Rebuild with current input after backpressure; ID 1 remains available.
    list.begin(renderer, bytes.data(), bytes.size());
    arcade::clear(&list, 0x001f);
    assert(arcade::submit(renderer, &list) == 40);
    list.begin(renderer, bytes.data(), bytes.size());
    arcade::clear(&list, 0x001f);
    assert(arcade::submit(renderer, &list) == 40 && attempts == 3);
}
