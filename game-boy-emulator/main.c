/* GB/GBC frontend. GPL-2.0-or-later; gnuboy remains GPL-2.0-or-later. */
#include "core.h"
#include "audio.h"
#include "audio_fifo.h"
#include "gb_profile.h"
#ifndef GB_APP_VERSION
#define GB_APP_VERSION "development"
#endif
#include "controls.h"
#include "video_renderer.h"
#include "scheduler.h"
#include "pacer.h"
#include "font8x8.h"
#include "ui_font.h"
#include "ui_strings.h"
#include "pxa.h"
#include "pxa_audio.h"
#include "pxa_clock.h"
#include "pxa_fs_roots.h"
#include "../common/rom/pxa_rom_source.h"
#include "pxa_permission.h"
#include "pxa_surface.h"
#include "pxa_ui.h"
#include "../common/emulator/menu_focus.h"
#include "pxa_window.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

enum { T_MENU_THEME = 190 };
enum { T_SURFACE = 1, T_LAYER, T_ROOT, T_DIR, T_ENTRY, T_ROM, T_SAVE_READ,
       T_SAVE_WRITE, T_SAVE_REPLACE, T_AUDIO_PERMISSION, T_AUDIO_OPEN, T_AUDIO_GAIN,
       T_LIMITS, T_CONFIGURE, T_WINDOW, T_MKDIR, T_ASSET, T_STATS, T_MEDIA_PATH };
enum { MENU, LOADING, PLAYING, PAUSED, ERROR_SCREEN, STATES };
enum { PAUSE_TOP = 68, PAUSE_PITCH = 30, PAUSE_CHOICES = 8 };

static uint16_t *frames;
static void *frames_raw;
static unsigned frame_bytes;
static unsigned frame_pixels;
static unsigned surface_stride;
static int pending_buffer=-1;
static unsigned clear_frames=3;
static char sd_media_path[256];
static unsigned menu_focus,menu_repeat_mode,menu_repeat_controller;
static uint32_t menu_repeat_buttons;
static uint64_t menu_repeat_at;
static gb_pacer_t pacer;
static gb_controls_t controls;
static gb_video_renderer_t video_renderer;
static gb_audio_t audio_state;
static int16_t pcm_out[GB_AUDIO_OUT_MAX];
static uint64_t surface, media, directory, file, save_file, audio, audio_pending, permission;
static uint64_t frame_id, metric_at, cpu_us, n_frames, n_present, skipped;
static uint64_t paint_us, paint_frames, paint_pixels, audio_dropped;
static uint64_t tick_us, tick_count;
static uint64_t core_avg_us;
static uint64_t save_cookie;
static void request_exit(void);
static void exit_tick(void);
static unsigned width = 320, height = 240, mode = MENU, source = 0, page, count, selected;
static unsigned input_scale = 1;
static uint32_t input_density_q16 = 65536;
static unsigned reading_save, writing_save, save_pending, save_replace_pending, scan_pending;
static unsigned save_failed;
static unsigned rom_loaded, background, redraw = 1;
static unsigned display_divisor = 1;
static char rom_names[2][128][128], (*names)[128] = rom_names[0];
static unsigned source_count[2], preferred_source;
static char message[112];
static char save_path[40], save_tmp[48];
static uint8_t *rom_data, *save_data;
static size_t rom_size, rom_used, save_size, save_used;
static pxa_rom_file_t rom_file;
static unsigned rom_paged;
static uint32_t rom_stream_crc;
static uint32_t saved_crc, pending_save_crc;
static uint8_t pad[2], touches[16];
static uint8_t controller_previous[2], controller_connected[2];
static uint32_t controller_raw_previous[2];
static unsigned pause_selected, pause_top, exit_requested;
static pxa_i18n_t i18n;
static int audio_requested, audio_failed;
static unsigned audio_channels = 1;
static unsigned audio_rate;
static gb_audio_fifo_t audio_fifo;
static gb_schedule_t schedule;

static uint64_t micros(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t) t.tv_sec * 1000000u + t.tv_nsec / 1000u;
}

void gb_port_log(const char *format, ...) {
    char line[384];
    va_list ap;
    va_start(ap, format);
    vsnprintf(line, sizeof(line), format, ap);
    va_end(ap);
    size_t n = strlen(line);
    if (n && line[n - 1] == '\n') line[n - 1] = 0;
    pxa_log_write(2, line);
}

static void core_log(const char *line) { gb_port_log("GB CORE %s", line); }

static void fail(const char *s) {
    snprintf(message, sizeof(message), "%s", s);
    mode = ERROR_SCREEN;
    redraw = 1;
    gb_port_log("GB ERROR %s", s);
}

static void close_handle(uint64_t *h) {
    if (h == &audio) {gb_core_set_audio_enabled(false);}
    if (*h) pxa_close_handle(*h);
    *h = 0;
}

static void release_input(void) {
    memset(pad, 0, sizeof(pad));
    memset(touches, 0, sizeof(touches));
}

/* Surface pixels and the Host Canvas share the screen. Hide the last game
 * frame while the Host menu is active; change this only at transitions. */
static unsigned surface_visible;
static int surface_visibility(unsigned visible) {
    if (!surface || surface_visible == visible) return 1;
    uint8_t packet[160];
    uint32_t n;
    if (!pxa_surface_build_configure_layer(packet, sizeof(packet), T_LAYER, surface, 0, 0,
                                           width, height, 0, visible, &n) ||
        pxa_control_packet(packet, n)) return 0;
    surface_visible = visible;
    return 1;
}

#include "system_menu.h"

static uint32_t menu_zoom(void) { return menu_zoom_q16; }
static unsigned menu_width(void) { return ((uint64_t) menu_area.width << 16) / menu_zoom(); }
static unsigned menu_height(void) { return ((uint64_t) menu_area.height << 16) / menu_zoom(); }

static void rect(uint16_t *out, int x, int y, int w, int h, uint16_t color) {
    if (menu_commands) { menu_box(x, y, w, h, 0, color); return; }
    unsigned stride = menu_width(), rows = menu_height();
    for (int j = y; j < y + h && j < (int) rows; ++j)
        for (int i = x; i < x + w && i < (int) stride; ++i)
            if (j >= 0 && i >= 0) out[j * stride + i] = color;
}

static void text(uint16_t *out, int x, int y, const char *s, uint16_t color, unsigned max) {
    if (menu_commands) { menu_label(x, y, max * 8, 20, s, color, 0); return; }
    gb_ui_draw(out, menu_width(), menu_height(), x, y, s, color, max * 8);
}

#include "../common/emulator/menu_theme.h"

static void card(uint16_t *out, int x, int y, int w, int h, uint16_t color) {
    if (menu_commands) { menu_box(x, y, w, h, 4, color); return; }
    rect(out, x + 4, y, w - 8, h, color);
    rect(out, x + 2, y + 1, w - 4, h - 2, color);
    rect(out, x + 1, y + 2, w - 2, h - 4, color);
    rect(out, x, y + 4, w, h - 8, color);
}

static void button(uint16_t *out, int x, int y, int w, const char *label, uint16_t color) {
    card(out, x, y, w, 23, color);
    if (menu_commands) { menu_label(x + 5, y + 1, w - 10, 22, label, color == UI_ACCENT ? UI_ON_ACCENT : UI_INK, 1); return; }
    unsigned label_width = gb_ui_width(label);
    int left = label_width < (unsigned) w - 10 ? x + (w - (int) label_width) / 2 : x + 5;
    gb_ui_draw(out, menu_width(), menu_height(), left, y + 3, label,
               color == UI_ACCENT ? UI_ON_ACCENT : UI_INK, w - 10);
}

/* Renders the menu at its logical resolution, then scales it into the canvas
 * when the Host cannot draw the system font for us. */
static void expand_menu(uint16_t *out, const uint16_t *source) {
    unsigned w = menu_width(), h = menu_height();
    for (unsigned n = 0; n < frame_pixels; ++n) out[n] = UI_BG;
    for (unsigned y = 0; y < menu_area.height; ++y)
        for (unsigned x = 0; x < menu_area.width; ++x)
            out[(menu_area.y + y) * surface_stride + menu_area.x + x] =
                source[(y * h / menu_area.height) * w + x * w / menu_area.width];
}

