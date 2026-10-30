// SPDX-License-Identifier: MIT
// Checks the actual production compositor, then exports uncropped audit sheets.
#include "../actor_renderer.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
using namespace starsoil;
using namespace starsoil_actor;
namespace {
struct Pixel {
  uint16_t color = 0;
  Tag tag = Tag::Empty;
  uint8_t index = 0;
};
struct Canvas {
  int width = 64, height = 64;
  std::vector<Pixel> pixels;
  std::vector<Pixel> assembly;
  std::vector<Pixel> trunk;
  bool main_grip = false, off_grip = false;
  Canvas(int scale = 1)
      : width(64 * scale), height(64 * scale), pixels(width * height) {}
  const Pixel &at(Point p) const { return pixels.at(p.y * width + p.x); }
  const Pixel &body_at(Point p) const { return assembly.at(p.y * width + p.x); }
  const Pixel &trunk_at(Point p) const { return trunk.at(p.y * width + p.x); }
};
using Outfit = std::array<uint8_t, 8>;
size_t cases = 0;
std::string where;
void require(bool ok, const char *why) {
  if (!ok)
    throw std::runtime_error(where + ": " + why);
}
Canvas render(const Outfit &outfit, int frame, bool face, int held = 0,
              bool active = false, int scale = 1, int light = 255, int skin = 0,
              int action = -1) {
  Canvas c(scale);
  Appearance a{32 * scale, 48 * scale, frame,  scale,         light,
               skin,       face,       active, outfit.data(), uint8_t(held)};
  a.action_frame = action;
  auto p = pose(uint8_t(held), active);
  Point hand = world(a, joint(a, starsoil_actor::Hand)),
        off = world(a, joint(a, Offhand));
  draw(a, [&](int x, int y, uint16_t color, Tag tag, uint8_t index) {
    require(x >= 0 && y >= 0 && x < c.width && y < c.height,
            "sprite outside bounded canvas");
    if (tag == Tag::Weapon && c.assembly.empty())
      c.assembly = c.pixels;
    if (tag == Tag::Arm && c.trunk.empty())
      c.trunk = c.pixels;
    c.pixels[y * c.width + x] = {color, tag, index};
    if (tag == Tag::Weapon && x == hand.x && y == hand.y)
      c.main_grip = true;
    if (tag == Tag::Weapon && x == off.x && y == off.y)
      c.off_grip = true;
  });
  if (c.assembly.empty())
    c.assembly = c.pixels;
  if (c.trunk.empty())
    c.trunk = c.assembly;
  ++cases;
  return c;
}
bool anatomy(Tag tag) {
  return tag != Tag::Empty && tag != Tag::Weapon && tag != Tag::Grip;
}
void connected(const Canvas &c) {
  std::vector<uint8_t> seen(c.pixels.size());
  std::vector<int> q;
  size_t total = 0;
  for (size_t i = 0; i < c.assembly.size(); ++i)
    if (anatomy(c.assembly[i].tag)) {
      ++total;
      if (q.empty()) {
        q.push_back(int(i));
        seen[i] = 1;
      }
    }
  for (size_t i = 0; i < q.size(); ++i) {
    int x = q[i] % c.width, y = q[i] / c.width;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int xx = x + dx, yy = y + dy;
        if (xx < 0 || yy < 0 || xx >= c.width || yy >= c.height)
          continue;
        size_t n = yy * c.width + xx;
        if (!seen[n] && anatomy(c.assembly[n].tag)) {
          seen[n] = 1;
          q.push_back(int(n));
        }
      }
  }
  require(total == q.size(),
          "detached body, garment, headgear or accessory pixels");
}
void landmarks(const Canvas &c, const Outfit &outfit, int frame, bool face,
               int held, bool active, int scale = 1) {
  Appearance a{32 * scale, 48 * scale, frame,  scale,         255,
               0,          face,       active, outfit.data(), uint8_t(held)};
  Pose p = pose(uint8_t(held), active);
  {
    int y = joint(a, Neck).y;
    int skin = 0;
    for (int x = 9; x <= 11; ++x) {
      const auto &v = c.trunk_at(world(a, {x, y}));
      skin +=
          v.tag == Tag::Neck && (v.index == 2 || v.index == 3 || v.index == 12);
    }
    require(skin >= 2, "neck is not visibly joined from jaw to collar");
    require(c.trunk_at(world(a, {10, y + 1})).tag != Tag::Neck &&
                c.trunk_at(world(a, {10, y + 2})).tag != Tag::Neck,
            "exposed neck extends into the shirt/chest");
  }
  Point far = joint(a, FarFoot), near = joint(a, NearFoot);
  require(std::abs(near.x - far.x) + std::abs(near.y - far.y) >= 2,
          "two foot anchors collapse into the same point");
  for (Joint j : {FarFoot, NearFoot}) {
    const auto &foot = c.body_at(world(a, joint(a, j)));
    require(foot.tag == Tag::Body || foot.tag == Tag::Boots ||
                foot.tag == Tag::Greaves || foot.tag == Tag::RearLeg ||
                foot.tag == Tag::CameraLeg || foot.tag == Tag::RearGreaves ||
                foot.tag == Tag::RearBoots,
            "foot hidden or detached");
    require(foot.index != 1 || (j == FarFoot && (foot.tag == Tag::CameraLeg ||
                                                 foot.tag == Tag::Boots ||
                                                 foot.tag == Tag::Greaves)),
            "only an outline remains at an unoccluded foot");
  }
  for (Joint j :
       {Shoulder, Elbow, starsoil_actor::Hand, FarShoulder, FarElbow, Offhand})
    require(c.body_at(world(a, joint(a, j))).tag != Tag::Empty,
            "shoulder/elbow/wrist joint is missing");
  for (Joint j : {starsoil_actor::Hand, Offhand}) {
    const auto &hand = c.body_at(world(a, joint(a, j)));
    bool skin = (hand.tag == Tag::Arm || hand.tag == Tag::RearArm) &&
                (hand.index == 2 || hand.index == 3 || hand.index == 12);
    bool rear_occluded = j == Offhand && p != BowAim &&
                         body_frame(a) < art::WalkFrames && anatomy(hand.tag);
    require(skin || rear_occluded,
            "unoccluded hand is replaced by a garment/outline");
  }
  for (bool camera : {true, false}) {
    Point shoulder = joint(a, camera ? CameraShoulder : RearShoulder);
    Point elbow = joint(a, camera ? CameraElbow : RearElbow);
    Point hand = joint(a, camera ? CameraHand : RearHand);
    double upper = std::hypot(elbow.x - shoulder.x, elbow.y - shoulder.y);
    double fore = std::hypot(hand.x - elbow.x, hand.y - elbow.y);
    require(std::abs(upper - (camera ? 4.25 : 3.5)) <= .65 &&
                std::abs(fore - (camera ? 4.25 : 3.8)) <= .65,
            "upper arm/forearm length changes between poses");
  }
  for (auto chain :
       {std::array<Joint, 4>{FarHip, FarKnee, FarAnkle, FarFoot},
        std::array<Joint, 4>{NearHip, NearKnee, NearAnkle, NearFoot}}) {
    for (size_t i = 0; i < chain.size(); ++i) {
      Point v = joint(a, chain[i]);
      if (i)
        require(joint(a, chain[i - 1]).y < v.y,
                "hip/knee/ankle/foot collapses or bends back during a step");
      const auto &part = c.trunk_at(world(a, v));
      bool robe = outfit[3] == SageRobe && part.tag == Tag::Clothing;
      bool hidden = chain[0] == FarHip &&
                    (part.tag == Tag::CameraLeg || part.tag == Tag::Boots ||
                     part.tag == Tag::Greaves);
      require(part.tag != Tag::Empty && (part.index != 1 || robe || hidden),
              ("leg joint " + std::to_string(int(chain[i])) +
               " is hollow or reduced to an outline (tag " +
               std::to_string(int(part.tag)) + ")")
                  .c_str());
    }
  }
  // Check the trunk itself, so a cape or accessory cannot bridge a missing
  // chest/waist and make a whole-assembly connectivity check pass accidentally.
  for (int y = joint(a, Neck).y + 1; y <= joint(a, NearHip).y; ++y) {
    int solid = 0;
    for (int x = 8; x <= 12; ++x) {
      const auto &v = c.trunk_at(world(a, {x, y}));
      solid += anatomy(v.tag) && v.tag != Tag::Cape &&
               v.tag != Tag::Accessory && v.index != 1;
    }
    require(solid >= 2, ("chest/abdomen/pelvis row " + std::to_string(y) +
                         " narrows to a gap or outline")
                            .c_str());
  }
  if (held) {
    const auto *grip = art::item_grips[held];
    require(art::icons[held][grip[1] * 16 + grip[0]] != 0,
            "item grip source pixel is transparent");
    require(c.main_grip, "held item's opaque handle does not reach the palm");
    const auto &hand = c.at(world(a, joint(a, starsoil_actor::Hand)));
    require(hand.tag == Tag::Grip && hand.index == 0,
            "gripping fingers are obscured");
    if (p == BowAim) {
      const auto *off = art::item_offhands[held];
      Point h = joint(a, starsoil_actor::Hand), o = joint(a, Offhand);
      require(art::icons[held][off[1] * 16 + off[0]] != 0,
              "bow string source pixel is transparent");
      require(h.x - o.x == off[0] - grip[0] && h.y - o.y == off[1] - grip[1],
              "bow/string hand spacing differs from authored grips");
      require(c.off_grip, "bow string does not reach the second hand");
      require(c.at(world(a, joint(a, Offhand))).tag == Tag::Grip,
              "bow's second hand is obscured");
    }
  }
}
void mirrored(const Canvas &a, const Canvas &b) {
  require(a.width == b.width, "mirror dimensions");
  for (int y = 0; y < a.height; ++y)
    for (int x = 0; x < a.width; ++x) {
      const auto &p = a.pixels[y * a.width + x],
                 &q = b.pixels[y * b.width + b.width - 1 - x];
      require(p.color == q.color && p.tag == q.tag && p.index == q.index,
              "left/right body or weapon transform differs");
    }
}
std::vector<Outfit> equipment_classes() {
  std::vector<Outfit> result;
  for (int head : {0, int(StrawHat), int(CopperHelmet), int(IronHelmet),
                   int(CrystalHelmet)})
    for (int body : {0, int(CrystalMail)})
      for (int legs : {0, int(CrystalGreaves)})
        for (int shirt : {0, int(BlueTunic), int(SageRobe)})
          for (int cape : {0, int(EmberCape)})
            for (int boots : {0, int(CloudBoots)})
              for (int charm : {0, int(GlowCharm)})
                result.push_back({uint8_t(head), uint8_t(body), uint8_t(legs),
                                  uint8_t(shirt), uint8_t(cape), uint8_t(boots),
                                  0, uint8_t(charm)});
  for (int id = 1; id < ItemCount; ++id)
    if (equip_slot(items[id].kind) >= 0) {
      Outfit o{};
      o[equip_slot(items[id].kind)] = uint8_t(id);
      result.push_back(o);
    }
  return result;
}
void arm_visibility_and_light(const Canvas &c, const Outfit &outfit, int frame,
                              bool face) {
  Appearance a{32, 48, frame, 1, 255, 0, face, false, outfit.data(), 0};
  auto luminance = [](uint16_t v) {
    return ((v >> 11) & 31) * 6 + ((v >> 5) & 63) * 6 + (v & 31) * 2;
  };
  const auto &near = c.body_at(world(a, joint(a, CameraHand)));
  const auto &far = c.body_at(world(a, joint(a, RearHand)));
  require(near.tag == Tag::Arm && far.tag == Tag::RearArm,
          "idle arm identity lost behind trunk");
  int camera = luminance(near.color), rear = luminance(far.color);
  require(rear * 100 >= camera * 85 && rear < camera,
          "camera-facing arm must be brighter than rear arm");
  auto n = world(a, joint(a, CameraShoulder)),
       r = world(a, joint(a, RearShoulder));
  require(face ? n.x < r.x : n.x > r.x,
          "camera-facing shoulder swaps sides without a whole-actor turn");
}
void write_ppm(const std::filesystem::path &path, int w, int h,
               const std::vector<uint16_t> &pixels) {
  FILE *f = std::fopen(path.c_str(), "wb");
  if (!f)
    throw std::runtime_error("cannot open gallery");
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (auto c : pixels) {
    uint8_t rgb[3] = {uint8_t(((c >> 11) & 31) * 255 / 31),
                      uint8_t(((c >> 5) & 63) * 255 / 63),
                      uint8_t((c & 31) * 255 / 31)};
    std::fwrite(rgb, 1, 3, f);
  }
  std::fclose(f);
}
void galleries(const std::filesystem::path &root) {
  std::filesystem::create_directories(root);
  const std::array<Outfit, 6> looks = {
      Outfit{StrawHat, 0, 0, BlueTunic, TealCape, SwiftBoots, 0, 0},
      Outfit{CopperHelmet, CopperMail, CopperGreaves, RedTunic, EmberCape,
             SwiftBoots, 0, GlowCharm},
      Outfit{IronHelmet, IronMail, IronGreaves, BlueTunic, TealCape, SwiftBoots,
             0, Magnet},
      Outfit{CrystalHelmet, CrystalMail, CrystalGreaves, RedTunic, EmberCape,
             CloudBoots, 0, GlowCharm},
      Outfit{StrawHat, 0, 0, SageRobe, TealCape, CloudBoots, 0, Magnet},
      Outfit{0, 0, 0, RedTunic, EmberCape, CloudBoots, 0, 0}};
  const int scale = 3, cell = 64 * scale, w = 8 * cell,
            h = int(looks.size()) * cell;
  for (bool face : {true, false}) {
    std::vector<uint16_t> sheet(w * h, color(0x17222c));
    for (size_t row = 0; row < looks.size(); ++row)
      for (int frame = 0; frame < art::WalkFrames; ++frame) {
        auto c = render(looks[row], frame, face, CrystalPick, false, scale);
        for (int y = 0; y < cell; ++y)
          for (int x = 0; x < cell; ++x)
            if (c.pixels[y * cell + x].tag != Tag::Empty)
              sheet[(row * cell + y) * w + frame * cell + x] =
                  c.pixels[y * cell + x].color;
      }
    write_ppm(root / (face ? "walk-right.ppm" : "walk-left.ppm"), w, h, sheet);
  }
  const int weapons[] = {WoodPick,  IronAxe,  CrystalSword,
                         HunterBow, StarWand, HealPotion};
  const int ww = 6 * cell, hh = 4 * cell;
  std::vector<uint16_t> sheet(ww * hh, color(0x17222c));
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 6; ++col) {
      auto c = render(looks[3], 0, row < 2, weapons[col], row % 2, scale);
      for (int y = 0; y < cell; ++y)
        for (int x = 0; x < cell; ++x)
          if (c.pixels[y * cell + x].tag != Tag::Empty)
            sheet[(row * cell + y) * ww + col * cell + x] =
                c.pixels[y * cell + x].color;
    }
  write_ppm(root / "weapon-grips.ppm", ww, hh, sheet);
  // Unobstructed anatomy sheets use the actual pre-weapon assembly for every
  // stance/frame. No held tool can conceal an elbow, waist or knee in review.
  const std::array<Outfit, 3> anatomy_looks = {
      Outfit{0, 0, 0, BlueTunic, 0, SwiftBoots, 0, 0},
      Outfit{0, 0, 0, SageRobe, 0, CloudBoots, 0, 0}, looks[3]};
  const int cw = 40 * scale, ch = 40 * scale, aw = 8 * cw, ah = 3 * ch;
  for (size_t look = 0; look < anatomy_looks.size(); ++look) {
    std::vector<uint16_t> anatomy_sheet(aw * ah, color(0x17222c));
    for (int stance = 0; stance < 3; ++stance)
      for (int frame = 0; frame < art::WalkFrames; ++frame) {
        int held = stance == 1 ? HunterBow : stance == 2 ? CopperPick : None;
        auto c = render(anatomy_looks[look],
                        stance == 2 ? art::StandFrame : frame, true, held,
                        stance == 2, scale, 255, 0, stance == 2 ? frame : -1);
        for (int y = 0; y < ch; ++y)
          for (int x = 0; x < cw; ++x) {
            const auto &v =
                c.assembly[(y + 12 * scale) * c.width + x + 16 * scale];
            if (v.tag != Tag::Empty)
              anatomy_sheet[(stance * ch + y) * aw + frame * cw + x] = v.color;
          }
      }
    write_ppm(root / ("anatomy-" + std::to_string(look) + ".ppm"), aw, ah,
              anatomy_sheet);
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    {
      Outfit o{0, 0, 0, BlueTunic, 0, SwiftBoots, 0, 0};
      Appearance a{32,   48,   art::StandFrame, 1,         255, 0,
                   true, true, o.data(),        CopperPick};
      auto base =
          render(o, art::StandFrame, true, CopperPick, true, 1, 255, 0, 0);
      auto fixed_foot = joint(a, NearFoot), fixed_support = joint(a, RearHand);
      for (int action = 0; action < art::WalkFrames; ++action) {
        a.action_frame = action;
        auto c = render(o, art::StandFrame, true, CopperPick, true, 1, 255, 0,
                        action);
        require(joint(a, NearFoot).x == fixed_foot.x &&
                    joint(a, NearFoot).y == fixed_foot.y,
                "tool action drives the leg gait");
        require(joint(a, RearHand).x == fixed_support.x &&
                    joint(a, RearHand).y == fixed_support.y,
                "support hand changes identity or waves during tool action");
        for (int y = 39; y < 48; ++y)
          for (int x = 16; x < 48; ++x) {
            auto v = c.trunk_at({x, y}), b = base.trunk_at({x, y});
            require(v.color == b.color && v.tag == b.tag,
                    "standing feet move during tool action");
          }
      }
      ActionClock clock;
      clock.step(true, CopperPick, 20);
      require(clock.active && clock.frame() == 0,
              "attack begins at a random global phase");
      for (int phase = 1; phase < 8; ++phase) {
        clock.step(true, CopperPick, 60);
        require(clock.frame() == phase, "attack phase skips or reverses");
      }
      clock.step(false, CopperPick, 60);
      require(!clock.active, "released action never returns to carry stance");
      require(locomotion_frame(true, false, 999) == art::StandFrame &&
                  locomotion_frame(false, true, 999) == art::JumpFrame,
              "idle/jump selects a walking frame");
      puts("PASS persistent main/support hands, planted feet during attacks, "
           "separate action clock and idle/jump states");
    }
    if (argc == 2)
      galleries(argv[1]);
    auto looks = equipment_classes();
    for (size_t k = 0; k < looks.size(); ++k)
      for (int id : {0, int(CopperPick), int(HunterBow)})
        for (int f = 0; f < art::HumanFrames; ++f) {
          where = "outfit=" + std::to_string(k) +
                  " frame=" + std::to_string(f) + " item=" + std::to_string(id);
          auto a = render(looks[k], f, true, id, true),
               b = render(looks[k], f, false, id, true);
          connected(a);
          connected(b);
          landmarks(a, looks[k], f, true, id, true);
          landmarks(b, looks[k], f, false, id, true);
          mirrored(a, b);
        }
    std::printf("PASS %zu clothing/armor geometry classes: neck, two feet, "
                "four arm stances, stand/jump frames, exposed hands or valid "
                "rear occlusion, hip/knee/ankle/foot order, "
                "solid trunk, connected assembly, mirrored facings\n",
                looks.size());
    const std::array<Outfit, 4> samples = {
        Outfit{}, Outfit{StrawHat, 0, 0, RedTunic, TealCape, 0, 0, 0},
        Outfit{CrystalHelmet, CrystalMail, CrystalGreaves, RedTunic, EmberCape,
               CloudBoots, 0, GlowCharm},
        Outfit{IronHelmet, IronMail, IronGreaves, SageRobe, TealCape,
               SwiftBoots, 0, Magnet}};
    for (size_t k = 0; k < samples.size(); ++k)
      for (int id = 1; id < ItemCount; ++id)
        for (bool active : {false, true})
          for (int f = 0; f < art::HumanFrames; ++f) {
            where = "weapon outfit=" + std::to_string(k) +
                    " item=" + std::to_string(id) +
                    " frame=" + std::to_string(f) +
                    " active=" + std::to_string(active);
            auto a = render(samples[k], f, true, id, active),
                 b = render(samples[k], f, false, id, active);
            landmarks(a, samples[k], f, true, id, active);
            landmarks(b, samples[k], f, false, id, active);
            mirrored(a, b);
          }
    std::puts("PASS every held item: handle-to-palm contact, two-hand bow "
              "grip, raised tool pose and mirrored attachments");
    for (const auto &o : samples)
      for (int f : {art::StandFrame})
        for (bool face : {true, false}) {
          where = "arm visibility/light frame=" + std::to_string(f);
          auto c = render(o, f, face);
          arm_visibility_and_light(c, o, f, face);
        }
    std::puts("PASS camera-facing arm/cuff brighter than rear arm; mirrored "
              "depth, visible forearms and stable upper/forearm lengths");
    for (int type = 0; type < 4; ++type) {
      Outfit o{};
      o[0] = type == 3 ? StrawHat : type == 2 ? IronHelmet : None;
      o[3] = type == 1 ? RedTunic : type == 3 ? SageRobe : BlueTunic;
      o[4] = type == 0 ? TealCape : None;
      int held = type == 2 ? IronAxe : None;
      for (int f = 0; f < art::HumanFrames; ++f) {
        where = "NPC=" + std::to_string(type) + " frame=" + std::to_string(f);
        auto a = render(o, f, true, held, false, 1, 255, type == 2 ? 2 : 1);
        auto b = render(o, f, false, held, false, 1, 255, type == 2 ? 2 : 1);
        connected(a);
        connected(b);
        landmarks(a, o, f, true, held, false);
        landmarks(b, o, f, false, held, false);
        mirrored(a, b);
      }
    }
    std::puts("PASS all four NPC appearances: eight walking frames, both "
              "facings and held axe");
    for (const auto &o : samples)
      for (int id :
           {int(WoodPick), int(CrystalSword), int(HunterBow), int(StarWand)})
        for (int f = 0; f < art::HumanFrames; ++f)
          for (int light : {255, 100, 28}) {
            auto base = render(o, f, true, id, true, 1, light);
            for (int scale : {2, 4}) {
              where = "scaled item=" + std::to_string(id) +
                      " frame=" + std::to_string(f) +
                      " scale=" + std::to_string(scale);
              auto c = render(o, f, true, id, true, scale, light);
              landmarks(c, o, f, true, id, true, scale);
              for (int y = 0; y < c.height; ++y)
                for (int x = 0; x < c.width; ++x) {
                  const auto
                      &a = base.pixels[(y / scale) * base.width + x / scale],
                      &b = c.pixels[y * c.width + x];
                  require(a.color == b.color && a.tag == b.tag &&
                              a.index == b.index,
                          "scaled attachment is not pixel-exact");
                }
            }
          }
    std::puts("PASS 1x/2x/4x scaling and three lighting levels; finger "
              "highlights and attachments retain their geometry");
    std::printf("Total production-compositor renders: %zu\n", cases);
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL %s\n", e.what());
    return 1;
  }
}
