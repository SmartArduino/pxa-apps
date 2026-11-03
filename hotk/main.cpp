#include <pxa/app.hpp>
#include "../common/cpp/vertical_scroll.hpp"
#include <pxa/ui_component.hpp>
#include <pxa/ui_environment.hpp>
#include <pxa/ui_layout.hpp>
#include <pxa/list.hpp>
#include "hotk_data.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace pxa;
using namespace pxa::ui;
using namespace pxa::ui::literals;

// Fan port of Oliver714/hotk-gba-demake (GPL-3.0) core loop.
// Original GBA sprites/music/avatars are NOT reused; this version uses text UI
// and the original game data (city/general names and stats) for gameplay.
struct CityRT {
    int owner = -1;
    int major = -1;
    int pop = 0;
    int def = 0;
    int money = 0;
    bool searched = false;
    bool recruited = false;
    bool developed = false;
};
struct GenRT {
    int city = -1;
    int king = -1;
    int status = 0;  // 0 absent, 1 wild, 2 employed, 3 prisoner
    int soldiers = 0;
};

struct Model {
    Navigator<8, 16384, 128> routes;
    // Selection (pager, works with touch and controller focus)
    State<int> selScenario{1};  // default 讨伐董卓 189
    State<int> selRulerPos{0};
    // Game UI texts (State so the view refreshes)
    State<std::string> statusText{"未开始"};
    State<std::string> cityText{""};
    State<std::string> generalsText{""};
    State<std::string> targetText{""};
    State<std::string> battleText{"暂无战斗"};
    State<std::string> messageText{"选择剧本和君主开始游戏。"};
    State<std::string> scenarioText{""};
    State<std::string> rulerText{""};

    int scenario = 1;
    int playerKing = 1;
    int year = 189;
    int month = 1;
    int selectedCity = 0;
    int targetCity = -1;
    int targetPos = 0;
    bool battleActive = false;
    int battleRoundNum = 0;
    int atkCity = -1;
    int defCity = -1;
    int atkTotal = 0;
    int defTotal = 0;
    int atkStart = 0;
    int defStart = 0;
    std::vector<int> atkGens;
    std::vector<int> defGens;
    std::string battleLog;
    std::string msgLog;
    std::uint32_t rng = 0x12345678u;
    CityRT cities[48]{};
    GenRT gens[255]{};
    bool started = false;

