template<class Owner>
bool ReaderApp::Controls::render(Owner& owner, std::uint32_t parent) {
  auto& state = app->state;
  const auto screen = app->screen;
  const auto busy = app->busy;
  auto& status = app->status;
  const auto notice_deadline = app->notice_deadline;
  const auto active = app->active;
  const auto list_start = app->list_start;
  const auto source_index = app->source_index;
  auto& results = app->results;
  auto& chapters = app->chapters;
  using reader::Rect;
  auto l = app->layout();
  std::size_t rendered_rows = 0;
  const WidgetColor bg = screen == reading ? WidgetColor(app->body_background()) : WidgetColor(Color::background);
  const WidgetColor ink = screen == reading && state.settings.theme != 3 ? WidgetColor(app->body_foreground()) : WidgetColor(Color::text);
  const WidgetColor muted = Color::muted;
  const WidgetColor surface = Color::surface_container_high, accent = Color::primary;
  const int gap = std::max(3, int(6 * l.density));
  const int inset = std::max(4, int(10 * l.density));
  auto region = [&](Rect r) {
    const double density = app->native_density();
    const int x = int(std::floor(r.x / density)), y = int(std::floor(r.y / density));
    return CanvasRegion{x, y, std::max(1, int(std::ceil((r.x + r.w) / density)) - x),
                        std::max(1, int(std::ceil((r.y + r.h) / density)) - y)};
  };
  auto font_for_pixels = [](int pixels) {
    return pixels >= 28 ? Font::display : pixels >= 24 ? Font::headline :
           pixels >= 20 ? Font::title : pixels >= 16 ? Font::body :
           pixels >= 14 ? Font::label : Font::caption;
  };
  auto font_pixels = [](Font font) {
    return font == Font::display ? 28 : font == Font::headline ? 24 :
           font == Font::title ? 20 : font == Font::body ? 16 :
           font == Font::label ? 14 : 12;
  };
  int button_pixels = l.ui_font;
  if (screen == settings) {
    const int first_y = l.top + l.row + gap / 2 + l.caption + gap / 2;
    for (int i = 0; i < 3; ++i) {
      auto r = l.box(first_y + i * l.row, l.row - gap / 2);
      button_pixels = std::min(button_pixels, reader::fit_text_size("A−  字号 28", button_pixels,
          {0, 0, (r.w - gap) / 2 - inset, r.h - 2}));
    }
  }
  bool ok = true;
  std::size_t callback_count = 0;
#ifdef READER_VALIDATE
  // Serial logging has a bounded queue. Batch geometry, and keep verbose label
  // traces desktop-only so instrumentation cannot drop the last live controls.
  std::array<char, 256> hit_trace{};
  std::size_t hit_trace_size = 0;
  auto flush_hits = [&] {
    if (hit_trace_size) app->log("READER-HITS " + std::string(hit_trace.data(), hit_trace_size));
    hit_trace_size = 0;
  };
  auto trace_hit = [&](int action, int arg, Rect r) {
    if (hit_trace.size() - hit_trace_size < 48) flush_hits();
    auto end = hit_trace.data() + hit_trace_size;
    auto literal = [&](std::string_view value) { end = std::copy(value.begin(), value.end(), end); };
    auto number = [&](int value) { end = std::to_chars(end, hit_trace.data() + hit_trace.size(), value).ptr; };
    number(action); literal(","); number(arg); literal(","); number(r.x + r.w / 2);
    literal(","); number(r.y + r.h / 2); literal(";");
    hit_trace_size = std::size_t(end - hit_trace.data());
  };
#ifdef READER_TRACE_LABELS
  app->log("READER-UI screen=" + reader::number_string(screen) + " busy=" + reader::number_string(busy) +
      " density=" + reader::number_string(app->metrics.density_q16) +
      " auto=" + reader::number_string(state.settings.automatic) + " seconds=" + reader::number_string(state.settings.auto_seconds) +
      " native_ui=1 draw_bytes=" + reader::number_string(app->draw.bytes_used()) +
      " input_bytes=" + reader::number_string(app->input.get().size()));
#else
  app->log("READER-UI screen="+reader::number_string(screen)+" busy="+reader::number_string(busy)+
           " native_ui=1 draw_bytes="+reader::number_string(app->draw.bytes_used()));
#endif
#endif
  auto rectangle = [&] [[gnu::noinline]] (Rect r, WidgetColor color, int radius = 12, bool outlined = false) {
    if (r.w > 0 && r.h > 0)
      ok = Box(WidgetStyle{region(r), ink, color, Font::body,
                           std::max(0, int(radius * l.density / app->native_density())), 0,
                           outlined ? 1 : 0, Color::outline_variant}).render(owner, parent) && ok;
  };
  auto text = [&] [[gnu::noinline]] (Rect r, std::string_view value, int pixels, WidgetColor color, std::uint8_t align = 0, State<std::string>* binding = nullptr) {
    if (r.w <= 0 || r.h < 16) return;
    Font font = screen == reading ? Font::caption : font_for_pixels(std::min(pixels, r.h * 5 / 8));
    pixels = screen == reading ? l.ui_font : font_pixels(font);
    const int line_h = reader::text_line_height(pixels);
    std::array<char, 192> scratch{};
    auto shown = reader::ellipsize(value, pixels, r.w, scratch);
    r.y += std::max(0, (r.h - line_h) / 2);
    r.h = std::max(line_h, r.h);
    auto style = WidgetStyle{region(r), color, 0, font, 0, align};
    if (binding) {
      binding->set(std::string(shown));
      if (binding == &app->header_text) { app->header_width = r.w; app->header_font = pixels; }
      else { app->subtitle_width = r.w; app->subtitle_font = pixels; }
      ok = Text(*binding, style).render(owner, parent) && ok;
    } else ok = Text(shown, style).render(owner, parent) && ok;
#ifdef READER_TRACE_LABELS
    app->log("READER-TEXT bytes=" + reader::number_string(shown.size()) + " height=" + reader::number_string(r.h) +
             " size=" + reader::number_string(font_pixels(font)) + " text=" + std::string(shown));
#endif
  };
  auto button = [&] [[gnu::noinline]] (Rect r, std::string_view label, Action a, int arg = 0, bool selected = false, bool data_label = false) {
    if (r.w <= 0 || r.h <= 0) return;
    if (callback_count == callbacks.size()) { ok = owner.fail(pxa::Error::resource_limit); return; }
    auto& callback = callbacks[callback_count++]; callback = {app, a, arg};
    int preferred = reader::fit_text_size(label, button_pixels, {0, 0, r.w - inset, r.h - 2});
    Font font = font_for_pixels(preferred);
    // Complete control labels are fitted before selecting a Host font role.
    std::array<char, 192> scratch{};
    if (data_label) label = reader::ellipsize(label, font_pixels(font), r.w - inset, scratch);
    const bool destructive = a == confirm_remove;
    auto control = Button(label, WidgetStyle{region(r), destructive ? WidgetColor(Color::on_error_container) : selected ? WidgetColor(Color::on_primary) : WidgetColor(Color::text),
                                   destructive ? WidgetColor(Color::error_container) : selected ? accent : surface,
                                   font, std::max(3, int(12 * l.density / app->native_density())), 1,
                                   selected || destructive ? 0 : 1, Color::outline_variant});
    if ((a == open_book && screen == shelf) || screen == toc)
      ok = control.on_pointer_ref(callback).render(owner, parent) && ok;
    else ok = control.on_click_ref(callback).render(owner, parent) && ok;
#ifdef READER_VALIDATE
    trace_hit(a, arg, r);
#endif
#ifdef READER_TRACE_LABELS
    app->log("READER-BUTTON action=" + reader::number_string(a) + " width=" + reader::number_string(r.w) +
             " height=" + reader::number_string(r.h) + " size=" + reader::number_string(font_pixels(font)) + " text=" + std::string(label));
#endif
  };
  auto row = [&](int y, std::string_view label, Action a, int arg = 0, bool data_label = false) {
    button(l.box(y, l.row - gap / 2), label, a, arg, false, data_label);
  };
  auto pair = [&](int y, std::string_view left, Action a, std::string_view right, Action b) {
    auto r = l.box(y, l.row - gap / 2); const int w = (r.w - gap) / 2;
    button({r.x, y, w, r.h}, left, a);
    button({r.x + w + gap, y, r.w - w - gap, r.h}, right, b);
  };
  auto book_card = [&] [[gnu::noinline]] (int y, int h, const reader::Book& book, int index, bool first) {
    auto r = l.box(y, h); button(r, "", open_book, index);
    int cover_w = std::min(int(38 * l.density), r.w / 4);
    Rect cover{r.x + inset / 2, y + inset / 2, cover_w, h - inset};
    rectangle(cover, Color::primary_container, 5);
    const int cover_font = std::max(12, l.ui_font * 3 / 4);
    text({cover.x + 2, cover.y, cover.w - 4, cover.h}, "书页", cover_font, Color::on_primary_container, 1);
    const int x = cover.x + cover.w + inset / 2, w = r.x + r.w - x - inset / 2;
    const int title_h = reader::text_line_height(font_pixels(font_for_pixels(l.ui_font)));
    const int detail_font = std::max(12, l.ui_font * 3 / 4);
    const int detail_h = reader::text_line_height(font_pixels(font_for_pixels(detail_font)));
    const int content_y = y + inset / 2, content_h = h - inset;
    text({x, content_y, w, std::min(content_h, title_h)}, book.title, l.ui_font, ink);
    if (content_h >= title_h + detail_h)
      text({x, content_y + title_h, w, detail_h}, book.author, detail_font, muted);
    if (content_h >= title_h + 2 * detail_h) {
      char progress[64];
      const auto percentage = book.total ? std::min(std::uint64_t(100), std::uint64_t(book.position) * 100 / book.total) : 0;
      const std::string_view label = book.complete ? "离线可读 · " : "联网阅读 · ";
      auto end = std::copy(label.begin(), label.end(), progress);
      end = std::to_chars(end, progress + sizeof(progress) - 2, percentage).ptr;
      *end++ = '%'; *end = 0;
      text({x, content_y + title_h + detail_h, w, detail_h},
           first && screen == shelf && !book.position ? "开始你的第一章" : progress, detail_font, accent);
    }
  };
  if (screen != reading && screen != settings) rectangle({0, 0, l.width, l.height}, bg, 0);
  auto header = l.box(l.top, l.row);
  const bool has_settings = screen == shelf;
  const int settings_width = has_settings ? l.ui_font * 2 + inset + 2 :
                              screen == search && !results.empty() ? 2 * l.row : 0;
  if (screen == reading) {
    // Font roles already scale in the Host. Selecting a role using scaled
    // pixels inflated the book name a second time on high-DPI screens.
    text(header, app->header_title(), l.ui_font, ink, l.metrics.shape == 2 ? 1 : 0, &app->header_text);
#ifdef READER_TRACE_LABELS
    app->log("READER-HEADER x="+reader::number_string(header.x)+" y="+reader::number_string(header.y)+
        " width="+reader::number_string(header.w)+" height="+reader::number_string(header.h)+
        " pixels="+reader::number_string(l.ui_font)+" text="+app->header_text.get());
#endif
  } else {
    const int header_size = std::min({int(23 * l.density), l.row - gap,
        has_settings ? std::max(8, (header.w - settings_width) / 2) : header.w});
    text({header.x, header.y + gap / 2, header.w - settings_width, header.h}, app->header_title(), header_size, ink, 0, &app->header_text);
  }
  if (has_settings) button({header.x + header.w - settings_width, header.y, settings_width, l.row - gap}, "设置", options);
  if (screen == search && !results.empty()) {
    button({header.x + header.w - 2 * l.row, header.y, l.row - gap / 2, l.row - gap}, "‹", list_previous);
    button({header.x + header.w - l.row, header.y, l.row, l.row - gap}, "›", list_next);
  }
  int y = l.top + l.row + gap / 2;
  if (screen != reading && screen != search && screen != settings) {
    text(l.box(y, l.caption), app->subtitle(), std::max(8, l.ui_font * 4 / 5), muted, 0, &app->subtitle_text);
    y += l.caption + gap / 2;
  }
  TextInputBox native_input{};
  const int stop = screen == search ? l.bottom - l.row - gap : l.bottom - l.row * 2;
  if (screen == read_menu) {
    pair(y, "继续阅读", back, "章节目录", contents); y += l.row;
    pair(y, busy ? "停止下载" : "下载全文", busy ? cancel_job : download, "保存进度", bookmark); y += l.row;
    pair(y, "阅读偏好", options, "返回书架", home); y += l.row;
  } else if (screen == book_menu) {
    row(y, "继续阅读", open_book, int(active)); y += l.row;
    row(y, state.shelf[active].complete ? "已缓存 · 离线可读" : "继续缓存全文", download); y += l.row;
    row(y, "移除书籍…", remove_book);
  } else if (screen == remove_confirm) {
    text(l.box(y, l.row), "将移除书架记录与本地缓存", std::max(12, l.ui_font * 4 / 5), muted); y += l.row;
    row(y, "确认移除", confirm_remove); y += l.row;
    row(y, "取消", back);
  } else if (screen == settings) {
    // Compact native controls leave two true-size body lines on small panels.
    const auto sample = reader::preference_preview(l, state.settings);
    const int row_h = std::min(l.row, std::max(20, (sample.first.y - y - gap) / 2));
    // Labels are encoded during this call; literals need no temporary strings.
    auto trio = [&](int yy, const std::array<std::string_view,3>& labels, const std::array<Action,3>& actions) {
      auto r = l.box(yy,row_h-gap/2); const int w = (r.w-2*gap)/3;
      for (int i=0;i<3;++i) button({r.x+i*(w+gap),yy,i==2?r.w-2*(w+gap):w,r.h},labels[i],actions[i]);
    };
    trio(y, {"A−  " + reader::number_string(state.settings.font), "A+", "行距 " + reader::number_string(state.settings.line_gap)},
         {font_less,font_more,gap_change}); y += row_h;
    const char* theme = state.settings.theme == 3 ? "系统" : state.settings.theme == 2 ? "深夜" : state.settings.theme == 1 ? "护眼" : "纸张";
    trio(y, {std::string("配色·")+theme, state.settings.automatic ? "自动·开" : "自动·关", reader::number_string(state.settings.auto_seconds)+"秒"},
         {theme_change,auto_toggle,auto_speed});
  } else if (screen == search || screen == source_add) {
    auto r = l.box(y, l.row); rectangle(r, surface, 12, true);
    int reserved = std::max(l.row, int(l.row * 1.3));
    Rect edit{r.x + inset / 2, y + gap / 2, std::max(1, r.w - reserved - inset), r.h - gap};
    // Hint belongs above the field, never below the transparent native editor.
    const Font font = font_for_pixels(std::min(l.ui_font, (l.row - gap) * 5 / 8));
    native_input = {region(edit), true, font, app->system_color(Color::text)};
#ifdef READER_VALIDATE
    trace_hit(30, 0, edit);
#endif
    button({r.x + r.w - reserved, y, reserved, r.h - gap / 2}, screen == search ? (busy ? "取消" : "搜索") : "添加",
            screen == search ? (busy ? cancel_job : search_go) : import_source, 0, true); y += l.row + gap / 2;
    if (screen == search) {
      row(y, state.sources[source_index].name + (results.empty() ? " ›" : " · " + reader::number_string(results.size()) + " 部 ›"), source_select, 0, true); y += l.row;
      for (std::size_t i = list_start; i < results.size() && y + l.row <= stop; ++i) {
        ++rendered_rows; book_card(y, l.row - gap / 2, results[i], int(i), i == list_start); y += l.row;
      }
      if (results.empty() && notice_deadline && y + l.row <= stop) {
        text(l.box(y, l.row), status, std::max(12, l.ui_font * 4 / 5), muted);
        y += l.row;
      }
      if (results.empty() && !busy && y + l.row <= stop) {
        auto r = l.box(y, l.row - gap / 2); int w = (r.w - gap) / 2;
        button({r.x, y, w, r.h}, "西游记", search_classic);
        button({r.x + w + gap, y, r.w - w - gap, r.h}, "红楼梦", search_classic, 1);
      }

    } else {
      text(l.box(y, l.row), "粘贴目录 JSON 或书源配置链接", std::max(8, l.ui_font * 4 / 5), muted);
      row(y + l.row, "返回书源列表", source_list);
    }
  } else if (screen == sources) {
    auto plus = l.box(l.top, l.row - gap);
    button({plus.x + plus.w - l.row, plus.y, l.row, plus.h}, "+", add_source);
    for (std::size_t i = list_start; i < state.sources.size() && y + l.row <= stop; ++i) {
      ++rendered_rows; auto r = l.box(y, l.row - gap / 2); int w = r.w - l.row;
      // Preserve the state suffix even when a user-defined source name is long.
      std::array<char, 192> scratch{};
      const std::string_view suffix = state.sources[i].enabled ? " · 已启用" : " · 已停用";
      const auto source_name = reader::ellipsize(state.sources[i].name, 12,
          w - inset - reader::text_width(suffix, 12), scratch);
      button({r.x, y, w, r.h}, std::string(source_name) + std::string(suffix),
             source_toggle, int(i), state.sources[i].enabled);
      button({r.x + w + gap / 2, y, r.w - w - gap / 2, r.h}, "−", remove_source, int(i)); y += l.row;
    }
    pair(l.bottom - l.row * 2, "上一组", list_previous, "下一组", list_next);
  } else if (screen == toc) {
    if (chapters.empty()) text(l.box(y, l.row), busy ? "正在整理章节…" : "暂无目录，可以按页阅读", l.ui_font, muted);
    for (std::size_t i = list_start; i < chapters.size() && y + l.row <= stop; ++i) {
      ++rendered_rows; row(y, (chapters[i].offset < app->cached_bytes || state.shelf[active].url.starts_with("sample:") ? "" : "待缓存 · ") + chapters[i].title, open_chapter, int(i), true); y += l.row;
    }
    pair(l.bottom - l.row * 2, "上一组", list_previous, "下一组", list_next);
  } else if (screen == shelf) {
    int height = std::max(l.row, int(l.row * 1.7));
    for (std::size_t i = list_start; i < state.shelf.size() && y + l.row <= stop; ++i) {
      const int card_h = std::min(height, stop - y);
      ++rendered_rows; book_card(y, card_h - gap / 2, state.shelf[i], int(i), i == list_start); y += card_h;
    }
    if (state.shelf.size() > rendered_rows) pair(l.bottom - l.row * 2, "上一组", list_previous, "下一组", list_next);
    if (state.shelf.empty()) text(l.box(y, l.row * 2), "这里等着你的第一本书", l.ui_font, muted);
  }
  if (screen != reading) {
    if (screen == toc || screen == settings || screen == read_menu || screen == book_menu || screen == remove_confirm)
      row(l.bottom - l.row, screen == settings ? "完成" : "返回", back);
    else {
      auto r = l.box(l.bottom - l.row, l.row - gap / 2); rectangle(r, Color::surface_container);
      int w = r.w / 3;
      for (int i = 0; i < 3; ++i) {
        Rect tab{r.x + w * i, r.y, i == 2 ? r.w - 2 * w : w, r.h};
        const bool selected = i == 0 ? screen == shelf : i == 1 ? screen == search : screen == sources || screen == source_add;
        button(tab, i == 0 ? "书架" : i == 1 ? "发现" : "书源",
               i == 0 ? home : i == 1 ? discover : source_list, 0, selected);
      }
    }
  }
#ifdef READER_VALIDATE
  flush_hits();
#ifdef READER_TRACE_LABELS
  auto pool = pxa::task_pool_stats();
  app->log("READER-MEM pool_bytes=" + reader::number_string(pool.reserved_bytes) +
           " pool_peak=" + reader::number_string(pool.peak_slots) + " failures=" + reader::number_string(pool.allocation_failures)
#if defined(__wasm__)
           + " linear_pages=" + reader::number_string(__builtin_wasm_memory_size(0))
#endif
           );
#endif
#endif
  app->visible_rows = std::max<std::size_t>(1, rendered_rows);
  app->input_box.set(native_input);
  return ok;
}
void ReaderApp::render() {
  auto l = layout();
  draw.reset();
  if (screen == reading || screen == settings) {
    const auto ink = body_foreground();
    const double density = native_density();
    auto region = [&](reader::Rect r) {
      const int x = int(std::floor(r.x / density)), y = int(std::floor(r.y / density));
      return CanvasRegion{x, y, std::max(1, int(std::ceil((r.x + r.w) / density)) - x),
                          std::max(1, int(std::ceil((r.y + r.h) / density)) - y)};
    };
    auto full = region({0, 0, l.width, l.height});
    draw.rect(full.x, full.y, full.width, full.height,
              screen == reading ? body_background() : system_color(Color::background));
    if (screen == settings) {
      const auto preview = reader::preference_preview(l, state.settings);
      auto render_line = [&](reader::Rect box, std::string_view value) {
        auto b = region(box);
        draw.rect(b.x,b.y,b.width,b.height,body_background());
        if (sized_text_supported) draw.text_sized(b,ink,value,std::uint16_t(preview.pixels));
        else draw.text(b.x,b.y,b.width,ink,value,preview.pixels>=18?2:1);
      };
      // Per-line spans protect both text and its paper background on circular
      // screens; a rectangle spanning both rows would be needlessly narrower.
      render_line(preview.first,"山有木兮，");
      render_line(preview.second,"风有诗意。");
#ifdef READER_VALIDATE
      log("READER-PREVIEW pixels="+reader::number_string(preview.pixels)+" step="+reader::number_string(preview.step)+
          " y="+reader::number_string(preview.first.y)+" x="+reader::number_string(preview.first.x)+" width="+reader::number_string(preview.first.w)+" height="+reader::number_string(preview.first.h)+" x2="+reader::number_string(preview.second.x)+" width2="+reader::number_string(preview.second.w));
#endif
    }
    const int size = std::max(10, int(state.settings.font * l.density));
    if (screen == reading && page.count == 0) {
      // Draw initial feedback with the body, so the first cached page clears
      // it immediately without retaining a native label above the text.
      auto box = region(l.box(std::max(l.body_start(), (l.height - l.row) / 2), l.row * 2));
      const auto message = busy ? std::string_view("正在打开章节…") : std::string_view(status);
      if (sized_text_supported) draw.text_sized(box, ink, message, std::uint16_t(l.ui_font));
      else draw.text(box.x, box.y, box.width, ink, message);
    }
    for (std::size_t i = 0; screen == reading && i < page.count; ++i) {
      const auto& value = page.lines[i];
      const auto line = std::string_view(page_text).substr(value.begin, value.end - value.begin);
#ifdef READER_TRACE_LABELS
      log("READER-LINE x="+reader::number_string(value.box.x)+" y="+reader::number_string(value.box.y)+
          " width="+reader::number_string(value.box.w)+" height="+reader::number_string(value.box.h)+
          " size="+reader::number_string(size)+" text="+std::string(line));
#endif
      if (sized_text_supported)
        draw.text_sized(region(value.box), ink, line, std::uint16_t(size));
      else {
        auto box = region(value.box);
        draw.text(box.x, box.y, box.width, ink, line, size >= 18 ? 2 : 1);
      }
    }
    if (screen==reading && page.count && !state.shelf.empty()) {
      std::array<char,96> scratch{};
      const auto label=page_numbers.label(state.shelf[active].position,scratch);
      const auto box=l.page_number_box();
      const int pixels=reader::fit_text_size(label,std::max(8,int(10*l.density)),box);
      if (sized_text_supported) draw.text_sized(region(box),ink,label,std::uint16_t(pixels),1);
      else { auto b=region(box); draw.text(b.x,b.y,b.width,ink,label,0,1); }
#ifdef READER_VALIDATE
      log("READER-NUMBER current="+reader::number_string(page_numbers.current)+" total="+reader::number_string(page_numbers.total)+
          " x="+reader::number_string(box.x)+" y="+reader::number_string(box.y)+" width="+reader::number_string(box.w)+
          " height="+reader::number_string(box.h)+" pixels="+reader::number_string(pixels)+" text="+std::string(label));
#endif
    }
    auto submitted = canvas.present(draw);
    if (!submitted) error("绘制", submitted.error());
  } else {
    // Discard retained body commands outside reading and its typography preview.
    auto submitted = canvas.present(draw);
    if (!submitted) error("绘制", submitted.error());
  }
  auto label = [](State<std::string>& state, std::string_view text, int size, int width) {
    std::array<char, 192> scratch{};
    state.set(std::string(reader::ellipsize(text, size, width, scratch)));
  };
  label(header_text, header_title(), header_font, header_width);
  if (screen != reading) label(subtitle_text, subtitle(), subtitle_font, subtitle_width);
  // Text/progress updates preserve pressed native controls. Rebuild only when
  // structure, geometry or button state changes, not on every cache range.
  if ((controls_dirty || rendered_busy != busy) && !(screen == toc && pressed_book != -1)) {
    ui_revision.update([](auto value) { return value + 1; });
    controls_dirty = false; rendered_busy = busy;
  }
}
void ReaderApp::ControlAction::operator()() const { app->action(action, arg); }
void ReaderApp::ControlAction::operator()(const CanvasPointer& event) const {
  if (app->screen != toc) { app->book_pointer(arg,event); return; }
  // New chapters may arrive while a directory button is pressed. Keep this
  // native tree mounted until release; reuse the shelf's dormant press fields.
  if (event.phase == pointer_phase_down) {
    app->pressed_book = 0; app->press_x = event.x; app->press_y = event.y;
  } else if (event.phase == pointer_phase_move &&
             (std::abs(event.x-app->press_x)>8 || std::abs(event.y-app->press_y)>8)) {
    app->pressed_book = -2;
  } else if (event.phase == pointer_phase_up || event.phase == pointer_phase_cancel) {
    const bool clicked = app->pressed_book == 0 && event.phase == pointer_phase_up;
    app->pressed_book = -1; app->dirty = true;
    if (clicked) app->action(action,arg);
  }
}
void ReaderApp::book_pointer(int index, const CanvasPointer& event) {
  if (screen != shelf) return;
  if (event.phase == pointer_phase_down) {
    pressed_book = index; press_time = event.timestamp_us; press_x = event.x; press_y = event.y;
  } else if (event.phase == pointer_phase_cancel ||
             (event.phase == pointer_phase_move && (std::abs(event.x - press_x) > 8 || std::abs(event.y - press_y) > 8))) {
    pressed_book = -1;
  } else if (event.phase == pointer_phase_up && pressed_book == index) {
    pressed_book = -1;
    action(event.timestamp_us >= press_time + 550000 ? manage_book : open_book, index);
  }
}
void ReaderApp::pointer(const CanvasPointer &event) {
  if (screen != reading) return;
  // Reuse the list gesture fields: a drag or cancelled system gesture must
  // never turn a page or open the menu on release.
  if (event.phase == pointer_phase_down) {
    pressed_book = 0; press_x = event.x; press_y = event.y;
    return;
  }
  if (event.phase == pointer_phase_cancel ||
      (event.phase == pointer_phase_move &&
       (std::abs(event.x - press_x) > 8 || std::abs(event.y - press_y) > 8))) {
    pressed_book = -1;
    return;
  }
  if (event.phase != pointer_phase_up) return;
  const bool tapped = pressed_book == 0 &&
      std::abs(event.x - press_x) <= 8 && std::abs(event.y - press_y) <= 8;
  pressed_book = -1;
  if (!tapped) return;
  int x = canvas_to_surface_coordinate(event.x, metrics);
  if (x < metrics.width / 3) turn(-1);
  else if (x > int(metrics.width) * 2 / 3) turn(1);
  else change(read_menu);
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
    if (busy) cancel_network();
    change(shelf);
    break;
  case discover:
    if (busy) cancel_network();
    change(search);
    break;
  case source_list:
    if (busy) cancel_network();
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
    if (state.shelf[active].complete) { notice("已缓存全文，可离线阅读"); break; }
    if (screen == book_menu) change(reading);
    if (!busy) {
      context->foreground_tasks().cancel();
      page_busy = false;
    }
    start(download_current());
    break;
  case font_less:
    state.settings.font = std::max(12, int(state.settings.font) - 2);
    previous_count = 0;
    reset_page_numbers(page_numbers.origin);
    save();
    start(load_page());
    break;
  case font_more:
    state.settings.font = std::min(28, int(state.settings.font) + 2);
    previous_count = 0;
    reset_page_numbers(page_numbers.origin);
    save();
    start(load_page());
    break;
  case gap_change:
    state.settings.line_gap = (state.settings.line_gap + 2) % 14;
    previous_count = 0;
    reset_page_numbers(page_numbers.origin);
    save();
    start(load_page());
    break;
  case theme_change:
    state.settings.theme = (state.settings.theme + 1) % 4;
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
    if (screen == book_menu || screen == remove_confirm) { change(shelf); break; }
    change(screen == settings ? (return_screen == read_menu ? reading : shelf)
                              : reading);
    break;
  case search_go:
    (void)input_ref.hide_keyboard();
    cancel_network();
    start(search_books());
    break;
  case search_classic:
    input.set(arg == 1 ? "红楼梦" : "西游记");
    start(search_books());
    break;
  case source_select:
    cancel_network();
    for (std::size_t i = 0; i < state.sources.size(); ++i) {
      source_index = (source_index + 1) % state.sources.size();
      if (state.sources[source_index].enabled)
        break;
    }
    results.clear();
    next_results.clear();
    break;
  case add_source:
    search_query = input.get();
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
    if (arg < 0 || std::size_t(arg) >= chapters.size()) break;
    if (chapters[arg].offset >= cached_bytes && !state.shelf[active].url.starts_with("sample:")) {
      notice("这一章还未缓存，请稍候或继续下载"); break;
    }
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
  case manage_book:
    if (arg < 0 || std::size_t(arg) >= state.shelf.size()) break;
    cancel_network(); active = std::size_t(arg); change(book_menu);
    break;
  case remove_book:
    change(remove_confirm);
    break;
  case confirm_remove:
    cancel_network(); start(erase_current());
    break;
  case cancel_job:
    cancel_network();
    notice(screen == search ? "搜索已取消" : "缓存已暂停，已下载部分仍可阅读");
    break;
  }
  dirty = true;
  controls_dirty = true;
}