static void format_number(pxa_i18n_message_id_t id, const char *name, unsigned value,
                           char *out, size_t bytes) {
    const pxa_i18n_argument_t arg = {.name = name, .name_size = (uint8_t) strlen(name),
                                     .type = PXA_I18N_ARGUMENT_U32, .value.u32 = value};
    pxa_i18n_format(&i18n, id, &arg, 1, out, bytes);
}

static int acquire(void) {
    if (!surface || pending_buffer>=0) return -1;
    uint8_t i = 255;
    int r = pxa_surface_acquire_buffer(surface, &i);
    if (r == PXA_STATUS_WOULD_BLOCK) return -1;
    if (r != 4 || i > 1) { fail("Surface acquire failed"); return -1; }
    return i;
}

static void present(unsigned i) {
    ++frame_id;
    int r = pxa_surface_present_buffer(surface, i, frame_id);
    if (r == PXA_STATUS_WOULD_BLOCK) {
        pending_buffer=(int)i;
    } else if (r == 16) {
        pending_buffer=-1;
        ++n_present;
    } else {
        fail("Surface submission failed");
    }
}

static void slots_open(void);
static void slots_tick(void);
static void paint_slots(uint16_t *out);
static unsigned list_rows(unsigned top,unsigned bottom,unsigned pitch);

static unsigned pause_rows(void) {
    unsigned n = (menu_height() - PAUSE_TOP - 18) / PAUSE_PITCH;
    return n ? n : 1;
}

static void scroll_bar(uint16_t *out, unsigned top, unsigned rows, unsigned total,
                       unsigned y, unsigned height) {
    if (total <= rows) return;
    rect(out, menu_width() - 18, y, 2, height, UI_EDGE);
    unsigned thumb = height * rows / total;
    if (thumb < 8) thumb = 8;
    rect(out, menu_width() - 18, y + (height - thumb) * top / (total - rows), 2, thumb, UI_ACCENT);
}

static void path_lines(uint16_t *out, int y, const char *path) {
    unsigned chars = (menu_width() - 64) / 9;
    if (chars < 12) chars = 12;
    if (chars > 80) chars = 80;
    char line[81];
    while (*path && y+16 <= (int)menu_height()-45) {
        unsigned n = strlen(path);
        if (n > chars) n = chars;
        while(n && ((unsigned char)path[n]&0xc0)==0x80)--n;
        if(!n)break;
        memcpy(line, path, n);
        if(path[n] && y+32>(int)menu_height()-45 && n>=3)memcpy(line+n-3,"...",3);
        line[n] = 0;
        if (menu_commands) menu_label(32, y, menu_width() - 64, 16, line, UI_MUTED, 0);
        else text(out, 32, y, line, UI_MUTED, chars);
        y += 16;
        path += n;
    }
}

static __attribute__((noinline, cold)) void paint_menu(uint16_t *out) {
    const unsigned w = menu_width(), h = menu_height();
    rect(out, 0, 0, w, h, UI_BG);
    card(out, 24, 9, 25, 19, UI_ACCENT);
    rect(out, 29, 17, 9, 3, UI_BG);
    rect(out, 32, 14, 3, 9, UI_BG);
    rect(out, 41, 14, 3, 3, UI_BG);
    rect(out, 39, 20, 3, 3, UI_BG);
    text(out, 57, 11, UI_TITLE, UI_INK, 24);
    if (mode == MENU) {
        unsigned visible = (h - 108) / 22;
        char position[24];
        snprintf(position, sizeof(position), "%u / %u", count ? selected + 1 : 0, count);
        unsigned tw = gb_ui_width(position);
        if (count) {
            if (menu_commands) menu_label(w - 100, 9, 76, 20, position, UI_MUTED, 2);
            else text(out, w - 24 - tw, 11, position, UI_MUTED, 12);
        }
        unsigned tabs = !!source_count[0] + !!source_count[1], slot = 0;
        unsigned tab = tabs ? (w - 48 - 4 * (tabs - 1)) / tabs : 0;
        for (unsigned n = 0; n < 2; ++n)
            if (source_count[n]) {
                button(out, 24 + slot * (tab + 4), 34, tab, n ? UI_SD : UI_FLASH,
                       source == n ? UI_ACCENT : UI_CARD);
                if(source==n && menu_focus==MENU_FOCUS_SOURCE)rect(out,24+slot*(tab+4),55,tab,2,UI_INK);
                ++slot;
            }
        for (unsigned row = 0; row < visible; ++row) {
            unsigned n = page + row;
            if (n >= count) break;
            unsigned y = 64 + row * 22;
            card(out, 24, y, w - 48, 20, n == selected && !menu_focus ? UI_FOCUS : UI_CARD);
            if (n == selected && !menu_focus) rect(out, 24, y + 5, 2, 10, UI_ACCENT);
            char number[8];
            snprintf(number, sizeof(number), "%02u", n + 1);
            text(out, 33, y + 2, number, n == selected ? UI_ACCENT : UI_MUTED, 3);
            if (menu_commands) menu_label(60, y, w - 108, 20, names[n], UI_INK, 0);
            else gb_ui_draw(out, w, h, 60, y + 2, names[n], UI_INK, w - 108);
            if (n == selected && !menu_focus) text(out, w - 39, y + 2, ">", UI_ACCENT, 1);
        }
        if (!count) {
            text(out, 32, 40, scan_pending ? UI_SCANNING : UI_EMPTY_HELP, UI_INK, 36);
            text(out, 32, 72, UI_FLASH_PATH, UI_ACCENT, 32);
            path_lines(out, 96, "roms/");
            text(out, 32, 112, UI_SD_PATH, UI_ACCENT, 32);
            path_lines(out, 134, sd_media_path[0] ? sd_media_path : UI_SD_QUERY);
        } else {
            scroll_bar(out, page, visible, count, 64, visible * 22 - 2);
        }
        rect(out, 24, h - 45, w - 48, 1, UI_EDGE);
        button(out, 24, h - 39, (w - 56) / 2, UI_REFRESH, menu_focus==1 ? UI_ACCENT : UI_CARD);
        button(out, 32 + (w - 56) / 2, h - 39, (w - 56) / 2, UI_EXIT, menu_focus==2 ? UI_ACCENT : UI_CARD);
    } else if (mode == STATES) {
        paint_slots(out);
    } else if (mode == PAUSED) {
        const char *status = save_failed ? UI_SAVE_ERROR :
            (save_pending || writing_save || save_replace_pending ? UI_SAVING : UI_PAUSED);
        text(out, 32, 36, status, save_failed ? UI_DANGER : UI_MUTED, 28);
        rect(out, 32, 58, w - 64, 1, UI_EDGE);
        char display[112], keys[112];
        format_number(PXA_MSG_UI_DISPLAY, "frames", display_divisor, display, sizeof(display));
        if (controls.controller_hidden) snprintf(keys, sizeof(keys), "%s", UI_KEYS_AUTO);
        else if (gb_controls_visible(&controls))
            format_number(PXA_MSG_UI_KEYS_ALPHA, "opacity", controls.alpha * 100 / 8, keys, sizeof(keys));
        else snprintf(keys, sizeof(keys), "%s", UI_KEYS_OFF);
        const char *labels[] = {UI_RESUME, UI_LIBRARY, UI_STATES,
                                audio ? UI_SOUND_ON : (audio_requested ? UI_SOUND_STARTING : (audio_failed ? UI_SOUND_FAILED : UI_SOUND_OFF)),
                                keys, display, video_renderer.fit ? UI_SCALE_FIT : UI_SCALE_NATIVE, UI_EXIT};
        for (unsigned visible = 0; visible < pause_rows() && pause_top + visible < PAUSE_CHOICES; ++visible) {
            unsigned row = pause_top + visible, y = PAUSE_TOP + visible * PAUSE_PITCH;
            card(out, 32, y, w - 64, 24, row == pause_selected ? UI_FOCUS : UI_CARD);
            if (row == pause_selected) rect(out, 32, y + 6, 2, 12, UI_ACCENT);
            if (menu_commands) menu_label(44, y + 1, w - 108, 23, labels[row], UI_INK, 0);
            else gb_ui_draw(out, w, h, 44, y + 4, labels[row], UI_INK, w - 108);
            text(out, w - 51, y + 4, ">", row == pause_selected ? UI_ACCENT : UI_MUTED, 1);
        }
        scroll_bar(out, pause_top, pause_rows(), PAUSE_CHOICES, PAUSE_TOP, pause_rows() * PAUSE_PITCH - 6);
    } else {
        card(out, 24, 66, w - 48, 104, UI_CARD);
        text(out, 36, 80, mode == LOADING ? UI_LOADING : UI_LOAD_ERROR, UI_ACCENT, 28);
        if (mode == ERROR_SCREEN) {
            text(out, 36, 111, strstr(message, "SD needs") ? UI_HOST_UNSUPPORTED : UI_ERROR_DETAIL, UI_INK, 28);
            text(out, 36, 137, UI_ERROR_RETRY, UI_MUTED, 28);
            button(out, 32, h - 54, w - 64, UI_BACK_LIBRARY, UI_FOCUS);
        }
    }
}

