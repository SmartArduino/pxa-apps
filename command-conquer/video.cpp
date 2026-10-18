/* PXA video adapter; surface blitting from recrof/cnc_video.cpp.
 * GPL-3.0-or-later. */
#include "video.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "gbuffer.h"
#include "port.h"
#include "font8x8.h"
#include <stdlib.h>
#include <string.h>
class VideoSurfaceESP;
static VideoSurfaceESP *frontSurface;
static bool consoleShown;
static const char *TAG = "cnc";
static bool esp_ptr_external_ram(void *) { return false; }
static void vTaskDelay(unsigned n) { port_sleep(n); }
#define pdMS_TO_TICKS(n) (n)
static void *allocSurfaceMemory(size_t n, bool internal) {
  (void)internal;
  return malloc(n);
}
class VideoSurfaceESP : public VideoSurface {
public:
  VideoSurfaceESP(int w, int h, GBC_Enum flags)
      : Width(w), Height(h), Flags(flags) {
    // the visible and hidden pages are used at every frame
    Data = (uint8_t *)allocSurfaceMemory(
        w * h, (flags & (GBC_VISIBLE | GBC_VIDEOMEM)) != 0);
    if (Data) {
      memset(Data, 0, w * h);
    }
    ESP_LOGI(TAG, "surface %dx%d flags %d at %p (%s)", w, h, flags, Data,
             esp_ptr_external_ram(Data) ? "PSRAM" : "internal");
    if (flags & GBC_VISIBLE) {
      frontSurface = this;
      consoleShown = false;
    }
  }

  virtual ~VideoSurfaceESP() {
    if (frontSurface == this) {
      frontSurface = nullptr;
      // let the display task finish with this buffer
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    free(Data);
  }

  virtual void *GetData() const { return Data; }
  virtual int GetPitch() const { return Width; }
  virtual bool IsAllocated() const { return false; }
  virtual void AddAttachedSurface(VideoSurface *surface) {}
  virtual bool IsReadyToBlit() { return true; }
  virtual bool LockWait() { return Data != nullptr; }
  virtual bool Unlock() { return true; }

  virtual void Blt(const Rect &destRect, VideoSurface *src, const Rect &srcRect,
                   bool mask) {
    VideoSurfaceESP *s = (VideoSurfaceESP *)src;
    int w = srcRect.Width, h = srcRect.Height;
    int sx = srcRect.X, sy = srcRect.Y, dx = destRect.X, dy = destRect.Y;
    // clip against both surfaces
    if (sx < 0) {
      w += sx;
      dx -= sx;
      sx = 0;
    }
    if (sy < 0) {
      h += sy;
      dy -= sy;
      sy = 0;
    }
    if (dx < 0) {
      w += dx;
      sx -= dx;
      dx = 0;
    }
    if (dy < 0) {
      h += dy;
      sy -= dy;
      dy = 0;
    }
    if (sx + w > s->Width)
      w = s->Width - sx;
    if (sy + h > s->Height)
      h = s->Height - sy;
    if (dx + w > Width)
      w = Width - dx;
    if (dy + h > Height)
      h = Height - dy;
    if (w <= 0 || h <= 0) {
      return;
    }
    const uint8_t *sp = s->Data + sy * s->Width + sx;
    uint8_t *dp = Data + dy * Width + dx;
    // overlapping copies within the same surface: go bottom up when needed
    int step = 1;
    if (s == this && dy > sy) {
      sp += (h - 1) * s->Width;
      dp += (h - 1) * Width;
      step = -1;
    }
    for (int y = 0; y < h; y++) {
      if (mask) {
        for (int x = 0; x < w; x++) {
          if (sp[x]) {
            dp[x] = sp[x];
          }
        }
      } else {
        memmove(dp, sp, w);
      }
      sp += step * s->Width;
      dp += step * Width;
    }
  }

  virtual void FillRect(const Rect &rect, unsigned char color) {
    // the rectangle is inclusive (see GraphicViewPortClass::Fill_Rect())
    int x0 = rect.X < 0 ? 0 : rect.X;
    int y0 = rect.Y < 0 ? 0 : rect.Y;
    int x1 = rect.X + rect.Width + 1;
    int y1 = rect.Y + rect.Height + 1;
    if (x1 > Width)
      x1 = Width;
    if (y1 > Height)
      y1 = Height;
    for (int y = y0; y < y1; y++) {
      memset(Data + y * Width + x0, color, x1 - x0);
    }
  }

  // display task
  const uint8_t *Line(int y) const { return Data + y * Width; }
  int Get_Width() const { return Width; }
  int Get_Height() const { return Height; }

private:
  uint8_t *Data;
  int Width;
  int Height;
  GBC_Enum Flags;
};

class Monitor : public SurfaceMonitorClass {
public:
  void Restore_Surfaces() override {}
  void Set_Surface_Focus(bool) override {}
  void Release() override {}
};
SurfaceMonitorClass::SurfaceMonitorClass() : SurfacesRestored(false) {}
static Monitor monitor;
SurfaceMonitorClass &AllSurfaces = monitor;
bool Set_Video_Mode(int w, int h, int bpp) {
  return w > 0 && h > 0 && w <= 640 && h <= 480 && bpp == 8;
}
void Reset_Video_Mode() {}
void Toggle_Video_Fullscreen() {}
bool Is_Video_Fullscreen() { return true; }
void Get_Video_Scale(float &x, float &y) { x = y = 1; }
void Set_Video_Cursor_Clip(bool) {}
static int mousex = 160, mousey = 120;
void Get_Video_Mouse(int &x, int &y) {
  x = mousex;
  y = mousey;
}
void Move_Video_Mouse(float x, float y) {
  mousex += x;
  mousey += y;
}
static const uint8_t *cursor_shape;
static int cursor_w, cursor_h, hotx, hoty;
void Set_Video_Cursor(void *shape, int w, int h, int x, int y) {
  cursor_shape = (uint8_t *)shape;
  cursor_w = w;
  cursor_h = h;
  hotx = x;
  hoty = y;
}
unsigned int Get_Free_Video_Memory() { return 8 * 1024 * 1024; }
unsigned Get_Video_Hardware_Capabilities() { return VIDEO_BLITTER; }
void Wait_Vert_Blank() { port_sleep(1); }
void Wait_Blit() {}
void Set_DD_Palette(void *p) { port_palette((const uint8_t *)p,1); }
void Video_Render_Frame() {
  if (!frontSurface) {port_sleep(1); return;}
  const port_cursor_t cursor = {cursor_shape,mousex-hotx,mousey-hoty,cursor_w,cursor_h};
  port_frame_cursor((const uint8_t *)frontSurface->GetData(),
                    frontSurface->Get_Width(),frontSurface->Get_Height(),
                    frontSurface->GetPitch(),&cursor);
}
Video::Video() {}
Video::~Video() {}
Video &Video::Shared() {
  static Video v;
  return v;
}
VideoSurface *Video::CreateSurface(int w, int h, GBC_Enum f) {
  return new VideoSurfaceESP(w, h, f);
}
extern "C" void videoGetMouse(int *x, int *y) {
  *x = mousex;
  *y = mousey;
}
extern "C" void videoSetMouse(float x, float y) {
  int w = frontSurface ? frontSurface->Get_Width() : 320,
      h = frontSurface ? frontSurface->Get_Height() : 240;
  mousex = x * w / 320;
  mousey = y * h / 240;
}
extern "C" void videoMoveMouse(float x, float y) {
  mousex += x;
  mousey += y;
}
