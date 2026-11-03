// SPDX-License-Identifier: MIT
// Render the actual app at each logical aspect and exercise its hit regions.
#include "../main.cpp"
#include "../input_geometry.hpp"
#include "../render_buffers.hpp"
#include "../ui_canvas.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <tuple>
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
void blend_rect(int x,int y,int w,int h,uint16_t c,unsigned alpha) {
  if(screen_mode) ui.blend_rect(x,y,w,h,c,alpha);
  for(int yy=std::max(0,y);yy<std::min(HEIGHT,y+h);++yy)
    for(int xx=std::max(0,x);xx<std::min(WIDTH,x+w);++xx) {
      auto &p=buffer[yy*WIDTH+xx];p=starsoil_color::mix(p,c,alpha);painted[yy*WIDTH+xx]=1;
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
void begin_controls() {}
static bool cutout_mode;
bool has_display_cutouts() { return cutout_mode; }
bool blit_pixels(int,int,const uint16_t *,int,int,bool,int,int,int) { return false; }
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
static void inventory_checks() {
  using pocket::Phase;
  pocket::WIDTH = pocket::HEIGHT = 288;
  pocket::buffer.resize(288 * 288); pocket::painted.resize(288 * 288);
  new_game(s, 1234567); session_active = true;
  for (auto &slot : s.bag) slot = {};
  s.bag[0] = {WoodPick, 1}; s.bag[1] = {WoodAxe, 1};
  s.bag[2] = {Wood, 990}; s.bag[3] = {Wood, 20}; s.bag[10] = {CopperHelmet, 1};
  set_view(Bag);
  auto point = [](int slot) { return std::pair{17 + slot % 8 * 25, 59 + slot / 8 * bag_row_height() + content_y()}; };
  auto press = [&](int slot) { auto [x,y] = point(slot); game::pointer(Phase::DOWN,x,y); };
  auto release = [&](int slot) { auto [x,y] = point(slot); game::pointer(Phase::UP,x,y); };
  auto hold = [&]() { for (int i=0;i<4;++i) game::tick(80); };
  auto drag = [&](int from, int to) {
    press(from); hold(); assert(inventory_touch.dragging);
    auto [x,y] = point(to); game::pointer(Phase::MOVE,x,y);
    game::draw(); release(to);
    assert(!inventory_touch.dragging && inventory_touch.source < 0);
  };
  drag(0,20); assert(s.bag[20].item == WoodPick && !s.bag[0].item);
  drag(1,20); assert(s.bag[1].item == WoodPick && s.bag[20].item == WoodAxe);
  drag(3,2); assert(s.bag[2].count == 999 && s.bag[3].count == 11);
  auto before = s;
  press(10); hold(); release(10); // Long hold in place must not equip.
  assert(s.equip[0] == before.equip[0] && s.bag[10].item == CopperHelmet);
  press(10); hold(); game::pointer(Phase::MOVE,280,8); game::pointer(Phase::UP,280,8);
  assert(std::memcmp(s.bag,before.bag,sizeof(s.bag)) == 0);
  press(10); hold(); game::pointer(Phase::CANCEL,0,0);
  assert(!inventory_touch.dragging && std::memcmp(s.bag,before.bag,sizeof(s.bag)) == 0);
  press(10); game::pointer(Phase::MOVE,200,250); hold(); release(10);
  assert(s.equip[0] == before.equip[0] && !inventory_touch.dragging);
  press(10); release(10); assert(s.equip[0] == before.equip[0]);
  game::tick(80); press(10); release(10); assert(s.equip[0] == CopperHelmet);
  set_view(Bag); press(20); release(20); // Single tap selects, never uses.
  assert(view == Bag && s.selected == 20); click(80);
  assert(view == World && s.selected == 20 && s.bag[20].item == WoodAxe);
  game::draw(); assert(hit(225,44) == 20);
  game::key(pocket::Key::R); assert(s.selected == 0);
  s.selected = 20; game::key(pocket::Key::L); assert(s.selected == 7);
  // Storage drag uses the same transaction, including cross-container swaps.
  set_view(Storage); s.bag[0] = {WoodPick,1}; s.chest[0] = {IronAxe,1};
  game::pointer(Phase::DOWN,17,59+content_y()); hold();
  game::pointer(Phase::MOVE,230,59+content_y()); game::pointer(Phase::UP,230,59+content_y());
  assert(s.bag[0].item == IronAxe && s.chest[0].item == WoodPick);
  game::pointer(Phase::DOWN,230,59+content_y()); hold();
  game::key(pocket::Key::B); game::pointer(Phase::UP,17,59+content_y());
  assert(view == World && s.chest[0].item == WoodPick && s.bag[0].item == IronAxe);
  puts("PASS actual inventory UI: hold/drag, swaps, overflow merge, cancelled/outside drops, no accidental equip, double tap, direct backpack use, hotbar return and storage transfers; one profile");
}
static void exploration_ui_checks() {
  using pocket::Phase;
  // Keep the classic-cursor release behavior covered alongside smart mode.
  game::set_smart_cursor(false);
  auto fresh = [&]() {
    new_game(s,1234567,WorldSize::Small);session_active=true;
    std::fill(std::begin(s.tiles),std::end(s.tiles),uint8_t(Air));
    std::fill(std::begin(s.walls),std::end(s.walls),uint8_t(0));
    for(int x=1;x<s.width-1;++x)set_tile(s,x,60,Stone);
    for(auto &m:s.mobs)m={};for(auto &n:s.npcs)n={};
    for(auto &slot:s.bag)slot={};
    s.player={100*16+8,60*16,0,0,100,0,1};s.clock=100000;s.invulnerable=60000;
    s.bag[0]={WoodPick,1};s.bag[1]={WoodAxe,1};s.bag[20]={WoodSword,1};s.selected=20;
    set_view(World);game::draw();
  };
  auto point = [&](int x,int y) { return std::pair{x*16+8-cx,y*16+8-cy}; };
  auto tap = [&](int x,int y,int hold=0) {
    auto [px,py]=point(x,y);game::pointer(Phase::DOWN,px,py);
    for(int i=0;i<hold;i+=40)game::tick(std::min(40,hold-i));
    game::pointer(Phase::UP,px,py);
  };
  fresh();s.selected=0;set_view(Bag);
  game::pointer(Phase::DOWN,117,107+content_y());game::pointer(Phase::UP,117,107+content_y());
  assert(view==Bag && s.selected==20 && s.bag[20].item==WoodSword);
  game::key(pocket::Key::B);assert(view==World && s.selected==20);
  game::draw();assert(hit(225,44)==20);
  set_view(Bag);game::key(pocket::Key::RIGHT);assert(s.selected==21);
  // Bag slot 14 shares its numeric ID with the jump button; it must not jump.
  set_view(Bag);
  game::pointer(Phase::DOWN,167,83+content_y());game::pointer(Phase::UP,167,83+content_y());
  assert(s.selected==14 && !jump_edge);
  game::key(pocket::Key::B);game::tick(40);assert(s.player.vy>=0);
  for(int slot=8;slot<24;++slot) {
    set_view(Bag);int x=17+slot%8*25,y=59+slot/8*bag_row_height()+content_y();
    game::pointer(Phase::DOWN,x,y);game::pointer(Phase::UP,x,y);
    assert(s.selected==slot && view==Bag && !jump_edge);
  }
  fresh();set_tile(s,103,58,DoorTop);set_tile(s,103,59,Door);
  tap(103,59,800);assert(tile(s,103,58)==OpenDoorTop && tile(s,103,59)==OpenDoor && !s.mined);
  tap(103,58,800);assert(tile(s,103,58)==DoorTop && tile(s,103,59)==Door);
  s.selected=0;tap(103,59,800);assert(tile(s,103,58)==Air && tile(s,103,59)==Air && s.mined==1 && count(s,Doors)==1);
  fresh();s.bag[20]={Boards,8};set_tile(s,102,59,Bed);auto home=s.home_x;
  tap(101,57);assert(s.home_x==home); // Clicking air cannot select the nearby bed.
  tap(102,59,400);assert(s.home_x==102 && count(s,Boards)==8);
  fresh();set_tile(s,103,59,Chest);tap(103,59,800);
  assert(count(s,TreasureChest)==1 && tile(s,103,59)==Air && !s.mined);
  fresh();set_tile(s,103,59,StorageChest);auto old=s;
  tap(103,59);assert(view==Storage && std::memcmp(s.bag,old.bag,sizeof(s.bag))==0);
  for(int tile_id:{Bench,Furnace,Anvil}) {
    fresh();set_tile(s,103,59,uint8_t(tile_id));tap(103,59);
    assert(view==Crafting && recipes[recipe].station==(tile_id==Bench?Table:tile_id==Furnace?Kiln:Forge));
    assert(!s.crafted);
  }
  fresh();s.npcs[0]={104*16+8,60*16,0,0,100,0,1};tap(104,59);
  assert(view==Dialogue && npc==0 && s.npcs[0].hp==100);
  fresh();s.npcs[0]={104*16+8,60*16,0,0,100,0,1};s.selected=0;tap(104,59,200);
  assert(view==Dialogue && s.npcs[0].hp==100 && !s.tool_cooldown);
  fresh();set_tile(s,107,58,DoorTop);set_tile(s,107,59,Door);tap(107,59,400);
  assert(tile(s,107,59)==Door && view==World && !s.cooldown);
  fresh();set_tile(s,103,58,DoorTop);set_tile(s,103,59,Door);s.selected=0;
  s.target_x=103;s.target_y=59;do_interact();assert(tile(s,103,59)==OpenDoor);
  fresh();set_tile(s,103,59,Bench);s.selected=0;s.target_x=103;s.target_y=59;
  do_interact();assert(view==Crafting && recipes[recipe].station==Table);
  // Every pick/axe may interact without causing a swing or mining progress.
  for(int item:{WoodPick,CopperPick,IronPick,CrystalPick,WoodAxe,IronAxe}) {
    fresh();s.bag[0]={uint8_t(item),1};s.selected=0;
    set_tile(s,103,58,DoorTop);set_tile(s,103,59,Door);
    tap(103,59,299);
    assert(tile(s,103,59)==OpenDoor && !s.mined && !s.tool_cooldown && !s.mining);
    tap(103,58,80);assert(tile(s,103,59)==Door && !s.mined);
    for(int t:{StorageChest,Bench,Furnace,Anvil,Bed,Chest}) {
      fresh();s.bag[0]={uint8_t(item),1};s.selected=0;set_tile(s,103,59,uint8_t(t));
      tap(103,59,120);
      assert(!s.mined && !s.tool_cooldown && !s.mining);
      if(t==StorageChest)assert(view==Storage);
      else if(t==Bench || t==Furnace || t==Anvil)assert(view==Crafting);
      else if(t==Bed)assert(s.home_x==103);
      else assert(count(s,TreasureChest)==1 && tile(s,103,59)==Air);
    }
  }
  // The 300 ms boundary is irreversible for this gesture, even if the tool
  // cannot mine the target; releasing a long axe hold never toggles a door.
  fresh();s.selected=1;set_tile(s,103,58,DoorTop);set_tile(s,103,59,Door);
  tap(103,59,300);assert(tile(s,103,59)==Door && !s.mined && !s.tool_cooldown);
  tap(103,59,800);assert(tile(s,103,59)==Door && !s.mined);
  fresh();s.selected=0;set_tile(s,103,59,Stone);
  tap(103,59,299);assert(tile(s,103,59)==Stone && !s.mined && !s.tool_cooldown);
  tap(103,59,900);assert(tile(s,103,59)==Air && s.mined==1);
  fresh();s.selected=1;set_tile(s,103,59,Log);
  tap(103,59,800);assert(tile(s,103,59)==Air && s.mined==1);
  // Movement, another contact, cancellation, and page changes cannot turn a
  // deferred gesture into an accidental interaction or leave mining held.
  auto press=[&](uint32_t id=0) {
    auto [x,y]=point(103,59);game::pointer(Phase::DOWN,x,y,id);return std::pair{x,y};
  };
  auto door_fixture=[&]() { fresh();s.selected=0;set_tile(s,103,58,DoorTop);set_tile(s,103,59,Door); };
  door_fixture();auto [x,y]=press();game::tick(100);game::pointer(Phase::CANCEL,x,y);
  game::tick(100);assert(tile(s,103,59)==Door && !s.mined && !touches.owns(16));
  door_fixture();std::tie(x,y)=press();game::pointer(Phase::UP,-20,-20);
  assert(tile(s,103,59)==Door && !s.mined);
  door_fixture();std::tie(x,y)=press();game::pointer(Phase::MOVE,x+16,y);game::pointer(Phase::MOVE,x,y);
  game::pointer(Phase::UP,x,y);assert(tile(s,103,59)==Door && !s.mined);
  door_fixture();std::tie(x,y)=press(42);game::tick(100);
  game::pointer(Phase::DOWN,x,y,900);game::pointer(Phase::UP,x,y,900);
  assert(touches.owns(16) && tile(s,103,59)==Door);
  game::pointer(Phase::UP,x,y,42);assert(tile(s,103,59)==OpenDoor && !s.mined);
  door_fixture();std::tie(x,y)=press();game::tick(100);set_view(Bag);set_view(World);
  game::pointer(Phase::UP,x,y);assert(tile(s,103,59)==Door && !touches.owns(16));
  door_fixture();std::tie(x,y)=press();game::tick(100);game::cancel_input();
  game::pointer(Phase::UP,x,y);assert(tile(s,103,59)==Door && !touches.owns(16));
  door_fixture();std::tie(x,y)=press();auto use=touch_layout().use();
  game::pointer(Phase::DOWN,use.x,use.y,7);game::tick(100);game::pointer(Phase::UP,x,y);
  assert(tile(s,103,59)==Door && s.mining>0); // Explicit USE keeps its responsive swing.
  for(int i=0;i<6;++i)game::tick(40);
  // Releasing the aiming finger returns USE to the forward target in air.
  game::pointer(Phase::UP,use.x,use.y,7);assert(tile(s,103,59)==Door && !s.mined && !touches.owns(15));
  puts("PASS actual touch UI: backpack selection, exact furniture/NPC targets, every pick/axe tap interacts without damage, 300 ms hold threshold, sustained mining/chopping, drag/cancel/page isolation and independent explicit USE");
}
static void smart_ui_checks() {
  using pocket::Phase;
  pocket::WIDTH=pocket::HEIGHT=288;
  pocket::buffer.resize(288*288);pocket::painted.resize(288*288);
  auto fresh=[] {
    new_game(s,314159,WorldSize::Small);session_active=true;
    std::fill_n(s.tiles,Cells,uint8_t(Air));std::fill_n(s.walls,Cells,uint8_t(0));
    for(int x=1;x<s.width-1;++x)set_tile(s,x,60,Stone);
    for(auto &m:s.mobs)m={};for(auto &n:s.npcs)n={};
    s.player={1608,960,0,0,100,0,1};s.player.grounded=true;s.invulnerable=60000;
    s.selected=0;s.bag[0]={WoodPick,1};set_tile(s,101,58,Stone);
    set_view(World);game::draw();
  };
  game::set_smart_cursor(true);fresh();
  auto u=touch_layout().use();game::pointer(Phase::DOWN,u.x,u.y,5);
  for(int i=0;i<20;++i)game::tick(40);
  assert(s.smart_cursor && s.mined==1 && tile(s,101,58)==Air);
  game::pointer(Phase::UP,u.x,u.y,5);
  fresh();game::pointer(Phase::DOWN,u.x,u.y,5);game::tick(80);
  game::set_smart_cursor(false);assert(!touches.owns(15) && !s.mining);
  game::pointer(Phase::UP,u.x,u.y,5);game::pointer(Phase::DOWN,u.x,u.y,5);
  for(int i=0;i<20;++i)game::tick(40);
  assert(!s.smart_cursor && !s.mined && tile(s,101,58)==Stone);
  game::pointer(Phase::UP,u.x,u.y,5);
  game::set_smart_cursor(true);fresh();set_tile(s,104,59,Stone);game::draw();
  int x=104*16+8-cx,y=59*16+8-cy;
  game::pointer(Phase::DOWN,x,y,42);
  for(int i=0;i<24;++i)game::tick(40);
  assert(!s.smart_cursor && tile(s,104,59)==Air && tile(s,101,58)==Stone);
  game::pointer(Phase::UP,x,y,42);
  puts("PASS actual UI: automatic selection via USE, persistent mode API, cancellation when switching mode and precise touch override");
}
static void multitouch_checks() {
  using pocket::Phase;
  pocket::WIDTH=pocket::HEIGHT=288;
  pocket::buffer.resize(288*288);pocket::painted.resize(288*288);
  new_game(s,314159);session_active=true;
  std::fill_n(s.tiles,Cells,uint8_t(Air));std::fill_n(s.walls,Cells,uint8_t(0));
  for(int x=1;x<s.width-1;++x)set_tile(s,x,60,Stone);
  s.player={1608,960,0,0,100,0,1};s.selected=2;s.invulnerable=60000;
  set_view(World);starsoil::tick(s,{},20);game::draw();
  auto layout=touch_layout();auto st=layout.stick(),j=layout.jump(),u=layout.use();
  auto down=[](uint32_t id,int x,int y){game::pointer(Phase::DOWN,x,y,id);};
  auto up=[](uint32_t id){game::pointer(Phase::UP,-100,-100,id);};
  down(42,st.x,st.y);game::pointer(Phase::MOVE,st.x+35,st.y,42);
  down(900,j.x,j.y);down(0xffffffffu,u.x,u.y);
  float x=s.player.x,y=s.player.y;game::tick(40);
  assert(s.player.x>x && s.player.y<y && s.player.vy<0 && s.cooldown>0);
  assert(touches.owns(17)&&touches.owns(14)&&touches.owns(15));
  game::draw();
  up(900);assert(touches.owns(17)&&touches.owns(15)&&!touches.owns(14));
  game::tick(40);assert(s.player.vx>0);
  down(55,200,130);assert(touches.owns(16));int target=aim_x;
  up(777);assert(aim_x==target && touches.owns(16));
  up(0xffffffffu);assert(touches.owns(16)&&touches.owns(17));
  game::pointer(Phase::CANCEL,0,0,42);assert(!touches.owns(17)&&touches.owns(16));
  game::tick(20);assert(s.player.vx==0);up(55);assert(aim_x==-1);
  // Two fingers over one button do not synthesize repeated jumps or steal it.
  down(1,j.x,j.y);down(2,j.x,j.y);up(1);assert(!touches.owns(14));up(2);
  // Opening a page blocks every held contact until it is released.
  down(3,st.x,st.y);game::pointer(Phase::MOVE,st.x-35,st.y,3);down(4,u.x,u.y);
  set_view(Bag);set_view(World);game::tick(20);
  assert(!touches.owns(17)&&!touches.owns(15)&&s.player.vx==0);
  game::pointer(Phase::MOVE,st.x+35,st.y,3);assert(!touches.owns(17));up(3);up(4);
  // A second menu contact must not steal an inventory hold/drag.
  set_view(Bag);s.bag[20]={Wood,17};s.bag[21]={};
  int sx=17+4*25,sy=59+2*bag_row_height()+content_y();
  down(5,sx,sy);down(6,sx+25,sy);
  for(int i=0;i<4;++i)game::tick(80);
  assert(inventory_touch.dragging && inventory_touch.source==20);
  up(6);assert(inventory_touch.dragging);
  game::pointer(Phase::MOVE,sx+25,sy,5);game::pointer(Phase::UP,sx+25,sy,5);
  assert(s.bag[21].item==Wood && !s.bag[20].item);
  set_view(World);s.player={1608,960,0,0,100,0,1};starsoil::tick(s,{},20);
  down(5,st.x,st.y);game::pointer(Phase::MOVE,st.x+22,st.y-25,5);down(6,u.x,u.y);
  x=s.player.x;y=s.player.y;game::tick(40);
  assert(s.player.x>x && s.player.y<y && touches.owns(15));up(6);up(5);
  down(8,j.x,j.y);game::pointer(Phase::CANCEL,0,0,999);assert(jump_edge);
  game::pointer(Phase::CANCEL,0,0,8);assert(!jump_edge);
  down(8,j.x,j.y);up(8);assert(jump_edge);game::cancel_input();assert(!jump_edge);
  // The exact same bounded capture table is used in the PXA runtime.
  starsoil_touch::Captures captures;
  for(uint32_t id=0;id<10;++id)assert(captures.begin(id));
  assert(!captures.begin(10) && !captures.begin(0));
  *captures.find(5)={};assert(captures.begin(0xffffffffu));
  captures.block();for(auto &p:captures.slots)assert(p.consumed);
  captures.clear();assert(!captures.find(0));
  puts("PASS independent pointer IDs: move+jump+use+scene aim, arbitrary release order, outside cancel, duplicate/capacity safety, page changes and multi-finger inventory isolation");
}
static void polish_ui_checks() {
  using pocket::Phase;
  pocket::WIDTH=pocket::HEIGHT=288;
  pocket::buffer.resize(288*288);pocket::painted.resize(288*288);
  new_game(s,314159);session_active=true;
  for(auto &a:s.bag)a={};for(auto &a:s.chest)a={};
  s.selected=23;s.bag[0]={WoodPick,1};s.bag[8]={Wood,990};
  s.bag[18]={Wood,20};s.bag[23]={IronSword,1};
  auto tap=[](int x,int y){game::pointer(Phase::DOWN,x,y);game::pointer(Phase::UP,x,y);};
  set_view(Bag);
  tap(content_x()+220,height()-12-FooterInset);
  assert(s.bag[0].item==WoodPick && s.bag[23].item==IronSword && s.selected==23);
  assert(s.bag[8].item==Wood && s.bag[8].count==999 && s.bag[9].count==11);
  set_tile(s,50,30,StorageChest);s.chest[0]={Wood,980};set_view(Storage);
  tap(content_x()+45,height()-14-FooterInset);
  assert(count(s,Wood)==0 && s.chest[0].count==999 && s.chest[1].count==991);
  assert(s.bag[0].item==WoodPick && s.bag[23].item==IronSword);
  tap(content_x()+145,height()-14-FooterInset);assert(count(s,Wood)==1990 && !s.chest[0].item);
  s.chest[0]={Rock,10};s.chest[5]={Rock,20};
  tap(content_x()+240,height()-14-FooterInset);assert(s.chest[0].count==30&&!s.chest[5].item);
  game::key(pocket::Key::Y);assert(count(s,Rock)==0); // Chest matching only.
  game::key(pocket::Key::SELECT);assert(count(s,Rock)==30);
  for(auto &a:s.bag)a={};s.selected=0;s.bag[20]={Wood,3};
  craft_ready_only=false;recipe=0;set_view(Crafting);
  assert(visible_recipes==RecipeCount);
  tap(width()-110,21);assert(craft_ready_only && visible_recipes==2 && recipe==0);
  assert(hit(content_x()+40,content_y()+53)==120);
  assert(hit(content_x()+40,content_y()+49+2*RecipeRowHeight)==-1);
  game::key(pocket::Key::DOWN);assert(recipe==1);
  game::key(pocket::Key::UP);assert(recipe==0);
  tap(content_x()+213,height()-14-FooterInset);
  assert(count(s,Wood)==0 && count(s,Boards)==12 && visible_recipes==0 && recipe==-1);
  for(auto k:{pocket::Key::UP,pocket::Key::DOWN,pocket::Key::LEFT,pocket::Key::RIGHT,pocket::Key::A,pocket::Key::Y})game::key(k);
  game::draw();assert(recipe==-1 && visible_recipes==0);
  game::key(pocket::Key::SELECT);assert(!craft_ready_only && visible_recipes==RecipeCount);
  recipe=4;set_tile(s,44,30,Air);refresh_recipes();do_craft();
  assert(std::strstr(message,"WORKBENCH") || std::strstr(message,"工作台"));
  recipe=0;do_craft();assert(std::strstr(message,"MORE") || std::strstr(message,"还缺"));
  game::draw();
  puts("PASS actual UI: protected bag sort, bulk storage buttons/controller, ready-only filter mapped to real recipes, last-recipe depletion, empty navigation and precise crafting hints");
}
int main(int argc, char **argv) {
  game::init();
  if (argc == 2 && std::strcmp(argv[1], "--smart") == 0) { smart_ui_checks(); return 0; }
  if (argc == 2 && std::strcmp(argv[1], "--touch") == 0) { inventory_checks(); multitouch_checks(); return 0; }
  if (argc == 2 && std::strcmp(argv[1], "--polish") == 0) { inventory_checks(); exploration_ui_checks(); polish_ui_checks(); return 0; }
  if (argc == 2 && std::strcmp(argv[1], "--inventory") == 0) { inventory_checks(); return 0; }
  if (argc == 2 && std::strcmp(argv[1], "--exploration") == 0) { inventory_checks(); exploration_ui_checks(); return 0; }
  for (int pw = 1; pw <= 1920; ++pw)
    for (int ph = 1; ph <= 1920; ++ph) {
      int divisor = 1;
      while ((pw / divisor > 480 || ph / divisor > 480) && divisor < 4) divisor *= 2;
      int sw = std::clamp(pw / divisor, 1, 480), sh = std::clamp(ph / divisor, 1, 480);
      int lw = 288, lh = 224;
      if (pw * 224 >= ph * 288) lw = std::clamp((pw * lh + ph / 2) / ph, 288, 480);
      else lh = std::clamp((ph * lw + pw / 2) / pw, 224, 480);
      assert(starsoil_buffers::fits(unsigned(sw * sh), unsigned(lw * lh)));
    }
  puts("PASS shared render arena covers bounded surface/scene geometries without overlapping either mapped output buffer");
  // The real Host reserves 14px at the top, 16px at the sides and 20px
  // at the bottom even while its system bars are hidden. Test actual targets.
  for (auto [pw, ph] : {std::pair{296,240}, {240,284}, {412,412},
                        {320,240}, {800,480}}) {
    int w, h;
    if (pw * 224 >= ph * 288) { h = 224; w = (pw * h + ph / 2) / ph; }
    else { w = 288; h = (ph * w + pw / 2) / pw; }
    pocket::WIDTH = w; pocket::HEIGHT = h;
    assert(w * h <= 288 * 480);
    auto safe = [&](int x, int y) {
      int px = x * pw / w, py = y * ph / h;
      assert(px >= 16 && px < pw - 16 && py >= 14 && py < ph - 20);
    };
    view = World;
    safe(w - 96, 22); assert(hit(w - 96, 22) == 20);
    safe(w - 60, 22); assert(hit(w - 60, 22) == 21);
    safe(w - 28, 22); assert(starsoil::pause_button_hit(w - 28, 22, w));
    safe(31, 45); assert(hit(31, 45) == 30);
    auto controls=touch_layout();
    for(auto p:{controls.stick(),controls.jump(),controls.use(),controls.talk()})safe(p.x,p.y);
    assert(hit(controls.stick().x,controls.stick().y)==17);
    view = Crafting;
    safe(168 + content_x(), h - 42);
    assert(hit(168 + content_x(), h - 42) == 92);
    view = Gear;
    safe(w - 52, 22); assert(hit(w - 52, 22) == 99);
    safe(144 + content_x(), h - 37);
    assert(hit(144 + content_x(), h - 37) == 82);
  }
  puts("PASS all connected device geometries: controls avoid system gesture strips; bounded logical framebuffer");
  pocket::WIDTH = pocket::BASE_WIDTH; pocket::HEIGHT = pocket::BASE_HEIGHT;
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
      assert(hit(w - 52, 22) == 99);
      assert(hit(w - 52, 9) != 99);
      for (int i = 0; i < 5; ++i)
        assert(hit(15 + i * (w - 8) / 5, 35) == 100 + i);
    }
    view = Bag;
    assert(hit(15 + ox, 60 + oy) == 0);
    assert(hit(238 + ox, 115 + oy) == 84);
    assert(hit(50 + ox, h - 12 - FooterInset) == 80 && hit(220 + ox, h - 12 - FooterInset) == 81);
    view = Crafting;
    assert(hit(160 + ox, h - 12 - FooterInset) == 92);
    assert(hit(213 + ox, h - 12 - FooterInset) == 93);
    for (int gap : {50, 80, 108, 189, 236})
      assert(hit(gap + ox, h - 12 - FooterInset) == -1);
    assert(hit(80 + ox, 60 + oy) == 120);
    view = Gear;
    assert(hit(62 + ox, 115 + oy) == 84);
    for (int i = 0; i < 7; ++i)
      assert(hit(210 + ox, 53 + i * gear_row_height() + oy) == 140 + (i < 6 ? i : 7));
    assert(hit(145, h - 12 - FooterInset) == 82);
    assert(hit(20 + ox, h - 47 - FooterInset) != 82);
    view = World;
    assert(hit(w - 96, 22) == 20);
    assert(hit(w - 96, 9) == -1);
    assert(hit(w - 60, 23) == 21);
    auto pad=touch_layout();
    assert(hit(pad.stick().x,pad.stick().y)==17);
    assert(hit(pad.jump().x,pad.jump().y)==14 && hit(pad.use().x,pad.use().y)==15);
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
    for (int y = std::max(57, int(mob.y) - cy - 24); y < scene_bottom(); ++y)
      for (int x = 0; x < 288; ++x)
        changed += empty[y * 288 + x] != pocket::buffer[y * 288 + x];
    assert(changed > 25);
    puts("PASS ground-level creatures remain visible in the resized "
         "world scene on a square display");
  }
  {
    pocket::WIDTH = pocket::HEIGHT = 288;
    pocket::buffer.assign(288 * 288, 0);
    pocket::painted.assign(288 * 288, 0);
    pocket::cutout_mode = true;
    scene();
    assert(std::find(pocket::painted.begin(), pocket::painted.end(), 0) == pocket::painted.end());
    pocket::cutout_mode = false;
    puts("PASS cutout displays repaint the complete world behind the safe control panel");
  }

}