static int paint_system_menu(void) {
    if (!surface_visibility(0)) return 0;
    uint8_t data[8192], packet[1024];
    pxa_canvas_frame_t frame;
    pxa_canvas_begin(&frame, data, sizeof(data));
    pxa_canvas_rect_rgba(&frame, 0, 0, menu_origin(width), menu_origin(height), menu_rgba(UI_BG), 0, 0, 0);
    menu_commands = &frame;
    paint_menu(NULL);
    menu_commands = NULL;
    int ok = pxa_canvas_present_regions(2, &frame, &menu_generation, &menu_initialized,
                                        PXA_UI_EVENT_MASK_POINTER | PXA_UI_EVENT_MASK_CONTROLLER_STATE, NULL, 0, packet, sizeof(packet), NULL, 0);
    if (ok) redraw = 0;
    return ok;
}

static void paint(unsigned i, int game) {
    static unsigned control_signature[2];
    static uint8_t dirty_rows[GB_DISPLAY_MAX_HEIGHT];
    uint16_t *out = frames + (size_t) i * frame_pixels;
    if (game) {
        uint64_t began = micros();
        unsigned pressed=pad[0]|pad[1];for(unsigned n=0;n<16;++n)pressed|=touches[n];
        unsigned signature=pressed|(controls.alpha<<16)|(controls.controller_hidden<<24);
        unsigned force=(clear_frames&(1u<<i)) || control_signature[i]!=signature;
        if (game==1 && (clear_frames & (1u<<i))) {
            memset(out,0,frame_bytes);clear_frames &= ~(1u<<i);
        }
        if(game==1)paint_pixels+=gb_video_render_cached(&video_renderer,out,gb_core_video(),i,force,dirty_rows);
        else {memset(dirty_rows,1,height);paint_pixels+=video_renderer.view_width*video_renderer.view_height;}
        gb_display_rect_t view={video_renderer.view_x,video_renderer.view_y,video_renderer.view_width,video_renderer.view_height};
        gb_controls_clear_bars_rows(&controls,out,&view,dirty_rows);
        gb_controls_draw_rows(&controls,out,pressed,dirty_rows);
        control_signature[i]=signature;
        paint_us += micros() - began;
        ++paint_frames;
    } else {
        size_t bytes = (size_t) menu_width() * menu_height() * sizeof(uint16_t);
        uint16_t *menu = malloc(bytes);
        if (!menu) {
            fail("Menu allocation failed");
            for (unsigned n = 0; n < frame_pixels; ++n) out[n] = UI_BG;
            gb_ui_draw(out, surface_stride, height, menu_area.x, menu_area.y, UI_LOAD_ERROR, UI_INK, menu_area.width);
            present(i);
            redraw = 0;
            return;
        }
        paint_menu(menu);
        expand_menu(out, menu);
        free(menu);
    }
    present(i);
    redraw = 0;
}

static int has_game_extension(const char *name, size_t size) {
    /* Both the three letter .gb and the four letter .gbc are accepted. */
    if (size >= 4) {
        const char *s = name + size - 4;
        if (s[0] == '.' && tolower((unsigned char) s[1]) == 'g' &&
            tolower((unsigned char) s[2]) == 'b' && tolower((unsigned char) s[3]) == 'c')
            return 1;
    }
    if (size >= 3) {
        const char *s = name + size - 3;
        if (s[0] == '.' && tolower((unsigned char) s[1]) == 'g' &&
            tolower((unsigned char) s[2]) == 'b')
            return 1;
    }
    return 0;
}

static void library_scan_source(unsigned which) {
    source = which;
    names = rom_names[source];
    count = 0;
    /* The private folder holds the roms/ directory, the SD card holds the
     * media root the Host selected for this app. */
    int r = source ? pxa_fs_request_app_media(T_ROOT)
                   : pxa_fs_request_open(T_DIR, "roms", 4,
                                         PXA_FS_OPEN_READ | PXA_FS_OPEN_DIRECTORY);
    if (r) {
        source_count[source] = 0;
        if (!source) {
            library_scan_source(1);
        } else {
            scan_pending = 0;
            source = preferred_source;
            count = source_count[source];
            names = rom_names[source];
            redraw = 1;
        }
    }
}

static void library_scanned(void) {
    menu_focus=0;
    if (directory == media) directory = 0;
    else close_handle(&directory);
    for (unsigned i = 0; i < count; ++i)
        for (unsigned j = i + 1; j < count; ++j)
            if (strcmp(names[i], names[j]) > 0) {
                char tmp[128];
                memcpy(tmp, names[i], 128);
                memcpy(names[i], names[j], 128);
                memcpy(names[j], tmp, 128);
            }
    source_count[source] = count;
    if (!source) { library_scan_source(1); return; }
    source = source_count[preferred_source] ? preferred_source : (source_count[0] ? 0 : 1);
    count = source_count[source];
    names = rom_names[source];
    scan_pending = 0;
    page = selected = 0;
    redraw = 1;
    gb_port_log("GB LIBRARY flash=%u sd=%u", source_count[0], source_count[1]);
}

static void open_library(void) {
    if(rom_loaded && (save_pending || writing_save || save_replace_pending || save_failed)){redraw=1;return;}
    if (file || reading_save || scan_pending) return;
    if (directory == media) directory = 0;
    else close_handle(&directory);
    close_handle(&media);
    preferred_source = source > 1 ? 0 : source;
    source_count[0] = source_count[1] = 0;
    count = page = selected = 0;
    scan_pending = 1;menu_focus=0;menu_repeat_buttons=0;menu_repeat_at=0;
    mode = MENU;
    redraw = 1;
    library_scan_source(0);
}

static void start_save(void) {
    if (!rom_loaded || save_pending || writing_save || save_replace_pending) return;
    size_t size = gb_core_save_size();
    if (!size) return;
    uint8_t *copy = realloc(save_data, size);
    if (!copy) {
        save_failed = 1;
        redraw = 1;
        gb_port_log("GB SRAM snapshot allocation failed");
        return;
    }
    save_data = copy;
    if(!gb_core_save_export(save_data,size)){save_failed=1;redraw=1;return;}
    uint32_t crc=gb_port_crc32(0,save_data,size);
    if(crc==saved_crc)return;
    save_size = size;
    save_used = 0;
    pending_save_crc = crc;
    save_failed = 0;
    save_pending = 1;
    int r = pxa_fs_request_open(T_SAVE_WRITE, save_tmp, strlen(save_tmp),
                                PXA_FS_OPEN_WRITE | PXA_FS_OPEN_CREATE | PXA_FS_OPEN_TRUNCATE);
    if (r) {
        save_pending = 0;
        save_failed = 1;
        redraw = 1;
        gb_port_log("GB SRAM open rejected %d", r);
    }
}