    int randInt(int n) {
        if (n <= 0) return 0;
        rng = rng * 1664525u + 1013904223u;
        return int((rng >> 16) % (unsigned)n);
    }
    const char* kingName(int scenarioIdx, int kingIdx) const {
        if (kingIdx < 0 || kingIdx >= 26) return "空";
        const char* n = hotk::kKings[scenarioIdx][kingIdx].name;
        return (n && n[0]) ? n : "空";
    }
    int ownCount() const {
        int c = 0;
        for (int i = 0; i < 48; ++i) if (cities[i].owner == playerKing) ++c;
        return c;
    }
    int citySoldiers(int city) const {
        int s = 0;
        for (int i = 0; i < 255; ++i)
            if (gens[i].city == city && gens[i].status == 2) s += gens[i].soldiers;
        return s;
    }
    std::vector<int> cityGenerals(int city, int kingFilter) const {
        std::vector<int> out;
        for (int i = 0; i < 255; ++i) {
            if (gens[i].city != city) continue;
            if (gens[i].status != 2) continue;
            if (kingFilter >= 0 && gens[i].king != kingFilter) continue;
            out.push_back(i);
        }
        std::sort(out.begin(), out.end(), [&](int a, int b) {
            return hotk::kGenerals[a].str > hotk::kGenerals[b].str;
        });
        return out;
    }
    std::vector<int> adjacentEnemies(int city) const {
        std::vector<int> out;
        for (int k = 0; k < 9; ++k) {
            int nb = hotk::kNeighbors[city][k];
            if (nb < 0 || nb >= 48) continue;
            if (cities[nb].owner == playerKing) continue;
            out.push_back(nb);
        }
        return out;
    }
    void pushMsg(const std::string& m) {
        msgLog = m + "\n" + msgLog;
        if (msgLog.size() > 900) {
            // UI text must remain valid UTF-8 after long games.
            std::size_t end = 900;
            while (end && (static_cast<unsigned char>(msgLog[end]) & 0xc0) == 0x80) --end;
            msgLog.resize(end);
        }
        messageText.set(msgLog);
    }
    void startGame(int scenarioIdx, int kingIdx) {
        scenario = scenarioIdx;
        playerKing = kingIdx;
        year = hotk::kScenarios[scenarioIdx].year;
        month = 1;
        rng = 0x12345678u + (unsigned)(scenarioIdx * 1000 + kingIdx * 77 + 13);
        for (int i = 0; i < 48; ++i) {
            auto& ci = hotk::kCityInit[scenarioIdx][i];
            cities[i].owner = ci.king;
            cities[i].major = ci.major;
            cities[i].pop = ci.pop;
            cities[i].def = ci.def;
            cities[i].money = ci.money;
            cities[i].searched = false;
            cities[i].recruited = false;
            cities[i].developed = false;
        }
        for (int i = 0; i < 255; ++i) {
            auto& gi = hotk::kGenInit[scenarioIdx][i];
            gens[i].city = gi.city;
            gens[i].king = gi.king;
            gens[i].status = gi.status;
            if (gi.status == 2 && gi.city >= 0) {
                int s = hotk::kGenerals[i].str;
                int it = hotk::kGenerals[i].intel;
                gens[i].soldiers = 800 + (s + it) * 5;
            } else {
                gens[i].soldiers = 0;
            }
        }
        selectedCity = 0;
        for (int i = 0; i < 48; ++i) if (cities[i].owner == playerKing) { selectedCity = i; break; }
        targetCity = -1;
        targetPos = 0;
        battleActive = false;
        battleRoundNum = 0;
        battleLog.clear();
        msgLog.clear();
        started = true;
        std::string s = std::string("游戏开始：") + hotk::kScenarios[scenarioIdx].name + " " +
            std::to_string(year) + "年，你是" + kingName(scenarioIdx, playerKing) + "。";
        pushMsg(s);
        refreshUI();
    }
    void refreshUI() {
        std::string st = std::string(hotk::kScenarios[scenario].name) + " " +
            std::to_string(year) + "年" + std::to_string(month) + "月\n君主：" +
            kingName(scenario, playerKing) + " 城池：" + std::to_string(ownCount()) + "/48";
        statusText.set(st);
        if (!started) { cityText.set(""); generalsText.set(""); targetText.set(""); battleText.set("暂无战斗"); return; }
        auto& c = cities[selectedCity];
        std::string ct = std::string(hotk::kCities[selectedCity].name) + " · " +
            (c.owner < 0 ? std::string("空城") : std::string(kingName(scenario, c.owner))) +
            (c.owner == playerKing ? std::string("（你的城）") : std::string("")) +
            "\n人口" + std::to_string(c.pop) + " 防" + std::to_string(c.def) +
            " 金" + std::to_string(c.money) + " 兵" + std::to_string(citySoldiers(selectedCity));
        cityText.set(ct);
        auto gs = cityGenerals(selectedCity, -1);
        std::string gt = "武将：";
        if (gs.empty()) gt += "无";
        else {
            gt += std::string(hotk::kGenerals[gs[0]].name) + "等" + std::to_string(gs.size()) + "人";
        }
        int wild = 0;
        for (int i = 0; i < 255; ++i)
            if (gens[i].city == selectedCity && gens[i].status == 1) ++wild;
        if (wild) gt += " 在野" + std::to_string(wild);
        generalsText.set(gt);
        auto enemies = adjacentEnemies(selectedCity);
        if (cities[selectedCity].owner != playerKing) {
            targetText.set("选中非己方城池，只能查看，不能内政。出征请先切回自己的城。");
        } else if (enemies.empty()) {
            targetText.set("相邻无可攻击目标（周围都是你的城或无路）。");
            targetCity = -1;
        } else {
            if (targetPos < 0) targetPos = 0;
            if (targetPos >= (int)enemies.size()) targetPos = 0;
            targetCity = enemies[targetPos];
            auto& tc = cities[targetCity];
            targetText.set(std::string("目标：") + hotk::kCities[targetCity].name +
                "（" + std::to_string(targetPos + 1) + "/" + std::to_string(enemies.size()) + "） " +
                (tc.owner < 0 ? std::string("空城") : std::string(kingName(scenario, tc.owner))) +
                " 防" + std::to_string(tc.def));
        }
        if (battleActive) {
            battleText.set(std::string("战斗：第") + std::to_string(battleRoundNum) + "回合" +
                "\n攻：" + std::to_string(atkTotal) + " 守：" + std::to_string(defTotal) +
                "\n" + battleLog);
        } else {
            battleText.set(battleLog.empty() ? std::string("暂无战斗") : battleLog);
        }
    }
    void doSearch() {
        if (!started) return;
        auto& c = cities[selectedCity];
        if (c.owner != playerKing) { pushMsg("只能在自己的城搜索。"); refreshUI(); return; }
        if (c.searched) { pushMsg("本月已搜索过，下月再来。"); refreshUI(); return; }
        c.searched = true;
        std::vector<int> wild;
        for (int i = 0; i < 255; ++i)
            if (gens[i].city == selectedCity && gens[i].status == 1) wild.push_back(i);
        if (wild.empty()) { pushMsg(std::string("在") + hotk::kCities[selectedCity].name + "搜索，没有发现人才。"); refreshUI(); return; }
        int pick = wild[randInt((int)wild.size())];
        gens[pick].status = 2;
        gens[pick].king = playerKing;
        gens[pick].soldiers = 800 + (hotk::kGenerals[pick].str + hotk::kGenerals[pick].intel) * 5;
        pushMsg(std::string("搜索成功！") + hotk::kGenerals[pick].name + "加入了你。");
        refreshUI();
    }
    void doRecruit() {
        if (!started) return;
        auto& c = cities[selectedCity];
        if (c.owner != playerKing) { pushMsg("只能在自己的城征兵。"); refreshUI(); return; }
        if (c.recruited) { pushMsg("本月已征兵过。"); refreshUI(); return; }
        if (c.money < 500) { pushMsg("金钱不足 500，无法征兵。"); refreshUI(); return; }
        c.recruited = true;
        c.money -= 500;
        auto gs = cityGenerals(selectedCity, playerKing);
        for (int gi : gs) gens[gi].soldiers += 600;
        c.def += 20;
        pushMsg(std::string("在") + hotk::kCities[selectedCity].name + "征兵：武将兵力+600，防御+20。");
        refreshUI();
    }
    void doDevelop() {
        if (!started) return;
        auto& c = cities[selectedCity];
        if (c.owner != playerKing) { pushMsg("只能在自己的城开发。"); refreshUI(); return; }
        if (c.developed) { pushMsg("本月已开发过。"); refreshUI(); return; }
        c.developed = true;
        c.money += 2000;
        c.pop += 3000;
        if (c.pop > 999999) c.pop = 999999;
        c.def += 20;
        pushMsg(std::string("在") + hotk::kCities[selectedCity].name + "开发：金钱+2000，人口+3000，防御+20。");
        refreshUI();
    }
    void startBattle() {
        if (!started || battleActive) return;
        if (cities[selectedCity].owner != playerKing) { pushMsg("请先切回自己的城再出征。"); refreshUI(); return; }
        auto enemies = adjacentEnemies(selectedCity);
        if (enemies.empty()) { pushMsg("无可攻击目标。"); refreshUI(); return; }
        if (targetPos < 0 || targetPos >= (int)enemies.size()) targetPos = 0;
        int target = enemies[targetPos];
        atkCity = selectedCity;
        defCity = target;
        atkGens = cityGenerals(atkCity, playerKing);
        if ((int)atkGens.size() > 3) atkGens.resize(3);
        int defOwner = cities[defCity].owner;
        defGens = (defOwner < 0) ? std::vector<int>{} : cityGenerals(defCity, defOwner);
        if ((int)defGens.size() > 3) defGens.resize(3);
        atkTotal = 0; for (int g : atkGens) atkTotal += gens[g].soldiers;
        defTotal = 0; for (int g : defGens) defTotal += gens[g].soldiers;
        defTotal += cities[defCity].def * 2;
        atkStart = atkTotal; defStart = defTotal;
        battleRoundNum = 0;
        battleActive = true;
        battleLog = std::string("出征：") + hotk::kCities[atkCity].name + " -> " + hotk::kCities[defCity].name + "，战斗开始！";
        pushMsg(battleLog);
        refreshUI();
    }
    void finishBattle(bool attackerWon) {
        int oldOwner = cities[defCity].owner;
        if (attackerWon) {
            cities[defCity].owner = playerKing;
            // defenders retreat or become wild
            for (int g : defGens) {
                int placed = -1;
                for (int k = 0; k < 9; ++k) {
                    int nb = hotk::kNeighbors[defCity][k];
                    if (nb < 0) continue;
                    if (cities[nb].owner == oldOwner) { placed = nb; break; }
                }
                if (placed >= 0) {
                    gens[g].city = placed;
                    gens[g].soldiers = std::max(100, gens[g].soldiers / 2);
                } else {
                    gens[g].city = defCity;
                    gens[g].status = 1;
                    gens[g].soldiers = 0;
                }
            }
            // distribute remaining attacker soldiers
            if (!atkGens.empty() && atkTotal > 0) {
                int per = atkTotal / (int)atkGens.size();
                for (int g : atkGens) gens[g].soldiers = per;
            } else {
                for (int g : atkGens) gens[g].soldiers = 100;
            }
            battleLog += "\n胜利！占领了" + std::string(hotk::kCities[defCity].name) + "。";
            pushMsg(battleLog);
        } else {
            for (int g : atkGens) gens[g].soldiers = std::max(100, gens[g].soldiers / 2);
            battleLog += "\n进攻失败，部队退回" + std::string(hotk::kCities[atkCity].name) + "。";
            pushMsg(battleLog);
        }
        battleActive = false;
        // check victory/defeat
        int own = ownCount();
        if (own >= 48) pushMsg("统一天下！你赢了！可以继续游玩或重新开始。");
        else if (own <= 0) pushMsg("你失去了所有城池，游戏结束。请重新开始。");
        refreshUI();
    }
    void doBattleRound() {
        if (!battleActive) return;
        ++battleRoundNum;
        int atkStr = 0, defStr = 0;
        for (int g : atkGens) atkStr += hotk::kGenerals[g].str;
        for (int g : defGens) defStr += hotk::kGenerals[g].str;
        if (atkGens.empty()) atkStr = 10;
        if (defGens.empty()) defStr = 10;
        int atkDmg = atkStr * 5 + randInt(40);
        int defDmg = defStr * 5 + cities[defCity].def / 10 + randInt(40);
        defTotal -= atkDmg;
        atkTotal -= defDmg;
        if (atkTotal < 0) atkTotal = 0;
        if (defTotal < 0) defTotal = 0;
        battleLog += "\n第" + std::to_string(battleRoundNum) + "回合：攻方造成" + std::to_string(atkDmg) +
            "，守方造成" + std::to_string(defDmg) + "。";
        if (battleRoundNum >= 20 || atkTotal == 0 || defTotal == 0) {
            finishBattle(atkTotal > defTotal);
        } else {
            refreshUI();
        }
    }
    void autoBattle() {
        if (!battleActive) return;
        while (battleActive && battleRoundNum < 20) doBattleRound();
    }
    void endMonth() {
        if (!started) return;
        if (battleActive) { pushMsg("战斗中不能结束本月，请先打完或撤退。"); refreshUI(); return; }
        // income + reset
        for (int i = 0; i < 48; ++i) {
            cities[i].money += cities[i].pop / 100;
            cities[i].pop += 1000;
            if (cities[i].pop > 999999) cities[i].pop = 999999;
            cities[i].def += 5;
            if (cities[i].def > 9999) cities[i].def = 9999;
            cities[i].searched = false;
            cities[i].recruited = false;
            cities[i].developed = false;
        }
        // AI: claim empty adjacent + attack player if much stronger
        int scenKings = hotk::kScenarios[scenario].king_count;
        for (int city = 0; city < 48; ++city) {
            int owner = cities[city].owner;
            if (owner < 0 || owner == playerKing) continue;
            if (owner >= scenKings) continue;
            for (int k = 0; k < 9; ++k) {
                int nb = hotk::kNeighbors[city][k];
                if (nb < 0) continue;
                if (cities[nb].owner == -1 && randInt(100) < 25) {
                    cities[nb].owner = owner;
                } else if (cities[nb].owner == playerKing && randInt(100) < 35) {
                    int aiSold = citySoldiers(city) + cities[city].def * 2;
                    int plSold = citySoldiers(nb) + cities[nb].def * 2;
                    if (aiSold > plSold * 13 / 10) {
                        // AI wins the city, player generals retreat
                        auto defenders = cityGenerals(nb, playerKing);
                        cities[nb].owner = owner;
                        for (int g : defenders) {
                            gens[g].city = nb;
                            gens[g].status = 1;
                            gens[g].soldiers = 0;
                        }
                        pushMsg(std::string("敌军攻陷了") + hotk::kCities[nb].name + "！");
                    }
                }
            }
        }
        ++month;
        if (month > 12) { month = 1; ++year; }
        pushMsg(std::string("进入 ") + std::to_string(year) + "年" + std::to_string(month) + "月。各城已发俸禄，AI 已行动。");
        // keep selected city if still owned, else jump to first owned
        if (cities[selectedCity].owner != playerKing) {
            for (int i = 0; i < 48; ++i) if (cities[i].owner == playerKing) { selectedCity = i; break; }
        }
        targetPos = 0;
        refreshUI();
        int own = ownCount();
        if (own <= 0) pushMsg("你失去了所有城池，游戏结束。请点“重新开始本局”。");
        else if (own >= 48) pushMsg("统一天下！你赢了！");
        refreshUI();
    }
};

