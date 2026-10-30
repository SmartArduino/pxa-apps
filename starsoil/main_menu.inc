// SPDX-License-Identifier: MIT
// Included in main.cpp after the shared drawing helpers.
int world_selected = -1, world_page = 0, seed_focus = 0, create_focus = 0;
WorldSize new_world_size = WorldSize::Medium;
char world_seed[11]{}, seed_draft[11]{};
bool delete_confirm;
bool home_view() { return view == Title || view >= CreateWorld; }
int world_count() {
  int count = 0;
  for (int i = 0; i < pocket::WorldSlots; ++i)
    count += pocket::world_info(i).occupied;
  return count;
}
int listed_world(int index) {
  for (int i = 0; i < pocket::WorldSlots; ++i)
    if (pocket::world_info(i).occupied && index-- == 0)
      return i;
  return -1;
}
void normalize_world_selection() {
  int count = world_count();
  world_page = std::clamp(world_page, 0, std::max(0, (count - 1) / 2));
  if (!pocket::world_info(world_selected).occupied)
    world_selected = listed_world(world_page * 2);
}
bool parse_seed(const char *text, uint32_t &value) {
  uint64_t n = 0;
  for (const char *p = text; *p; ++p) {
    if (*p < '0' || *p > '9')
      return false;
    n = n * 10 + unsigned(*p - '0');
    if (n > UINT32_MAX)
      return false;
  }
  value = uint32_t(n);
  return true;
}
void menu_background() {
  for (int y = 0; y < height(); ++y)
    for (int x = 0; x < width(); ++x)
      pixel(x, y,
            dim(art::cover[0][(y * 144 / height()) * 144 + x * 144 / width()],
                150 - y * 55 / height()));
  // Quiet drifting lights keep the menu connected to the game's night sky.
  for (int i = 0; i < 16; ++i) {
    int x =
        int((hash(i * 53 + 7) + animation / (90 + i * 7)) % unsigned(width()));
    int y = int(hash(i * 71 + 31) % unsigned(std::max(1, height() - 30)));
    pixel(x, y,
          dim(i % 3 ? Teal : GoldInk, 110 + (animation / 35 + i * 11) % 110));
  }
  pocket::begin_ui(true);
  content_origin();
}
void draw_title() {
  menu_background();
  normalize_world_selection();
  center(0, 10, 288, "STARSOIL", GoldInk, 3);
  center(0, 49, 288, tr("THE WORLDS BELOW", "星壤物语"), Ink);
  int count = world_count();
  if (!count) {
    center(18, 85, 252, tr("YOUR NEXT ADVENTURE", "下一段冒险，由你开启"),
           GoldInk);
    uint8_t eq[8]{};
    eq[3] = BlueTunic;
    eq[4] = TealCape;
    human(144, 174, art::StandFrame, true, eq, 0, 255, 2);
    button(30, 198, 228,
           hint("CREATE WORLD", "创建世界", "X CREATE WORLD", "X 创建世界"),
           true);
  } else {
    label(20, 71, tr("YOUR WORLDS", "你的世界"), GoldInk);
    for (int row = 0; row < 2; ++row) {
      int slot = listed_world(world_page * 2 + row);
      if (slot < 0)
        break;
      const auto &info = pocket::world_info(slot);
      int y = 88 + row * 42;
      panel(18, y, 252, 40, slot == world_selected ? rgb(45, 67, 65) : Panel);
      if (slot == world_selected)
        outline(18, y, 252, 40, GoldInk);
      sprite(25, y + 4, art::tiles[Grass * 16], 16, 16);
      char b[64];
      std::snprintf(b, sizeof(b), tr("WORLD %02d", "世界 %02d"), slot + 1);
      label(50, y + 2, b, Ink);
      if (info.valid) {
        std::snprintf(b, sizeof(b), "%ux%u", info.width, info.height);
        label(132, y + 2, b, Muted);
        std::snprintf(b, sizeof(b), tr("SEED %u", "种子 %u"), info.seed);
        label(27, y + 22, b, Muted);
        std::snprintf(b, sizeof(b), tr("D%u", "第%u天"), info.day);
        label(161, y + 22, b, Muted);
      } else
        label(27, y + 22, tr("UNREADABLE WORLD", "存档无法读取"), Red);
      // Play and trash use the same pixel-art language as the game controls.
      panel(214, y + 5, 22, 28);
      for (int k = 0; k < 6; ++k)
        rect(222 + k, y + 12 + k, 1, 13 - k * 2, info.valid ? Teal : Muted);
      panel(242, y + 5, 22, 28);
      rect(250, y + 12, 6, 1, Muted);
      rect(248, y + 15, 10, 2, Muted);
      outline(249, y + 17, 8, 10, Muted);
      line(252, y + 19, 252, y + 24, Muted);
    }
    int pages = (count + 1) / 2;
    if (pages > 1) {
      button(18, 173, 40, "<");
      char b[16];
      std::snprintf(b, sizeof(b), "%d / %d", world_page + 1, pages);
      center(70, 176, 148, b, Muted);
      button(230, 173, 40, ">");
    }
    button(18, 198, 108,
           hint("NEW WORLD", "创建世界", "X NEW WORLD", "X 创建世界"));
    button(134, 198, 136, hint("PLAY", "进入世界", "A PLAY", "A 进入世界"),
           pocket::world_info(world_selected).valid);
  }
}
void create_world_draw() {
  menu_background();
  panel(18, 32, 252, 184);
  center(18, 42, 252, tr("CREATE A WORLD", "创建新世界"), GoldInk);
  label(28, 70, tr("WORLD SIZE", "世界大小"), Muted);
  const char *en[] = {"SMALL", "MEDIUM", "LARGE"};
  const char *cn[] = {"小型", "中型", "大型"};
  for (int i = 0; i < 3; ++i) {
    button(28 + i * 80, 91, 72, tr(en[i], cn[i]), int(new_world_size) == i);
    if (pocket::using_controller() && create_focus == i)
      outline(28 + i * 80, 91, 72, 20, GoldInk);
  }
  auto bounds = dimensions(new_world_size);
  char b[40];
  std::snprintf(b, sizeof(b), "%u x %u", bounds.width, bounds.height);
  center(28, 117, 232, b, Muted);
  label(28, 143, tr("SEED", "种子"), GoldInk);
  button(88, 138, 172, *world_seed ? world_seed : tr("RANDOM", "随机"),
         create_focus == 3);
  button(28, 184, 108, tr("BACK", "返回"), create_focus == 4);
  button(144, 184, 116, tr("CREATE", "创建"), create_focus == 5);
}
void seed_editor_draw() {
  menu_background();
  panel(24, 15, 240, 205);
  center(24, 25, 240, tr("WORLD SEED", "世界种子"), GoldInk);
  uint32_t value;
  bool valid = parse_seed(seed_draft, value);
  panel(36, 51, 216, 27, Dark);
  center(36, 56, 216, *seed_draft ? seed_draft : tr("RANDOM", "随机"),
         valid ? Ink : Red);
  for (int i = 0; i < 12; ++i) {
    char b[2] = {char('1' + i), 0};
    const char *caption = i == 9    ? tr("CLEAR", "清空")
                          : i == 10 ? "0"
                          : i == 11 ? tr("DEL", "删除")
                                    : b;
    button(40 + i % 3 * 72, 85 + i / 3 * 26, 64, caption,
           pocket::using_controller() && seed_focus == i);
  }
  button(40, 192, 100, tr("CANCEL", "取消"), seed_focus == 12);
  button(148, 192, 100,
         valid ? tr("DONE", "完成") : tr("TOO LARGE", "超出范围"),
         seed_focus == 13 && valid);
}
void delete_world_draw() {
  menu_background();
  panel(24, 65, 240, 116);
  char b[64];
  std::snprintf(b, sizeof(b), tr("DELETE WORLD %02d?", "删除世界 %02d？"),
                world_selected + 1);
  center(24, 78, 240, b, GoldInk);
  center(24, 105, 240, tr("THIS CANNOT BE UNDONE", "删除后无法恢复"), Muted);
  button(40, 143, 100, tr("CANCEL", "取消"), !delete_confirm);
  button(148, 143, 100, tr("DELETE", "删除"), delete_confirm);
}
void draw_home() {
  if (view == Title)
    draw_title();
  else if (view == CreateWorld)
    create_world_draw();
  else if (view == SeedEditor)
    seed_editor_draw();
  else
    delete_world_draw();
  if (pocket::worlds_busy()) {
    panel(32, 83, 224, 68, Dark);
    center(32, 108, 224, tr("PREPARING WORLD...", "正在准备世界…"), GoldInk);
  } else if (pocket::world_error()) {
    panel(24, 65, 240, 116, Dark);
    center(24, 77, 240, tr("WORLD UNAVAILABLE", "无法打开世界"), GoldInk);
    wrap(38, 102, 212,
         pocket::world_error() == 3
             ? tr("All world slots are full.", "世界已满，请先删除一个世界。")
         : pocket::world_error() == 1
             ? tr("Invalid save. Original file kept.",
                  "存档损坏，原文件已保留。")
             : tr("Storage failed. Try again.", "存储操作失败，请重试。"),
         Muted, 2);
    button(88, 149, 112, tr("BACK", "返回"), true);
  }
}
int home_hit(int x, int y) {
  x -= content_x();
  y -= content_y();
  if (pocket::worlds_busy())
    return -1;
  if (pocket::world_error())
    return y >= 149 && y < 169 && x >= 88 && x < 200 ? 380 : -1;
  if (view == Title) {
    for (int row = 0; row < 2; ++row)
      if (listed_world(world_page * 2 + row) >= 0 && y >= 88 + row * 42 &&
          y < 128 + row * 42 && x >= 18 && x < 270) {
        int dy = y - (88 + row * 42);
        if (dy >= 5 && dy < 33) {
          if (x >= 242 && x < 264) return 330 + row;
          if (x >= 214 && x < 236) return 320 + row;
        }
        return 310 + row;
      }
    if (world_count() > 2 && y >= 173 && y < 194)
      return x >= 18 && x < 58 ? 303 : x >= 230 && x < 270 ? 304 : -1;
    if (y >= 198 && y < 218) {
      if (!world_count()) return x >= 30 && x < 258 ? 301 : -1;
      if (x >= 18 && x < 126) return 301;
      if (x >= 134 && x < 270) return 302;
    }
  } else if (view == CreateWorld) {
    if (y >= 91 && y < 112 && x >= 28 && x < 260)
      return 340 + std::min(2, (x - 28) / 80);
    if (y >= 138 && y < 160 && x >= 88 && x < 260)
      return 345;
    if (y >= 184 && y < 204) {
      if (x >= 28 && x < 136) return 346;
      if (x >= 144 && x < 260) return 347;
    }
  } else if (view == SeedEditor) {
    if (y >= 85 && y < 185 && x >= 40 && x < 248) {
      int row = (y - 85) / 26, col = (x - 40) / 72;
      if ((y - 85) % 26 < 20 && (x - 40) % 72 < 64)
        return 350 + row * 3 + col;
    }
    if (y >= 192 && y < 212) {
      if (x >= 40 && x < 140) return 362;
      if (x >= 148 && x < 248) return 363;
    }
  } else if (y >= 143 && y < 163) {
    if (x >= 40 && x < 140) return 370;
    if (x >= 148 && x < 248) return 371;
  }
  return -1;
}
void home_click(int id) {
  if (pocket::worlds_busy())
    return;
  if (pocket::world_error()) {
    if (id == 380)
      pocket::clear_world_error();
    return;
  }
  if (id == 301) {
    create_focus = 0;
    set_view(CreateWorld);
  } else if (id == 302)
    pocket::load_world(world_selected);
  else if (id == 303 || id == 304) {
    int pages = (world_count() + 1) / 2;
    if (pages)
      world_page = (world_page + (id == 304 ? 1 : pages - 1)) % pages;
    world_selected = listed_world(world_page * 2);
  } else if (id >= 310 && id <= 311)
    world_selected = listed_world(world_page * 2 + id - 310);
  else if (id >= 320 && id <= 321) {
    world_selected = listed_world(world_page * 2 + id - 320);
    pocket::load_world(world_selected);
  } else if (id >= 330 && id <= 331) {
    world_selected = listed_world(world_page * 2 + id - 330);
    delete_confirm = false;
    set_view(DeleteWorld);
  } else if (id >= 340 && id <= 342)
    new_world_size = WorldSize(id - 340);
  else if (id == 345) {
    std::strcpy(seed_draft, world_seed);
    seed_focus = 0;
    set_view(SeedEditor);
  } else if (id == 346 || id == 370)
    set_view(Title);
  else if (id == 347) {
    uint32_t seed;
    if (parse_seed(world_seed, seed))
      pocket::create_world(*world_seed ? seed : pocket::random(),
                           uint8_t(new_world_size));
  } else if (id >= 350 && id <= 361) {
    int key = id - 350;
    size_t n = std::strlen(seed_draft);
    if (key == 9)
      seed_draft[0] = 0;
    else if (key == 11) {
      if (n)
        seed_draft[n - 1] = 0;
    } else if (n < 10) {
      seed_draft[n] = key == 10 ? '0' : char('1' + key);
      seed_draft[n + 1] = 0;
    }
  } else if (id == 362)
    set_view(CreateWorld);
  else if (id == 363) {
    uint32_t seed;
    if (parse_seed(seed_draft, seed)) {
      std::strcpy(world_seed, seed_draft);
      set_view(CreateWorld);
    }
  } else if (id == 371 && pocket::delete_world(world_selected))
    set_view(Title);
  pocket::redraw();
}
void home_key(Key k) {
  if (pocket::worlds_busy())
    return;
  if (pocket::world_error()) {
    if (k == Key::A || k == Key::B)
      pocket::clear_world_error();
    return;
  }
  normalize_world_selection();
  if (view == Title) {
    int count = world_count();
    if (k == Key::X || (!count && k == Key::A))
      home_click(301);
    else if (k == Key::A)
      home_click(302);
    else if (k == Key::Y && count) {
      delete_confirm = false;
      set_view(DeleteWorld);
    } else if ((k == Key::LEFT || k == Key::L) && count > 2)
      home_click(303);
    else if ((k == Key::RIGHT || k == Key::R) && count > 2)
      home_click(304);
    else if ((k == Key::UP || k == Key::DOWN) && count) {
      int at = 0;
      while (listed_world(at) != world_selected && at < count)
        ++at;
      at = (at + (k == Key::DOWN ? 1 : count - 1)) % count;
      world_selected = listed_world(at);
      world_page = at / 2;
    }
  } else if (view == CreateWorld) {
    if (k == Key::B)
      home_click(346);
    if (k == Key::LEFT)
      create_focus = (create_focus + 5) % 6;
    if (k == Key::RIGHT)
      create_focus = (create_focus + 1) % 6;
    if (k == Key::UP)
      create_focus = std::max(0, create_focus - 3);
    if (k == Key::DOWN)
      create_focus = std::min(5, create_focus + 3);
    if (k == Key::A)
      home_click(create_focus < 3    ? 340 + create_focus
                 : create_focus == 3 ? 345
                 : create_focus == 4 ? 346
                                     : 347);
  } else if (view == SeedEditor) {
    if (k == Key::B)
      home_click(362);
    if (k == Key::LEFT)
      seed_focus = (seed_focus + 13) % 14;
    if (k == Key::RIGHT)
      seed_focus = (seed_focus + 1) % 14;
    if (k == Key::UP)
      seed_focus = std::max(0, seed_focus - 3);
    if (k == Key::DOWN)
      seed_focus = std::min(13, seed_focus + 3);
    if (k == Key::A)
      home_click(350 + seed_focus);
  } else {
    if (k == Key::LEFT || k == Key::RIGHT)
      delete_confirm = !delete_confirm;
    if (k == Key::B)
      home_click(370);
    if (k == Key::A)
      home_click(delete_confirm ? 371 : 370);
  }
}