static void game_ready(void) {
    gb_core_set_audio_enabled(audio!=0);
    reading_save = 0;
    rom_loaded = 1;
    if (!menu_dismiss()) { fail("Cannot dismiss menu"); return; }
    mode = PLAYING;
    release_input();
    gb_schedule_reset(&schedule);gb_pacer_reset(&pacer);core_avg_us=0;clear_frames=3;
    metric_at = micros();
    n_frames = n_present = skipped = cpu_us = paint_us = paint_frames = paint_pixels = audio_dropped = 0;
    tick_us = tick_count = 0;
    gb_audio_reset(&audio_state);
    if(audio)gb_audio_configure(&audio_state,audio_rate,audio_channels);
    gb_audio_fifo_clear(&audio_fifo);
    gb_port_log("GB READY name=%s model=%u hz_x10000=%u bytes=%u crc=%08x sram=%u",
                names[selected], gb_core_model(), (unsigned)(UINT64_C(10000)*GB_CLOCK_HZ/GB_FRAME_CYCLES), (unsigned) rom_size,
                gb_core_checksum(), (unsigned) gb_core_sram_size());
}

static void finish_rom(void) {
    const char *error;
    if(rom_paged){
        memset(&rom_file,0,sizeof(rom_file));rom_file.handle=file;file=0;
        rom_source_t source={&rom_file,pxa_rom_read,rom_size,rom_stream_crc};
        error=gb_core_load_source(&source,256u*1024u);
        free(rom_data);rom_data=NULL;
    }else {close_handle(&file);error=gb_core_load(rom_data,rom_size);}
    if(!error && gb_core_rom_cache()->error)error="Cartridge paging read failed";
    if (error) { fail(error); return; }
    snprintf(save_path, sizeof(save_path), "gnuboy-%08x.sav", gb_core_checksum());
    snprintf(save_tmp, sizeof(save_tmp), "%s.tmp", save_path);
    saved_crc = 0;
    if(!gb_core_save_size()){game_ready();return;}
    reading_save = 1;
    int r = pxa_fs_request_open(T_SAVE_READ, save_path, strlen(save_path), PXA_FS_OPEN_READ);
    if (r) game_ready();
}

static void select_rom(unsigned n) {
    if (n >= count || save_pending || writing_save || save_replace_pending) return;
    gb_core_close();
    close_handle(&rom_file.handle);
    rom_loaded = 0;
    free(rom_data);
    rom_data = NULL;
    rom_size = rom_used = 0;
    selected = n;
    mode = LOADING;
    redraw = 1;
    release_input();
    gb_schedule_reset(&schedule);
    char path[150];
    snprintf(path, sizeof(path), "roms/%s", names[n]);
    int r = source ? pxa_fs_request_open_at(T_ROM,media,names[n],strlen(names[n]),PXA_FS_OPEN_READ)
                   : pxa_fs_request_open(T_ROM,path,strlen(path),PXA_FS_OPEN_READ);
    if (r) fail("ROM open request failed");
}

static void audio_failure(const char *stage, int status) {
    close_handle(&audio);
    close_handle(&audio_pending);
    close_handle(&permission);
    gb_audio_fifo_clear(&audio_fifo);
    audio_requested = 0;
    audio_failed = 1;
    redraw = 1;
    gb_port_log("GB SOUND FAILED stage=%s status=%d", stage, status);
}


static void request_audio(void) {
    if (audio) {
        close_handle(&audio);
        close_handle(&permission);
        gb_audio_fifo_clear(&audio_fifo);
        audio_requested = audio_failed = 0;
        redraw = 1;
        gb_port_log("GB SOUND OFF");
        return;
    }
    if (audio_requested) return;
    audio_requested = 1;
    audio_failed = 0;
    redraw = 1;
    uint8_t packet[160];
    uint32_t size;
    if (!pxa_permission_build(packet, sizeof(packet), PXA_PERMISSION_ACQUIRE, T_AUDIO_PERMISSION,
                              "audio.playback", 14, (const uint8_t *) "media", 5, &size)) {
        audio_failure("permission-build", -1);
        return;
    }
    int status = pxa_control_packet(packet, size);
    if (status) audio_failure("permission-request", status);
    else gb_port_log("GB SOUND STARTING");
}

static void drain_audio(void) {
    if (!audio) return;
    /* The Host accepts at most 20 ms of PCM per write. */
    unsigned limit = (audio_rate ? audio_rate / 50u : 320u)*audio_channels;
    if (!limit) limit = 320u;
    if (limit > GB_AUDIO_OUT_MAX) limit = GB_AUDIO_OUT_MAX;
    for(unsigned attempt=0;attempt<4 && audio_fifo.count;++attempt) {
        unsigned samples;
        int16_t *pcm = gb_audio_fifo_peek(&audio_fifo, limit, &samples);
        int r = pxa_audio_write_pcm(audio, (uint8_t *) pcm, samples * 2);
        if (r == PXA_STATUS_WOULD_BLOCK || r == 0) break;
        if (r < 0 || (unsigned) r > samples * 2 || (r & 1)) {
            audio_dropped += audio_fifo.count;
            gb_audio_fifo_clear(&audio_fifo);
            audio_failure("pcm-write", r);
            break;
        }
        gb_audio_fifo_consume(&audio_fifo, r / 2);
    }
}

static void enqueue_audio(const int16_t *pcm, unsigned samples) {
    if (!audio) return;
    if(samples>GB_AUDIO_FIFO_SAMPLES-audio_fifo.count)drain_audio();
    if(!audio)return;
    audio_dropped += gb_audio_fifo_push(&audio_fifo, pcm, samples);
    /* LCD/core work may have freed sink capacity since io_tick. Bounded,
     * nonblocking writes also handle ring-wrap fragments and catch-up frames. */
    drain_audio();
}

static __attribute__((noinline)) void io_tick(void) {
    drain_audio();
    if(pending_buffer>=0)present((unsigned)pending_buffer);
    uint64_t started = micros();
    if (file && rom_data && !reading_save && mode == LOADING) {
        for (unsigned k = 0; k < 8 && micros() - started < 4000; ++k) {
            size_t left = rom_size - rom_used;
            if (left > 4096) left = 4096;
            int r = pxa_fs_read(file, rom_data + (rom_paged?0:rom_used), left);
            if (r == PXA_STATUS_WOULD_BLOCK) break;
            if (r <= 0 || (unsigned) r > left) { close_handle(&file); fail("ROM read failed or truncated"); break; }
            if(rom_paged)rom_stream_crc=rom_crc32(rom_stream_crc,rom_data,(unsigned)r);
            rom_used += r;
            if (rom_used == rom_size) { finish_rom(); break; }
        }
    }
    if (file && reading_save) {
        size_t size = gb_core_save_size();
        if (save_used < size) {
            unsigned chunk = size - save_used;
            if (chunk > 4096) chunk = 4096;
            int r = pxa_fs_read(file, save_data + save_used, chunk);
            if (r > 0 && (unsigned) r <= chunk) save_used += r;
            else if (r != PXA_STATUS_WOULD_BLOCK) {
                close_handle(&file);
                if(r==0 && gb_core_save_import(save_data,save_used)) {
                    saved_crc=gb_port_crc32(0,save_data,save_used);game_ready();
                } else {reading_save=0;fail("Cannot read saved progress; original file retained");}
            }
        }
        if (reading_save && save_used == size) {
            if(!gb_core_save_import(save_data,size)){close_handle(&file);reading_save=0;fail("Invalid saved progress");return;}
            saved_crc = gb_port_crc32(0, save_data, size);
            close_handle(&file);
            game_ready();
        }
    }
    if (writing_save && save_file) {
        unsigned chunk = save_size - save_used;
        if (chunk > 4096) chunk = 4096;
        int r = pxa_fs_write(save_file, save_data + save_used, chunk);
        if (r > 0 && (unsigned) r <= chunk) save_used += r;
        else if (r != PXA_STATUS_WOULD_BLOCK) {
            gb_port_log("GB SRAM write failed %d", r);
            close_handle(&save_file);
            writing_save = 0;
            save_failed = 1;
            redraw = 1;
        }
        if (writing_save && save_used == save_size) {
            close_handle(&save_file);
            writing_save = 0;
            uint8_t packet[200];
            uint32_t size;
            if (pxa_fs_build_replace(packet, sizeof(packet), T_SAVE_REPLACE, save_tmp, strlen(save_tmp),
                                     save_path, strlen(save_path), &size) && !pxa_control_packet(packet, size))
                save_replace_pending = 1;
            else {save_failed=1;redraw=1;gb_port_log("GB SRAM replace rejected");}
        }
    }
}