// ---------- Screens (Navigator order: Game first, then Ruler, Scenario, Help, Title) ----------
struct GameScreen {
    Model& model;
    explicit GameScreen(Model& m) : model(m) {}
    auto view() {
        return VerticalScroll(Column(
            Text(model.statusText).font(Font::caption).fill(),
            Text(model.cityText).font(Font::caption).fill(),
            Text(model.generalsText).font(Font::caption).fill(),
            Text(model.targetText).font(Font::caption).fill(),
            Text(model.battleText).font(Font::caption).fill(),
            Row(
                Button("上一城").on_click([this] {
                    model.selectedCity = (model.selectedCity + 47) % 48;
                    model.targetPos = 0; model.refreshUI();
                }).height(32_dp).grow(),
                Button("下一城").on_click([this] {
                    model.selectedCity = (model.selectedCity + 1) % 48;
                    model.targetPos = 0; model.refreshUI();
                }).height(32_dp).grow()
            ).gap(6_dp).fill(),
            Row(
                Button("搜索人才").on_click([this] { model.doSearch(); }).height(32_dp).grow(),
                Button("征兵").on_click([this] { model.doRecruit(); }).height(32_dp).grow(),
                Button("开发").on_click([this] { model.doDevelop(); }).height(32_dp).grow()
            ).gap(6_dp).fill(),
            Row(
                Button("上一目标").on_click([this] {
                    auto e = model.adjacentEnemies(model.selectedCity);
                    if (!e.empty()) { model.targetPos = (model.targetPos + (int)e.size() - 1) % (int)e.size(); model.refreshUI(); }
                }).height(32_dp).grow(),
                Button("下一目标").on_click([this] {
                    auto e = model.adjacentEnemies(model.selectedCity);
                    if (!e.empty()) { model.targetPos = (model.targetPos + 1) % (int)e.size(); model.refreshUI(); }
                }).height(32_dp).grow(),
                Button("出征").on_click([this] { model.startBattle(); }).height(32_dp).grow()
            ).gap(6_dp).fill(),
            Row(
                Button("进攻一回合").on_click([this] { model.doBattleRound(); }).height(32_dp).grow(),
                Button("自动打完").on_click([this] { model.autoBattle(); }).height(32_dp).grow(),
                Button("撤退").on_click([this] {
                    if (model.battleActive) { model.battleActive = false; model.battleLog += "\n你选择了撤退。"; model.pushMsg("撤退回城。"); model.refreshUI(); }
                }).height(32_dp).grow()
            ).gap(6_dp).fill(),
            Row(
                Button("结束本月").on_click([this] { model.endMonth(); }).height(36_dp).grow(),
                Button("重新开始本局").on_click([this] { model.startGame(model.scenario, model.playerKing); }).height(36_dp).grow()
            ).gap(6_dp).fill(),
            Text(model.messageText).font(Font::caption).fill()
        ).gap(4_dp).fill().padding(16_dp));
    }
};

