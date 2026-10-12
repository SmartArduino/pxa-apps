#pragma once
#include <cstddef>
#include <cstdint>
namespace pxa { class Transport; }
void ime_bind_dictionary(pxa::Transport&, std::uint64_t handle, std::uint64_t bytes) noexcept;
