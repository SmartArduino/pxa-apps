#include <pxa/app.hpp>
#include "../common/cpp/vertical_scroll.hpp"
#include <pxa/ui_component.hpp>
#include <pxa/ui_environment.hpp>
#include <pxa/ui_layout.hpp>
#include <pxa/list.hpp>
#include "richman_data.h"

#include <string>
#include <vector>

using namespace pxa;
using namespace pxa::ui;
using namespace pxa::ui::literals;

// 大富翁类回合制桌游（原创棋盘数据与规则实现，未使用任何原版素材）
// UI 采用静态按钮 + 选择弹窗屏（hotk 战斗分屏验证过的模式），避开动态标签与条件视图。
struct TileRT {
int owner = -1;
int level = 0;
};
struct PlayerRT {
int money = 15000;
int pos = 0;
int skip = 0;
bool bankrupt = false;
};

struct Model {
Navigator<8, 16384, 128> routes;
State<std::string> boardText{""};
State<std::string> statusText{""};
State<std::string> tileText{""};
State<std::string> msgText{""};
State<std::string> choiceText{""};

int phase = 0; // 0 可掷骰 4 可结束回合 5 终局
int cur = 0;
int round = 1;
int dice = 0;
int pendingTile = -1;
int pendingPrice = 0;
bool started = false;
bool gameOver = false;
bool win = false;
std::uint32_t rng = 0x87654321u;
TileRT tiles[30]{};
PlayerRT ps[3]{};
std::string msgLog;

const char* pname(int i) const { return i == 0? "你": (i == 1? "阿强": "小美");}
const char* pmark(int i) const { return i == 0? "你": (i == 1? "强": "美");}

int randInt(int n) {
if (n <= 0) return 0;
rng = rng * 1664525u + 1013904223u;
return int((rng >> 16) % (unsigned)n);
}
static int tollOf(int price, int level) { return price * (level + 1) / 4;}
static int upCostOf(int price) { return price / 2;}

void pushMsg(const std::string& m) {
msgLog = m + "\n" + msgLog;
if (msgLog.size() > 900) {
            // UI text must remain valid UTF-8 after long games.
            std::size_t end = 900;
            while (end && (static_cast<unsigned char>(msgLog[end]) & 0xc0) == 0x80) --end;
            msgLog.resize(end);
        }
}

std::string tileToken(int i) const {
std::string t = rich::kTiles[i].name;
if (rich::kTiles[i].type == rich::T_PROP && tiles[i].owner >= 0) {
t += pmark(tiles[i].owner);
if (tiles[i].level > 0) t += std::to_string(tiles[i].level);
}
for (int p = 0; p < 3; ++p) {
if (!ps[p].bankrupt && ps[p].pos == i) { t = "●" + t; break;}
}
return t;
}

void refreshUI() {
std::string b;
for (int i = 0; i < 30; ++i) {
if (i) b += (i % 3 == 0 ? "\n" : " ");
b += tileToken(i);
}
boardText.set(b);
std::string s = "第" + std::to_string(round) + "轮 ";
for (int p = 0; p < 3; ++p) {
s += "\n";
s += std::string(pname(p)) + (ps[p].bankrupt? "(破产)":
("¥" + std::to_string(ps[p].money) + "@" + rich::kTiles[ps[p].pos].name));
}
statusText.set(s);
if (started &&!gameOver) {
int i = ps[cur].pos;
std::string t = std::string(pname(cur)) + "在" + rich::kTiles[i].name;
if (rich::kTiles[i].type == rich::T_PROP) {
t += "·";
if (tiles[i].owner < 0) {
t += "无主，售价¥" + std::to_string(rich::kTiles[i].price);
} else {
t += std::string("主人") + pname(tiles[i].owner) + "·" +
rich::kLevelName[tiles[i].level] + "·过路费¥" +
std::to_string(tollOf(rich::kTiles[i].price, tiles[i].level));
}
}
if (dice > 0) t += "（掷出" + std::to_string(dice) + "点）";
tileText.set(t);
} else if (gameOver) {
tileText.set(win? "你赢了！两家AI都破产了。": "你破产了，游戏结束。");
} else {
tileText.set("");
}
std::string m;
int lines = 0;
for (char c: msgLog) {
if (c == '\n' && ++lines >= 4) break;
m += c;
}
msgText.set(m);
}

void startGame() {
for (int i = 0; i < 30; ++i) tiles[i] = TileRT{};
for (int p = 0; p < 3; ++p) ps[p] = PlayerRT{};
msgLog.clear();
cur = 0; round = 1; dice = 0; phase = 0;
pendingTile = -1; pendingPrice = 0; pendingAction = 0;
started = true; gameOver = false; win = false;
pushMsg("游戏开始！每人¥15000，经过起点+¥2000。");
refreshUI();
}

void movePlayer(int p, int steps, bool trigger) {
int old = ps[p].pos;
ps[p].pos = (ps[p].pos + steps) % 30;
if (ps[p].pos < old) {
ps[p].money += 2000;
pushMsg(std::string(pname(p)) + "经过起点，+¥2000。");
}
if (trigger) resolveTile(p, p == 0);
}

void doRoll(int p) {
dice = 1 + randInt(6);
pushMsg(std::string(pname(p)) + "掷出" + std::to_string(dice) + "点。");
movePlayer(p, dice, true);
}

void applyCard(int p, const rich::Card& c) {
pushMsg(std::string(pname(p)) + "抽卡：" + c.text);
switch (c.kind) {
case 0: ps[p].money += c.amount; break;
case 1: {
int old = ps[p].pos;
ps[p].pos = (ps[p].pos + c.amount) % 30;
if (ps[p].pos < old) ps[p].money += 2000;
break;
}
case 2: ps[p].pos = (ps[p].pos - c.amount + 30) % 30; break;
case 3: ps[p].pos = 6; ps[p].skip = 2; break;
case 4:
for (int q = 0; q < 3; ++q)
if (q!= p &&!ps[q].bankrupt) { ps[q].money -= c.amount; ps[p].money += c.amount;}
break;
case 5: ps[p].money -= ps[p].money * c.amount / 100; break;
}
sellToCover(p);
checkBankrupt(p);
}

void sellToCover(int p) {
while (ps[p].money < 0) {
int best = -1, bestVal = -1;
for (int i = 0; i < 30; ++i) {
if (rich::kTiles[i].type!= rich::T_PROP || tiles[i].owner!= p) continue;
int v = (rich::kTiles[i].price + tiles[i].level * upCostOf(rich::kTiles[i].price)) / 2;
if (v > bestVal) { bestVal = v; best = i;}
}
if (best < 0) break;
ps[p].money += bestVal;
pushMsg(std::string(pname(p)) + "变卖" + rich::kTiles[best].name + "回血¥" +
std::to_string(bestVal) + "。");
tiles[best] = TileRT{};
}
}

void checkBankrupt(int p) {
if (ps[p].money >= 0 || ps[p].bankrupt) return;
ps[p].bankrupt = true;
for (int i = 0; i < 30; ++i)
if (tiles[i].owner == p) tiles[i] = TileRT{};
pushMsg(std::string(pname(p)) + "破产出局！");
checkGameOver();
}

void checkGameOver() {
if (gameOver) return;
if (ps[0].bankrupt) { gameOver = true; win = false; phase = 5;}
else if (ps[1].bankrupt && ps[2].bankrupt) { gameOver = true; win = true; phase = 5;}
}

void payToll(int p, int owner, int tile) {
int toll = tollOf(rich::kTiles[tile].price, tiles[tile].level);
ps[p].money -= toll;
ps[owner].money += toll;
pushMsg(std::string(pname(p)) + "交过路费¥" + std::to_string(toll) +
"给" + pname(owner) + "。");
sellToCover(p);
checkBankrupt(p);
}

// 人类玩家的选择通过弹窗屏完成（调用方在 resolveTile 后按 pendingAction 推屏）
int pendingAction = 0; // 0 无 1 买地 2 升级 3 监狱

void resolveTile(int p, bool human) {
if (gameOver) return;
int i = ps[p].pos;
int type = rich::kTiles[i].type;
if (type == rich::T_PROP) {
int price = rich::kTiles[i].price;
if (tiles[i].owner < 0) {
if (ps[p].money < price) {
pushMsg(std::string(pname(p)) + "买不起" + rich::kTiles[i].name + "。");
} else if (human) {
pendingTile = i; pendingPrice = price; pendingAction = 1;
choiceText.set(std::string("买下") + rich::kTiles[i].name + "？售价¥" + std::to_string(price) + "。");
} else {
if (ps[p].money > price * 2) {
ps[p].money -= price; tiles[i].owner = p;
pushMsg(std::string(pname(p)) + "买下" + rich::kTiles[i].name +
"（¥" + std::to_string(price) + "）。");
} else {
pushMsg(std::string(pname(p)) + "放弃购买" + rich::kTiles[i].name + "。");
}
}
} else if (tiles[i].owner == p) {
if (tiles[i].level >= 3) {
pushMsg(std::string(rich::kTiles[i].name) + "已是大厦，无需升级。");
} else {
int cost = upCostOf(price);
if (ps[p].money < cost) {
pushMsg(std::string(pname(p)) + "没钱升级" + rich::kTiles[i].name + "。");
} else if (human) {
pendingTile = i; pendingPrice = cost; pendingAction = 2;
choiceText.set(std::string("把") + rich::kTiles[i].name + "升级到" +
rich::kLevelName[tiles[i].level + 1] + "？费用¥" + std::to_string(cost) + "。");
} else {
if (ps[p].money > cost * 3) {
ps[p].money -= cost; tiles[i].level++;
pushMsg(std::string(pname(p)) + "把" + rich::kTiles[i].name + "升级到" +
rich::kLevelName[tiles[i].level] + "。");
}
}
}
} else {
payToll(p, tiles[i].owner, i);
}
} else if (type == rich::T_CHANCE) {
applyCard(p, rich::kChance[randInt(8)]);
} else if (type == rich::T_FATE) {
applyCard(p, rich::kFate[randInt(8)]);
} else if (type == rich::T_JAIL) {
if (human) {
pendingAction = 3;
choiceText.set("入狱！交¥1000保释，还是服刑2轮？");
} else {
if (ps[p].money > 3000) {
ps[p].money -= 1000;
pushMsg(std::string(pname(p)) + "交¥1000保释出狱。");
} else {
ps[p].skip = 2;
pushMsg(std::string(pname(p)) + "入狱，停2轮。");
}
}
} else if (type == rich::T_LOTTERY) {
if (ps[p].money >= 500) {
ps[p].money -= 500;
if (randInt(100) < 50) {
ps[p].money += 5000;
pushMsg(std::string(pname(p)) + "彩票中奖+¥5000！");
} else {
pushMsg(std::string(pname(p)) + "买彩票没中（-¥500）。");
}
} else {
pushMsg(std::string(pname(p)) + "没钱买彩票。");
}
}
refreshUI();
}

void humanRoll() {
if (!started || gameOver || phase!= 0 || cur!= 0) return;
if (ps[0].skip > 0) return;
pendingAction = 0;
doRoll(0);
if (!gameOver) phase = 4;
refreshUI();
}

void humanBuy(bool yes) {
if (pendingAction!= 1) return;
if (yes && pendingTile >= 0 && tiles[pendingTile].owner < 0 && ps[0].money >= pendingPrice) {
ps[0].money -= pendingPrice;
tiles[pendingTile].owner = 0;
pushMsg(std::string("你买下") + rich::kTiles[pendingTile].name +
"（¥" + std::to_string(pendingPrice) + "）。");
} else if (!yes) {
pushMsg("你放弃购买。");
}
pendingTile = -1; pendingAction = 0;
refreshUI();
}

void humanUpgrade(bool yes) {
if (pendingAction!= 2) return;
if (yes && pendingTile >= 0 && tiles[pendingTile].owner == 0 &&
tiles[pendingTile].level < 3 && ps[0].money >= pendingPrice) {
ps[0].money -= pendingPrice;
tiles[pendingTile].level++;
pushMsg(std::string("你把") + rich::kTiles[pendingTile].name + "升级到" +
rich::kLevelName[tiles[pendingTile].level] + "。");
} else if (!yes) {
pushMsg("你跳过升级。");
}
pendingTile = -1; pendingAction = 0;
refreshUI();
}

void humanJail(bool pay) {
if (pendingAction!= 3) return;
if (pay && ps[0].money >= 1000) {
ps[0].money -= 1000;
pushMsg("你交¥1000保释出狱。");
} else {
ps[0].skip = 2;
pushMsg("你入狱，停2轮。");
}
pendingAction = 0;
refreshUI();
}

void aiTurn(int p) {
if (gameOver || ps[p].bankrupt) return;
if (ps[p].skip > 0) {
ps[p].skip--;
pushMsg(std::string(pname(p)) + "跳过一轮（剩余" + std::to_string(ps[p].skip) + "轮）。");
return;
}
doRoll(p);
}

void endTurn() {
if (!started || gameOver || phase!= 4) return;
for (int p = 1; p < 3; ++p) {
cur = p;
aiTurn(p);
if (gameOver) { cur = 0; refreshUI(); return;}
}
cur = 0;
round++;
if (ps[0].skip > 0) {
ps[0].skip--;
pushMsg("你跳过一轮（剩余" + std::to_string(ps[0].skip) + "轮）。");
phase = 4;
} else {
phase = 0;
}
dice = 0;
refreshUI();
}
};