struct RulerScreen {
    Model& model;
    explicit RulerScreen(Model& m) : model(m) {}
    std::vector<int> options() const {
        std::vector<int> out;
        int sc = model.selScenario.get();
        int kc = hotk::kScenarios[sc].king_count;
        for (int i = 0; i < kc; ++i) if (hotk::kKings[sc][i].selectable) out.push_back(i);
        return out;
    }
    auto view() {
        return VerticalScroll(Column(
            Text("选择君主").font(Font::title).fill(),
            Text(model.rulerText).font(Font::headline).fill(),
            Text("切换君主，确认开始。").font(Font::label).fill(),
            Row(
                Button("上一个").on_click([this] {
                    auto o = options();
                    if (!o.empty()) {
                        int p = model.selRulerPos.get();
                        p = (p + (int)o.size() - 1) % (int)o.size();
                        model.selRulerPos.set(p);
                        int king = o[p];
                        model.rulerText.set(std::string(hotk::kKings[model.selScenario.get()][king].name) + "（可扮演）");
                    }
                }).height(32_dp).grow(),
                Button("下一个").on_click([this] {
                    auto o = options();
                    if (!o.empty()) {
                        int p = model.selRulerPos.get();
                        p = (p + 1) % (int)o.size();
                        model.selRulerPos.set(p);
                        int king = o[p];
                        model.rulerText.set(std::string(hotk::kKings[model.selScenario.get()][king].name) + "（可扮演）");
                    }
                }).height(32_dp).grow()
            ).gap(6_dp).fill(),
            Button("确认开始").on_click([this] {
                auto o = options();
                if (o.empty()) return;
                int king = o[model.selRulerPos.get() % (int)o.size()];
                model.startGame(model.selScenario.get(), king);
                (void)model.routes.push<GameScreen>(std::ref(model));
            }).height(36_dp).fill(),
            Button("返回").on_click([this] { (void)model.routes.pop(); }).height(32_dp).fill()
        ).gap(10_dp).fill().padding(20_dp));
    }
};

