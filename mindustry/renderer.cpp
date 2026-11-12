// SPDX-License-Identifier: GPL-3.0-only
#ifndef MINDUSTRY_LEGACY_RENDERER
#include "../common/pocket-game/runtime.hpp"
#include "font.hpp"
#include "pxa_font.h"
#include "pxa_raster.h"
#include "render_assets.hpp"
#include "render_backend.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
namespace gfx {
namespace {
constexpr unsigned DrawCapacity = 49152, FontPages = 4, CellsPerPage = 64,
                   CacheSize = FontPages * CellsPerPage, Cell = 32, Page = 256;
constexpr unsigned TerrainFirst = 1, TerrainCount = 9, MinimapSlot = 10,
                   FontFirst = 11;
constexpr uint64_t GlyphToken = 0x4d440000;
constexpr int lights[] = {255, 80, 100, 110, 120, 130, 170, 205, 210, 235};
uint64_t context = 0, clockAge = 0, frameNumber = 0;
uint32_t capabilities = 0;
int width = 320, height = 240;
bool drawing = false;
uint8_t commands[DrawCapacity], upload[65536 + 20], colorIndex[65536];
uint16_t litPalette[10 * 256];
pxa_raster_draw_list_t drawList{};
struct Glyph {
  uint32_t cp = 0;
  uint64_t age = 0;
  uint16_t font = 0, line = 0, base = 0, advance = 0, w = 0, h = 0;
  int16_t ox = 0, oy = 0;
  uint8_t state = 0;
  bool pinned = false;
  uint64_t retry = 0;
};
Glyph glyphs[CacheSize];
uint8_t fontPixels[FontPages][Page * Page];
bool pageDirty[FontPages]{}, pageUsed[FontPages]{}, pageUploaded[FontPages]{};
unsigned inFlight = 0, searchCursor = 0;
uint64_t hits = 0, misses = 0, requests = 0, evictions = 0, uploadCount = 0;
struct TerrainKey {
  uint32_t seed = 0;
  uint8_t sector = 0, version = 0;
  bool title = false, valid = false;
} terrainKey;
bool terrainUploaded[TerrainCount]{};
uint32_t minimapHash = 0;
bool minimapUploaded = false;
uint64_t timingSum = 0, timingMax = 0, timingFrames = 0, lastProfile = 0,
         submitSum = 0;
uint64_t micros() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return uint64_t(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}
uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return uint16_t(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3));
}
uint16_t dim(uint16_t c, unsigned light) {
  return uint16_t(((((c >> 11) & 31) * light / 255) << 11) |
                  ((((c >> 5) & 63) * light / 255) << 5) |
                  ((c & 31) * light / 255));
}
unsigned shadeIndex(int light) {
  for (unsigned i = 0; i < 10; ++i)
    if (lights[i] == light)
      return i;
  return 0;
}
void emitSprite(unsigned slot, unsigned flags, int x, int y, int w, int h,
                int u, int v, int sourceW, int sourceH, uint16_t color = 0) {
  if (!drawing || w <= 0 || h <= 0 || x >= width || y >= height || x + w <= 0 ||
      y + h <= 0)
    return;
  pxa_raster_sprite(&drawList, uint8_t(slot), uint8_t(flags), capabilities,
                    int16_t(x), int16_t(y), uint16_t(w), uint16_t(h),
                    uint16_t(u), uint16_t(v), uint16_t(sourceW),
                    uint16_t(sourceH), color);
}
const md_art::Region *region(const art::Sprite &s, uint16_t tint) {
  // Team colors are precomposed using the original three-color palette.
  if (tint) return nullptr;
  for (auto &r : md_art::regions)
    if (r.sprite == &s)
      return &r;
  return nullptr;
}
bool load_baked(Glyph &g, unsigned index) {
  unsigned first = 0,
           last = sizeof(md_font::glyphs) / sizeof(md_font::glyphs[0]);
  uint32_t key = g.cp * 32 + g.font;
  while (first < last) {
    unsigned middle = (first + last) / 2;
    auto &candidate = md_font::glyphs[middle];
    if (candidate.cp * 32 + candidate.size < key)
      first = middle + 1;
    else
      last = middle;
  }
  if (first >= sizeof(md_font::glyphs) / sizeof(md_font::glyphs[0]))
    return false;
  auto &b = md_font::glyphs[first];
  if (b.cp != g.cp || b.size != g.font)
    return false;
  g.line = b.line;
  g.base = b.base;
  g.advance = b.advance;
  g.w = b.w;
  g.h = b.h;
  g.ox = b.ox;
  g.oy = b.oy;
  unsigned page = index / CellsPerPage, cell = index % CellsPerPage;
  unsigned xx = cell % 8 * Cell, yy = cell / 8 * Cell;
  for (unsigned y = 0; y < Cell; ++y)
    std::memset(fontPixels[page] + (yy + y) * Page + xx, 0, Cell);
  unsigned at = b.offset, end = at + b.bytes, pixel = 0;
  while (at < end) {
    uint8_t control = md_font::data[at++];
    unsigned count = (control & 127) + 1;
    uint8_t repeated = control & 128 ? md_font::data[at++] : 0;
    for (unsigned n = 0; n < count; ++n) {
      uint8_t c = control & 128 ? repeated : md_font::data[at++];
      fontPixels[page][(yy + pixel / b.w) * Page + xx + pixel % b.w] = c;
      ++pixel;
    }
  }
  g.state = 3;
  pageDirty[page] = true;
  return true;
}
Glyph *lookup(uint32_t cp, unsigned font) {
  Glyph *victim = nullptr;
  for (auto &g : glyphs) {
    if (g.state && g.cp == cp && g.font == font) {
      g.pinned = true;
      g.age = ++clockAge;
      ++hits;
      return &g;
    }
    if (!g.pinned && g.state != 2 &&
        (!victim || (!g.state && victim->state) ||
         (g.state && victim->state && g.age < victim->age)))
      victim = &g;
  }
  ++misses;
  if (!victim)
    return nullptr;
  if (victim->state)
    ++evictions;
  *victim = Glyph{};
  victim->cp = cp;
  victim->font = uint16_t(font);
  victim->state = 1;
  victim->pinned = true;
  victim->age = ++clockAge;
  load_baked(*victim, unsigned(victim - glyphs));
  return victim;
}
int advance(Glyph *g, uint32_t cp, int font) {
  return g && g->state == 3 ? g->advance : cp < 128 ? (font * 3 + 2) / 5 : font;
}
void glyphSprite(Glyph &g, int x, int y, uint16_t color, int clipLeft,
                 int clipRight) {
  if (!g.w || !g.h)
    return;
  unsigned index = unsigned(&g - glyphs), page = index / CellsPerPage,
           cell = index % CellsPerPage;
  int u = int(cell % 8) * Cell, v = int(cell / 8) * Cell, w = g.w, h = g.h;
  if (x < clipLeft) {
    int n = clipLeft - x;
    x += n;
    u += n;
    w -= n;
  }
  if (x + w > clipRight)
    w = clipRight - x;
  if (y < 0) {
    int n = -y;
    v += n;
    h -= n;
    y = 0;
  }
  if (y + h > height)
    h = height - y;
  if (w <= 0 || h <= 0)
    return;
  pageUsed[page] = true;
  emitSprite(FontFirst + page,
             PXA_RASTER_SPRITE_TRANSPARENT_INDEX0 |
                 PXA_RASTER_SPRITE_SOLID_COLOR | PXA_RASTER_SPRITE_TEXEL_ALPHA,
             x, y, w, h, u, v, w, h, color);
}
void buildChunk(const md::State &s, int chunk, bool title) {
  int chunkX = chunk % 3, chunkY = chunk / 3;
  uint8_t *out = upload + 20;
  std::memset(out, colorIndex[rgb(23, 24, 28)], 65536);
  auto blit = [&](const art::Sprite &image, int x, int y, int w, int h,
                  int light) {
    int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(256, x + w),
        y1 = std::min(256, y + h);
    for (int py = y0; py < y1; ++py)
      for (int px = x0; px < x1; ++px) {
        uint32_t c =
            image.p[(py - y) * image.h / h * image.w + (px - x) * image.w / w];
        if (c >> 16)
          out[py * 256 + px] = colorIndex[dim(uint16_t(c), unsigned(light))];
      }
  };
  for (int y = std::max(0, chunkY * 16 - 1);
       y < std::min(md::H, (chunkY + 1) * 16 + 1); ++y)
    for (int x = chunkX * 16; x < (chunkX + 1) * 16; ++x) {
      auto &t = s.tiles[y * md::W + x];
      auto n = md::noise(s.seed, x, y);
      int px = (x - chunkX * 16) * 16, py = (y - chunkY * 16) * 16;
      auto &floor = t.ground == md::Sandy   ? art::sand1
                    : t.ground == md::Water ? art::water
                    : s.sector == 1         ? art::snow
                    : n % 3 == 0            ? art::stone1
                    : n % 3 == 1            ? art::stone2
                                            : art::stone3;
      blit(floor, px, py, 16, 16, title ? 110 : 205);
      if (t.ore < 3 || t.ore == md::Titanium)
        blit(t.ore == md::Copper     ? art::copper
             : t.ore == md::Lead     ? art::lead
             : t.ore == md::Titanium ? art::titanium
                                     : art::coal,
             px, py, 16, 16, title ? 110 : 235);
      if (t.ground == md::Rock)
        blit(art::rock, px, py - 16 / 3, 16, 16 * 4 / 3, title ? 100 : 210);
    }
  auto status =
      pxa_raster_upload_texture_index8(context, uint8_t(TerrainFirst + chunk),
                                       256, 256, out, upload, sizeof(upload));
  if (status < 0) {
    drawList.status = status;
    return;
  }
  terrainUploaded[chunk] = true;
  ++uploadCount;
}
} // namespace
bool attach(uint64_t handle, uint32_t caps) {
  context = handle;
  capabilities = caps;
  uint32_t required =
      PXA_RASTER_CAP_TEXTURED_QUAD | PXA_RASTER_CAP_PAINTER_POLYGON |
      PXA_RASTER_CAP_AFFINE_UV | PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA;
  if ((caps & required) != required) {
    pxa_log_write(
        3,
        "Mindustry: Host lacks required GameRender glyph/painter capabilities");
    return false;
  }
  std::memset(colorIndex, 0, sizeof(colorIndex));
  for (int i = 1; i < 256; ++i)
    colorIndex[md_art::palette[i]] = uint8_t(i);
  for (int l = 0; l < 10; ++l)
    for (int i = 0; i < 256; ++i)
      litPalette[l * 256 + i] = dim(md_art::palette[i], lights[l]);
  if (pxa_raster_upload_lit_palette_rgb565(context, 10, litPalette, upload,
                                           sizeof(upload)) < 0)
    return false;
  if (pxa_raster_upload_texture_index8(context, 0, 256, 256, md_art::atlas,
                                       upload, sizeof(upload)) < 0)
    return false;
  terrainKey.valid = false;
  std::fill(terrainUploaded, terrainUploaded + TerrainCount, false);
  minimapUploaded = false;
  for (unsigned p = 0; p < FontPages; ++p) {
    pageUploaded[p] = false;
    pageDirty[p] = true;
  }
  return true;
}
void detach() {
  context = 0;
  drawing = false;
}
bool begin(uint64_t frame, int w, int h) {
  if (!context)
    return false;
  width = w;
  height = h;
  frameNumber = frame;
  drawing = true;
  for (auto &g : glyphs)
    g.pinned = false;
  std::fill(pageUsed, pageUsed + FontPages, false);
  pxa_raster_draw_list_begin(&drawList, commands, sizeof(commands), frame);
  return true;
}
void clear(uint16_t c) {
  if (drawing)
    pxa_raster_clear(&drawList, c);
}
void rect(int x, int y, int w, int h, uint16_t color) {
  if (!drawing || w <= 0 || h <= 0)
    return;
  int x1 = std::min(width, x + w), y1 = std::min(height, y + h);
  x = std::max(0, x);
  y = std::max(0, y);
  if (x1 <= x || y1 <= y)
    return;
  int16_t xy[] = {int16_t(x * 16), int16_t(y * 16),  int16_t(x1 * 16),
                  int16_t(y * 16), int16_t(x1 * 16), int16_t(y1 * 16),
                  int16_t(x * 16), int16_t(y1 * 16)};
  pxa_raster_flat_quad(&drawList, xy, color);
}
void line(int x, int y, int xx, int yy, uint16_t color) {
  if (!drawing)
    return;
  // World-space endpoints can be thousands of pixels off screen at high zoom.
  // Clip before converting to signed Q4 coordinates, including shot/range
  // lines.
  float ax = float(x), ay = float(y), dx0 = float(xx) - ax,
        dy0 = float(yy) - ay;
  float enter = 0, leave = 1;
  auto edge = [&](float p, float q) {
    if (p == 0)
      return q >= 0;
    float t = q / p;
    if (p < 0) {
      if (t > leave)
        return false;
      enter = std::max(enter, t);
    } else {
      if (t < enter)
        return false;
      leave = std::min(leave, t);
    }
    return true;
  };
  if (!edge(-dx0, ax) || !edge(dx0, width - 1 - ax) || !edge(-dy0, ay) ||
      !edge(dy0, height - 1 - ay))
    return;
  x = int(std::lround(ax + enter * dx0));
  y = int(std::lround(ay + enter * dy0));
  xx = int(std::lround(ax + leave * dx0));
  yy = int(std::lround(ay + leave * dy0));
  if (x == xx) {
    rect(x, std::min(y, yy), 1, std::abs(y - yy) + 1, color);
    return;
  }
  if (y == yy) {
    rect(std::min(x, xx), y, std::abs(x - xx) + 1, 1, color);
    return;
  }
  float dx = float(xx - x), dy = float(yy - y), length = std::hypot(dx, dy);
  float nx = -dy / length * .5f, ny = dx / length * .5f;
  int16_t xy[] = {int16_t(std::lround((x + .5f + nx) * 16)),
                  int16_t(std::lround((y + .5f + ny) * 16)),
                  int16_t(std::lround((xx + .5f + nx) * 16)),
                  int16_t(std::lround((yy + .5f + ny) * 16)),
                  int16_t(std::lround((xx + .5f - nx) * 16)),
                  int16_t(std::lround((yy + .5f - ny) * 16)),
                  int16_t(std::lround((x + .5f - nx) * 16)),
                  int16_t(std::lround((y + .5f - ny) * 16))};
  pxa_raster_flat_quad(&drawList, xy, color);
}
void sprite(const art::Sprite &s, int x, int y, int w, int h, int rotation,
            int light, uint16_t tint, bool flipY) {
  if (!drawing || w <= 0 || h <= 0 || x >= width || y >= height || x + w <= 0 ||
      y + h <= 0)
    return;
  auto *r = region(s, tint);
  if (!r) {
    drawList.status = PXA_STATUS_NOT_FOUND;
    return;
  }
  if (!rotation && !flipY && light == 255) {
    emitSprite(0, PXA_RASTER_SPRITE_TRANSPARENT_INDEX0, x, y, w, h, r->x, r->y,
               r->w, r->h);
    return;
  }
  // Painter UVs rotate the texture, with light selecting an exact RGB565 row.
  int uv[4][2] = {{r->x, r->y},
                  {r->x + r->w, r->y},
                  {r->x + r->w, r->y + r->h},
                  {r->x, r->y + r->h}};
  int xy[4][2] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
  pxa_raster_vertex_t v[4]{};
  for (int i = 0; i < 4; ++i) {
    int j = (i + rotation) % 4;
    v[i].x_q4 = int16_t(xy[i][0] * 16);
    v[i].y_q4 = int16_t(xy[i][1] * 16);
    v[i].u_q4 = int16_t(uv[j][0] * 16);
    v[i].v_q4 = int16_t((flipY ? 2 * r->y + r->h - uv[j][1] : uv[j][1]) * 16);
    v[i].light = uint8_t(shadeIndex(light));
  }
  pxa_raster_textured_quad_flags(&drawList, v, 0,
                                 PXA_RASTER_QUAD_PAINTER |
                                     PXA_RASTER_QUAD_AFFINE_UV |
                                     PXA_RASTER_QUAD_TRANSPARENT_INDEX0);
}
void icon(const art::Sprite &s, int x, int y, int w, int h, uint16_t color) {
  auto *r = region(s, 0);
  if (!r) {
    drawList.status = PXA_STATUS_NOT_FOUND;
    return;
  }
  emitSprite(
      0, PXA_RASTER_SPRITE_TRANSPARENT_INDEX0 | PXA_RASTER_SPRITE_SOLID_COLOR,
      x, y, w, h, r->x, r->y, r->w, r->h, color);
}
void terrain(const md::State &s, int ox, int oy, int zoom, bool title) {
  if (!drawing)
    return;
  if (!terrainKey.valid || terrainKey.seed != s.seed ||
      terrainKey.sector != s.sector || terrainKey.version != s.terrainVersion ||
      terrainKey.title != title) {
    terrainKey = {s.seed, s.sector, s.terrainVersion, title, true};
    std::fill(terrainUploaded, terrainUploaded + TerrainCount, false);
  }
  for (int c = 0; c < 9; ++c) {
    int x = ox + (c % 3) * 16 * zoom, y = oy + (c / 3) * 16 * zoom,
        size = 16 * zoom;
    if (x >= width || y >= height || x + size <= 0 || y + size <= 0)
      continue;
    if (!terrainUploaded[c])
      buildChunk(s, c, title);
    emitSprite(TerrainFirst + c, 0, x, y, size, size, 0, 0, 256, 256);
  }
}
void minimap(const md::State &s, int x, int y, int scale) {
  uint32_t hash = 2166136261u;
  for (auto &t : s.tiles)
    hash = (hash ^ t.ground ^ uint32_t(t.ore) << 8) * 16777619u;
  for (auto &b : s.buildings)
    if (b.type)
      hash = (hash ^ b.type ^ uint32_t(b.x) << 8 ^ uint32_t(b.y) << 16) *
             16777619u;
  if (!minimapUploaded || minimapHash != hash) {
    uint8_t pixels[md::W * md::H];
    for (int yy = 0; yy < md::H; ++yy)
      for (int xx = 0; xx < md::W; ++xx) {
        auto &t = s.tiles[yy * md::W + xx];
        uint16_t c = t.ground == md::Water  ? rgb(55, 81, 119)
                     : t.ground == md::Rock ? rgb(32, 33, 39)
                     : t.ore == md::Copper  ? rgb(146, 104, 67)
                                            : rgb(83, 83, 90);
        pixels[yy * md::W + xx] = colorIndex[c];
      }
    for (auto &b : s.buildings)
      if (b.type)
        for (int dy = 0; dy < md::defs[b.type].size; ++dy)
          for (int dx = 0; dx < md::defs[b.type].size; ++dx)
            pixels[(b.y + dy) * md::W + b.x + dx] =
                colorIndex[rgb(255, 211, 127)];
    auto status = pxa_raster_upload_texture_index8(
        context, MinimapSlot, md::W, md::H, pixels, upload, sizeof(upload));
    if (status < 0)
      drawList.status = status;
    else {
      minimapHash = hash;
      minimapUploaded = true;
      ++uploadCount;
    }
  }
  emitSprite(MinimapSlot, 0, x, y, md::W * scale, md::H * scale, 0, 0, md::W,
             md::H);
}
int font_size(int requested) {
  return requested <= 13 ? 12 : requested <= 18 ? 16 : 24;
}
void preload(const char *text, int font) {
  if (!text)
    return;
  while (*text) {
    auto cp = pocket_ui::next_utf8(text);
    auto *g = lookup(cp, unsigned(font_size(font)));
    if (g)
      g->pinned = false;
  }
}
void label(int x, int y, int w, const char *value, uint16_t color, int font,
           int align) {
  if (!drawing || !value || w <= 0)
    return;
  font = font_size(font);
  int pen = x, rowY = y - font / 4;
  const char *s = value;
  // Layout uses the system advances; missing glyphs queue asynchronous
  // requests.
  while (*s) {
    const char *lineStart = s, *lineEnd = s;
    int lineWidth = 0;
    bool complete = true;
    while (*s && *s != '\n') {
      const char *next = s;
      auto cp = pocket_ui::next_utf8(next);
      auto *g = lookup(cp, unsigned(font));
      int a = advance(g, cp, font);
      if (!g || g->state != 3)
        complete = false;
      if (lineWidth + a > w && s != lineStart)
        break;
      lineWidth += a;
      s = next;
      lineEnd = s;
      if (lineWidth > w)
        break;
    }
    if (lineEnd == lineStart) {
      pocket_ui::next_utf8(s);
      lineEnd = s;
    }
    pen = x + (align ? std::max(0, (w - lineWidth) / 2) : 0);
    while (lineStart < lineEnd) {
      uint32_t cp = pocket_ui::next_utf8(lineStart);
      auto *g = lookup(cp, unsigned(font));
      int a = advance(g, cp, font);
      if (complete && g && g->state == 3)
        glyphSprite(*g, pen + g->ox, rowY + g->line - g->base - g->h - g->oy,
                    color, x, x + w);
      pen += a;
    }
    if (*s == '\n')
      ++s;
    rowY += font + 2;
    if (rowY > y + font * 2 + 4 || rowY >= height)
      break;
  }
}
bool fonts_pending() {
  for (auto &g : glyphs)
    if (g.pinned && g.state && g.state != 3)
      return true;
  return false;
}
void service_fonts(unsigned max_requests) {
  if (!context)
    return;
  uint64_t now = micros();
  unsigned submitted = 0;
  for (unsigned pass = 0; pass < 2; ++pass)
    for (unsigned n = 0;
         n < CacheSize && inFlight < 32 && submitted < max_requests; ++n) {
      unsigned i = searchCursor++ % CacheSize;
      auto &g = glyphs[i];
      if (g.state != 1 || now < g.retry || g.pinned != (pass == 0))
        continue;
      auto status = pxa_font_request_glyph(GlyphToken + i, g.cp, g.font);
      if (!status) {
        g.state = 2;
        ++inFlight;
        ++requests;
        ++submitted;
      } else {
        // A reply immediately frees the reservation. Do not impose an extra
        // 20 ms delay after queue backpressure; the event handler refills it.
        g.retry = (status == PXA_STATUS_RESOURCE_LIMIT ||
                   status == PXA_STATUS_LIMIT_EXCEEDED)
                      ? 0
                      : now + 1000000;
        // FONT_GLYPH reserves a full-size reply. Yield to the event drain when
        // the bounded completion queue fills, then continue on the next loop.
        return;
      }
    }
}
bool font_event(const pxa_event_t &event) {
  if (event.service != PXA_FONT_SERVICE || event.opcode != PXA_FONT_GLYPH ||
      event.token < GlyphToken || event.token >= GlyphToken + CacheSize)
    return false;
  auto index = unsigned(event.token - GlyphToken);
  auto &g = glyphs[index];
  if (g.state != 2)
    return true;
  if (inFlight)
    --inFlight;
  pxa_font_glyph_t glyph{};
  if (!pxa_font_parse_glyph(&event, event.token, &glyph) || glyph.status ||
      glyph.width > Cell || glyph.height > Cell) {
    static unsigned failures = 0;
    if (failures++ < 8) {
      char message[120];
      snprintf(message, sizeof(message),
               "Mindustry: glyph failed cp=%u size=%u status=%ld w=%u h=%u",
               g.cp, g.font, (long)glyph.status, glyph.width, glyph.height);
      pxa_log_write(3, message);
    }
    g.state = 1;
    g.retry = micros() + 1000000;
    return true;
  }
  g.line = glyph.line_height;
  g.base = glyph.baseline;
  g.advance = glyph.advance;
  g.w = glyph.width;
  g.h = glyph.height;
  g.ox = glyph.offset_x;
  g.oy = glyph.offset_y;
  g.state = 3;
  unsigned page = index / CellsPerPage, cell = index % CellsPerPage,
           xx = cell % 8 * Cell, yy = cell / 8 * Cell;
  for (unsigned y = 0; y < Cell; ++y)
    std::memset(fontPixels[page] + (yy + y) * Page + xx, 0, Cell);
  for (unsigned y = 0; y < g.h; ++y)
    std::memcpy(fontPixels[page] + (yy + y) * Page + xx,
                glyph.coverage + y * g.w, g.w);
  pageDirty[page] = true;
  return true;
}
int32_t finish() {
  if (!drawing)
    return PXA_STATUS_BAD_STATE;
  for (unsigned p = 0; p < FontPages; ++p)
    if (pageUsed[p] && (pageDirty[p] || !pageUploaded[p])) {
      int32_t status = pxa_raster_upload_texture_index8(
          context, uint8_t(FontFirst + p), Page, Page, fontPixels[p], upload,
          sizeof(upload));
      if (status < 0) {
        drawing = false;
        return status;
      }
      pageDirty[p] = false;
      pageUploaded[p] = true;
      ++uploadCount;
    }
  bool ready = true;
  for (auto &g : glyphs)
    if (g.pinned && g.state != 3)
      ready = false;
  static bool announced = false;
  if (ready && !announced) {
    pxa_log_write(2, "Mindustry: glyph cache ready");
    announced = true;
  }
  drawing = false;
  int32_t status =
      drawList.status ? drawList.status : pxa_raster_submit(context, &drawList);
  return status;
}
void profile(uint64_t build_us, uint64_t submit_us) {
  submitSum += submit_us;
  timingSum += build_us;
  timingMax = std::max(timingMax, build_us);
  ++timingFrames;
#ifdef MINDUSTRY_PROFILE
  uint64_t now = micros();
  if (!lastProfile)
    lastProfile = now;
  if (now - lastProfile < 2000000)
    return;
  pxa_game_render_telemetry_t t{};
  pxa_game_render_query_telemetry(context, &t);
  char message[320];
  snprintf(
      message, sizeof(message),
      "Mindustry perf frames=%llu cpu_us=%llu max_us=%llu host_us=%u "
      "draw_bytes=%u glyph_hits=%llu glyph_misses=%llu glyph_requests=%llu "
      "evictions=%llu uploads=%llu visible=%llu submit_us=%llu",
      (unsigned long long)timingFrames,
      (unsigned long long)(timingSum / std::max<uint64_t>(1, timingFrames)),
      (unsigned long long)timingMax, t.last_host_raster_us,
      t.last_draw_list_bytes, (unsigned long long)hits,
      (unsigned long long)misses, (unsigned long long)requests,
      (unsigned long long)evictions, (unsigned long long)uploadCount,
      (unsigned long long)t.visible_frames,
      (unsigned long long)(submitSum / std::max<uint64_t>(1, timingFrames)));
  pxa_log_write(2, message);
  lastProfile = now;
  timingSum = timingMax = timingFrames = submitSum = 0;
#endif
}
} // namespace gfx
#endif
