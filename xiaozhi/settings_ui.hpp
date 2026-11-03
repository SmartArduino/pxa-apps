// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "canvas_metrics.hpp"
#include "companion.hpp"
#include "settings_layout.hpp"
#include <array>
#include <string_view>
namespace xiaozhi {
enum class SettingAction {
  none,
  back,
  tab,
  details,
  official,
  custom,
  voice,
  listen,
  wake,
  auto_scene,
  awake,
  scene,
  edit_url,
  edit_token,
  edit_endpoint,
  edit_device,
  edit_client,
  edit_text,
  save,
  edit_done,
  edit_cancel
};
struct SettingHit {
  pxa::ui::CanvasRegion box;
  SettingAction action = SettingAction::none;
  unsigned value = 0;
};
struct SettingsData {
  unsigned page = 0, scene = 0;
  bool official = true, voice = true, listen = true, editing = false,
       sending = false, awake = true, saving = false,
       wake_supported = false, wake = false;
  std::string_view url, token, endpoint, device, client, notice, edit_title,
      edit_error, wake_note;
};
struct SettingsUi {
  static constexpr std::uint32_t ink = 0x594760ff, muted = 0x95818fff,
      plum = 0x8d739fff, pale = 0xeadfeaff, border = 0xd6c0ccff,
      cream = 0xfff8efff, accent = 0xd69b66ff;
  pxa::ui::CanvasCommands<4000> commands;
  std::array<SettingHit, 24> hits{};
  unsigned count = 0, focus = 0, content_start = 0;
  bool keyboard_focus = false, pending_keyboard_focus = false;
  std::uint64_t previous_hash = 0;
  std::uint32_t density = 65536;
  SettingsLayout layout{320, 240};
  SettingsRect editor_rect{};
  using Rect = SettingsRect;
  pxa::ui::CanvasRegion box(Rect r) const {
    return {int(std::int64_t(r.x) * 65536 / std::max(1u, density)),
            int(std::int64_t(r.y) * 65536 / std::max(1u, density)),
            int(canvas_extent(std::max(1, r.width), density)),
            int(canvas_extent(std::max(1, r.height), density))};
  }
  void rect(Rect r, std::uint32_t color, int radius = 5) {
    auto b = box(r);
    commands.rect(b.x, b.y, b.width, b.height, color,
                  radius ? int(canvas_extent(layout.px(radius), density)) : 0);
  }
  void text(Rect r, std::string_view value, int font = 13,
            std::uint32_t color = ink, unsigned align = 0) {
    int pixels = layout.font(font);
    int line = caption_metrics(pixels, 65536).line_height;
    r.y += std::max(0, (r.height - line) / 2);
    r.height = std::max(r.height, line);
    auto fitted = wrap_caption(value, r.width, pixels, 1)[0];
    commands.text_sized(box(r), color, fitted, pixels, align);
  }
  void target(Rect r, SettingAction action, unsigned value = 0) {
    if (count < hits.size()) hits[count++] = {box(r), action, value};
  }
  void button(Rect r, std::string_view label, SettingAction action,
              unsigned value = 0, bool active = false) {
    rect(r, active ? plum : pale);
    text({r.x + layout.px(3), r.y, r.width - layout.px(6), r.height},
         label, 12, active ? cream : ink, 1);
    target(r, action, value);
  }
  void back_button(Rect r, SettingAction action) {
    rect(r, pale);
    auto a = box({r.x + r.width * 3 / 5, r.y + r.height / 3, 1, 1});
    auto b = box({r.x + r.width * 2 / 5, r.y + r.height / 2, 1, 1});
    auto c = box({r.x + r.width * 3 / 5, r.y + r.height * 2 / 3, 1, 1});
    commands.line(a.x, a.y, b.x, b.y, ink, 2).line(b.x, b.y, c.x, c.y, ink, 2);
    target(r, action);
  }
  void row(Rect r, std::string_view label, std::string_view value, SettingAction action) {
    rect(r, cream);
    int pad = layout.px(8), arrow = layout.px(16);
    if (layout.width >= layout.px(300) || r.height < layout.px(40)) {
      int label_width = std::min(layout.px(90), r.width * 3 / 8);
      text({r.x + pad, r.y, label_width, r.height}, label, 12);
      text({r.x + pad + label_width, r.y, r.width - pad * 2 - label_width - arrow, r.height},
           value, 10, muted);
    } else {
      int title_height = caption_metrics(layout.font(11), 65536).line_height;
      text({r.x + pad, r.y + 1, r.width - pad * 2 - arrow, title_height}, label, 11);
      text({r.x + pad, r.y + title_height, r.width - pad * 2 - arrow, r.height - title_height}, value, 9, muted);
    }
    text({r.x + r.width - arrow - pad, r.y, arrow, r.height}, "›", 16, muted, 1);
    target(r, action);
  }
  void toggle(Rect r, std::string_view title, std::string_view note,
              bool on, SettingAction action, bool enabled = true) {
    rect(r, cream);
    int pad = layout.px(8), switch_width = layout.px(33), switch_height = layout.px(19);
    bool compact = r.height < layout.px(44);
    int title_font = compact ? 12 : 14, note_font = compact ? 9 : 10;
    int title_height = caption_metrics(layout.font(title_font), 65536).line_height;
    int note_height = caption_metrics(layout.font(note_font), 65536).line_height;
    bool notes = r.height >= title_height + note_height + 2;
    int top = r.y + std::max(1, (r.height - title_height - (notes ? note_height : 0)) / 2);
    text({r.x + pad, top, r.width - pad * 3 - switch_width, title_height}, title,
         title_font, enabled ? ink : muted);
    if (notes) text({r.x + pad, top + title_height, r.width - pad * 2, note_height}, note, note_font, muted);
    if (enabled) {
      Rect sw{r.x + r.width - pad - switch_width,
              top + std::max(0, (title_height-switch_height)/2), switch_width, switch_height};
      rect(sw, on ? plum : border, 10);
      int knob = switch_height - layout.px(4);
      rect({sw.x + (on ? sw.width-knob-layout.px(2) : layout.px(2)), sw.y + layout.px(2), knob, knob}, cream, 8);
      target(r, action);
    }
  }
  SettingHit hit(int x, int y) const {
    for (unsigned i = count; i > 0; --i) {
      const auto& h = hits[i-1];
      if (x >= h.box.x && y >= h.box.y && x < h.box.x+h.box.width && y < h.box.y+h.box.height) return h;
    }
    return {};
  }
  void move_focus(int direction) {
    if (!count) return;
    if (!keyboard_focus) focus = std::min(content_start, count-1);
    else focus = unsigned((int(focus)+direction+int(count))%int(count));
    keyboard_focus = true; previous_hash = 0;
  }
  SettingHit selected() const { return count && keyboard_focus ? hits[focus%count] : SettingHit{}; }
  pxa::ui::CanvasRegion editor_bounds() const { return box(editor_rect); }
  void draw(pxa::ui::CanvasRef& canvas, const pxa::ui::DisplayMetrics& surface,
            const std::optional<pxa::Asset>& background, const SettingsData& d) {
    if (!canvas.mounted()) return;
    std::uint64_t hash = 1469598103934665603ull;
    for (auto s : {d.url,d.token,d.endpoint,d.device,d.client,d.notice,d.edit_title,d.edit_error,d.wake_note})
      for (unsigned char c : s) hash = (hash ^ c)*1099511628211ull;
    hash ^= std::uint64_t(d.page) | std::uint64_t(d.scene)<<4 | std::uint64_t(d.official)<<8 |
        std::uint64_t(d.voice)<<9 | std::uint64_t(d.listen)<<10 | std::uint64_t(d.editing)<<11 |
        std::uint64_t(keyboard_focus)<<12 | std::uint64_t(focus)<<16 |
        std::uint64_t(surface.width)<<24 | std::uint64_t(surface.height)<<40;
    hash ^= std::uint64_t(d.awake)<<60 | std::uint64_t(d.saving)<<61 |
        std::uint64_t(d.wake_supported)<<58 | std::uint64_t(d.wake)<<59;
    hash ^= pending_keyboard_focus ? UINT64_C(1)<<63 : 0;
    if (hash == previous_hash) return;
    layout = SettingsLayout(surface.width, surface.height);
    density = surface.density_q16;
    count = 0; commands.reset();
    rect({0,0,layout.width,layout.height}, 0xf5edf4ff, 0);
    // A quiet edge tint keeps the room's palette while the whole screen is usable.
    rect({0,0,layout.width,layout.px(3)}, plum, 0);
    auto h = layout.header;
    int back = h.height, auxiliary = layout.px(43), gap = layout.gap;
    back_button({h.x,h.y,back,h.height}, SettingAction::back);
    text({h.x+back+gap,h.y,h.width-back-gap-auxiliary,h.height}, d.page==3 ? "接入详情" : "小智设置", 17);
    if (d.page==0 || d.page==1)
      button({h.x+h.width-auxiliary,h.y,auxiliary,h.height}, d.page==0 ? "详情" : "文字",
             d.page==0 ? SettingAction::details : SettingAction::edit_text);
    const std::string_view tabs[] = {"连接","对话","画面"};
    auto tb=layout.tabs;
    int tab_width=(tb.width-gap*2)/3;
    for (unsigned i=0;i<3;++i)
      button({tb.x+int(i)*(tab_width+gap),tb.y,tab_width,tb.height},tabs[i],SettingAction::tab,i,i==(d.page==3?0:d.page));
    content_start=count;
    if (d.page==0) {
      int mode_height=layout.px(layout.compact?28:34), half=(h.width-gap)/2;
      button({h.x,layout.body_top,half,mode_height},"官方小智",SettingAction::official,0,d.official);
      button({h.x+half+gap,layout.body_top,half,mode_height},"自定义",SettingAction::custom,0,!d.official);
      int top=layout.body_top+mode_height+gap;
      if (d.official) {
        Rect card{h.x,top,h.width,layout.body_bottom-top};rect(card,cream);
        int pad=layout.px(10), line=caption_metrics(layout.font(13),65536).line_height;
        text({card.x+pad,card.y+pad,card.width-pad*2,line},"已选择官方服务器",13);
        auto notes=wrap_caption("自动获取连接信息，首次使用请按激活提示绑定设备。",card.width-pad*2,layout.font(10),3);
        int note_height=caption_metrics(layout.font(10),65536).line_height;
        for (unsigned i=0;i<3;++i)
          if (card.y+pad+line+int(i+1)*note_height<=card.y+card.height)
            text({card.x+pad,card.y+pad+line+int(i)*note_height,card.width-pad*2,note_height},notes[i],10,muted);
      } else {
        int row_height=(layout.body_bottom-top-gap)/2;
        row({h.x,top,h.width,row_height},"服务器地址",d.url.empty()?"填写 ws:// 或 wss:// 地址":d.url,SettingAction::edit_url);
        row({h.x,top+row_height+gap,h.width,row_height},"访问令牌",d.token.empty()?"未设置":"已设置 · 点按修改",SettingAction::edit_token);
      }
    } else if (d.page==1) {
      toggle(layout.row(0,3),"语音回复","让她读出回答",d.voice,SettingAction::voice);
      toggle(layout.row(1,3),"自动聆听","回复结束后，继续听你说",d.listen,SettingAction::listen);
      toggle(layout.row(2,3),"语音唤醒",d.wake_note,d.wake,SettingAction::wake,d.wake_supported);
    } else if (d.page==2) {
      bool narrow=layout.width<layout.px(300);
      int picker_height=layout.px(narrow?60:30), top=layout.body_bottom-picker_height;
      int toggle_height=(top-layout.body_top-gap*2)/2;
      toggle({h.x,layout.body_top,h.width,toggle_height},"对话时保持亮屏","聆听、思考与回复时不自动锁屏",d.awake,SettingAction::awake);
      toggle({h.x,layout.body_top+toggle_height+gap,h.width,toggle_height},"跟随系统时间","按当地时间自动切换房间",d.scene==0,SettingAction::auto_scene);
      const std::string_view names[]={"清晨","白天","傍晚","夜晚"};
      const std::uint32_t colors[]={0xe5b6a0ff,0x9fbcd5ff,0xda9c90ff,0x77759fff};
      int columns=narrow?2:4, width=(h.width-gap*(columns-1))/columns;
      int height=(picker_height-(narrow?gap:0))/(narrow?2:1);
      for (unsigned i=0;i<4;++i) {
        Rect r{h.x+int(i%columns)*(width+gap),top+int(i/columns)*(height+gap),width,height};
        button(r,names[i],SettingAction::scene,i+1,d.scene==i+1);
        rect({r.x+layout.px(6),r.y,r.width-layout.px(12),layout.px(2)},colors[i],0);
      }
    } else {
      row(layout.row(0,4),"配置服务",d.endpoint,SettingAction::edit_endpoint);
      row(layout.row(1,4),"Device-Id",d.device,SettingAction::edit_device);
      row(layout.row(2,4),"Client-Id",d.client,SettingAction::edit_client);
      row(layout.row(3,4),"文字对话","写一句话给她",SettingAction::edit_text);
    }
    button(layout.save,d.saving?"正在保存…":"保存并返回",SettingAction::save,0,true);
    if (!d.notice.empty()) {
      auto banner=layout.save;banner.y-=layout.px(25);banner.height=layout.px(23);
      rect(banner,cream);text(banner,d.notice,10,0xa15662ff,1);
    }
    if (d.editing) {
      auto full=box({0,0,layout.width,layout.height});
      commands.rect(full.x,full.y,full.width,full.height,0x30273ec0);
      int pad=layout.pad, height=layout.px(layout.compact?117:142);
      Rect panel{pad,pad,layout.width-pad*2,height};rect(panel,cream,6);
      count=0;content_start=0;
      Rect back_rect{pad*2,pad*2,layout.px(28),layout.px(28)};
      back_button(back_rect,SettingAction::edit_cancel);
      int done=layout.px(45);
      text({back_rect.x+back_rect.width+gap,back_rect.y,layout.width-back_rect.width-done-pad*5-gap,back_rect.height},d.edit_title,14);
      button({layout.width-pad*2-done,back_rect.y,done,back_rect.height},d.sending?"发送":"完成",SettingAction::edit_done,0,true);
      editor_rect={pad*2,pad*2+back_rect.height+gap,layout.width-pad*4,layout.px(38)};
      rect(editor_rect,border,3);
      text({editor_rect.x,editor_rect.y+editor_rect.height+gap,editor_rect.width,layout.px(20)},
           d.edit_error.empty()?"完成后确认保存":d.edit_error,10,d.edit_error.empty()?muted:0xa15662ff);
    }
    if (pending_keyboard_focus && count) {
      focus=std::min(content_start,count-1);keyboard_focus=true;pending_keyboard_focus=false;
    }
    if (keyboard_focus && count) {
      auto r=hits[focus%count].box;
      int inset=2, right=r.x+r.width-inset, bottom=r.y+r.height-inset;
      commands.line(r.x+inset,r.y+inset,right,r.y+inset,accent,2)
          .line(right,r.y+inset,right,bottom,accent,2)
          .line(right,bottom,r.x+inset,bottom,accent,2)
          .line(r.x+inset,bottom,r.x+inset,r.y+inset,accent,2);
    }
    auto shown=canvas.present(commands);
    if (shown) previous_hash=hash;
    (void)background;
  }
};
} // namespace xiaozhi