struct ScenarioScreen {
    Model& model;
    explicit ScenarioScreen(Model& m) : model(m) {}
    auto view() {
        return VerticalScroll(Column(
            Text("选择剧本").font(Font::title).fill(),
            Text(model.scenarioText).font(Font::headline).fill(),
            Text("切换剧本，确认选君主。").font(Font::label).fill(),
            Row(
                Button("上一个").on_click([this] {
                    int s = (model.selScenario.get() + 4) % 5;
                    model.selScenario.set(s);
                    model.selRulerPos.set(0);
                    model.scenarioText.set(std::string(hotk::kScenarios[s].name) + " " + std::to_string(hotk::kScenarios[s].year) + "年");
                    // default ruler text
                    int kc = hotk::kScenarios[s].king_count;
                    for (int i = 0; i < kc; ++i) if (hotk::kKings[s][i].selectable) { model.rulerText.set(std::string(hotk::kKings[s][i].name) + "（可扮演）"); break; }
                }).height(32_dp).grow(),
                Button("下一个").on_click([this] {
                    int s = (model.selScenario.get() + 1) % 5;
                    model.selScenario.set(s);
                    model.selRulerPos.set(0);
                    model.scenarioText.set(std::string(hotk::kScenarios[s].name) + " " + std::to_string(hotk::kScenarios[s].year) + "年");
                    int kc = hotk::kScenarios[s].king_count;
                    for (int i = 0; i < kc; ++i) if (hotk::kKings[s][i].selectable) { model.rulerText.set(std::string(hotk::kKings[s][i].name) + "（可扮演）"); break; }
                }).height(32_dp).grow()
            ).gap(6_dp).fill(),
            Button("确认剧本，选君主").on_click([this] {
                (void)model.routes.push<RulerScreen>(std::ref(model));
            }).height(36_dp).fill(),
            Button("返回").on_click([this] { (void)model.routes.pop(); }).height(32_dp).fill()
        ).gap(10_dp).fill().padding(20_dp));
    }
};

