// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core.hpp"
namespace md_audio {
enum class Scene { Menu, Calm, Dark };
void scene(Scene scene, unsigned mode);
void wave();
void effect(md::Sound id, unsigned track, unsigned volume = 100);
void loop(md::Sound id, unsigned track, unsigned volume);
void stop(unsigned track);
}