// ---------- Screens ----------
struct ChoiceScreen {
Model& model;
explicit ChoiceScreen(Model& m): model(m) {}
auto view() {
return VerticalScroll(Column(
Text("选择").font(Font::title).fill(),
Text(model.choiceText).font(Font::headline).fill(),
Column(
Button("是").on_click([this] {
int a = model.pendingAction;
if (a == 1) model.humanBuy(true);
else if (a == 2) model.humanUpgrade(true);
else if (a == 3) model.humanJail(true);
(void)model.routes.pop();
}).height(40_dp).fill(),
Button("否").on_click([this] {
int a = model.pendingAction;
if (a == 1) model.humanBuy(false);
else if (a == 2) model.humanUpgrade(false);
else if (a == 3) model.humanJail(false);
(void)model.routes.pop();
}).height(40_dp).fill()
).gap(8_dp).fill()
).gap(12_dp).fill().padding(20_dp));
}
};

struct GameScreen {
Model& model;
explicit GameScreen(Model& m): model(m) {}
auto view() {
return VerticalScroll(Column(
Text(model.statusText).font(Font::caption).fill(),
Text(model.tileText).font(Font::headline).fill(),
Column(
Button("掷骰子").on_click([this] {
if (model.phase == 0) {
model.humanRoll();
if (model.pendingAction > 0 &&!model.gameOver)
(void)model.routes.push<ChoiceScreen>(std::ref(model));
} else {
model.pushMsg("现在不能掷骰子。"); model.refreshUI();
}
}).height(36_dp).fill(),
Button("结束回合").on_click([this] {
if (model.phase == 4) model.endTurn();
else { model.pushMsg("还没掷骰子，不能结束回合。"); model.refreshUI();}
}).height(36_dp).fill(),
Button("重开").on_click([this] { model.startGame();}).height(36_dp).fill()
).gap(6_dp).fill(),
Text(model.msgText).font(Font::caption).fill(),
Text(model.boardText).font(Font::caption).fill()
).gap(4_dp).fill().padding(14_dp));
}
};

