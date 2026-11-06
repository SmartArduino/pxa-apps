// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace farm_palette {
constexpr uint16_t rgb(int r, int g, int b) {
  return uint16_t(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3));
}
// The title's amber timber, honey, cream and olive form the shared art palette.
constexpr uint16_t Ink = rgb(79, 47, 28), Muted = rgb(129, 83, 43);
constexpr uint16_t Paper = rgb(245, 224, 179), Soft = rgb(226, 190, 132);
constexpr uint16_t Frame = rgb(153, 93, 43), Shadow = rgb(71, 47, 28);
constexpr uint16_t Sage = rgb(93, 112, 45), Selected = rgb(240, 190, 99);
constexpr uint16_t Button = rgb(207, 147, 79), Highlight = rgb(250, 229, 177);
constexpr uint16_t Wood = rgb(176, 111, 54), WoodDark = rgb(119, 70, 36),
                   Brass = rgb(231, 178, 99), Grain = rgb(160, 96, 45);
constexpr uint16_t Grass = rgb(124, 151, 67), GrassDark = rgb(101, 129, 55),
                   GrassLight = rgb(148, 173, 84);
constexpr uint16_t Dirt = rgb(181, 135, 77), DirtDark = rgb(164, 116, 65),
                   DirtLight = rgb(196, 151, 88), Path = rgb(212, 173, 107);
constexpr uint16_t Water = rgb(65, 135, 151), Shore = rgb(137, 159, 99);
constexpr uint16_t DrySoil = rgb(141, 94, 55), WetSoil = rgb(94, 64, 44);
} // namespace farm_palette
