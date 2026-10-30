// SPDX-License-Identifier: CC0-1.0
#include "../common/pocket-play/runtime.hpp"
#include "../common/pocket-studio/studio.hpp"
#include "model.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace {
using namespace pocket;
calc::Model model;
enum Page {
  Calculate,
  Tools,
  History,
  Convert,
  Practical,
  Help,
  ClearHistory
} page = Calculate;
bool scientific = false, more = false, nav = false;
bool replace_amount = true, replace_first = true, replace_second = true;
int focus = 104, pressed = -1, history_page = 0, active = 0, help_page = 0;
char amount[32] = "1", first[32] = "100", second[32] = "10", toast[80]{};
uint32_t toast_ms = 0;
calc::Error last_error = calc::Error::None;
constexpr uint16_t BG0 = rgb(10, 22, 31), CARD = rgb(19, 40, 51),
                   EDGE = rgb(43, 64, 75), DIGIT = rgb(28, 49, 61),
                   INK0 = rgb(232, 240, 236), DIM = rgb(140, 167, 174),
                   MINT = rgb(122, 230, 190), GOLD = rgb(239, 193, 119),
                   RED = rgb(244, 158, 148);
struct Button {
  int id, x, y, w, h;
  char text[100];
  uint16_t fill;
  int scale;
};
Button buttons[40];
int count = 0;
const char *basic_labels[] = {"AC", "DEL", "%", "÷", "7", "8", "9",
                              "×",  "4",   "5", "6", "−", "1", "2",
                              "3",  "+",   "±", "0", ".", "="};
const char *basic_tokens[] = {"",  "",  "%", "/", "7", "8", "9", "*", "4", "5",
                              "6", "-", "1", "2", "3", "+", "",  "0", ".", ""};
const char *sci_labels[] = {"sin", "cos", "tan", "DEG", "asin", "acos", "atan",
                            "sqrt", "ln",  "log", "exp", "x^y",  "(",    ")",
                            "pi",   "e",   "Ans", "x^2", "More", "123"};
const char *extra_labels[] = {
    "MS",    "MR",  "M+", "MC", "abs", "n!", "1/x", "%",   "floor", "ceil",
    "round", "x^y", "(",  ")",  "pi",  "e",  "Ans", "DEL", "Back",  "123"};