static void menu_controller_repeat(uint64_t now);

static __attribute__((noinline)) void tick(uint64_t now) {
    io_tick();
    exit_tick();
    if (background) return;
    if (mode != PLAYING) {
        slots_tick();
        menu_controller_repeat(now);
        if (redraw) {
            if (system_menu_supported) paint_system_menu();
            else { int i = acquire(); if (i >= 0) paint(i, 0); }
        }
        return;
    }
    now = micros();
    gb_schedule_tick(&schedule, now, gb_core_rate());
    uint64_t started = micros();
    for (unsigned step = 0; step < 4 && gb_schedule_due(&schedule); ++step) {
        uint64_t frame_started = micros();
        gb_pacer_tick(&pacer,frame_started,schedule.debt/GB_FRAME_CYCLES,60,display_divisor);
        int draw_due=gb_pacer_due(&pacer,frame_started);
        /* The drawn frame includes PPU work. Let the pacer measure skipped
         * frames separately before deciding that simulation is too slow. */
        /* The LCD can finish its previous DMA while this frame emulates.
         * Acquire early only for native-size direct output. A scaled frame
         * already has an internal PPU target, so acquire after emulation. */
        int native_view=video_renderer.view_width==GB_VIDEO_WIDTH && video_renderer.view_height==GB_VIDEO_HEIGHT;
        int index=draw_due && native_view?acquire():-1;
        uint8_t p = pad[0] | pad[1];
        for (unsigned i = 0; i < 16; ++i) p |= touches[i];
        /* The hardware cannot report opposing directions at once. */
        if ((p & (GB_PAD_LEFT | GB_PAD_RIGHT)) == (GB_PAD_LEFT | GB_PAD_RIGHT)) p &= ~GB_PAD_LEFT;
        if ((p & (GB_PAD_UP | GB_PAD_DOWN)) == (GB_PAD_UP | GB_PAD_DOWN)) p &= ~GB_PAD_UP;
        uint64_t t = micros();
        int direct=index>=0 && native_view;
        if(direct) {
            uint16_t *out=frames+(size_t)index*frame_bytes/2;
            if(clear_frames&(1u<<index)) {
                memset(out,0,frame_bytes);clear_frames&=~(1u<<index);
            }
            gb_core_frame_to(p,out+video_renderer.view_y*surface_stride+video_renderer.view_x,surface_stride);
        } else gb_core_frame_render(p,draw_due);
        if(gb_core_rom_cache()->error){
            if(index>=0)present((unsigned)index);
            rom_loaded=0;close_handle(&audio);fail("ROM read failed; check storage and reopen game");break;
        }
        uint64_t frame_us = micros() - t;
        cpu_us += frame_us;
        core_avg_us = core_avg_us ? (core_avg_us * 7u + frame_us) / 8u : frame_us;
        ++n_frames;
        gb_schedule_step(&schedule);
        if(draw_due && index<0)index=acquire();
        if (index >= 0) { paint(index, direct?2:1);gb_pacer_drew(&pacer,micros()); }
        else ++skipped;
        if (audio) {
            GB_PROFILE_SCOPE(GB_PROFILE_OUTPUT);
            size_t frames = 0;
            const int16_t *pcm = gb_core_audio(&frames);
            unsigned produced = gb_audio_convert(&audio_state, pcm, frames, pcm_out, GB_AUDIO_OUT_MAX/audio_channels);
            enqueue_audio(pcm_out, produced * audio_channels);
        }
        gb_pacer_observe(&pacer,index>=0,micros()-frame_started);
        if (micros() - started >= 24000) break;
    }
    uint64_t wall_now = micros();
    tick_us += wall_now - started;
    ++tick_count;
    if (wall_now >= metric_at + 5000000) {
        uint64_t elapsed = wall_now - metric_at;
        if(rom_paged){const rom_cache_t *cache=gb_core_rom_cache();
            gb_port_log("GB ROM cache_bytes=%u hits=%llu misses=%llu evictions=%llu io_bytes=%llu io_us=%llu max_io_us=%llu",
                cache->count*cache->page_size,(unsigned long long)cache->hits,(unsigned long long)cache->misses,
                (unsigned long long)cache->evictions,(unsigned long long)rom_file.read_bytes,
                (unsigned long long)rom_file.read_us,(unsigned long long)rom_file.max_read_us);
        }
        gb_port_log("GB PERF version=" GB_APP_VERSION " rom_crc=%08x wall_us=%llu frames=%llu elapsed_us=%llu sim_x100=%llu submit_x100=%llu cpu_avg_us=%llu core_avg_us=%llu paint_avg_us=%llu debt=%llu sound=%u",
                    gb_core_checksum(), (unsigned long long) wall_now, (unsigned long long) n_frames,
                    (unsigned long long) elapsed, (unsigned long long) (n_frames * 100000000 / elapsed),
                    (unsigned long long) (n_present * 100000000 / elapsed),
                    (unsigned long long) (cpu_us / (n_frames ? n_frames : 1)),
                    (unsigned long long) core_avg_us,
                    (unsigned long long) (paint_frames ? paint_us / paint_frames : 0),
                    (unsigned long long)(schedule.debt/(UINT64_C(1000000)*GB_FRAME_CYCLES)),audio?1u:0u);
        gb_port_log("GB STATS wall_us=%llu skipped=%llu audio_drop=%llu model=%u keys_auto=%u fit=%u draw_us=%u skip_us=%u interval_q16=%u feedback=%u budget_pct=95",
                    (unsigned long long)wall_now, (unsigned long long)skipped,
                    (unsigned long long)audio_dropped, gb_core_model(), controls.controller_hidden,
                    video_renderer.fit,pacer.draw_us,pacer.skip_us,pacer.interval_q16,pacer.feedback);
        if(surface) {
            uint8_t packet[80];uint32_t size;
            if(pxa_surface_build_query_state(packet,sizeof(packet),T_STATS,surface,&size))pxa_control_packet(packet,size);
        }
        metric_at = wall_now;
        n_frames = n_present = skipped = cpu_us = paint_us = paint_frames = paint_pixels = audio_dropped = 0;
        tick_us = tick_count = 0;
    }
}

static void pause_game(void) {
    mode = PAUSED;
    pause_selected = pause_top = 0;
    redraw = 1;
    release_input();
    gb_schedule_reset(&schedule);
    start_save();
    gb_audio_fifo_clear(&audio_fifo);
    if (audio) pxa_audio_flush(++save_cookie + 1000, audio);
}

static void pause_action(unsigned choice) {
    if (choice == 0) {
        if (!menu_dismiss()) return;
        clear_frames=3;
        mode = PLAYING;
        gb_schedule_reset(&schedule);
    } else if (choice == 1) {
        open_library();
    } else if (choice == 2) {
        slots_open();
    } else if (choice == 3) {
        request_audio();
    } else if (choice == 4) {
        gb_controls_cycle(&controls);clear_frames=3;
        release_input();
        redraw = 1;
    } else if (choice == 5) {
        display_divisor=display_divisor%4+1;
        redraw=1;
    } else if (choice == 6) {
        gb_video_set_fit(&video_renderer,menu_area,!video_renderer.fit);clear_frames=3;
        redraw=1;
    } else if (choice == 7) {
        request_exit();
    }
}

#include "slots.h"

static void request_exit(void) {
    if(slot_phase)return;
    exit_requested = 1;
    release_input();
    start_save();
    if (mode == PLAYING) pause_game();
    redraw = 1;
}

static void exit_tick(void) {
    if (!exit_requested || save_pending || writing_save || save_replace_pending || slot_phase) return;
    exit_requested = 0;
    if (save_failed) { redraw = 1; return; }
    exit_requested = 2;
    if (pxa_control_call(PXA_WINDOW_SERVICE, 4, 0, NULL, 0)) {
        exit_requested = 0;
        pxa_window_show_toast(3000, UI_EXIT_BACK, strlen(UI_EXIT_BACK));
    }
}

