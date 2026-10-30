// SPDX-License-Identifier: CC0-1.0
// Text is a retained native Canvas overlay. Artwork is painted directly into
// the output surface: no enlarged bitmap glyphs or intermediate low-res frame.
struct StudioText {
    int x, y, w, scale;
    uint16_t color;
    unsigned align;
    bool visible;
    int minimum_font;
    char value[128];
};
StudioText studio_texts[112];
unsigned studio_count;
uint8_t studio_bytes[12288];
uint32_t studio_generation, studio_hash, studio_density = 65536;
uint8_t studio_initialized = 1;
#ifdef PXA_POCKET_FILL
int studio_dx(int x) { return int(int64_t(x) * content_viewport.width / WIDTH); }
int studio_dy(int y) { return int(int64_t(y) * content_viewport.height / HEIGHT); }
int studio_x(int x) { return (content_viewport.x - viewport.x) / viewport.host_scale + studio_dx(x); }
int studio_y(int y) { return (content_viewport.y - viewport.y) / viewport.host_scale + studio_dy(y); }
// Continue the scene/background into the safe border; only controls are inset.
void studio_bleed_edges() {
    const int x0 = studio_x(0), x1 = studio_x(WIDTH), y0 = studio_y(0), y1 = studio_y(HEIGHT);
    for (int y = y0; y < y1; ++y) {
        uint16_t *row = canvas + y * viewport.width;
        std::fill(row, row + x0, row[x0]);
        std::fill(row + x1, row + viewport.width, row[x1 - 1]);
    }
    for (int y = 0; y < y0; ++y) std::copy_n(canvas + y0 * viewport.width, viewport.width, canvas + y * viewport.width);
    for (int y = y1; y < viewport.height; ++y) std::copy_n(canvas + (y1 - 1) * viewport.width, viewport.width, canvas + y * viewport.width);
}
#else
int studio_dx(int x) { return int(int64_t(x) * viewport.width / WIDTH); }
int studio_dy(int y) { return int(int64_t(y) * viewport.height / HEIGHT); }
int studio_x(int x) { return studio_dx(x); }
int studio_y(int y) { return studio_dy(y); }
#endif
void studio_begin() { studio_count = 0; }
void studio_occlude(int x, int y, int w, int h) {
    if (w < 3 || h < 3)
        return;
    for (unsigned i = 0; i < studio_count; ++i) {
        auto &t = studio_texts[i];
        if (x < t.x + t.w && x + w > t.x && y < t.y + 10 * t.scale && y + h > t.y)
            t.visible = false;
    }
}
void studio_rect(int x, int y, int w, int h, uint16_t c) {
    if (!canvas || w <= 0 || h <= 0)
        return;
    studio_occlude(x, y, w, h);
    int x0 = std::clamp(studio_x(x), 0, viewport.width),
        x1 = std::clamp(studio_x(x + w), 0, viewport.width);
    int y0 = std::clamp(studio_y(y), 0, viewport.height),
        y1 = std::clamp(studio_y(y + h), 0, viewport.height);
    for (int j = y0; j < y1; ++j)
        std::fill(canvas + j * viewport.width + x0, canvas + j * viewport.width + x1, c);
}
void studio_add(int x, int y, int w, const char *s, uint16_t c, int scale, unsigned align,
                int minimum_font = 9) {
    if (!s || !*s || w <= 0 || scale < 1 || scale > 4 || strlen(s) >= 128 || studio_count >= 112)
        return;
    auto &t = studio_texts[studio_count++];
    t = {x, y, w, scale, c, align, true, minimum_font, {}};
    memcpy(t.value, s, strlen(s) + 1);
}
bool studio_create(pxa_ui_transaction_t *tx) {
    return pxa_ui_create(tx, 2, 1, 0, PXA_UI_NODE_CANVAS) &&
           pxa_ui_set_length(tx, 2, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_length(tx, 2, PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_u8(tx, 2, PXA_UI_PROPERTY_COMPOSITION, PXA_UI_COMPOSITION_ALPHA_OVERLAY) &&
           pxa_ui_set_rgba(tx, 2, PXA_UI_PROPERTY_BACKGROUND, 0) && pxa_ui_set_event_mask(tx, 2, 0);
}
int studio_logical(int px) {
    return int((int64_t(px) * 65536 + studio_density / 2) / studio_density);
}
bool studio_present() {
    pxa_canvas_frame_t f;
    pxa_canvas_begin(&f, studio_bytes, sizeof(studio_bytes));
    // Paint the matte only outside the game viewport. The UI root must stay
    // transparent because the surface is composed below the retained UI tree.
    auto matte = [&](int x, int y, int w, int h) {
        if (w <= 0 || h <= 0) return true;
        const auto horizontal = pocket_ui::cover_pixel_span(x, w, studio_density);
        const auto vertical = pocket_ui::cover_pixel_span(y, h, studio_density);
        return pxa_canvas_rect_rgba(&f, horizontal.origin, vertical.origin,
                                    horizontal.length, vertical.length, 0x08162bff, 0, 0, 0) != 0;
    };
    const int right = viewport.x + viewport.physical_width(),
              bottom = viewport.y + viewport.physical_height();
    if (!matte(0, 0, viewport.x, display_h) || !matte(right, 0, display_w - right, display_h) ||
        !matte(viewport.x, 0, viewport.physical_width(), viewport.y) ||
        !matte(viewport.x, bottom, viewport.physical_width(), display_h - bottom))
        return false;
    for (unsigned i = 0; i < studio_count; ++i) {
        const auto &t = studio_texts[i];
        if (!t.visible)
            continue;
        int x = viewport.x + studio_x(t.x) * viewport.host_scale,
            y = viewport.y + studio_y(t.y) * viewport.host_scale;
        int w = std::max(1, (studio_x(t.x + t.w) - studio_x(t.x)) * viewport.host_scale);
        int font =
            std::clamp(10 * t.scale *
#ifdef PXA_POCKET_FILL
            content_viewport.physical_height()
#else
            viewport.physical_height()
#endif
            / HEIGHT, t.minimum_font, 96);
        uint16_t c = t.color;
        uint32_t rgba = (((c >> 11) & 31) * 255 / 31 << 24) | (((c >> 5) & 63) * 255 / 63 << 16) |
                        ((c & 31) * 255 / 31 << 8) | 255;
        unsigned n = unsigned(strlen(t.value));
        // Sized-text (UI 0.6) font size is in physical pixels; positions use dp.
        if (!pxa_canvas_clip_push(&f, studio_logical(x), studio_logical(y - 2), studio_logical(w),
                                  studio_logical(font + 6)) ||
            !pxa_canvas_primitive(&f, 12, uint16_t(24 + n)) ||
            !pxa_put_u32(&f, studio_logical(x)) || !pxa_put_u32(&f, studio_logical(y - font / 4)) ||
            !pxa_put_u32(&f, studio_logical(w)) || !pxa_put_u32(&f, studio_logical(font * 2 + 4)) ||
            !pxa_put_u32(&f, rgba) || !pxa_put_u16(&f, uint16_t(font)) ||
            !pxa_put_u8(&f, uint8_t(t.align)) || !pxa_put_u8(&f, 0) ||
            !pxa_put_bytes(&f, reinterpret_cast<const uint8_t *>(t.value), n) ||
            !pxa_canvas_clip_pop(&f))
            return false;
    }
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < f.length; ++i)
        hash = (hash ^ studio_bytes[i]) * 16777619u;
    if (hash == studio_hash)
        return true;
    if (!pxa_canvas_present_regions(2, &f, &studio_generation, &studio_initialized, 0, nullptr, 0,
                                    packet, sizeof(packet), nullptr, 0))
        return false;
    studio_hash = hash;
    return true;
}
void studio_menu(bool paused,int row,bool exiting){
#if defined(PXA_POCKET_UTILITY) || defined(PXA_POCKET_FILL)
    const int mx=WIDTH-48,x=(WIDTH-192)/2,y=HEIGHT>224?(HEIGHT-180)/2:32;
#else
    const int mx=240,x=48,y=32;
#endif
    studio::round(mx,3,24,18,5,pocket::rgb(35,57,76));
    for(int i=0;i<3;++i)pocket::rect(mx+7,8+3*i,10,1,pocket::INK);
    if(!paused)return;
    studio::card(x,y,192,180,pocket::rgb(16,31,48),pocket::rgb(75,108,132),10);
#if defined(PXA_POCKET_CALCULATOR)
    studio::label(x+8,y+11,176,pocket::tr("CALCULATOR","计算器菜单"),pocket::INK,2);
    const char *v[]={pocket::tr("Continue","继续计算"),pocket::tr("Clear entry","清空输入"),pocket::tr("Save & exit","保存并退出"),pocket::tr("Language: English","语言：中文")};
#elif defined(PXA_POCKET_UTILITY)
    studio::label(x+8,y+11,176,pocket::tr("METER MENU","测量菜单"),pocket::INK,2);
    const char *v[]={pocket::tr("Continue measuring","继续测量"),pocket::tr("Absolute reference","恢复绝对基准"),pocket::tr("Save & exit","保存并退出"),pocket::tr("Language: English","语言：中文")};
#else
    studio::label(x+8,y+11,176,pocket::tr("TAKE A BREATH","休息一下"),pocket::INK,2);
    const char *v[]={pocket::tr("Continue","继续游戏"),pocket::tr("Restart","重新开始"),pocket::tr("Save & exit","保存并退出"),pocket::tr("Language: English","语言：中文")};
#endif
    for(int i=0;i<4;++i){
        bool selected=i==row;
#ifdef PXA_POCKET_CALCULATOR
        selected=selected&&game::navigation_visible();
#endif
        studio::button(x+8,y+40+i*30,176,27,v[i],selected?pocket::rgb(43,112,136):pocket::rgb(31,49,68),pocket::INK,selected);
    }
    studio::label(x+8,y+167,176,exiting?pocket::tr("Saving...","正在保存…"):
#ifdef PXA_POCKET_UTILITY
    pocket::tr("Select · Confirm to use","上/下选择 · 确认执行")
#else
    (pocket::portrait_display() ? pocket::tr("Select · Confirm to use","上/下选择 · 确认执行")
                                : pocket::tr("A select · B resume","A 选择 · B 返回"))
#endif
,pocket::MUTED);
}