struct HelpScreen {
    Model& model;
    explicit HelpScreen(Model& m) : model(m) {}
    auto view() {
        return VerticalScroll(Column(
            Text("玩法说明").font(Font::title).fill(),
            Text("目标：占领48城统一天下。").font(Font::label).fill(),
            Text("大地图：切换48城查看信息。").font(Font::label).fill(),
            Text("内政：搜索、征兵、开发。").font(Font::label).fill(),
            Text("出征：选相邻敌城进攻。").font(Font::label).fill(),
            Text("结束本月：发俸禄，AI行动。").font(Font::label).fill(),
            Text("操作：触摸或手柄A确认。").font(Font::label).fill(),
            Text("来源：hotk-gba-demake 致敬移植").font(Font::caption).fill(),
            Button("返回").on_click([this] { (void)model.routes.pop(); }).height(36_dp).fill()
        ).gap(8_dp).fill().padding(20_dp));
    }
};

struct TitleScreen {
    Model& model;
    explicit TitleScreen(Model& m) : model(m) {}
    auto view() {
        return VerticalScroll(Column(
            Text("三国群英传").font(Font::title).fill(),
            Text("PXA 移植版").font(Font::headline).fill(),
            Text("48城 · 255将 · 5剧本").font(Font::label).fill(),
            Button("开始游戏").on_click([this] {
                int s = model.selScenario.get();
                model.scenarioText.set(std::string(hotk::kScenarios[s].name) + " " + std::to_string(hotk::kScenarios[s].year) + "年");
                int kc = hotk::kScenarios[s].king_count;
                for (int i = 0; i < kc; ++i) if (hotk::kKings[s][i].selectable) { model.rulerText.set(std::string(hotk::kKings[s][i].name) + "（可扮演）"); break; }
                model.selRulerPos.set(0);
                (void)model.routes.push<ScenarioScreen>(std::ref(model));
            }).height(36_dp).fill(),
            Button("玩法说明").on_click([this] {
                (void)model.routes.push<HelpScreen>(std::ref(model));
            }).height(36_dp).fill()
        ).gap(12_dp).fill().padding(20_dp));
    }
};

struct HotkApp {
    Model model;
    Navigator<8, 16384, 128>& navigation() noexcept { return model.routes; }
    Result<void> on_start(Context& context) {
        auto window = context.window().fullscreen();
        if (!window) return std::unexpected(window.error());
        model.scenarioText.set(std::string(hotk::kScenarios[1].name) + " 189年");
        model.rulerText.set(std::string("刘备（可扮演）"));
        return model.routes.push<TitleScreen>(std::ref(model));
    }
    BackAction on_back() {
        if (!model.routes.can_pop()) return BackAction::close;
        (void)model.routes.pop();
        return BackAction::stay;
    }
};

PXA_APPLICATION(HotkApp)