#include "menu_controller.h"

#include "../common/emulator/menu_list_pointer.h"

static uint16_t map_buttons(uint32_t b) {
    uint16_t pad = 0;
    if (b & PXA_CONTROLLER_RIGHT) pad |= GB_PAD_RIGHT;
    if (b & PXA_CONTROLLER_LEFT) pad |= GB_PAD_LEFT;
    if (b & PXA_CONTROLLER_UP) pad |= GB_PAD_UP;
    if (b & PXA_CONTROLLER_DOWN) pad |= GB_PAD_DOWN;
    if (b & PXA_CONTROLLER_A) pad |= GB_PAD_A;
    if (b & PXA_CONTROLLER_B) pad |= GB_PAD_B;
    if (b & PXA_CONTROLLER_SELECT) pad |= GB_PAD_SELECT;
    if (b & PXA_CONTROLLER_START) pad |= GB_PAD_START;
    return pad;
}

static void pointer(const pxa_ui_pointer_data_t *p) {
    if (p->pointer_id >= 16 || background) return;
    touches[p->pointer_id] = 0;
    menu_repeat_buttons=0;menu_repeat_at=0;
    if (p->phase == PXA_POINTER_CANCEL) { drag_active = 0; return; }
    /* UI pointers are dp, while the Surface extents are pixels. Undo the UI
     * density before converting panel pixels to our smaller canvas. */
    int x = (int) ((int64_t) p->x * input_density_q16 / (65536 * (int64_t) input_scale));
    int y = (int) ((int64_t) p->y * input_density_q16 / (65536 * (int64_t) input_scale));
    if (mode == PLAYING) {
        if (p->phase == PXA_POINTER_UP) return;
        if(controls.controller_hidden && p->phase==PXA_POINTER_DOWN){controls.controller_hidden=0;clear_frames=3;}
        int bit = gb_controls_hit(&controls, x, y);
        if (bit < 0) {
            if (p->phase == PXA_POINTER_DOWN) pause_game();
            return;
        }
        touches[p->pointer_id] = (uint8_t) bit;
        return;
    }
    if (x < (int) menu_area.x || y < (int) menu_area.y ||
        x >= (int) (menu_area.x + menu_area.width) || y >= (int) (menu_area.y + menu_area.height)) return;
    x -= menu_area.x;
    y -= menu_area.y;
    x = (int) (((int64_t) x << 16) / menu_zoom());
    y = (int) (((int64_t) y << 16) / menu_zoom());
    if (mode == MENU || mode == PAUSED || mode == STATES) {
        list_pointer(p, x, y);
        if (p->phase != PXA_POINTER_DOWN) return;
    }
    if (p->phase != PXA_POINTER_DOWN) return;
    if (mode == ERROR_SCREEN) { open_library(); return; }
    if (mode == PAUSED) return;
    if (mode == STATES) {
        if(!slot_phase && y>=(int)menu_height()-39 && y<(int)menu_height()-16 && x>=24 && x<(int)menu_width()-24) {
            if(x<24+(int)(menu_width()-48)/3)slot_new();else if(x<24+2*(int)(menu_width()-48)/3)slot_delete();else{slot_delete_pending=0;mode=PAUSED;redraw=1;}
        }
        return;
    }
    if (mode != MENU || scan_pending) return;
    if (y >= 34 && y < 57 && x >= 24 && x < (int) menu_width() - 24) {
        unsigned tabs = !!source_count[0] + !!source_count[1];
        if (!tabs) return;
        unsigned tab = (menu_width() - 48 - 4 * (tabs - 1)) / tabs;
        unsigned slot = (x - 24) / (tab + 4);
        unsigned chosen = source_count[0] ? (slot ? 1 : 0) : 1;
        if (!source_count[chosen]) return;
        menu_focus=MENU_FOCUS_LIST;
        source = chosen;
        count = source_count[source];
        names = rom_names[source];
        page = selected = 0;
        redraw = 1;
        return;
    }
    if (y >= (int) menu_height() - 39 && y < (int) menu_height() - 16 && x >= 24 && x < (int) menu_width() - 24) {
        if (x < (int) menu_width() / 2) open_library();
        else request_exit();
    }
}

static void update_locale(void) {
    /* Match the NES game overlay. System i18n/fonts remain on cold menus. */
    gb_controls_set_labels(&controls,"SELECT","START","MENU");clear_frames=3;
    redraw = 1;
    gb_port_log("GB LOCALE %s", i18n.locale);
}

int32_t pxa_app_start(const uint8_t *config, uint32_t size) {
    pxa_i18n_init_from_start_config(&i18n, &pxa_app_i18n_bundle, config, size);
    pxa_ui_environment_t env = {0};
    if (pxa_ui_parse_start_environment(config, size, &env)) {
        width = env.width;
        height = env.height;
        input_density_q16 = env.density_q16;
    }
    menu_environment(&env);
    input_scale = gb_display_canvas(&width, &height);
    if (!input_scale) return PXA_STATUS_INVALID_ARGUMENT;
    gb_port_log("GB DISPLAY canvas=%ux%u input_scale=%u density_q16=%u", width, height, input_scale, input_density_q16);
    pxa_window_fullscreen();
    pxa_window_request_snapshot(T_WINDOW);
    (void)pxa_ui_theme_get(T_MENU_THEME);
    uint8_t packet[512];
    uint32_t n;
    pxa_ui_transaction_t tx = {0};
    if (!pxa_ui_transaction_begin(&tx, 1, PXA_UI_TRANSACTION_REPLACE_SURFACE, packet, sizeof(packet)) ||
        !pxa_ui_create(&tx, 1, 0, 0, PXA_UI_NODE_ROOT) ||
        !pxa_ui_set_rgba(&tx, 1, PXA_UI_PROPERTY_BACKGROUND, 0) ||
        !pxa_ui_set_event_mask(&tx, 1, PXA_UI_EVENT_MASK_POINTER | PXA_UI_EVENT_MASK_CONTROLLER_STATE) ||
        !pxa_ui_transaction_commit(&tx)) return PXA_STATUS_INTERNAL;
    gb_controls_init(&controls, width, height);
    controls.safe = menu_area;
    update_locale();
    gb_video_init(&video_renderer, width, height, menu_area);
    pxa_surface_desc_t desc = {.width = (uint16_t) width, .height = (uint16_t) height,
                               .format = PXA_SURFACE_RGB565, .buffer_count = 2,
                               .flags = PXA_SURFACE_GUEST_MAPPED | PXA_SURFACE_PREFER_DIRECT_SCANOUT};
    if (!pxa_surface_build_create(packet, sizeof(packet), T_SURFACE, desc, &n)) return PXA_STATUS_INTERNAL;
    int r = pxa_control_packet(packet, n);
    if (r) return r;
    r = pxa_clock_set_period(16);
    if (r) return r;
    pxa_clock_limits(T_LIMITS);
    pxa_fs_request_path(PXA_FS_MAKE_DIRECTORY, T_MKDIR, "roms", 4);
    /* The media path query needs FS 0.4. Older Hosts still expose the media
     * root, so the query stays optional. */
    if (pxa_fs_request_app_media_path(T_MEDIA_PATH)) {
        gb_port_log("GB SD media path unavailable on this Host");
    }
    gb_core_set_log_sink(core_log);
    open_library();
    return 0;
}

void pxa_app_stop(uint32_t reason) {
    (void) reason;
    gb_core_close();
    close_handle(&rom_file.handle);
    free(rom_data);
    free(save_data);
    free(slot_data);slot_data=NULL;slot_dir=slot_file=0;slot_phase=slot_open_pending=0;
    free(frames_raw);
    frames = NULL;
    frames_raw = NULL;
    rom_data = save_data = NULL;
}