struct HelpScreen {
Model& model;
explicit HelpScreen(Model& m): model(m) {}
auto view() {
return VerticalScroll(Column(
Text("玩法说明").font(Font::title).fill(),
Text("目标：让两家AI破产，自己别破产。").font(Font::label).fill(),
Text("掷骰走格：经过起点+¥2000。").font(Font::label).fill(),
Text("地产：无主可买，自己的可升级（空地→小屋→洋房→大厦），踩到别人地盘交过路费。").font(Font::label).fill(),
Text("机会/命运：抽卡，好坏参半。").font(Font::label).fill(),
Text("监狱：交¥1000或停2轮。彩票：¥500博¥5000。").font(Font::label).fill(),
Text("没钱交费会自动变卖地产，还不够就破产出局。").font(Font::label).fill(),
Button("返回").on_click([this] { (void)model.routes.pop();}).height(36_dp).fill()
).gap(8_dp).fill().padding(20_dp));
}
};

struct TitleScreen {
Model& model;
explicit TitleScreen(Model& m): model(m) {}
auto view() {
return VerticalScroll(Column(
Text("大富翁").font(Font::title).fill(),
Text("PXA 移植版").font(Font::headline).fill(),
Text("30格 · 3玩家 · 回合制").font(Font::label).fill(),
Button("开始游戏").on_click([this] {
model.startGame();
(void)model.routes.push<GameScreen>(std::ref(model));
}).height(36_dp).fill(),
Button("玩法说明").on_click([this] {
(void)model.routes.push<HelpScreen>(std::ref(model));
}).height(36_dp).fill()
).gap(12_dp).fill().padding(20_dp));
}
};

struct RichmanApp {
Model model;
Navigator<8, 16384, 128>& navigation() noexcept { return model.routes;}
Result<void> on_start(Context& context) {
        auto window = context.window().fullscreen();
        if (!window) return std::unexpected(window.error());
return model.routes.push<TitleScreen>(std::ref(model));
}
BackAction on_back() {
if (model.pendingAction > 0) {
int a = model.pendingAction;
if (a == 1) model.humanBuy(false);
else if (a == 2) model.humanUpgrade(false);
else if (a == 3) model.humanJail(false);
}
if (!model.routes.can_pop()) return BackAction::close;
(void)model.routes.pop();
return BackAction::stay;
}
};

PXA_APPLICATION(RichmanApp)
