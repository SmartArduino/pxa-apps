void ReaderApp::render() {
  using reader::Rect;
  auto l = layout();
  draw.reset();
  hit_count = 0;
  std::size_t rendered_rows = 0;
  const std::array<std::uint32_t, 3> backgrounds{0xf5f0e4ff, 0xe5eedfff,
                                                 0x171b20ff};
  auto bg = backgrounds[state.settings.theme],
       ink = state.settings.theme == 2 ? 0xdbe1e5ff : 0x253430ff;
  auto muted = state.settings.theme == 2 ? 0x929ba3ff : 0x60736aff;
  std::uint32_t surface = state.settings.theme == 2 ? 0x252d34ff : 0xe6e9ddff,
                accent = 0x327a62ff;
  auto dp = [&](int px) {
    return std::max(0, int(std::lround(px / native_density())));
  };
  auto region = [&](Rect r) {
    return CanvasRegion{dp(r.x), dp(r.y), std::max(1, dp(r.w)),
                        std::max(1, dp(r.h))};
  };
  auto rectangle = [&](Rect r, std::uint32_t color) {
    if (r.w > 0 && r.h > 0)
      draw.rect(dp(r.x), dp(r.y), std::max(1, dp(r.w)), std::max(1, dp(r.h)),
                color, 3);
  };
  auto text = [&](Rect r, std::string_view value, int pixels,
                  std::uint32_t color, std::uint8_t align = 0) {
    if (r.w <= 0 || r.h <= 0)
      return;
    std::size_t chars = std::max(1, r.w / std::max(1, pixels)), at = 0,
                count = 0;
    while (at < value.size() && count < chars) {
      auto rune = reader::rune(value.substr(at));
      at += rune.bytes;
      ++count;
    }
    auto clipped = value.substr(0, at);
    if (ui_features & (1ull << 11))
      draw.text_sized(region(r), color, clipped,
                      std::uint16_t(std::clamp(pixels, 8, 128)), align);
    else
      draw.text(dp(r.x), dp(r.y), std::max(1, dp(r.w)), color, clipped,
                pixels >= 18 ? 2 : 1, align);
  };
  auto button = [&](Rect r, std::string_view label, Action a, int arg = 0,
                    bool selected = false) {
    rectangle(r, selected ? accent : surface);
    Rect inside{r.x + 3, r.y + 1, r.w - 6, r.h - 2};
    text(inside, label, l.ui_font, selected ? 0xffffffff : ink, 1);
    if (hit_count < hits.size())
      hits[hit_count++] = {r, a, arg};
  };
  auto row = [&](int y, std::string_view label, Action a, int arg = 0) {
    button(l.box(y, l.row - 3), label, a, arg);
  };
  auto pair = [&](int y, std::string_view left, Action a,
                  std::string_view right, Action b) {
    auto r = l.box(y, l.row - 3);
    int w = (r.w - 4) / 2;
    button({r.x, y, w, r.h}, left, a);
    button({r.x + w + 4, y, r.w - w - 4, r.h}, right, b);
  };
  rectangle({0, 0, l.width, l.height}, bg);
  const char *title = screen == shelf        ? "书页 · 书架"
                      : screen == search     ? "发现好书"
                      : screen == sources    ? "网络书源"
                      : screen == source_add ? "添加书源"
                      : screen == settings   ? "阅读设置"
                      : screen == toc        ? "目录"
                      : screen == read_menu  ? "阅读菜单"
                                             : "";
  if (screen == reading && !state.shelf.empty())
    title = (busy || notice_deadline) ? status.c_str()
                                      : state.shelf[active].title.c_str();
  auto header = l.box(l.top, l.row);
  text(header, title, std::min(int(18 * l.density), int(l.row / 1.6f)), ink);
  int y = l.top + l.row + 3;
  if (screen != reading) {
    text(l.box(y, l.caption), status, l.ui_font, muted);
    y += l.caption + 2;
  }

  InputBox native_input{};
  if (screen == reading) {
    int size = std::max(10, int(state.settings.font * l.density));
    for (std::size_t i = 0; i < page.count; ++i) {
      auto line = page.lines[i];
      auto value =
          std::string_view(page_text).substr(line.begin, line.end - line.begin);
      text(line.box, value, size, ink);
    }
    auto r = l.box(l.bottom - l.row, l.row - 2);
    int w = r.w / 3;
    button({r.x, r.y, w, r.h}, "上一页", previous_page);
    button({r.x + w, r.y, w, r.h}, "菜单", menu);
    button({r.x + 2 * w, r.y, r.w - 2 * w, r.h}, "下一页", next_page);
    if (page.count == 0)
      text(l.box(y, l.row * 2), busy ? "正在缓存正文…" : status, l.ui_font,
           muted);
  } else if (screen == read_menu) {
    pair(y, "继续阅读", back, "目录", contents);
    y += l.row;
    pair(y, busy ? "停止下载" : "下载全文", busy ? cancel_job : download,
         "保存进度", bookmark);
    y += l.row;
    pair(y, "字号与设置", options, "返回书架", home);
    y += l.row;
    row(y, "移除本书", remove_book);
  } else if (screen == settings) {
    pair(y, "字号 − " + std::to_string(state.settings.font), font_less,
         "字号 +", font_more);
    y += l.row;
    pair(y, "行距 " + std::to_string(state.settings.line_gap), gap_change,
         "主题 " + std::to_string(state.settings.theme + 1), theme_change);
    y += l.row;
    pair(y, state.settings.automatic ? "自动翻页 开" : "自动翻页 关",
         auto_toggle,
         "间隔 " + std::to_string(state.settings.auto_seconds) + " 秒",
         auto_speed);
    y += l.row;
    row(y, "完成", back);
  } else if (screen == search || screen == source_add) {
    auto r = l.box(y, l.row);
    rectangle(r, surface);
    int reserved = std::max(l.row, int(l.row * 1.5));
    auto current = input.get();
    Rect edit{r.x, y, r.w - reserved - 3, r.h - 3};
    if (current.empty())
      text(edit, "点此输入", l.ui_font, muted);
    // Reuse the Host's existing font roles; no additional input font/cache.
    int input_pixels = std::min(int(16 * l.density), l.row - 8);
    Font input_font = input_pixels >= 26 ? Font::display
                      : input_pixels >= 22 ? Font::headline
                      : input_pixels >= 18 ? Font::title
                                           : Font::body;
    native_input = {region(edit), true, input_font, ink};
    if (hit_count < hits.size())
      hits[hit_count++] = {edit, text_edit};
    button({r.x + r.w - reserved, y, reserved, r.h - 3},
           screen == search ? "搜索" : "添加",
           screen == search ? search_go : import_source);
    y += l.row + 3;
    if (screen == search) {
      row(y, state.sources[source_index].name, source_select);
      y += l.row;
      const int stop = l.bottom - l.row * 2;
      for (std::size_t i = list_start; i < results.size() && y + l.row <= stop;
           ++i) {
        ++rendered_rows;
        row(y, results[i].title, open_book, int(i));
        y += l.row;
      }
      if (results.empty() && !busy) {
        pair(y, "西游记", search_classic, "红楼梦", search_classic);
        if (hit_count >= 2)
          hits[hit_count - 1].arg = 1;
      }
      if (!results.empty()) {
        auto controls = l.box(l.bottom - l.row * 2, l.row - 3);
        int w = controls.w / 2;
        button({controls.x, controls.y, w - 2, controls.h}, "上一组",
               list_previous);
        button({controls.x + w + 2, controls.y, controls.w - w - 2, controls.h},
               "下一组", list_next);
      }
    } else {
      text(l.box(y, l.row * 2), "输入目录 JSON 或书源配置链接", l.ui_font,
           muted);
      y += l.row * 2;
      row(y, "返回", source_list);
    }
  } else if (screen == sources) {
    auto stop = l.bottom - l.row * 2;
    for (std::size_t i = list_start;
         i < state.sources.size() && y + l.row <= stop; ++i) {
      ++rendered_rows;
      auto r = l.box(y, l.row - 3);
      int w = r.w - l.row;
      button({r.x, y, w, r.h},
             state.sources[i].name + (state.sources[i].enabled ? " ✓" : " ×"),
             source_toggle, int(i));
      button({r.x + w + 2, y, r.w - w - 2, r.h}, "−", remove_source, int(i));
      y += l.row;
    }
    auto header_plus = l.box(l.top, l.row - 3);
    button({header_plus.x + header_plus.w - l.row, header_plus.y, l.row,
            header_plus.h},
           "+", add_source);
    pair(l.bottom - l.row * 2, "上一组", list_previous, "下一组", list_next);
  } else if (screen == toc) {
    auto stop = l.bottom - l.row * 2;
    if (chapters.empty())
      text(l.box(y, l.row * 2),
           busy ? "目录将在下载完成后建立" : "未识别章节，可正常按页阅读",
           l.ui_font, muted);
    for (std::size_t i = list_start; i < chapters.size() && y + l.row <= stop;
         ++i) {
      ++rendered_rows;
      row(y, chapters[i].title, open_chapter, int(i));
      y += l.row;
    }
    pair(l.bottom - l.row * 2, "上一组", list_previous, "下一组", list_next);
  } else {
    int stop = l.bottom - l.row * 2;
    for (std::size_t i = list_start;
         i < state.shelf.size() && y + l.row <= stop; ++i) {
      ++rendered_rows;
      row(y, state.shelf[i].title + (state.shelf[i].complete ? " · 离线" : ""),
          open_book, int(i));
      y += l.row;
    }
    if (state.shelf.size() > rendered_rows)
      pair(l.bottom - l.row * 2, "上一组", list_previous, "下一组", list_next);
    if (state.shelf.empty())
      text(l.box(y, l.row * 2), "去发现页找一本好书吧", l.ui_font, muted);
  }
  if (screen != reading) {
    if (screen == toc || screen == settings || screen == read_menu) {
      row(l.bottom - l.row, "返回", back);
    } else {
      auto r = l.box(l.bottom - l.row, l.row - 2);
      int w = r.w / 3;
      button({r.x, r.y, w - 2, r.h}, "书架", home, 0, screen == shelf);
      button({r.x + w, r.y, w - 2, r.h}, "发现", discover, 0, screen == search);
      button({r.x + 2 * w, r.y, r.w - 2 * w, r.h}, "书源", source_list, 0,
             screen == sources);
    }
  }
  visible_rows = std::max<std::size_t>(1, rendered_rows);
  input_box.set(native_input);
  auto submitted = canvas.present(draw);
  if (!submitted)
    error("绘制", submitted.error());
#ifdef READER_VALIDATE
  auto pool = pxa::task_pool_stats();
  log("READER-POOL bytes=" + std::to_string(pool.reserved_bytes) +
      " peak=" + std::to_string(pool.peak_slots) +
      " failures=" + std::to_string(pool.allocation_failures));
  log("READER-UI screen=" + std::to_string(screen) + " busy=" +
      std::to_string(busy) + " density=" + std::to_string(metrics.density_q16) +
      " auto=" + std::to_string(state.settings.automatic) +
      " seconds=" + std::to_string(state.settings.auto_seconds) +
      " hits=" + std::to_string(hit_count));
  for (std::size_t i = 0; i < hit_count; ++i) {
    auto &h = hits[i];
    log("READER-HIT action=" + std::to_string(h.action) + " arg=" +
        std::to_string(h.arg) + " x=" + std::to_string(h.box.x + h.box.w / 2) +
        " y=" + std::to_string(h.box.y + h.box.h / 2));
  }
#endif
}
void ReaderApp::pointer(const CanvasPointer &event) {
  if (event.phase != pointer_phase_up)
    return;
  int x = canvas_to_surface_coordinate(event.x, metrics),
      y = canvas_to_surface_coordinate(event.y, metrics);
  for (std::size_t i = 0; i < hit_count; ++i)
    if (hits[i].box.contains(x, y)) {
      action(hits[i].action, hits[i].arg);
      if (dirty) {
        render();
        dirty = false;
      }
      return;
    }
  if (screen == reading) {
    if (x < metrics.width / 3)
      turn(-1);
    else if (x > int(metrics.width) * 2 / 3)
      turn(1);
    else
      change(read_menu);
  }
}
void ReaderApp::action(Action a, int arg) {
  if (!state_ready)
    return;
  auto list_rows = visible_rows;
  switch (a) {
  case text_edit: {
    auto shown = input_ref.show_keyboard();
    if (!shown)
      error("系统输入法", shown.error());
    break;
  }
  case home:
    change(shelf);
    break;
  case discover:
    change(search);
    break;
  case source_list:
    change(sources);
    break;
  case options:
    return_screen = screen;
    change(settings);
    break;
  case open_book:
    open(std::size_t(arg), screen == search);
    break;
  case next_page:
    turn(1);
    break;
  case previous_page:
    turn(-1);
    break;
  case menu:
    change(read_menu);
    break;
  case contents:
    change(toc);
    break;
  case download:
    if (!busy) {
      context->foreground_tasks().cancel();
      page_busy = false;
    }
    start(download_current());
    break;
  case font_less:
    state.settings.font = std::max(12, int(state.settings.font) - 2);
    previous_count = 0;
    save();
    start(load_page());
    break;
  case font_more:
    state.settings.font = std::min(28, int(state.settings.font) + 2);
    previous_count = 0;
    save();
    start(load_page());
    break;
  case gap_change:
    state.settings.line_gap = (state.settings.line_gap + 2) % 14;
    previous_count = 0;
    save();
    start(load_page());
    break;
  case theme_change:
    state.settings.theme = (state.settings.theme + 1) % 3;
    save();
    break;
  case auto_toggle:
    state.settings.automatic = !state.settings.automatic;
    auto_deadline = 0;
    break;
  case auto_speed:
    state.settings.auto_seconds =
        state.settings.auto_seconds >= 30 ? 5 : state.settings.auto_seconds + 5;
    auto_deadline = 0;
    save();
    break;
  case back:
    change(screen == settings ? (return_screen == read_menu ? reading : shelf)
                              : reading);
    break;
  case search_go:
    (void)input_ref.hide_keyboard();
    start(search_books());
    break;
  case search_classic:
    input.set(arg == 1 ? "红楼梦" : "西游记");
    start(search_books());
    break;
  case source_select:
    for (std::size_t i = 0; i < state.sources.size(); ++i) {
      source_index = (source_index + 1) % state.sources.size();
      if (state.sources[source_index].enabled)
        break;
    }
    results.clear();
    next_results.clear();
    break;
  case add_source:
    input.set("");
    change(source_add);
    break;
  case import_source:
    (void)input_ref.hide_keyboard();
    start(import_remote_source());
    break;
  case source_toggle:
    state.sources[arg].enabled = !state.sources[arg].enabled;
    save();
    break;
  case remove_source:
    if (state.sources.size() > 1) {
      state.sources.erase(state.sources.begin() + arg);
      source_index = 0;
      save();
    } else
      notice("请至少保留一个书源");
    break;
  case list_previous:
    list_start = std::size_t(std::max(0, int(list_start) - int(list_rows)));
    break;
  case list_next: {
    auto count = screen == toc       ? chapters.size()
                 : screen == search  ? results.size()
                 : screen == sources ? state.sources.size()
                                     : state.shelf.size();
    if (list_start + list_rows < count)
      list_start += list_rows;
    else if (screen == search && !next_results.empty() && !busy)
      start(search_books(next_results));
    break;
  }
  case open_chapter:
    state.shelf[active].position = chapters[arg].offset;
    previous_count = 0;
    save();
    change(reading);
    start(load_page());
    break;
  case bookmark:
    save();
    notice("阅读位置已保存");
    break;
  case remove_book:
    if (busy) {
      notice("请先停止下载");
      break;
    }
    context->foreground_tasks().cancel();
    page_busy = false;
    start(erase_current());
    break;
  case cancel_job:
    context->foreground_tasks().cancel();
    busy = false;
    page_busy = false;
    notice("下载已停止，重试会重新下载");
    break;
  }
  dirty = true;
}