int32_t pxa_app_on_event(const uint8_t *data, uint32_t size) {
    pxa_event_t e;
    if (!pxa_parse_event(data, size, &e)) return PXA_EVENT_UNHANDLED;
    int locale_changed = pxa_i18n_handle_event(&i18n, &e);
    if (locale_changed) { if (locale_changed == 1) update_locale(); return PXA_EVENT_HANDLED; }

    uint64_t now;
    if (pxa_clock_parse_tick(&e, &now)) { tick(now); return PXA_EVENT_HANDLED; }
    if (e.token == T_STATS && e.service == PXA_SURFACE_SERVICE) {
        pxa_surface_state_t state;
        if (pxa_surface_parse_state(&e, T_STATS, &state) && !state.status)
            gb_port_log("GB DISPLAY version=" GB_APP_VERSION " wall_us=%llu submitted=%llu consumed=%llu replaced=%llu free=%u",
                        (unsigned long long) micros(), (unsigned long long) state.submitted_frames,
                        (unsigned long long) state.presented_frames,
                        (unsigned long long) state.replaced_frames, state.free_buffers);
        return PXA_EVENT_HANDLED;
    }
    pxa_ui_environment_t env;
    if (pxa_ui_parse_environment_event(&e, &env)) {
        input_density_q16 = env.density_q16;
        menu_environment(&env);
        release_input();
        controls.safe = menu_area;
        update_locale();
        unsigned fit=video_renderer.fit;
        gb_video_init(&video_renderer, width, height, menu_area);
        gb_video_set_fit(&video_renderer,menu_area,fit);
        gb_video_set_stride(&video_renderer,surface_stride?surface_stride:width);
        clear_frames=3;
        redraw = 1;
        return PXA_EVENT_HANDLED;
    }
    pxa_ui_theme_t theme;
    if(pxa_ui_parse_theme_event(&e,&theme)) {menu_theme_apply(&theme);return PXA_EVENT_HANDLED;}
    if(slots_event(&e))return PXA_EVENT_HANDLED;
    pxa_ui_pointer_data_t p;
    if (pxa_ui_parse_pointer(&e, &p)) { pointer(&p); return PXA_EVENT_HANDLED; }
    pxa_ui_controller_data_t c;
    if (pxa_ui_parse_controller(&e, &c)) {
        if (c.controller < 2) {
            uint16_t b = c.connected ? map_buttons(c.buttons) : 0;
            uint16_t previous = controller_previous[c.controller];
            uint32_t pressed = c.connected ? c.buttons & ~controller_raw_previous[c.controller] : 0;
            controller_raw_previous[c.controller] = c.connected ? c.buttons : 0;
            controller_previous[c.controller] = b;
            controller_connected[c.controller] = c.connected;
            if (background) { pad[c.controller] = 0; return PXA_EVENT_HANDLED; }
            if(c.connected && c.buttons && !controls.controller_hidden) {
                controls.controller_hidden=1;memset(touches,0,sizeof(touches));clear_frames=3;
            } else if(!controller_connected[0] && !controller_connected[1] && controls.controller_hidden) {
                controls.controller_hidden=0;clear_frames=3;
            }

            if ((pressed & PXA_CONTROLLER_X) && (mode == PLAYING || mode == PAUSED)) {
                if (mode == PLAYING) pause_game(); else pause_action(0);
                return PXA_EVENT_HANDLED;
            }
            if ((b & (GB_PAD_SELECT | GB_PAD_START)) == (GB_PAD_SELECT | GB_PAD_START) &&
                (previous & (GB_PAD_SELECT | GB_PAD_START)) != (GB_PAD_SELECT | GB_PAD_START) && mode == PLAYING) {
                pause_game();
                return PXA_EVENT_HANDLED;
            }
            if(mode!=PLAYING) {
                menu_controller_event(c.controller,pressed,c.connected?c.buttons:0);
                pad[c.controller]=0;
                return PXA_EVENT_HANDLED;
            }
            pad[c.controller] = background ? 0 : b;
        }
        return PXA_EVENT_HANDLED;
    }
    uint8_t life;
    if (pxa_lifecycle_parse(&e, &life)) {
        background = life == PXA_SYSTEM_LIFECYCLE_BACKGROUND;
        release_input();menu_repeat_buttons=0;menu_repeat_at=0;
        gb_schedule_reset(&schedule);
        if (background) {
            start_save();
            gb_audio_fifo_clear(&audio_fifo);
            if (audio) pxa_audio_flush(++save_cookie + 1000, audio);
        } else redraw = 1;
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_WINDOW_SERVICE && e.opcode == PXA_WINDOW_BACK_REQUESTED) {
        if (exit_requested == 2) { exit_requested = 0; return 0; }
        if(slot_phase)return PXA_EVENT_HANDLED;
        if(mode==STATES){mode=PAUSED;redraw=1;return PXA_EVENT_HANDLED;}
        if (mode == PLAYING) pause_game();
        else if (mode == PAUSED) pause_action(0);
        else if (mode == MENU) { if (save_pending || writing_save || save_replace_pending) request_exit(); else return 0; }
        else open_library();
        return PXA_EVENT_HANDLED;
    }
    if (e.token == T_SURFACE && e.service == PXA_SURFACE_SERVICE) {
        pxa_surface_create_result_t r;
        if (!pxa_surface_parse_create(&e, T_SURFACE, &r) || r.status) return PXA_STATUS_INTERNAL;
        surface = r.handle;
        /* The Host pads the row stride, so the buffers are allocated once the
         * negotiated geometry is known. */
        surface_stride = r.stride_bytes / sizeof(uint16_t);
        if (!surface_stride) surface_stride = width;
        if((r.stride_bytes&1) || surface_stride<width)return PXA_STATUS_PROTOCOL_ERROR;
        frame_pixels = surface_stride * (unsigned) height;
        frame_bytes = frame_pixels * sizeof(uint16_t);
        free(frames_raw);
        frames_raw = malloc((size_t) frame_bytes * 2 + 64);
        if (!frames_raw) return PXA_STATUS_INTERNAL;
        frames = (uint16_t *) (((uintptr_t) frames_raw + 63) & ~(uintptr_t) 63);
        memset(frames, 0, (size_t) frame_bytes * 2);
        clear_frames = 3;
        gb_video_set_stride(&video_renderer, surface_stride);
        controls.stride = surface_stride;
        gb_port_log("GB SURFACE stride=%u pixels frame_bytes=%u", surface_stride, frame_bytes);
        int result = pxa_surface_register_buffers(surface, frames, frame_bytes, 2);
        if (result != (int) frame_bytes * 2) return PXA_STATUS_INTERNAL;
        unsigned visible = mode == PLAYING || !system_menu_supported;
        surface_visible = !visible;
        if (!surface_visibility(visible)) return PXA_STATUS_INTERNAL;
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_CLOCK_SERVICE && e.token == T_CONFIGURE) {
        pxa_clock_configure_result_t r;
        if (pxa_clock_parse_configure(&e, T_CONFIGURE, 8000, &r) && !r.status) display_divisor = 1;
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_CLOCK_SERVICE && e.token == T_LIMITS) {
        pxa_clock_limits_result_t r;
        if (pxa_clock_parse_limits(&e, T_LIMITS, &r) && !r.status && pxa_clock_period_supported(&r.limits, 8000))
            pxa_clock_configure(T_CONFIGURE, 8000);
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_FS_SERVICE) {
        if(e.token==T_MEDIA_PATH) {
            if(pxa_fs_parse_app_media_path(&e,T_MEDIA_PATH,sd_media_path,sizeof(sd_media_path))) {
                gb_port_log("GB SD media=%s",sd_media_path);redraw=1;
            }
            return PXA_EVENT_HANDLED;
        }
        if (e.token == T_ROOT) {
            pxa_fs_root_result_t r;
            if (pxa_fs_parse_root(&e, T_ROOT, &r) && !r.status) {
                media = r.handle;
                if (source) { directory = media; pxa_fs_request_read_directory(T_ENTRY, directory); }
            } else { count = 0; library_scanned(); }
            return PXA_EVENT_HANDLED;
        }
        if (e.token == T_DIR) {
            int ok = 0;
            if (e.opcode == PXA_FS_OPEN_AT) {
                pxa_fs_at_result_t r;
                if (pxa_fs_parse_open_at(&e, T_DIR, &r) && !r.status) { directory = r.handle; ok = 1; }
            } else {
                pxa_fs_open_result_t r;
                if (pxa_fs_parse_open(&e, T_DIR, &r) && !r.status) { directory = r.handle; ok = 1; }
            }
            if (ok) pxa_fs_request_read_directory(T_ENTRY, directory);
            else { count = 0; library_scanned(); }
            return PXA_EVENT_HANDLED;
        }
        if (e.token == T_ENTRY) {
            pxa_fs_directory_result_t r;
            if (!pxa_fs_parse_directory(&e, T_ENTRY, &r) || r.status || r.end || count == 128) {
                library_scanned();
            } else {
                if (r.kind == PXA_FS_KIND_REGULAR && r.name_size >= 4 && r.name_size < 128 &&
                    has_game_extension((const char *) r.name, r.name_size)) {
                    memcpy(names[count], r.name, r.name_size);
                    names[count++][r.name_size] = 0;
                }
                pxa_fs_request_read_directory(T_ENTRY, directory);
            }
            return PXA_EVENT_HANDLED;
        }
        if (e.token == T_ROM) {
            if (e.opcode == PXA_FS_OPEN_AT) {
                pxa_fs_at_result_t r;
                if (!pxa_fs_parse_open_at(&e, T_ROM, &r) || r.status) { fail("Cannot open SD ROM"); return PXA_EVENT_HANDLED; }
                file = r.handle;
                rom_size = r.size;
            } else {
                pxa_fs_open_result_t r;
                if (!pxa_fs_parse_open(&e, T_ROM, &r) || r.status) { fail("Cannot open flash ROM"); return PXA_EVENT_HANDLED; }
                file = r.handle;
                uint8_t packet[80];
                uint32_t n;
                if (pxa_fs_build_seek(packet, sizeof(packet), T_ROM + 100, file, 0, PXA_FS_SEEK_END, &n)) pxa_control_packet(packet, n);
                return PXA_EVENT_HANDLED;
            }
        }
        if (e.token == T_ROM + 100) {
            pxa_fs_seek_result_t r;
            if (!pxa_fs_parse_seek(&e, T_ROM + 100, &r) || r.status) { close_handle(&file); fail("Cannot determine ROM size"); return PXA_EVENT_HANDLED; }
            rom_size = r.position;
            pxa_fs_request_seek(T_ROM + 101, file, 0, PXA_FS_SEEK_START);
            return PXA_EVENT_HANDLED;
        }
        if (e.token == T_ROM + 101) {
            pxa_fs_seek_result_t r;
            if (!pxa_fs_parse_seek(&e, T_ROM + 101, &r) || r.status) { close_handle(&file); fail("Cannot seek ROM"); return PXA_EVENT_HANDLED; }
        }
        if (e.token == T_ROM || e.token == T_ROM + 101) {
            if (rom_size < 0x150 || rom_size > GB_ROM_LIMIT) { close_handle(&file); fail("ROM size invalid (max 8 MiB)"); return PXA_EVENT_HANDLED; }
            rom_paged=0;rom_stream_crc=0;
            void *reserve=malloc(512u*1024u);
            rom_data=reserve?malloc(rom_size):NULL;
            free(reserve);
            if(!rom_data){rom_paged=1;rom_data=malloc(4096);}
            if (!rom_data) { close_handle(&file); fail("Not enough memory for ROM"); }
            return PXA_EVENT_HANDLED;
        }
        if (e.token == T_SAVE_READ) {
            pxa_fs_open_result_t r={0};
            save_used = 0;
            if (pxa_fs_parse_open(&e, T_SAVE_READ, &r) && !r.status) {
                size_t n = gb_core_save_size();
                uint8_t *p = n ? realloc(save_data, n) : NULL;
                if (!p) {pxa_close_handle(r.handle);reading_save=0;fail("Cannot allocate saved progress buffer");}
                else { save_data = p; file = r.handle; }
            } else if(r.status==PXA_STATUS_NOT_FOUND)game_ready();
            else {reading_save=0;fail("Cannot open saved progress; original file retained");}
            return PXA_EVENT_HANDLED;
        }
        if (e.token == T_SAVE_WRITE) {
            pxa_fs_open_result_t r;
            save_pending = 0;
            if (pxa_fs_parse_open(&e, T_SAVE_WRITE, &r) && !r.status) { save_file = r.handle; writing_save = 1; save_used = 0; }
            else { save_failed = 1; redraw = 1; gb_port_log("GB SRAM open failed"); }
            return PXA_EVENT_HANDLED;
        }
        if (e.token == T_SAVE_REPLACE) {
            int32_t status;
            save_replace_pending = 0;
            if (pxa_fs_parse_status(&e, T_SAVE_REPLACE, PXA_FS_REPLACE, &status) && !status) {
                saved_crc = pending_save_crc;
                redraw = 1;
                gb_port_log("GB SRAM SAVED bytes=%u crc=%08x", (unsigned) save_size, saved_crc);
            } else { save_failed = 1; redraw = 1; gb_port_log("GB SRAM atomic replace failed"); }
            return PXA_EVENT_HANDLED;
        }
    }
    pxa_permission_revoked_t revoked;
    if (pxa_permission_parse_revoked(&e, &revoked) && revoked.name_size == 14 &&
        !memcmp(revoked.name, "audio.playback", 14)) {
        audio_failure("permission-revoked", PXA_STATUS_DENIED);
        return PXA_EVENT_HANDLED;
    }
    if (e.token == T_AUDIO_PERMISSION && e.service == PXA_PERMISSION_SERVICE) {
        pxa_permission_acquire_result_t r={0};
        if (!pxa_permission_parse_acquire(&e, T_AUDIO_PERMISSION, &r) || r.status)
            audio_failure("permission", r.status ? r.status : -1);
        else if (!audio_requested) pxa_close_handle(r.handle);
        else {
            permission = r.handle;
            /* The media session is the only audio path every Host implements.
             * The device also offers a PCM stream, but the desktop simulator
             * does not, so the app resamples to the media format itself. */
            int status = pxa_audio_open_media(T_AUDIO_OPEN, permission);
            if (status) audio_failure("open-request", status);
        }
    }
    if (e.token == T_AUDIO_OPEN && e.service == PXA_AUDIO_SERVICE) {
        pxa_audio_open_result_t r={0};
        int parsed = pxa_audio_parse_open(&e, T_AUDIO_OPEN, &r);
        if(!audio_requested) {
            if(parsed && !r.status)pxa_close_handle(r.handle);
        } else if (!parsed || r.status || r.channels < 1 || r.channels > 2 ||
                   r.sample_rate < 8000 || r.sample_rate > 48000) {
            if(parsed && !r.status)pxa_close_handle(r.handle);
            audio_failure("open-format", parsed && r.status ? r.status : PXA_STATUS_PROTOCOL_ERROR);
        }
        else {
            /* The media session only accepts PCM once a graph is committed. */
            audio_pending = r.handle;
            audio_channels = r.channels;
            audio_rate = r.sample_rate;
            gb_audio_configure(&audio_state, r.sample_rate, r.channels);
            int status = pxa_audio_commit_gain(T_AUDIO_GAIN, audio_pending, -6 * 256);
            if (status) {
                close_handle(&audio_pending);
                audio_failure("gain-request", status);
            }
        }
    }
    if (e.token == T_AUDIO_GAIN && e.service == PXA_AUDIO_SERVICE) {
        int32_t status = 0;
        if (pxa_audio_parse_status(&e, T_AUDIO_GAIN, PXA_AUDIO_COMMIT_GRAPH, &status) &&
            !status && audio_pending && audio_requested) {
            audio = audio_pending;
            audio_pending = 0;
            gb_core_set_audio_enabled(true);
            gb_audio_start(&audio_state,audio_rate,audio_channels);
            gb_audio_fifo_clear(&audio_fifo);
            redraw = 1;
            gb_port_log("GB SOUND ON rate=%u channels=%u gain_db=-6", audio_rate, audio_channels);
        } else {
            close_handle(&audio_pending);
            audio_failure("gain", status ? status : -1);
        }
    }
    return PXA_EVENT_HANDLED;
}
