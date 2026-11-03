#pragma once
#include <SDL.h>
#include <algorithm>
#include <vector>
#include <cstring>
// The engine retains SDL3's float-colour geometry; SDL2 submits byte colours.
struct KysColor { float r, g, b, a; };
struct KysVertex { SDL_FPoint position; KysColor color; SDL_FPoint tex_coord; };
inline int kys_render_geometry(SDL_Renderer* r, SDL_Texture* t, const KysVertex* v,
                              int count, const int* indices, int n) {
    std::vector<SDL_Vertex> out(count);
    auto byte=[](float x) {return Uint8(std::clamp(x,0.f,1.f)*255.f+.5f);};
    for(int i=0;i<count;++i) out[i]={v[i].position,
        {byte(v[i].color.r),byte(v[i].color.g),byte(v[i].color.b),byte(v[i].color.a)},v[i].tex_coord};
    return SDL_RenderGeometry(r,t,out.data(),count,indices,n);
}
using SDL_FColor = KysColor;
using KysEventFilter = bool (*)(void*, SDL_Event*);
inline KysEventFilter kys_watch;
inline void* kys_watch_data;
inline int kys_watch_bridge(void*, SDL_Event* e) {return kys_watch(kys_watch_data,e);}
inline void kys_add_event_watch(KysEventFilter f,void* d) {
    kys_watch=f;kys_watch_data=d;SDL_AddEventWatch(kys_watch_bridge,nullptr);
}
inline bool SDL_GetTextureSize(SDL_Texture* t,float* w,float* h) {
    int x=0,y=0;int rc=SDL_QueryTexture(t,nullptr,nullptr,&x,&y);
    if(w)*w=x;if(h)*h=y;return rc==0;
}
inline SDL_Surface* SDL_CreateSurface(int w,int h,Uint32 fmt) {
    return SDL_CreateRGBSurfaceWithFormat(0,w,h,SDL_BITSPERPIXEL(fmt),fmt);
}
inline SDL_Surface* kys_convert_surface(SDL_Surface* s,Uint32 f) {return SDL_ConvertSurfaceFormat(s,f,0);}
#define SDL_ConvertSurface kys_convert_surface
#define SDL_DestroySurface SDL_FreeSurface
#define SDL_FillSurfaceRect SDL_FillRect
inline int SDL_RenderTextureRotated(SDL_Renderer* r,SDL_Texture* t,const SDL_FRect* src,
        const SDL_FRect* dst,double angle,const SDL_FPoint* center,SDL_RendererFlip flip) {
    SDL_Rect rect;
    if(src)rect={int(src->x),int(src->y),int(src->w),int(src->h)};
    return SDL_RenderCopyExF(r,t,src?&rect:nullptr,dst,angle,center,flip);
}
#define SDL_GetCurrentRenderOutputSize SDL_GetRendererOutputSize
#define SDL_RenderFillRect SDL_RenderFillRectF
#define SDL_IOFromConstMem SDL_RWFromConstMem
#define SDL_IOFromFile SDL_RWFromFile
#define IMG_Load_IO IMG_Load_RW
#define SDL_BUTTON_MASK SDL_BUTTON
#define SDL_BLENDMODE_BLEND_PREMULTIPLIED SDL_BLENDMODE_BLEND
#define SDL_BLENDMODE_ADD_PREMULTIPLIED SDL_BLENDMODE_ADD
inline void SDL_RectToFRect(const SDL_Rect* r,SDL_FRect* f) {*f={float(r->x),float(r->y),float(r->w),float(r->h)};}
inline bool SDL_ReadSurfacePixel(SDL_Surface* s,int x,int y,Uint8* r,Uint8* g,Uint8* b,Uint8* a) {
    if(!s || x<0 || y<0 || x>=s->w || y>=s->h)return false;
    Uint32 pixel=0;memcpy(&pixel,(Uint8*)s->pixels+y*s->pitch+x*s->format->BytesPerPixel,s->format->BytesPerPixel);
    SDL_GetRGBA(pixel,s->format,r,g,b,a);return true;
}
inline bool SDL_WriteSurfacePixel(SDL_Surface* s,int x,int y,Uint8 r,Uint8 g,Uint8 b,Uint8 a) {
    if(!s || x<0 || y<0 || x>=s->w || y>=s->h)return false;
    Uint32 pixel=SDL_MapRGBA(s->format,r,g,b,a);
    memcpy((Uint8*)s->pixels+y*s->pitch+x*s->format->BytesPerPixel,&pixel,s->format->BytesPerPixel);return true;
}
inline SDL_Surface* kys_read_pixels(SDL_Renderer* r,const SDL_Rect* rect) {
    auto s=SDL_CreateSurface(rect->w,rect->h,SDL_PIXELFORMAT_RGBA32);
    if(s && SDL_RenderReadPixels(r,rect,s->format->format,s->pixels,s->pitch)) {SDL_FreeSurface(s);return nullptr;}
    return s;
}
#define SDL_RenderReadPixels kys_read_pixels
inline SDL_TouchID* SDL_GetTouchDevices(int* count) {
    *count=SDL_GetNumTouchDevices();
    auto p=static_cast<SDL_TouchID*>(SDL_malloc(*count*sizeof(SDL_TouchID)));
    if(!p){*count=0;return nullptr;}
    for(int i=0;i<*count;++i)p[i]=SDL_GetTouchDevice(i);
    return p;
}
inline SDL_Finger** SDL_GetTouchFingers(SDL_TouchID id,int* count) {
    *count=SDL_GetNumTouchFingers(id);
    auto p=static_cast<SDL_Finger**>(SDL_malloc(*count*sizeof(SDL_Finger*)));
    if(!p){*count=0;return nullptr;}
    for(int i=0;i<*count;++i)p[i]=SDL_GetTouchFinger(id,i);
    return p;
}