bool portrait() { return HEIGHT >= WIDTH; }
void log_focus() {
  char b[100];
  std::snprintf(b, sizeof(b), "Calc: focus=%d page=%u sci=%u", focus,
                unsigned(page), scientific ? 1u : 0u);
  log(b);
}
void say(const char *en, const char *zh) {
  std::snprintf(toast, sizeof(toast), "%s", tr(en, zh));
  toast_ms = 2200;
  redraw();
}
void go(Page p, int f) {
  page = p;
  focus = f;
  pressed = -1;
  log_focus();
  redraw();
}
void add(int id, int x, int y, int w, int h, const char *s,
         uint16_t fill = DIGIT, int scale = 1) {
  if (count >= 40)
    return;
  auto &b = buttons[count++];
  b = {id, x, y, w, h, {}, fill, scale};
  std::snprintf(b.text, sizeof(b.text), "%s", s);
}
void pad(int x, int y, int w, int h, bool aux = false) {
  int rows = aux ? 4 : 5;
  for (int i = 0; i < rows * 4; ++i) {
    int col = i % 4, row = i / 4;
    int left = x + col * w / 4, top = y + row * h / rows,
        bw = (x + (col + 1) * w / 4) - left - 5,
        bh = (y + (row + 1) * h / rows) - top - 5;
    if (aux) {
      const char *labels[] = {"7", "8", "9", "DEL", "4", "5", "6",   "AC",
                              "1", "2", "3", "±",   "0", ".", "A/B", "Back"};
      add(700 + i, left, top, bw, bh,
          i == 14   ? (page == Convert ? "↔" : "A/B")
          : i == 15 ? tr("Back", "返回")
                    : labels[i],
          i == 15 ? rgb(30, 91, 77) : DIGIT, i % 4 != 3 && i != 14 ? 2 : 1);
    } else if (scientific) {
      const char *s = more ? extra_labels[i] : sci_labels[i];
      if (i == 3 && !more)
        s = model.degrees ? "DEG" : "RAD";
      if (i == 18)
        s = more ? tr("Prev", "上页") : tr("More", "更多");
      add(200 + i, left, top, bw, bh, s, i == 19 ? rgb(30, 91, 77) : DIGIT, 1);
    } else
      add(100 + i, left, top, bw, bh, basic_labels[i],
          i == 19 ? MINT
                  : (i == 0 || i == 1 ? rgb(76, 53, 48)
                                      : (i % 4 == 3 ? rgb(36, 76, 78) : DIGIT)),
          i >= 4 && i % 4 != 3 ? 2 : (i == 19 ? 2 : 1));
  }
}
void build() {
  count = 0;
  if (page != Calculate)
    add(9, WIDTH - 100, 3, 44, 20, tr("Back", "返回"), CARD);
  if (page == Calculate) {
    int kw = portrait() ? WIDTH - 19 : std::max(164, WIDTH * 54 / 100),
        leftw = portrait() ? WIDTH - 24 : WIDTH - kw - 31;
    int ty = portrait() ? 30 : 143, tw = (leftw - 8) / 3;
    add(1, 12, ty, tw, 21, scientific ? "123" : tr("Fn", "科学"), CARD);
    add(2, 16 + tw, ty, tw, 21, tr(portrait() ? "History" : "Hist", "历史"),
        CARD);
    add(3, 20 + 2 * tw, ty, tw, 21, tr("Tools", "工具"), CARD);
    if (portrait())
      pad(12, 133, WIDTH - 19, HEIGHT - 146);
    else
      pad(WIDTH - kw - 2, 32, kw, HEIGHT - 45);
  } else if (page == Tools) {
    const char *items[] = {tr("Unit converter", "单位换算"),
                           tr("Everyday calculations", "日常计算"),
                           tr("How to use", "使用说明"),
                           tr("Back to calculator", "返回计算器")};
    for (int i = 0; i < 4; ++i)
      add(30 + i, 18, 43 + i * (HEIGHT - 68) / 4, WIDTH - 36,
          (HEIGHT - 68) / 4 - 7, items[i], CARD);
  } else if (page == History) {
    int rows = std::min(4, (HEIGHT - 85) / 41), offset = history_page * rows;
    for (int i = 0; i < rows && offset + i < int(model.count); ++i) {
      char v[32], b[100];
      calc::format(v, sizeof(v), model.history[offset + i].value);
      const auto &entry = model.history[offset + i];
      bool trig = std::strstr(entry.expression, "sin") ||
                  std::strstr(entry.expression, "cos") ||
                  std::strstr(entry.expression, "tan");
      int length = trig ? 4 : 8;
      std::snprintf(
          b, sizeof(b), "%s%.*s%s = %s",
          trig ? (entry.degrees ? "DEG " : "RAD ") : "", length,
          entry.expression,
          std::strlen(entry.expression) > unsigned(length) ? "..." : "", v);
      add(500 + offset + i, 14, 39 + i * (HEIGHT - 82) / rows, WIDTH - 28,
          (HEIGHT - 82) / rows - 6, b, CARD);
    }
    int w = (WIDTH - 40) / 3;
    add(520, 12, HEIGHT - 34, w, 23, tr("Prev", "上页"));
    add(521, 20 + w, HEIGHT - 34, w, 23, tr("Next", "下页"));
    add(522, 28 + 2 * w, HEIGHT - 34, w, 23, tr("Clear", "清除"));
  } else if (page == ClearHistory) {
    add(550, 24, HEIGHT / 2, (WIDTH - 60) / 2, 28, tr("Keep", "保留"));
    add(551, 36 + (WIDTH - 60) / 2, HEIGHT / 2, (WIDTH - 60) / 2, 28,
        tr("Clear", "清除"), rgb(87, 53, 48));
  } else if (page == Help) {
    add(552, 16, HEIGHT - 35, WIDTH - 32, 24,
        help_page ? tr("First page", "第一页") : tr("Next page", "下一页"));
  } else {
    bool port = portrait();
    int pane = port ? WIDTH - 24 : std::max(114, WIDTH * 43 / 100),
        px = port ? 12 : pane + 18, pw = port ? WIDTH - 19 : WIDTH - px - 7,
        py = port ? 145 : 33, ph = HEIGHT - py - 13;
    if (page == Convert) {
      const auto &q = calc::quantity(model.category);
      char b[60];
      std::snprintf(b, sizeof(b), "%s · %s", tr("Type", "类别"),
                    english() ? q.en : q.zh);
      add(301, 12, 31, pane, 21, b, CARD);
      std::snprintf(b, sizeof(b), "%s → %s", q.units[model.from].name,
                    q.units[model.to].name);
      add(302, 12, 57, (pane - 6) / 2, 21, q.units[model.from].name, CARD);
      add(303, 18 + (pane - 6) / 2, 57, (pane - 6) / 2, 21,
          q.units[model.to].name, CARD);
    } else {
      const char *m[] = {tr("Percent of a value", "求百分比"),
                         tr("Discount price", "折后价格"),
                         tr("Percent change", "变化幅度"),
                         tr("Split equally", "平均分摊")};
      add(401, 12, 31, pane, 21, m[model.practical_mode], CARD);
      char a[44], b[44];
      int digits = std::max(3, ((pane - 6) / 2 - 8) / 6 - 2);
      auto field = [digits](char *out, size_t n, char name, const char *value) {
        size_t len = std::strlen(value);
        if (len > unsigned(digits))
          std::snprintf(out, n, "%c:...%s", name,
                        value + len - unsigned(std::max(1, digits - 3)));
        else
          std::snprintf(out, n, "%c:%s", name, value);
      };
      field(a, sizeof(a), 'A', first);
      field(b, sizeof(b), 'B', second);
      add(402, 12, 57, (pane - 6) / 2, 21, a,
          active == 0 ? rgb(36, 76, 78) : CARD);
      add(403, 18 + (pane - 6) / 2, 57, (pane - 6) / 2, 21, b,
          active == 1 ? rgb(36, 76, 78) : CARD);
    }
    pad(px, py, pw, ph, true);
  }
  add(6, WIDTH - 48, 3, 24, 18, "", CARD);
  bool found = false;
  for (int i = 0; i < count; ++i)
    found |= buttons[i].id == focus;
  if (!found) {
    focus = buttons[0].id;
    log_focus();
  }
}
int hit(int x, int y) {
  build();
  for (int i = 0; i < count; ++i) {
    auto &b = buttons[i];
    if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h)
      return b.id;
  }
  return -1;
}
void paint_buttons() {
  for (int i = 0; i < count; ++i) {
    auto &b = buttons[i];
    bool selected = nav && focus == b.id;
    if (b.id == 6) {
      if (selected)
        outline(b.x - 2, b.y - 2, b.w + 4, b.h + 4, GOLD);
      continue;
    }
    if (selected)
      studio::round(b.x - 2, b.y - 2, b.w + 4, b.h + 4, 7, GOLD);
    uint16_t fill = pressed == b.id ? studio::mix(b.fill, 0, 40) : b.fill;
    studio::card(b.x, b.y, b.w, b.h, fill, EDGE, 5);
    int scale = b.scale;
    while (scale > 1 && text_width(b.text, scale) > b.w - 8)
      --scale;
    studio::label(b.x + 4, b.y + (b.h - 10 * scale) / 2, b.w - 8, b.text,
                  b.id == 119 ? BG0 : INK0, scale);
  }
}
void tail(char *out, size_t n, const char *in, int width) {
  const char *p = in;
  while (*p && text_width(p) > width - 12)
    ++p;
  std::snprintf(out, n, "%s%s", p == in ? "" : "...", p);
  while (text_width(out) > width && *p) {
    ++p;
    std::snprintf(out, n, "...%s", p);
  }
}
void header(const char *s) {
  studio::gradient(0, 0, WIDTH, HEIGHT, BG0, rgb(12, 31, 40));
  studio::label(12, 5, page == Calculate ? WIDTH - 140 : WIDTH - 126, s, INK0,
                2, 0);
  line(12, 27, WIDTH - 12, 27, EDGE);
}
int display_number(char *out, size_t n, double value, int width, int scale) {
  calc::format(out, n, value);
  while (scale > 1 && text_width(out, scale) > width)
    --scale;
  for (int digits = 11; digits >= 3 && text_width(out, scale) > width; --digits)
    std::snprintf(out, n, "%.*g", digits, value == 0 ? 0 : value);
  return scale;
}
void readout() {
  int kw = std::max(164, WIDTH * 54 / 100),
      w = portrait() ? WIDTH - 24 : WIDTH - kw - 31, y = portrait() ? 59 : 45,
      h = portrait() ? 64 : 84;
  studio::card(12, y, w, h, CARD, EDGE, 8);
  char line[110];
  tail(line, sizeof(line),
       *model.input
           ? (model.completed && *model.previous ? model.previous : model.input)
           : tr("Enter a calculation", "输入算式"),
       w - 16);
  studio::label(20, y + 9, w - 16, line, DIM, 1, 2);
  auto r = model.current();
  char value[40];
  if (last_error != calc::Error::None)
    std::snprintf(value, sizeof(value), "%s",
                  calc::error_text(last_error, english()));
  else if (!*model.input)
    std::snprintf(value, sizeof(value), "0");
  else if (r.ok())
    calc::format(value, sizeof(value), r.value);
  else
    std::snprintf(value, sizeof(value), "...");
  int scale = last_error == calc::Error::None ? 3 : 1;
  if (r.ok() && last_error == calc::Error::None)
    scale = display_number(value, sizeof(value), r.value, w - 18, scale);
  else
    while (scale > 1 && text_width(value, scale) > w - 18)
      --scale;
  studio::label(20, y + 31, w - 16, value,
                last_error == calc::Error::None ? MINT : RED, scale, 2);
  char status[40];
  std::snprintf(status, sizeof(status), "%s%s", model.degrees ? "DEG" : "RAD",
                model.has_memory ? " · M" : "");
  studio::label(WIDTH - 126, 9, 68, status, DIM);
  if (!portrait()) {
    char ans[40], v[28];
    calc::format(v, sizeof(v), model.answer);
    std::snprintf(ans, sizeof(ans), "Ans %.20s", v);
    char visible[40];
    tail(visible, sizeof(visible), ans, w);
    studio::label(12, 181, w, visible, DIM);
  }
}
calc::Result number(const char *s) {
  return *s ? calc::evaluate(s) : calc::Result{};
}
void tools_display() {
  const bool port = portrait();
  int pane = port ? WIDTH - 24 : std::max(114, WIDTH * 43 / 100);
  calc::Result r;
  char v[40], expression[70];
  if (page == Convert) {
    auto n = number(amount);
    r = n.ok() ? calc::convert(n.value, model.category, model.from, model.to)
               : n;
    std::snprintf(expression, sizeof(expression), "%.22s %s → %s", amount,
                  calc::quantity(model.category).units[model.from].name,
                  calc::quantity(model.category).units[model.to].name);
  } else {
    auto a = number(first), b = number(second);
    r = !a.ok()   ? a
        : !b.ok() ? b
                  : calc::practical(a.value, b.value, model.practical_mode);
    const char *f[] = {"A × B%", "A × B / 10", "(B − A) / |A| × 100%", "A / B"};
    std::snprintf(expression, sizeof(expression), "%s",
                  f[model.practical_mode]);
  }
  studio::card(12, 85, pane, port ? 51 : 82, CARD, EDGE, 7);
  if (r.ok()) {
    calc::format(v, sizeof(v), r.value);
    if (page == Practical && model.practical_mode == 2)
      std::strcat(v, "%");
  } else
    std::snprintf(v, sizeof(v), "%s", calc::error_text(r.error, english()));
  int scale = r.ok() ? 2 : 1;
  if (r.ok()) {
    scale = display_number(v, sizeof(v), r.value, pane - 22, scale);
    if (page == Practical && model.practical_mode == 2)
      std::strcat(v, "%");
  }
  studio::label(20, 91, pane - 16, v, r.ok() ? MINT : RED, scale, 2);
  char visible[70];
  tail(visible, sizeof(visible), expression, pane - 12);
  studio::label(18, port ? 119 : 139, pane - 12, visible, DIM, 1, 2);
  if (page == Practical && !port) {
    const char *h[] = {tr("A value · B percent", "A 数值 · B 百分数"),
                       tr("B: 0–10 discount", "B 折数：0至10"),
                       tr("A before · B after", "A 原值 · B 现值"),
                       tr("B positive integer", "B 为正整数")};
    studio::label(12, 183, pane, h[model.practical_mode], DIM);
  }
}
void wrap(int &y, const char *s) {
  while (*s) {
    char b[120];
    unsigned n = 0, lastspace = 0;
    while (s[n]) {
      unsigned c = (unsigned char)s[n], z = c < 128   ? 1
                                            : c < 224 ? 2
                                            : c < 240 ? 3
                                                      : 4;
      if (n + z >= sizeof(b))
        break;
      std::memcpy(b + n, s + n, z);
      b[n + z] = 0;
      if (text_width(b) > WIDTH - 36)
        break;
      if (s[n] == ' ')
        lastspace = n;
      n += z;
    }
    if (!n)
      break;
    if (s[n] && lastspace)
      n = lastspace;
    std::memcpy(b, s, n);
    b[n] = 0;
    studio::label(18, y, WIDTH - 36, b, DIM, 1, 0);
    y += 13;
    s += n;
    while (*s == ' ')
      ++s;
  }
}
void draw_help() {
  const char *lines[] = {
      tr("Use Fn for scientific keys; 123 returns to digits.",
         "科学键可分页，123返回数字键盘。"),
      tr("DEG uses degrees; RAD uses radians.", "DEG为角度，RAD为弧度。"),
      tr("Power precedes unary minus: -2^2 = -4.", "先算乘方：-2^2 = -4。"),
      tr("% means divide by 100: 200+10% = 200.1.",
         "%表示除以100：200+10%=200.1。"),
      tr("Missing closing brackets are added on equals.",
         "按等号时自动补齐末尾右括号。"),
      tr("History recalls results. MS saves. MR recalls. M+ adds. MC clears.",
         "历史取回结果；MS存入，MR读取，M+累加，MC清除。"),
      tr("Discount B is 0–10. Split B is a positive integer.",
         "折扣B填0至10折，分摊B填正整数。"),
      tr("Up/Down select; confirm uses the focused key.",
         "上/下选择焦点，确认执行；无须组合键。")};
  int y = 41;
  for (int i = help_page * 4; i < help_page * 4 + 4; ++i) {
    wrap(y, lines[i]);
    y += 5;
  }
}
void evaluate() { // Close only missing right parentheses; never conceal an
                  // operator error.
  int balance = 0;
  bool bad = false;
  for (const char *p = model.input; *p; ++p) {
    if (*p == '(')
      ++balance;
    if (*p == ')' && --balance < 0) {
      bad = true;
      break;
    }
  }
  if (!bad)
    while (balance > 0 && std::strlen(model.input) < calc::EXPR) {
      model.insert(")");
      --balance;
    }
  auto r = model.calculate();
  last_error = r.error;
  if (r.ok()) {
    save();
    log("Calc: evaluated");
  } else
    log("Calc: calculation error");
}
void aux(int i) {
  char *s = page == Convert ? amount : (active == 0 ? first : second);
  bool &replace = page == Convert
                      ? replace_amount
                      : (active == 0 ? replace_first : replace_second);
  if (replace && i != 3 && i != 7 && i != 14 && i != 15) {
    s[0] = 0;
    replace = false;
  }
  if (i == 3 || i == 7)
    replace = false;
  size_t n = std::strlen(s);
  const char *digits[] = {"7", "8", "9", "", "4", "5", "6", "",
                          "1", "2", "3", "", "0", ".", "",  ""};
  if (i == 3) {
    if (n)
      s[n - 1] = 0;
  } else if (i == 7)
    s[0] = 0;
  else if (i == 11) {
    if (*s == '-')
      std::memmove(s, s + 1, n);
    else if (n + 1 < 32) {
      std::memmove(s + 1, s, n + 1);
      s[0] = '-';
    }
  } else if (i == 14) {
    if (page == Convert)
      std::swap(model.from, model.to);
    else {
      active ^= 1;
      if (active == 0)
        replace_first = true;
      else
        replace_second = true;
    }
  } else if (i == 15) {
    go(Tools, 30);
    return;
  } else if (n + 1 < 32) {
    if (i == 13 && std::strchr(s, '.'))
      return;
    std::strcat(s, digits[i]);
  }
  save();
}
void action(int id) {
  last_error = calc::Error::None;
  if (id == 6) {
    open_menu();
    return;
  }
  if (id == 9) {
    go(Calculate, 104);
    return;
  }
  if (id == 1) {
    scientific = !scientific;
    focus = scientific ? 200 : 104;
  } else if (id == 2) {
    history_page = 0;
    go(History, model.count ? 500 : 9);
  } else if (id == 3)
    go(Tools, 30);
  else if (id >= 100 && id < 120) {
    int i = id - 100;
    if (i == 0)
      model.clear();
    else if (i == 1)
      model.erase();
    else if (i == 16) {
      if (!model.negate())
        say("Expression is full", "算式已满");
    } else if (i == 19)
      evaluate();
    else if (!model.insert(basic_tokens[i]))
      say("Expression is full", "算式已满");
    save();
  } else if (id >= 200 && id < 220) {
    int i = id - 200;
    const char *tokens[] = {"sin(",  "cos(",  "tan(",  "",    "asin(",
                            "acos(", "atan(", "sqrt(", "ln(", "log(",
                            "exp(",  "^",     "(",     ")",   "(pi)",
                            "(e)",   "(Ans)", "^2",    "",    ""};
    if (i == 19) {
      scientific = false;
      focus = 104;
    } else if (i == 18) {
      more = !more;
      focus = 200;
    } else if (!more && i == 3) {
      model.degrees = !model.degrees;
      if (model.completed)
        model.previous[0] = 0;
    } else if (more && i < 4) {
      bool ok = true;
      if (i == 0)
        ok = model.memory_store();
      if (i == 1)
        ok = model.insert("(Mem)");
      if (i == 2)
        ok = model.memory_add();
      if (i == 3)
        model.memory_clear();
      say(ok ? "Memory updated" : "Enter a valid value",
          ok ? "记忆值已更新" : "请先输入有效数值");
    } else if (more && i < 12) {
      const char *t[] = {"abs(",   "!",     "^(-1)",  "%",
                         "floor(", "ceil(", "round(", "^"};
      if (!model.insert(t[i - 4]))
        say("Expression is full", "算式已满");
    } else if (more && i == 17)
      model.erase();
    else if (!model.insert(tokens[i]))
      say("Expression is full", "算式已满");
    save();
  } else if (id == 30)
    go(Convert, 301);
  else if (id == 31)
    go(Practical, 401);
  else if (id == 32) {
    help_page = 0;
    go(Help, 552);
  } else if (id == 33)
    go(Calculate, 104);
  else if (id == 301) {
    model.category = (model.category + 1) % calc::QUANTITIES;
    model.from = 0;
    model.to = 1;
    save();
  } else if (id == 302) {
    model.from = (model.from + 1) % calc::quantity(model.category).count;
    save();
  } else if (id == 303) {
    model.to = (model.to + 1) % calc::quantity(model.category).count;
    save();
  } else if (id == 401) {
    model.practical_mode = (model.practical_mode + 1) % 4;
    save();
  } else if (id == 402) {
    active = 0;
    replace_first = true;
  } else if (id == 403) {
    active = 1;
    replace_second = true;
  } else if (id >= 700 && id < 716)
    aux(id - 700);
  else if (id >= 500 && id < 508) {
    model.recall(unsigned(id - 500));
    save();
    go(Calculate, 104);
  } else if (id == 520 || id == 521) {
    int rows = std::min(4, (HEIGHT - 85) / 41),
        pages = std::max(1, (int(model.count) + rows - 1) / rows);
    history_page = (history_page + pages + (id == 520 ? -1 : 1)) % pages;
  } else if (id == 522)
    go(ClearHistory, 550);
  else if (id == 550)
    go(History, model.count ? 500 : 9);
  else if (id == 551) {
    for (auto &e : model.history)
      e = {};
    model.count = 0;
    history_page = 0;
    save();
    go(History, 9);
  } else if (id == 552)
    help_page ^= 1;
  build();
  log_focus();
  redraw();
}
} // namespace
namespace game {
const char *title() { return pocket::tr("Calculator", "计算器"); }
const char *app_id() { return "pxa-calculator"; }
void init() {
  model = {};
  replace_amount = replace_first = replace_second = true;
  std::strcpy(amount, "1");
  std::strcpy(first, "100");
  std::strcpy(second, "10");
  active = 0;
  page = Calculate;
  scientific = more = false;
  focus = 104;
  pressed = -1;
  nav = false;
  toast_ms = 0;
  last_error = calc::Error::None;
  build();
  log_focus();
}
void reset() {
  model.clear();
  last_error = calc::Error::None;
  scientific = false;
  go(Calculate, 104);
  save();
}
bool navigation_visible() { return nav; }
void set_navigation(bool visible) {
  if (nav != visible) {
    nav = visible;
    pocket::redraw();
  }
}
void key(pocket::Key k) {
  build();
  set_navigation(true);
  if (k == Key::A) {
    action(focus);
    return;
  }
  if (k == Key::UP || k == Key::DOWN || k == Key::LEFT || k == Key::RIGHT) {
    int at = 0;
    for (int i = 0; i < count; ++i)
      if (buttons[i].id == focus)
        at = i;
    focus = buttons[(at + count + (k == Key::UP || k == Key::LEFT ? -1 : 1)) %
                    count]
                .id;
    log_focus();
    redraw();
  } else if (k == Key::B) {
    if (page == Calculate) {
      model.erase();
      last_error = calc::Error::None;
      save();
      redraw();
    } else
      go(Calculate, 104);
  } else if (k == Key::X)
    reset();
  else if (k == Key::Y)
    go(Tools, 30);
  else if (k == Key::L)
    action(1);
  else if (k == Key::R)
    action(2);
}
void pointer(pocket::Phase phase, int x, int y) {
  if (phase == Phase::DOWN) {
    set_navigation(false);
    pressed = hit(x, y);
  } else if (phase == Phase::CANCEL)
    pressed = -1;
  else if (phase == Phase::UP) {
    int p = pressed;
    pressed = -1;
    if (p >= 0 && p == hit(x, y)) {
      focus = p;
      action(p);
    }
  }
  redraw();
}
void tick(uint32_t ms) {
  if (toast_ms) {
    if (ms >= toast_ms) {
      toast_ms = 0;
      redraw();
    } else
      toast_ms -= ms;
  }
}
void draw() {
  build();
  const char *names[] = {
      pocket::tr("Calculator", "计算器"),     pocket::tr("Tools", "实用工具"),
      pocket::tr("History", "计算历史"),      pocket::tr("Convert", "单位换算"),
      pocket::tr("Everyday", "日常计算"),     pocket::tr("Guide", "使用说明"),
      pocket::tr("Clear history", "清空历史")};
  header(names[page]);
  if (page == Calculate)
    readout();
  else if (page == Convert || page == Practical)
    tools_display();
  else if (page == History && !model.count)
    studio::label(
        24, HEIGHT / 2 - 16, WIDTH - 48,
        tr("Your calculations will appear here", "完成的计算会保存在这里"),
        DIM);
  else if (page == ClearHistory)
    studio::label(18, HEIGHT / 2 - 36, WIDTH - 36,
                  tr("Clear all saved history?", "清空全部计算历史？"), INK0);
  else if (page == Help)
    draw_help();
  paint_buttons();
  if (toast_ms)
    studio::label(8, HEIGHT - 11, WIDTH - 16, toast, GOLD);
}
size_t serialize(uint8_t *p, size_t n) { return model.encode(p, n); }
void load(const uint8_t *p, size_t n) {
  if (model.decode(p, n)) {
    last_error = calc::Error::None;
    build();
    log("Calc: restored");
    redraw();
  } else
    log("Calc: rejected save");
}
} // namespace game
