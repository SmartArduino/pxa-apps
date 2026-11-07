// SPDX-License-Identifier: MIT
#pragma once
#include "art.hpp"
#include "world.hpp"
#include <algorithm>
#include <cstdint>
namespace terrain {
enum Kind : uint8_t { Grass, Dirt, Path, Pond };
enum Neighbor : unsigned {
  N = 1,
  E = 2,
  S = 4,
  W = 8,
  NW = 16,
  NE = 32,
  SE = 64,
  SW = 128
};
// Hand-cut native pixel corners. The shore uses two-pixel steps; earth
// uses a short chamfer. Sharing this cut with infill keeps all junctions solid.
inline bool corner_cut(int x, int y, int radius) {
  if (radius >= 8)
    return x / 2 + y / 2 < 3;
  return x + y < (radius >= 5 ? 3 : 2);
}
inline bool rounded(unsigned neighbors, int x, int y, int radius = 5) {
  const int dx[] = {x, 15 - x, 15 - x, x}, dy[] = {y, y, 15 - y, 15 - y};
  const unsigned a[] = {N, N, S, S}, b[] = {W, E, E, W};
  for (int corner = 0; corner < 4; ++corner) {
    if (!(neighbors & a[corner]) && !(neighbors & b[corner]) &&
        dx[corner] < radius && dy[corner] < radius) {
      if (corner_cut(dx[corner], dy[corner], radius))
        return false;
    }
  }
  return true;
}
// A concave material corner fills the opposite (empty) tile's corner.
// Cutting the occupied tile instead would make a notch.
inline bool infill(unsigned neighbors, int x, int y, int radius = 5) {
  const int dx[] = {x, 15 - x, 15 - x, x}, dy[] = {y, y, 15 - y, 15 - y};
  const unsigned a[] = {N, N, S, S}, b[] = {W, E, E, W},
                 diag[] = {NW, NE, SE, SW};
  for (int c = 0; c < 4; ++c)
    if ((neighbors & a[c]) && (neighbors & b[c]) && (neighbors & diag[c]) &&
        dx[c] < radius && dy[c] < radius) {
      if (corner_cut(dx[c], dy[c], radius))
        return true;
    }
  return false;
}
inline uint16_t mix(uint16_t a, uint16_t b, int k) {
  int ra = (a >> 11) & 31, ga = (a >> 5) & 63, ba = a & 31, rb = (b >> 11) & 31,
      gb = (b >> 5) & 63, bb = b & 31;
  return uint16_t((((ra * (8 - k) + rb * k) / 8) << 11) |
                  (((ga * (8 - k) + gb * k) / 8) << 5) |
                  ((ba * (8 - k) + bb * k) / 8));
}
struct Map {
  uint8_t kinds[farm::MapW * farm::MapH]{};
  uint8_t materials[farm::MapW * farm::MapH * 16 * 16 / 4]{};
  Kind kind(int x, int y) const {
    return x >= 0 && y >= 0 && x < farm::MapW && y < farm::MapH
               ? Kind(kinds[y * farm::MapW + x])
               : Grass;
  }
  static int family(Kind k) {
    return k == Dirt || k == Path ? 1 : k == Pond ? 2 : 0;
  }
  bool same(int x, int y, Kind k) const {
    return family(kind(x, y)) == family(k);
  }
  unsigned neighbors(int x, int y, Kind k) const {
    const int dx[] = {0, 1, 0, -1, -1, 1, 1, -1},
              dy[] = {-1, 0, 1, 0, -1, -1, 1, 1};
    unsigned bits = 0;
    for (int i = 0; i < 8; ++i)
      if (same(x + dx[i], y + dy[i], k))
        bits |= 1u << i;
    return bits;
  }
  void build(const farm::State &s) {
    for (int y = 0; y < farm::MapH; ++y)
      for (int x = 0; x < farm::MapW; ++x) {
        auto g = farm::ground(s, x, y);
        bool clearing=!s.region && (farm::plot_index(x,y)>=0 ||
          (x==9&&y>=10&&y<=15) || (x==20&&y>=9&&y<=14) ||
          (y==8&&x>=12&&x<=17) || (y==17&&x>=11&&x<=18));
        Kind k =
            g == farm::Ground::Water      ? Pond
            : clearing ? Dirt
            : farm::road_tile(s.region,x,y)
                ? Path
                : Grass;
        kinds[y * farm::MapW + x] = k;
      }
    rasterize();
  }
  void rasterize() {
    std::fill(materials, materials + sizeof(materials), uint8_t(0));
    for (int y = 0; y < farm::MapH; ++y)
      for (int x = 0; x < farm::MapW; ++x) {
        Kind k = kind(x, y);
        unsigned n = neighbors(x, y, k), soil = neighbors(x, y, Dirt),
                 water = neighbors(x, y, Pond);
        for (int yy = 0; yy < 16; ++yy)
          for (int xx = 0; xx < 16; ++xx) {
            Kind actual = k;
            if (k != Grass) {
              if (!rounded(n, xx, yy, k == Pond ? 8 : 5))
                actual = Grass;
            } else if (infill(water, xx, yy, 8))
              actual = Pond;
            else if (infill(soil, xx, yy, 5)) {
              actual = Path;
              for (int dir = 0; dir < 4; ++dir) {
                int nx = x + (dir == 1) - (dir == 3),
                    ny = y + (dir == 2) - (dir == 0);
                if (kind(nx, ny) == Dirt)
                  actual = Dirt;
              }
            }
            unsigned at = (y * 16 + yy) * (farm::MapW * 16) + x * 16 + xx;
            materials[at >> 2] |= uint8_t(actual << ((at & 3) * 2));
          }
      }
  }
  int material(int x, int y) const {
    if (x < 0 || y < 0 || x >= farm::MapW * 16 || y >= farm::MapH * 16)
      return Grass;
    unsigned at = y * (farm::MapW * 16) + x;
    return (materials[at >> 2] >> ((at & 3) * 2)) & 3;
  }
  bool earth(int x, int y) const {
    int k = material(x, y);
    return k == Dirt || k == Path;
  }
  int distance(int x, int y, bool water, int cap = 5) const {
    for (int d = 1; d <= cap; ++d)
      for (int direction = 0; direction < 4; ++direction) {
        int xx = x + (direction == 1   ? d
                      : direction == 3 ? -d
                                       : 0),
            yy = y + (direction == 2   ? d
                      : direction == 0 ? -d
                                       : 0);
        bool in = water ? material(xx, yy) == Pond : earth(xx, yy);
        if (!in)
          return d - 1;
      }
    return cap;
  }
  static uint16_t grass_color(int season, int tone = 0) {
    const uint16_t spring[] = {farm_palette::Grass, farm_palette::GrassDark,
                               farm_palette::GrassLight};
    const uint16_t fall[] = {art::rgb(148, 151, 103), art::rgb(130, 133, 85),
                             art::rgb(163, 164, 116)};
    const uint16_t winter[] = {art::rgb(187, 203, 193), art::rgb(167, 187, 179),
                               art::rgb(204, 216, 204)};
    return season == 2 ? fall[tone] : season == 3 ? winter[tone] : spring[tone];
  }
  static uint16_t earth_color(int x, int y, Kind kind) {
    // Non-tile-sized cells and staggered origins prevent a repeating 16px
    // stamp. Each feature is a deliberate multi-pixel cluster, not grain.
    uint32_t h = art::hash(x / 9, y / 8);
    int u = x % 9, v = y % 8, cx = 1 + int((h >> 4) % 5),
        cy = 1 + int((h >> 10) % 4), dx = u - cx, dy = v - cy;
    if (kind == Dirt) {
      int width = 2 + int((h >> 20) % 3);
      if (h % 4 && dx >= 0 && dx < width && dy >= 0 && dy <= 1) {
        if ((h & 32) && dy == 0 && dx == 0)
          return farm_palette::Dirt;
        if (dy == 1)
          return farm_palette::DirtDark;
        return (h & 8) ? farm_palette::DirtLight : art::rgb(170, 124, 69);
      }
      return farm_palette::Dirt;
    }
    // Packed paths: short scuffs and occasional small, two-tone pebbles.
    if (h % 7 == 0 && dx >= 0 && dx <= 2 && dy >= 0 && dy <= 1)
      return dy == 0 ? art::rgb(232, 201, 150) : art::rgb(174, 135, 79);
    if (h % 3 != 0 && dy == 0 && dx >= -2 && dx <= 1)
      return art::rgb(194, 150, 87);
    if (h % 5 == 1 && dy == -1 && dx >= 0 && dx <= 2)
      return art::rgb(225, 187, 123);
    return farm_palette::Path;
  }
  uint16_t pixel(int x, int y, int season) const {
    int k = material(x, y);
    uint16_t grass = grass_color(season);
    if (k == Grass) {
      uint32_t h = art::hash(x / 11, y / 9);
      int u = x % 11, v = y % 9;
      int dx = u - (2 + int((h >> 5) % 5)), dy = v - (2 + int((h >> 10) % 5));
      uint16_t c = grass;
      if (h % 4 != 0 && std::abs(dx) <= 2 && std::abs(dy) <= 1 &&
          !(std::abs(dx) == 2 && dy == -1))
        c = grass_color(season, (h & 16) ? 1 : 2);
      // Broken three-pixel turf/loam clusters soften the boundary without
      // a continuous dark outline or interpolated/blurred tile edges.
      for(int d=1;d<=3;++d) {
        if(earth(x-d,y)||earth(x+d,y)||earth(x,y-d)||earth(x,y+d)) {
          auto fringe=art::hash(x/3,y/2);
          if(d==1 || (d==2&&fringe%3==0) || (d==3&&fringe%7==0))
            c=mix(grass,farm_palette::Dirt,d==1?3:2);
          break;
        }
      }
      if (material(x + 1, y) == Pond || material(x - 1, y) == Pond ||
          material(x, y + 1) == Pond || material(x, y - 1) == Pond)
        c = grass_color(season, 1);
      return c;
    }
    if (k == Pond) {
      int d = distance(x, y, true, 5);
      uint32_t h = art::hash(x / 4, y / 3);
      if (d == 0)
        return art::rgb(99, 122, 67);
      if (d == 1)
        return h % 3 ? art::rgb(192, 163, 104) : art::rgb(153, 144, 87);
      if (d == 2)
        return h % 4 ? art::rgb(113, 160, 151) : farm_palette::Water;
      // Low-contrast, horizontal water clusters; no concentric soft gradient.
      uint32_t wave = art::hash(x / 9, y / 6);
      if (wave % 4 == 0 && x % 9 >= 2 && x % 9 <= 6 && y % 6 == 2)
        return art::rgb(98, 153, 162);
      if (wave % 7 == 0 && x % 9 >= 3 && x % 9 <= 6 && y % 6 == 3)
        return art::rgb(68, 125, 142);
      return farm_palette::Water;
    }
    int edge = distance(x, y, false, 2);
    uint32_t fringe = art::hash(x / 3, y / 2);
    if (edge == 0) {
      return mix(earth_color(x,y,Kind(k)),grass,fringe%3==0?4:3);
    }
    if (edge == 1 && fringe % 3 == 0)
      return mix(earth_color(x,y,Kind(k)),grass,2);
    // Join path and soil using small, solid-colour clusters instead of
    // feathering intermediate colours into an airbrushed seam.
    Kind surface = Kind(k);
    for (int distance = 1; distance <= 4; ++distance) {
      bool other = false;
      for (int dir = 0; dir < 4; ++dir) {
        int xx = x + (dir == 1   ? distance
                      : dir == 3 ? -distance
                                 : 0),
            yy = y + (dir == 2   ? distance
                      : dir == 0 ? -distance
                                 : 0);
        int neighbor = material(xx, yy);
        other |= neighbor != k && (neighbor == Dirt || neighbor == Path);
      }
      if (other) {
        if (art::hash(x / 3, y / 2) % 5 < unsigned(5 - distance))
          surface = k == Dirt ? Path : Dirt;
        if(distance<=2)return mix(earth_color(x,y,Kind(k)),earth_color(x,y,surface),4);
        break;
      }
    }
    return earth_color(x, y, surface);
  }
  void draw(art::Raster &r, int season) const {
    for (int y = 0; y < farm::MapH * 16; ++y) {
      int start = 0;
      uint16_t c = pixel(0, y, season);
      for (int x = 1; x <= farm::MapW * 16; ++x) {
        uint16_t next =
            x < farm::MapW * 16 ? pixel(x, y, season) : uint16_t(~c);
        if (next != c) {
          r.rect(start, y, x - start, 1, c);
          start = x;
          c = next;
        }
      }
    }
    // Tufts vary inside their cells and use the seasonal terrain palette.
    // Every stroke is checked against the material, including concave seams.
    for (int y = 2; y < farm::MapH * 16 - 7; y += 9)
      for (int x = 2; x < farm::MapW * 16 - 7; x += 9) {
        uint32_t h = art::hash(x, y);
        if (h % 3)
          continue;
        int xx = x + int((h >> 5) % 3), yy = y + int((h >> 9) % 3);
        auto stroke = [&](int sx, int sy, int width, int height, uint16_t c) {
          for (int dy = 0; dy < height; ++dy)
            for (int dx = 0; dx < width; ++dx)
              if (material(sx + dx, sy + dy) == Grass)
                r.rect(sx + dx, sy + dy, 1, 1, c);
        };
        stroke(xx, yy + 1, 1, 2, grass_color(season, 1));
        stroke(xx + 1, yy + 2, 2, 1, grass_color(season, 1));
        stroke(xx + 3, yy, 1, 3, grass_color(season, 1));
        stroke(xx + 1, yy, 1, 1, grass_color(season, 2));
        if (h % 23 == 0 && season != 3 && material(xx - 2, yy - 2) == Grass &&
            material(xx + 2, yy + 4) == Grass)
          art::flower(r, xx, yy, art::rgb(223, 216, 177));
      }
  }
};
inline bool tilled(const farm::State &s, int x, int y) {
  int i = farm::plot_index(x, y);
  return i >= 0 && s.plots[i].tilled;
}
inline void plots(art::Raster &r, const farm::State &s) {
  const int dx[] = {0, 1, 0, -1, -1, 1, 1, -1},
            dy[] = {-1, 0, 1, 0, -1, -1, 1, 1};
  for (int i = 0; i < farm::PlotCount; ++i) {
    auto &p = s.plots[i];

    int tx = farm::PlotX + i % farm::PlotW, ty = farm::PlotY + i / farm::PlotW,
        x = tx * 16, y = ty * 16;
    if (x + 16 < r.ox || y + 16 < r.oy || x >= r.ox + r.w || y >= r.oy + r.h)
      continue;
    unsigned n = 0, wet_neighbors = 0;
    for (int k = 0; k < 8; ++k) {
      int at = farm::plot_index(tx + dx[k], ty + dy[k]);
      if (at >= 0 && s.plots[at].tilled) {
        n |= 1u << k;
        if (s.plots[at].wet)
          wet_neighbors |= 1u << k;
      }
    }
    if (!p.tilled && !infill(n, 0, 0, 4) && !infill(n, 15, 0, 4) &&
        !infill(n, 0, 15, 4) && !infill(n, 15, 15, 4))
      continue;
    for (int yy = 0; yy < 16; ++yy) {
      int start = -1;
      uint16_t last = 0;
      for (int xx = 0; xx <= 16; ++xx) {
        bool inside = xx < 16 &&
                      (p.tilled ? rounded(n, xx, yy, 4) : infill(n, xx, yy, 4));
        uint16_t c = 0;
        if (inside) {
          auto tone = soil::tone(x + xx, y + yy);
          bool wet = p.wet;
          if (!p.tilled) {
            // Concave fills inherit moisture from the three joining cells.
            unsigned corner = yy < 8 ? (xx < 8 ? N | W | NW : N | E | NE)
                                      : (xx < 8 ? S | W | SW : S | E | SE);
            wet = __builtin_popcount(wet_neighbors & corner) >= 2;
          } else {
            unsigned different = (wet ? ~wet_neighbors : wet_neighbors) & n;
            bool boundary = (xx == 0 && (different & W)) ||
                            (xx == 15 && (different & E)) ||
                            (yy == 0 && (different & N)) ||
                            (yy == 15 && (different & S));
            if (boundary && (tone == 2 || tone == 3))
              wet = !wet; // Broken damp edges, rather than square water tiles.
          }
          const auto &colors = soil::colors(wet);
          c = colors.tones[tone];
          bool edge = (xx == 0 && !(n & W)) || (xx == 15 && !(n & E)) ||
                      (yy == 0 && !(n & N)) || (yy == 15 && !(n & S));
          if (edge)
            c = tone == 2 ? colors.lip : c;
          int margin=4;
          if(!(n&W))margin=std::min(margin,xx);
          if(!(n&E))margin=std::min(margin,15-xx);
          if(!(n&N))margin=std::min(margin,yy);
          if(!(n&S))margin=std::min(margin,15-yy);
          int bx=x+xx-r.ox,by=y+yy-r.oy;
          if(margin<4 && bx>=0 && by>=0 && bx<r.w && by<r.h) {
            int amount=margin==0?5:margin==1?3:margin==2?2:1;
            if(soil::hash((x+xx)/3,(y+yy)/2)%5==0)amount=std::min(6,amount+1);
            c=mix(c,r.p[by*r.w+bx],amount);
          }
          // A shallow planting pocket and irregular front crumbs bind the
          // plant to this exact ground point. They are part of the ground pass.
          int rx = xx - soil::RootX, ry = yy - soil::RootY;
          if (p.crop) {
            if ((ry == -1 && rx >= -2 && rx <= 1) ||
                (ry == 0 && rx >= -1 && rx <= 1))
              c = colors.hole;
            if ((ry == 1 && (rx == -3 || rx == -2 || rx == 2)) ||
                (ry == 0 && rx == 3))
              c = colors.clod;
            if ((ry == 2 && rx == -2) || (ry == 1 && rx == 3))
              c = colors.tones[2];
          }
        }
        if (start >= 0 && (!inside || c != last)) {
          r.rect(x + start, y + yy, xx - start, 1, last);
          start = -1;
        }
        if (inside && start < 0) {
          start = xx;
          last = c;
        }
      }
    }
  }
}
} // namespace terrain
