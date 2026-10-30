// SPDX-License-Identifier: MIT
// Render the actual app at each logical aspect and exercise its hit regions.
#include "../main.cpp"
#include "../ui_canvas.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>
namespace pocket {
int WIDTH = BASE_WIDTH, HEIGHT = BASE_HEIGHT;
static std::vector<uint16_t> buffer;
static std::vector<uint8_t> painted;
static bool screen_mode;
static std::vector<uint16_t> screen(480 * 480);
static starsoil_ui::Canvas ui{screen.data(), 480, 480, 288, 288};
void rect(int x, int y, int w, int h, uint16_t c) {
  if (screen_mode) ui.rect(x, y, w, h, c);
  for (int yy = std::max(0, y); yy < std::min(HEIGHT, y + h); ++yy)
    for (int xx = std::max(0, x); xx < std::min(WIDTH, x + w); ++xx) {
      buffer[yy * WIDTH + xx] = c;
      painted[yy * WIDTH + xx] = 1;
    }
}
void pixel(int x, int y, uint16_t c) { pocket::rect(x, y, 1, 1, c); }
void clear(uint16_t c) {
  if (screen_mode) ui.rect(0, 0, WIDTH, HEIGHT, c);
  std::fill(buffer.begin(), buffer.end(), c);
  std::fill(painted.begin(), painted.end(), 1);
}
void outline(int x, int y, int w, int h, uint16_t c) {
  pocket::rect(x, y, w, 1, c);
  pocket::rect(x, y + h - 1, w, 1, c);
  pocket::rect(x, y, 1, h, c);
  pocket::rect(x + w - 1, y, 1, h, c);
}
void line(int x, int y, int xx, int yy, uint16_t c) {
  int dx = std::abs(xx - x), sx = x < xx ? 1 : -1, dy = -std::abs(yy - y),
      sy = y < yy ? 1 : -1, error = dx + dy;
  for (;;) {
    pixel(x, y, c);
    if (x == xx && y == yy)
      break;
    int e = error * 2;
    if (e >= dy) {
      error += dy;
      x += sx;
    }
    if (e <= dx) {
      error += dx;
      y += sy;
    }
  }
}
void text(int x, int y, const char *s, uint16_t c, int scale) {
  if (screen_mode) {
    // All menu text must stay within the full display after reflow.
    assert(ui.x(x) >= 0 && ui.y(y) >= 0);
    assert(ui.x(x) + starsoil_text::width(s) * ui.font_scale(scale) <= 480);
    assert(ui.y(y) + 12 * ui.font_scale(scale) <= 480);
    ui.text(x, y, s, c, scale);
    return;
  }
  starsoil_text::draw(x, y, s, scale,
                      [&](int xx, int yy) { pixel(xx, yy, c); });
}
int text_width(const char *s, int scale) {
  return screen_mode ? ui.text_width(s, scale) : starsoil_text::width(s, scale);
}
int text_height(int scale) { return screen_mode ? ui.text_height(scale) : 12 * scale; }
int text_line_height() { return screen_mode ? ui.line_height() : 14; }
void begin_ui(bool) {}
void redraw() {}
void save() {}
int save_status() { return 0; }
uint32_t random() { return 1234567; }
uint32_t millis() { return 0; }
void log(const char *) {}
bool held(Key) { return false; }
bool using_controller() { return false; }
static WorldInfo worlds[WorldSlots];
const WorldInfo &world_info(int slot) {
  static WorldInfo empty;
  return slot >= 0 && slot < WorldSlots ? worlds[slot] : empty;
}
int active_world_slot() { return 0; }
bool worlds_busy() { return false; }
int world_error() { return 0; }
void clear_world_error() {}
bool load_world(int) { return false; }
bool delete_world(int) { return false; }
bool create_world(uint32_t, uint8_t) { return false; }
uint16_t *framebuffer() { return buffer.data(); }
void tone(float, float, float) {}
void silence() {}
} // namespace pocket
int main(int argc, char **argv) {
  game::init();
  new_game(s, 1234567);
  session_active = true;
  pocket::worlds[0] = {true, true, 384, 112, 1234567, 1, 0};
  view = DeleteWorld;
  assert(hit(198, 153) == 371 && hit(90, 153) == 370);
  for (int x : {0, 28, 144, 260, 287}) assert(hit(x, 153) == -1);
  view = SeedEditor;
  assert(hit(198, 202) == 363 && hit(90, 202) == 362);
  for (int x : {0, 28, 144, 260, 287}) assert(hit(x, 202) == -1);
  view = Title;
  puts("PASS world-menu action hit regions exclude gaps and backdrop, including irreversible deletion");
  if (argc == 2) {
    // The actual UI compositor and application pages, one 480px profile.
    std::filesystem::path root(argv[1]);
    assert(root.string().find("local/artifacts/") != std::string::npos);
    std::filesystem::create_directories(root);
    pocket::WIDTH = pocket::HEIGHT = 288;
    pocket::screen_mode = true;
    pocket::buffer.resize(288 * 288);
    pocket::painted.resize(288 * 288);
    event_at = 0;
    auto dump = [&](const std::string &name) {
      std::fill(pocket::screen.begin(), pocket::screen.end(), 0);
      game::draw();
      std::ofstream out(root / (name + ".ppm"), std::ios::binary);
      out << "P6\n480 480\n255\n";
      for (uint16_t c : pocket::screen) {
        unsigned char rgb[] = {uint8_t(((c >> 11) & 31) * 255 / 31),
                               uint8_t(((c >> 5) & 63) * 255 / 63),
                               uint8_t((c & 31) * 255 / 31)};
        out.write(reinterpret_cast<char *>(rgb), 3);
      }
    };
    for (bool en : {false, true}) {
      pocket::set_english(en);
      guide_page = npc = 0;
      std::string lang = en ? "en-" : "cn-";
      for (auto v : {Title, World, Bag, Crafting, Gear, Map, Guide, Dialogue, Storage,
                     CreateWorld, SeedEditor, DeleteWorld}) {
        view = v;
        dump(lang + std::to_string(int(v)));
      }
      view = Guide;
      for (guide_page = 0; guide_page < 5; ++guide_page)
        dump(lang + "guide-" + std::to_string(guide_page));
      view = Dialogue;
      for (npc = 0; npc < 4; ++npc)
        dump(lang + "npc-" + std::to_string(npc));
    }
    puts("PASS native 480px UI: all game/world-menu views, five guide pages and four NPCs in both languages; text stays inside the display");
    return 0;
  }
  {
    // Rounding concave terrain adds to an empty quadrant. Solid pixels stay.
    assert(starsoil_terrain::fill_corners(9, 1) == 1);
    assert(starsoil_terrain::fill_corners(9, 0) == 0);
    uint16_t pixels[256];
    std::fill_n(pixels, 256, uint16_t(0xffff));
    int count = 0;
    starsoil_terrain::draw(0, 0, pixels, 255,
                           [&](int, int, uint16_t) { ++count; });
    assert(count == 256);
    assert(connects(Stone, CopperOre) && connects(Grass, Dirt) &&
           !connects(Stone, Air));
    puts("PASS solid terrain is not cut when rounding concave joins");
  }

  {
    State fixture;
    new_game(fixture, 1234567);
    fixture.mobs[0].hp = 10;
    Effects feedback;
    feedback.step(fixture, 16, true);
    uint32_t rng_before = fixture.random;
    add(fixture, Copper, 2);
    ++fixture.mined;
    fixture.hp -= 3;
    fixture.mobs[0].hp -= 3;
    feedback.step(fixture, 16, true);
    assert(fixture.random == rng_before);
    assert(std::any_of(feedback.particles.begin(), feedback.particles.end(),
                       [](auto p) { return p.life > 0; }));
    assert(std::any_of(feedback.pops.begin(), feedback.pops.end(), [](auto p) {
      return p.item == Copper && p.amount == 2;
    }));
    assert(std::count_if(feedback.pops.begin(), feedback.pops.end(),
                         [](auto p) { return !p.item && p.amount == -3; }) ==
           2);
    add(fixture, Copper, 3);
    feedback.step(fixture, 16, true);
    assert(std::any_of(feedback.pops.begin(), feedback.pops.end(), [](auto p) {
      return p.item == Copper && p.amount == 5;
    }));
    feedback.step(fixture, 2000, true);
    assert(std::none_of(feedback.particles.begin(), feedback.particles.end(),
                        [](auto p) { return p.life > 0; }));
    assert(std::none_of(feedback.pops.begin(), feedback.pops.end(),
                        [](auto p) { return p.life > 0; }));
    puts("PASS mining debris, merged pickups, damage feedback, expiry and "
         "unchanged gameplay RNG");
  }
  for (auto [w, h] : {std::pair{299, 224}, std::pair{288, 288},
                      std::pair{336, 224}, std::pair{288, 384}}) {
    pocket::WIDTH = w;
    pocket::HEIGHT = h;
    for (auto v :
         {Title, World, Bag, Crafting, Gear, Map, Guide, Dialogue, Storage,
          CreateWorld, SeedEditor, DeleteWorld}) {
      view = v;
      pocket::buffer.assign(w * h, 0x0123);
      pocket::painted.assign(w * h, 0);
      game::draw();
      assert(std::find(pocket::painted.begin(), pocket::painted.end(), 0) ==
             pocket::painted.end());
      assert(hit(-1, 0) == -1 && hit(w, 0) == -1 && hit(0, h) == -1);
    }
    view = Guide;
    for (int page = 0; page < 5; ++page) {
      guide_page = page;
      game::draw();
    }
    guide_page = 0;
    int ox = (w - 288) / 2, oy = (h - 224) / 2;
    view = Title;
    assert(hit(144 + ox, 208 + oy) == 302);
    assert(hit(90 + ox, 105 + oy) == 310);
    for (auto v : {Bag, Crafting, Gear, Map, Guide}) {
      view = v;
      assert(hit(w - 32, 10) == 99);
      for (int i = 0; i < 5; ++i)
        assert(hit(15 + i * (w - 8) / 5, 35) == 100 + i);
    }
    view = Bag;
    assert(hit(15 + ox, 60 + oy) == 0);
    assert(hit(238 + ox, 115 + oy) == 84);
    assert(hit(50 + ox, h - 12) == 80 && hit(220 + ox, h - 12) == 81);
    view = Crafting;
    assert(hit(160 + ox, h - 12) == 92);
    assert(hit(80 + ox, 60 + oy) == 120);
    view = Gear;
    assert(hit(62 + ox, 115 + oy) == 84);
    for (int i = 0; i < 7; ++i)
      assert(hit(210 + ox, 56 + i * 20 + oy) == 140 + (i < 6 ? i : 7));
    assert(hit(145, h - 12) == 82);
    assert(hit(20 + ox, h - 47) != 82);
    view = World;
    assert(hit(w - 75, 10) == 20);
    assert(hit(w - 40, 23) == 21);
    assert(hit(w * 17 / 288, h - 14) == 10 && hit(w * 55 / 288, h - 14) == 11);
    assert(hit(w * 181 / 288, h - 14) == 14 &&
           hit(w * 248 / 288, h - 14) == 15);
    assert(hit(w / 2, h - 70) == 16);
    std::printf("PASS %dx%d: all 12 views cover the full canvas; header/tabs, "
                "inventory, gear, footer and world touch anchors\n",
                w, h);
  }
  {
    pocket::WIDTH = 288;
    pocket::HEIGHT = 288;
    view = Gear;
    preview_face = true;
    pocket::buffer.assign(288 * 288, 0);
    pocket::painted.assign(288 * 288, 0);
    animation = 0;
    game::draw();
    auto before = pocket::buffer;
    animation = 1800;
    game::draw();
    for (int y = 81; y < 207; ++y)
      for (int x = 8; x < 121; ++x)
        assert(pocket::buffer[y * 288 + x] == before[y * 288 + x]);
    puts("PASS idle wardrobe preview stays still while the global UI clock "
         "advances");
  }
  {
    // Regression: a square canvas places the ground below the old 185px cut.
    pocket::WIDTH = 288;
    pocket::HEIGHT = 288;
    view = World;
    pocket::buffer.assign(288 * 288, 0);
    pocket::painted.assign(288 * 288, 0);
    for (auto &mob : s.mobs)
      mob.hp = 0;
    game::draw();
    auto empty = pocket::buffer;
    auto &mob = s.mobs[0];
    mob = {};
    mob.type = 1;
    mob.hp = 20;
    mob.x = s.player.x + 55;
    mob.y = s.player.y;
    game::draw();
    size_t changed = 0;
    for (int y = 186; y < scene_bottom(); ++y)
      for (int x = 0; x < 288; ++x)
        changed += empty[y * 288 + x] != pocket::buffer[y * 288 + x];
    assert(changed > 25);
    puts("PASS ground-level creatures remain visible below the old 185px "
         "cutoff on a square display");
  }
}
