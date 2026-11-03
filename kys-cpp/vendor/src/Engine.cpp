#include "Engine.h"

#include "GameUtil.h"
extern "C" {
#include "SDL_image.h"
}
#include "UIRenderer.h"
#include "strfunc.h"
#ifdef __ANDROID__
#include "ZipFile2.h"
#endif
#include <cmath>
#include <vector>
#ifdef _MSC_VER
#include <windows.h>
#pragma comment(lib, "user32.lib")
#endif

//#include "opencv4/opencv2/opencv.hpp"

Engine::Engine()
{
}

Engine::~Engine()
{
    destroy();
}

#ifdef __ANDROID__
void Engine::extractAssetsIfNeeded()
{
    const std::string external = SDL_GetAndroidExternalStoragePath();
    const std::string dest = "/sdcard/kys-cpp/game/";
    const std::string marker = dest + ".game_extracted";
    auto create_parent_dirs = [](const std::string& path) {
        size_t pos = path.find('/', 1);
        while (pos != std::string::npos)
        {
            SDL_CreateDirectory(path.substr(0, pos).c_str());
            pos = path.find('/', pos + 1);
        }
        SDL_CreateDirectory(path.c_str());
    };

    if (filefunc::fileExist(marker))
    {
        return;
    }

    SDL_Log("kys-cpp: extracting game assets to %s ...", dest.c_str());
    SDL_RWops* io = SDL_RWFromFile("game.zip", "rb");
    if (!io)
    {
        SDL_Log("kys-cpp: cannot open game.zip from assets: %s", SDL_GetError());
        return;
    }
    Sint64 size = SDL_GetIOSize(io);
    std::string buf(size, '\0');
    SDL_ReadIO(io, buf.data(), (size_t)size);
    SDL_CloseIO(io);

    const std::string tmp_zip = external + "/.game_tmp.zip";
    filefunc::writeFile(buf.c_str(), (int)buf.size(), tmp_zip);

    {
        ZipFile2 zip;
        zip.openRead(tmp_zip);
        if (!zip.opened())
        {
            SDL_Log("kys-cpp: cannot open temp zip at %s", tmp_zip.c_str());
            return;
        }

        for (const auto& f : zip.getFileNames())
        {
            std::string normalized = f;
            for (char& c : normalized)
            {
                if (c == '\\')
                {
                    c = '/';
                }
            }
            if (normalized.empty() || normalized.back() == '/')
            {
                continue;
            }
            const std::string out_path = dest + normalized;
            const size_t slash = out_path.rfind('/');
            if (slash != std::string::npos)
            {
                create_parent_dirs(out_path.substr(0, slash));
            }

            const auto data = zip.readFile(f);
            filefunc::writeFile(data.c_str(), (int)data.size(), out_path);
        }
    }

    remove(tmp_zip.c_str());
    filefunc::writeFile("ok", 2, marker);
    SDL_Log("kys-cpp: assets extracted successfully.");
}
#endif

int Engine::init(void* handle /*= nullptr*/, int handle_type /*= 0*/, int maximized, const std::string& str, int fullscreen)
{
    if (inited_)
    {
        return 0;
    }
    inited_ = true;
    texture_top_left_highlight_enabled_ = GameUtil::getInstance()->getInt("game", "texture_top_left_highlight", 1) != 0;

#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif

#ifndef _WINDLL
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) < 0)
    {
        return -1;
    }
#endif

    window_mode_ = handle_type;

    window_ = SDL_CreateWindow(title_.c_str(), 0, 0, 320, 240, 0);
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
    renderer_self_ = true;
    if (!window_ || !renderer_) return -1;
    max_texture_size_ = 2048;
    //SDL_SetDefaultTextureScaleMode(renderer_, SDL_SCALEMODE_PIXELART);

    //SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    //SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    //屏蔽触摸板
    //SDL_EventState(SDL_FINGERUP, SDL_DISABLE);
    //SDL_EventState(SDL_FINGERDOWN, SDL_DISABLE);
    //SDL_EventState(SDL_FINGERMOTION, SDL_DISABLE);

    //手柄
    checkGameControllers();

    rect_ = { 0, 0, ui_w_, ui_h_ };

    renderPresent();
    TTF_Init();

#ifdef _MSC_VER
    RECT r;
    SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&r, 0);
    int w = GetSystemMetrics(SM_CXEDGE);
    int h = GetSystemMetrics(SM_CYEDGE);
    min_x_ = r.left + w;
    min_y_ = r.top + h + GetSystemMetrics(SM_CYCAPTION);
    max_x_ = r.right - w;
    max_y_ = r.bottom - h;
#else
    Rect r;
    SDL_GetDisplayBounds(0, &r);
    min_x_ = r.x;
    min_y_ = r.y;
    max_x_ = r.w + r.x;
    max_y_ = r.h + r.y;
#endif

    square_ = createRectTexture(100, 100, 0);

    std::print("maximum width and height are: {}, {}\n", max_x_, max_y_);

    createMainTexture(Uint32(0), TEXTUREACCESS_TARGET, ui_w_, ui_h_);
    return 0;
}

int Engine::getWindowWidth() const
{
    int w, h;
    getWindowSize(w, h);
    return w;
}

int Engine::getWindowHeight() const
{
    int w, h;
    getWindowSize(w, h);
    return h;
}

void Engine::setWindowIsMaximized(bool b) const
{
    if (b)
    {
        SDL_MaximizeWindow(window_);
    }
    else
    {
        SDL_RestoreWindow(window_);
    }
}

void Engine::setWindowSize(int w, int h)
{
    if (getWindowIsMaximized())
    {
        return;
    }
    if (rotation_ == 90 || rotation_ == 270)
    {
        std::swap(w, h);
    }
    if (w <= 0 || h <= 0)
    {
        return;
    }
    //w = 1920;
    //h = 1080;
    win_w_ = std::min(max_x_ - min_x_, w);
    win_h_ = std::min(max_y_ - min_y_, h);
    double ratio;
    ratio = std::min(1.0 * win_w_ / w, 1.0 * win_h_ / h);
    win_w_ = w * ratio;
    win_h_ = h * ratio;
    //std::print("{}, {}, {}, {}, {}\n", win_w_, win_h_, w, h, ratio);
    if (!window_)
    {
        return;
    }

    SDL_SetWindowSize(window_, win_w_, win_h_);
    setPresentPosition(tex_);

    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
    SDL_GetWindowSize(window_, &win_w_, &win_h_);
    //std::print("{}, {}, {}, {}, {}\n", win_w_, win_h_, w, h, ratio);
    //resetWindowsPosition();
    //renderPresent();
}

void Engine::setWindowPosition(int x, int y) const
{
    int w, h;
    getWindowSize(w, h);
    if (x == WINDOWPOS_CENTERED)
    {
        x = min_x_ + (max_x_ - min_x_ - w) / 2;
    }
    if (y == WINDOWPOS_CENTERED)
    {
        y = min_y_ + (max_y_ - min_y_ - h) / 2;
    }
    SDL_SetWindowPosition(window_, x, y);
}

void Engine::createMainTexture(PixelFormat pixfmt, TextureAccess a, int w, int h)
{
    resetRenderTarget();
    if (tex_)
    {
        SDL_DestroyTexture(tex_);
    }
    w = 320; h = 180; // UI coordinates remain logical; target scale resolves them.
    if (pixfmt < 0)
    {
        tex_ = createRenderedTexture(w, h);
    }
    else
    {
        tex_ = createTexture(pixfmt, a, w, h);
    }
    setPresentPosition(tex_);
}

void Engine::resizeMainTexture(int w, int h) const
{
    float w0, h0;
    uint32_t pix_fmt;
    if (!SDL_GetTextureSize(tex_, &w0, &h0))
    {
        if (int(w0) != w || int(h0) != h)
        {
            //createMainTexture(pix_fmt, w, h);
        }
    }
}

//创建一个专用于画场景的，后期放大
void Engine::createAssistTexture(const std::string& name, int w, int h)
{
    //tex_ = createYUVTexture(w, h);

    auto& tex = tex_map_[name];
    if (tex)
    {
        SDL_DestroyTexture(tex);
    }
    int64_t pixfmt = 0;
    pixfmt = SDL_PIXELFORMAT_RGBA8888;
    tex = createTexture((Uint32)pixfmt, TEXTUREACCESS_TARGET, w, h);
    //tex_ = createRenderedTexture(768, 480);
    //SDL_SetTextureBlendMode(tex2_, SDL_BLENDMODE_BLEND);
}

bool Engine::resizeRenderTexturesToWindow() { return false; }

void Engine::setPresentPosition(Texture* tex)
{
    if (!tex)
    {
        return;
    }
    int w_dst = 0, h_dst = 0;
    int w_src = 0, h_src = 0;
    getWindowSize(w_dst, h_dst);
    getTextureSize(tex, w_src, h_src);
    w_src *= ratio_x_;
    h_src *= ratio_y_;
    if (keep_ratio_)
    {
        if (w_src == 0 || h_src == 0)
        {
            return;
        }
        double ratio = std::min(1.0 * w_dst / w_src, 1.0 * h_dst / h_src);
        if (rotation_ == 90 || rotation_ == 270)
        {
            ratio = std::min(1.0 * w_dst / h_src, 1.0 * h_dst / w_src);
        }
        rect_.x = (w_dst - w_src * ratio) / 2;
        rect_.y = (h_dst - h_src * ratio) / 2;
        rect_.w = w_src * ratio;
        rect_.h = h_src * ratio;
    }
    else
    {
        //unfinshed
        rect_.x = 0;
        rect_.y = 0;
        rect_.w = w_dst;
        rect_.h = h_dst;
        if (rotation_ == 90 || rotation_ == 270)
        {
            rect_.x = (h_dst - w_dst) / 2;
            rect_.y = (w_dst - h_dst) / 2;
            rect_.w = h_dst;
            rect_.h = w_dst;
        }
    }
}

Texture* Engine::createTexture(PixelFormat pix_fmt, TextureAccess a, int w, int h) const
{
    if (pix_fmt == SDL_PIXELFORMAT_UNKNOWN)
    {
        pix_fmt = SDL_PIXELFORMAT_RGBA8888;
    }
    if (max_texture_size_ > 0)
    {
        w = std::min(w, max_texture_size_);
        h = std::min(h, max_texture_size_);
    }
    return SDL_CreateTexture(renderer_, pix_fmt, (SDL_TextureAccess)a, w, h);
}

Texture* Engine::createYUVTexture(int w, int h) const
{
    return SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, w, h);
}

void Engine::updateYUVTexture(Texture* t, uint8_t* data0, int size0, uint8_t* data1, int size1, uint8_t* data2, int size2)
{
    SDL_UpdateYUVTexture(t, nullptr, data0, size0, data1, size1, data2, size2);
}

Texture* Engine::createTexture(int w, int h)
{
    return SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w, h);
}

Texture* Engine::createRenderedTexture(int w, int h)
{
    if (max_texture_size_ > 0)
    {
        w = std::min(w, max_texture_size_);
        h = std::min(h, max_texture_size_);
    }
    return SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
}

void Engine::updateTexture(Texture* t, uint8_t* buffer, int pitch)
{
    SDL_UpdateTexture(t, nullptr, buffer, pitch);
}

int Engine::lockTexture(Texture* t, Rect* r, void** pixel, int* pitch)
{
    return SDL_LockTexture(t, r, pixel, pitch);
}

void Engine::unlockTexture(Texture* t)
{
    SDL_UnlockTexture(t);
}

void Engine::renderPresent() const
{
    //renderMainTextureToWindow();
    SDL_RenderPresent(renderer_);
    SDL_RenderClear(renderer_);
    //setRenderMainTexture();
}

void Engine::renderTexture(Texture* t /*= nullptr*/, double angle)
{
    FRect rectf;
    SDL_RectToFRect(&rect_, &rectf);
    SDL_RenderTextureRotated(renderer_, t, nullptr, &rectf, angle, nullptr, SDL_FLIP_NONE);
    render_times_++;
}

void Engine::renderTexture(Texture* t, int x, int y, int w, int h, double angle, int inPresent)
{
    if (inPresent == 1)
    {
        x += rect_.x;
        y += rect_.y;
    }
    int w0, h0;
    getTextureSize(t, w0, h0);
    if (w < 0)
    {
        w = w0;
    }
    if (h < 0)
    {
        h = h0;
    }
    Rect r = { x, y, w, h };
    renderTexture(t, nullptr, &r, angle);
}

void Engine::renderTexture(Texture* t, Rect* rect0, Rect* rect1, double angle, int inPresent /*= 0*/)
{
    FRect rect0f, rect1f;
    FRect *rect0f_ptr = nullptr, *rect1f_ptr = nullptr;
    if (rect0)
    {
        SDL_RectToFRect(rect0, &rect0f);
        rect0f_ptr = &rect0f;
    }
    if (rect1)
    {
        SDL_RectToFRect(rect1, &rect1f);
        rect1f_ptr = &rect1f;
    }
    SDL_RenderTextureRotated(renderer_, t, rect0f_ptr, rect1f_ptr, angle, nullptr, SDL_FLIP_NONE);
    render_times_++;
}

void Engine::renderTexture(Texture* t, Rect* rect0, const std::vector<FPoint>& v, const std::vector<FPoint>& v2)
{
    if (!t || v.size() < 4)
    {
        if (rect0)
        {
            renderTexture(t, rect0, rect0);
        }
        else
        {
            renderTexture(t);
        }
        return;
    }

    float tw = 1.0f, th = 1.0f;
    SDL_GetTextureSize(t, &tw, &th);

    FPoint src[4];
    if (v2.size() >= 4)
    {
        for (int i = 0; i < 4; i++)
        {
            src[i] = v2[i];
        }
    }
    else if (rect0)
    {
        src[0] = { float(rect0->x), float(rect0->y) };
        src[1] = { float(rect0->x + rect0->w), float(rect0->y) };
        src[2] = { float(rect0->x + rect0->w), float(rect0->y + rect0->h) };
        src[3] = { float(rect0->x), float(rect0->y + rect0->h) };
    }
    else
    {
        src[0] = { 0, 0 };
        src[1] = { tw, 0 };
        src[2] = { tw, th };
        src[3] = { 0, th };
    }

    KysVertex vertices[4];
    for (int i = 0; i < 4; i++)
    {
        vertices[i].position = v[i];
        vertices[i].tex_coord = { src[i].x / tw, src[i].y / th };
        vertices[i].color = { 1.0f, 1.0f, 1.0f, 1.0f };
    }

    int indices[6] = { 0, 1, 2, 2, 3, 0 };
    kys_render_geometry(renderer_, t, vertices, 4, indices, 6);
    render_times_++;
}

void Engine::renderTextureMesh(Texture* t, const std::vector<FPoint>& v, const std::vector<FPoint>& v2,
    const std::vector<Color>& colors, const std::vector<int>& indices, float top_left_brightness)
{
    if (v.empty() || v.size() != v2.size() || indices.empty())
    {
        return;
    }

    float tw = 1.0f, th = 1.0f;
    if (t)
    {
        SDL_GetTextureSize(t, &tw, &th);
    }

    std::vector<KysVertex> vertices(v.size());
    bool use_vertex_colors = colors.size() == v.size();
    for (size_t i = 0; i < v.size(); i++)
    {
        vertices[i].position = v[i];
        vertices[i].tex_coord = { v2[i].x / tw, v2[i].y / th };
        if (use_vertex_colors)
        {
            vertices[i].color = {
                colors[i].r / 255.0f,
                colors[i].g / 255.0f,
                colors[i].b / 255.0f,
                colors[i].a / 255.0f
            };
        }
        else
        {
            vertices[i].color = { 1.0f, 1.0f, 1.0f, 1.0f };
        }
    }

    kys_render_geometry(renderer_, t, vertices.data(), int(vertices.size()), indices.data(), int(indices.size()));
    render_times_++;

    if (texture_top_left_highlight_enabled_ && top_left_brightness > 0.0f)
    {
        float min_x = v[0].x;
        float max_x = v[0].x;
        float min_y = v[0].y;
        float max_y = v[0].y;
        for (const auto& point : v)
        {
            min_x = (std::min)(min_x, point.x);
            max_x = (std::max)(max_x, point.x);
            min_y = (std::min)(min_y, point.y);
            max_y = (std::max)(max_y, point.y);
        }

        float width = (std::max)(1.0f, max_x - min_x);
        float height = (std::max)(1.0f, max_y - min_y);
        for (auto& vertex : vertices)
        {
            float left_weight = (max_x - vertex.position.x) / width;
            float top_weight = (max_y - vertex.position.y) / height;
            float brightness = top_left_brightness * std::clamp(left_weight, 0.0f, 1.0f) * std::clamp(top_weight, 0.0f, 1.0f);
            vertex.color = { brightness, brightness, brightness, 1.0f };
        }

        SDL_BlendMode previous_blend = SDL_BLENDMODE_BLEND;
        SDL_GetTextureBlendMode(t, &previous_blend);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD);
        kys_render_geometry(renderer_, t, vertices.data(), int(vertices.size()), indices.data(), int(indices.size()));
        SDL_SetTextureBlendMode(t, previous_blend);
        render_times_++;
    }
}

void Engine::renderTextureLight(Texture* t, Rect* rect0, Rect* rect1, const std::vector<Color>& colors,
    const std::vector<float>& brightness_v, double angle)
{
    if (!t || !rect1)
    {
        return;
    }

    int w = rect1->w;
    int h = rect1->h;

    int w0, h0;
    getTextureSize(t, w0, h0);
    if (w < 0)
    {
        w = w0;
    }
    if (h < 0)
    {
        h = h0;
    }

    float x0 = float(rect1->x);
    float y0 = float(rect1->y);
    float x1 = x0 + w;
    float y1 = y0 + h;

    float tex_x0 = rect0 ? float(rect0->x) : 0.0f;
    float tex_y0 = rect0 ? float(rect0->y) : 0.0f;
    float tex_x1 = tex_x0 + (rect0 ? rect0->w : w0);
    float tex_y1 = tex_y0 + (rect0 ? rect0->h : h0);

    float tw = 1.0f, th = 1.0f;
    float tw_f, th_f;
    SDL_GetTextureSize(t, &tw_f, &th_f);
    tw = tw_f;
    th = th_f;

    auto color_to_fcolor = [](const Color& c) -> SDL_FColor
    {
        return { float(c.r) / 255.0f, float(c.g) / 255.0f, float(c.b) / 255.0f, float(c.a) / 255.0f };
    };

    SDL_FColor default_color = { 1.0f, 1.0f, 1.0f, 1.0f };

    KysVertex vertices[4];

    // colors 的顺序对应四个顶点：
    // colors[0] -> 左上(x0, y0)
    // colors[1] -> 右上(x1, y0)
    // colors[2] -> 右下(x1, y1)
    // colors[3] -> 左下(x0, y1)
    // 若未提供某个索引的颜色，则该顶点使用默认白色。
    vertices[0].position = { x0, y0 };
    vertices[0].tex_coord = { tex_x0 / tw, tex_y0 / th };
    vertices[0].color = colors.size() > 0 ? color_to_fcolor(colors[0]) : default_color;

    vertices[1].position = { x1, y0 };
    vertices[1].tex_coord = { tex_x1 / tw, tex_y0 / th };
    vertices[1].color = colors.size() > 1 ? color_to_fcolor(colors[1]) : default_color;

    vertices[2].position = { x1, y1 };
    vertices[2].tex_coord = { tex_x1 / tw, tex_y1 / th };
    vertices[2].color = colors.size() > 2 ? color_to_fcolor(colors[2]) : default_color;

    vertices[3].position = { x0, y1 };
    vertices[3].tex_coord = { tex_x0 / tw, tex_y1 / th };
    vertices[3].color = colors.size() > 3 ? color_to_fcolor(colors[3]) : default_color;

    if (angle != 0)
    {
        const double pi = 3.14159265358979323846;
        const float rad = float(angle * pi / 180.0);
        const float c = float(std::cos(rad));
        const float s = float(std::sin(rad));
        const float cx = x0 + w * 0.5f;
        const float cy = y0 + h * 0.5f;
        for (auto& v : vertices)
        {
            float rx = v.position.x - cx;
            float ry = v.position.y - cy;
            v.position.x = cx + rx * c - ry * s;
            v.position.y = cy + rx * s + ry * c;
        }
    }

    int indices[6] = { 0, 1, 2, 2, 3, 0 };

    kys_render_geometry(renderer_, t, vertices, 4, indices, 6);
    render_times_++;

    if (!brightness_v.empty())
    {
        float b[4] = { 0, 0, 0, 0 };
        for (int i = 0; i < 4; i++)
        {
            if (i < (int)brightness_v.size())
            {
                b[i] = (std::max)(0.0f, brightness_v[i]);
            }
        }
        if (!texture_top_left_highlight_enabled_)
        {
            b[0] = 0.0f;
        }

        float max_b = (std::max)((std::max)(b[0], b[1]), (std::max)(b[2], b[3]));
        int full_pass = int(std::floor(max_b));
        float remain = max_b - full_pass;

        SDL_BlendMode prev_blend = SDL_BLENDMODE_BLEND;
        SDL_GetTextureBlendMode(t, &prev_blend);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD);

        auto do_add_pass = [&](float pass_base)
        {
            KysVertex add_vertices[4] = { vertices[0], vertices[1], vertices[2], vertices[3] };
            for (int i = 0; i < 4; i++)
            {
                float k = b[i] - pass_base;
                if (k > 1.0f) { k = 1.0f; }
                if (k < 0.0f) { k = 0.0f; }
                add_vertices[i].color = { k, k, k, 1.0f };
            }
            kys_render_geometry(renderer_, t, add_vertices, 4, indices, 6);
            render_times_++;
        };

        for (int p = 0; p < full_pass; p++)
        {
            do_add_pass(float(p));
        }
        if (remain > 0.0f)
        {
            do_add_pass(float(full_pass));
        }

        SDL_SetTextureBlendMode(t, prev_blend);
    }
}

void Engine::destroy() const
{
    destroyTexture(tex_);
    for (auto& [k, tex] : tex_map_)
    {
        destroyTexture(tex);
    }
    if (renderer_self_)
    {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_mode_ == 0)
    {
        SDL_DestroyWindow(window_);
    }

#ifndef _WINDLL
    SDL_Quit();
#endif
}

bool Engine::isFullScreen()
{
    uint32_t state = SDL_GetWindowFlags(window_);
    full_screen_ = (state & SDL_WINDOW_FULLSCREEN);
    return full_screen_;
}

void Engine::setFullScreen(bool b)
{
    full_screen_ = b;
    SDL_SetWindowFullscreen(window_, full_screen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    renderClear();
}

void Engine::toggleFullscreen()
{
    setFullScreen(!isFullScreen());
}

Texture* Engine::loadImage(const std::string& filename, int as_white)
{
    //std::print("%s", filename.c_str());
    //屏蔽libpng的错误输出
    DisableStream d(stderr);
    auto sur = IMG_Load(filename.c_str());
    if (!sur) { return nullptr; }
    if (as_white) { toWhite(sur); }
    auto tex = SDL_CreateTextureFromSurface(renderer_, sur);
    SDL_DestroySurface(sur);
    return tex;
}

Texture* Engine::loadImageFromMemory(const std::string& content, int as_white) const
{
    auto rw = SDL_RWFromConstMem(content.data(), content.size());
    auto sur = IMG_Load_IO(rw, 1);
    if (!sur) { return nullptr; }
    if (as_white) { toWhite(sur); }
    auto tex = SDL_CreateTextureFromSurface(renderer_, sur);
    SDL_DestroySurface(sur);
    return tex;
}

void Engine::toWhite(Surface* sur)
{
    if (!sur) return;
    for (int y = 0; y < sur->h; y++)
    {
        for (int x = 0; x < sur->w; x++)
        {
            uint8_t r, g, b, a;
            if (!SDL_ReadSurfacePixel(sur, x, y, &r, &g, &b, &a))
            {
                continue;
            }
            SDL_WriteSurfacePixel(sur, x, y, 255, 255, 255, a);
        }
    }
}

bool Engine::setKeepRatio(bool b)
{
    return keep_ratio_ = b;
}

Texture* Engine::transRGBABitmapToTexture(const uint8_t* src, uint32_t color, int w, int h, int stride) const
{
    auto s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA8888);
    SDL_FillSurfaceRect(s, nullptr, color);
    auto p = (uint8_t*)s->pixels;
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            p[4 * (y * w + x)] = src[y * stride + x];
        }
    }
    auto t = SDL_CreateTextureFromSurface(renderer_, s);
    SDL_DestroySurface(s);
    setTextureBlendMode(t);
    setTextureAlphaMod(t, 192);
    return t;
}

void Engine::resetWindowPosition()
{
    int x, y, w, h, x0, y0;
    getWindowSize(w, h);
    SDL_GetWindowPosition(window_, &x0, &y0);
    x = std::max(min_x_, x0);
    y = std::max(min_y_, y0);
    if (x + w > max_x_)
    {
        x = std::min(x, max_x_ - w);
    }
    if (y + h > max_y_)
    {
        y = std::min(y, max_y_ - h);
    }
    if (x != x0 || y != y0)
    {
        setWindowPosition(x, y);
    }
}

void Engine::setColor(Texture* tex, Color c)
{
    SDL_SetTextureColorMod(tex, c.r, c.g, c.b);
    setTextureAlphaMod(tex, c.a);
    setTextureBlendMode(tex);
}

void Engine::fillColor(Color color, int x, int y, int w, int h, BlendMode blend) const
{
    if (!UIRenderer::getInstance()->isExecuting() && getRenderTarget() == tex_)
    {
        UIRenderer::getInstance()->fillColor(color, x, y, w, h, blend);
        return;
    }
    if (w < 0 || h < 0)
    {
        SDL_GetCurrentRenderOutputSize(renderer_, &w, &h);
    }
    Rect r{ x, y, w, h };
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BlendMode(blend));
    FRect rf;
    SDL_RectToFRect(&r, &rf);
    SDL_RenderFillRect(renderer_, &rf);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
}

void Engine::renderMainTextureToWindow()
{
    resetRenderTarget();
    renderTexture(tex_);
}

void Engine::createRenderedTexture(const std::string& name, int w, int h)
{
    auto tex = tex_map_[name];
    if (tex)
    {
        int w0, h0;
        getTextureSize(tex, w0, h0);
        if (w0 == w && h0 == h)
        {
            return;
        }
        destroyTexture(tex);
    }
    tex_map_[name] = createRenderedTexture(w, h);
}

void Engine::renderTextureToMain(const std::string& name)
{
    setRenderTarget(tex_);
    renderTexture(tex_map_[name], nullptr, nullptr);
    //setRenderTarget(tex_);
    //SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    //SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    //SDL_RenderFillRect(renderer_, nullptr);
}

void Engine::getMouseState(int& x, int& y)
{
    int xf, yf;
    SDL_GetMouseState(&xf, &yf);
    x = xf;
    y = yf;
}

bool Engine::windowToUISpace(int wx, int wy, int& ux, int& uy, bool clamp) const
{
    int px, py, pw, ph;
    getPresentRect(px, py, pw, ph);
    if (pw <= 0 || ph <= 0 || ui_w_ <= 0 || ui_h_ <= 0)
    {
        ux = 0;
        uy = 0;
        return false;
    }

    bool inside = (wx >= px && wx < px + pw && wy >= py && wy < py + ph);
    if (!inside && !clamp)
    {
        return false;
    }

    int cx = wx;
    int cy = wy;
    if (clamp)
    {
        cx = std::clamp(cx, px, px + pw - 1);
        cy = std::clamp(cy, py, py + ph - 1);
    }

    ux = (cx - px) * ui_w_ / pw;
    uy = (cy - py) * ui_h_ / ph;
    ux = std::clamp(ux, 0, ui_w_ - 1);
    uy = std::clamp(uy, 0, ui_h_ - 1);
    return inside;
}

void Engine::uiToWindowSpace(int ux, int uy, int& wx, int& wy) const
{
    int px, py, pw, ph;
    getPresentRect(px, py, pw, ph);
    if (pw <= 0 || ph <= 0 || ui_w_ <= 0 || ui_h_ <= 0)
    {
        wx = px;
        wy = py;
        return;
    }

    ux = std::clamp(ux, 0, ui_w_ - 1);
    uy = std::clamp(uy, 0, ui_h_ - 1);
    wx = px + ux * pw / ui_w_;
    wy = py + uy * ph / ui_h_;
}

void Engine::getMouseStateInStartWindow(int& x, int& y) const
{
    getMouseState(x, y);
    windowToUISpace(x, y, x, y, true);
}

void Engine::setMouseStateInStartWindow(int x, int y) const
{
    int wx, wy;
    uiToWindowSpace(x, y, wx, wy);
    SDL_WarpMouseInWindow(window_, wx, wy);
}

int Engine::pollEvent(EngineEvent& e)
{
    int r = SDL_PollEvent(&e);
    if (r && e.type == SDL_WINDOWEVENT) e.type = 0x10000 + e.window.event;
    if (r)
    {
        if (e.type == EVENT_WINDOW_RESIZED
            || e.type == EVENT_WINDOW_PIXEL_SIZE_CHANGED
            || e.type == EVENT_WINDOW_MAXIMIZED
            || e.type == EVENT_WINDOW_RESTORED)
        {
            resizeRenderTexturesToWindow();
        }
    }
    return r;
}

bool Engine::checkKeyPress(Keycode key)
{
    int num = 0;
    SDL_GetKeyboardState(&num);
    auto s = SDL_GetScancodeFromKey(key);
    return SDL_GetKeyboardState(&num)[SDL_GetScancodeFromKey(key)];
}

bool Engine::gameControllerGetButton(int key)
{
    bool pressed = false;
    if (getTicks() > prev_controller_press_ + interval_controller_press_)
    {
        int i = 0;
        for (auto gc : game_controllers_)
        {
            if (gc)
            {
                if (nintendo_switch_[i])
                {
                    if (key == GAMEPAD_BUTTON_SOUTH) { key = GAMEPAD_BUTTON_EAST; }
                    else if (key == GAMEPAD_BUTTON_EAST) { key = GAMEPAD_BUTTON_SOUTH; }
                    else if (key == GAMEPAD_BUTTON_WEST) { key = GAMEPAD_BUTTON_NORTH; }
                    else if (key == GAMEPAD_BUTTON_NORTH) { key = GAMEPAD_BUTTON_WEST; }
                }
                pressed = SDL_GameControllerGetButton(gc, SDL_GameControllerButton(key));
                if (pressed)
                {
                    cur_game_controller_ = gc;
                    break;
                }
            }
            i++;
        }
        if (!pressed) { pressed = virtual_stick_button_[key] != 0; }
        if (pressed)
        {
            prev_controller_press_ = getTicks();
        }
        interval_controller_press_ = 0;
    }
    return pressed;
}

int16_t Engine::gameControllerGetAxis(int axis)
{
    int16_t ret = 0;
    for (auto gc : game_controllers_)
    {
        if (gc)
        {
            ret = SDL_GameControllerGetAxis(gc, SDL_GameControllerAxis(axis));
        }
        if (ret)
        {
            cur_game_controller_ = gc;
            break;
        }
    }
    if (ret != 0)
    {
        return ret;
    }
    return virtual_stick_axis_[axis];
}

void Engine::gameControllerRumble(int l, int h, uint32_t time) const
{
    if (GameUtil::getInstance()->getInt("game", "controller_rumble", 1) != 0 && cur_game_controller_)
    {
        auto s = SDL_GameControllerRumble(cur_game_controller_, l * 65535 / 100, h * 65535 / 100, time);
    }
}

void Engine::checkGameControllers()
{
    int num_joysticks = 0;
    num_joysticks = SDL_NumJoysticks();
    std::vector<int> joystick_ids(num_joysticks);
    for (int i = 0; i < num_joysticks; ++i) joystick_ids[i] = i;
    if (num_joysticks <= 0)
    {
        std::print("Warning: No joysticks connected!\n");
    }
    else
    {
        //按照游戏控制器打开
        std::print("Found {} game controller(s)\n", num_joysticks);
        game_controllers_.resize(num_joysticks);
        nintendo_switch_.resize(game_controllers_.size());
        for (int i = 0; i < game_controllers_.size(); i++)
        {
            auto joystick_id = joystick_ids[i];
            game_controllers_[i] = SDL_GameControllerOpen(joystick_id);
            if (game_controllers_[i])
            {
                std::string name = SDL_GameControllerName(game_controllers_[i]);
                std::print("{}\n", name);
                if (name.find("Switch") != std::string::npos) { nintendo_switch_[i] = 1; }
            }
            else
            {
                std::print("Warning: Unable to open game controller {}! SDL Error: {}\n", joystick_id, SDL_GetError());
            }
        }
    }
    
}

Texture* Engine::createRectTexture(int w, int h, int style) const
{
    auto square_s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA8888);

    //SDL_FillSurfaceRect(square_s, nullptr, 0xffffffff);ai
    Rect r = { 0, 0, 1, 1 };
    auto& x = r.x;
    auto& y = r.y;
    uint8_t a = 0;
    for (x = 0; x < w; x++)
    {
        for (y = 0; y < h; y++)
        {
            int c;
            if (style == 0)
            {
                a = 100 + 150 * cos(M_PI * (1.0 * y / w - 0.5));
                c = 0xffffff00 | a;
            }
            else if (style == 1)
            {
                c = 0xffffffff;
            }
            SDL_FillSurfaceRect(square_s, &r, c);
            /*if ((x - d / 2)*(x - d / 2) + (y - d / 2)*(y - d / 2) < (d / 2) * (d / 2))
            {
                SDL_FillSurfaceRect(square_s, &r, 0x00ffffff | (a<<24));
            }*/
        }
    }
    auto square = SDL_CreateTextureFromSurface(renderer_, square_s);
    setTextureBlendMode(square);
    //setTextureAlphaMod(square, 128);
    SDL_DestroySurface(square_s);
    return square;
}

Texture* Engine::createTextTexture(const std::string& fontname, const std::string& text, int size, Color c) const
{
    auto font = TTF_OpenFont(fontname.c_str(), size);
    auto text_t = createTextTexture(font, text, c);
    TTF_CloseFont(font);
    return text_t;
}

Texture* Engine::createTextTexture(TTF_Font* font, const std::string& text, Color c) const
{
    if (!font)
    {
        return nullptr;
    }
    auto text_s = TTF_RenderUTF8_Blended(font, text.c_str(), c);
    if (!text_s) return nullptr;
    auto text_t = SDL_CreateTextureFromSurface(renderer_, text_s);
    SDL_DestroySurface(text_s);
    return text_t;
}

int Engine::showMessage(const std::string& content) const
{
    const SDL_MessageBoxButtonData buttons[] = {
        { /* .flags, .buttonid, .text */ 0, 0, "no" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "yes" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "cancel" },
    };
    const SDL_MessageBoxColorScheme colorScheme = {
        { /* .colors (.r, .g, .b) */
            /* [SDL_MESSAGEBOX_COLOR_BACKGROUND] */
            { 255, 0, 0 },
            /* [SDL_MESSAGEBOX_COLOR_TEXT] */
            { 0, 255, 0 },
            /* [SDL_MESSAGEBOX_COLOR_BUTTON_BORDER] */
            { 255, 255, 0 },
            /* [SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND] */
            { 0, 0, 255 },
            /* [SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED] */
            { 255, 0, 255 } }
    };
    const SDL_MessageBoxData messageboxdata = {
        SDL_MESSAGEBOX_INFORMATION, /* .flags */
        NULL,                       /* .window */
        title_.c_str(),             /* .title */
        content.c_str(),            /* .message */
        SDL_TABLESIZE(buttons),     /* .numbuttons */
        buttons,                    /* .buttons */
        &colorScheme                /* .colorScheme */
    };
    int buttonid;
    SDL_ShowMessageBox(&messageboxdata, &buttonid);
    return buttonid;
}

void Engine::renderSquareTexture(Rect* rect, Color color, uint8_t alpha)
{
    color.a = alpha;
    if (!UIRenderer::getInstance()->isExecuting() && getRenderTarget() == tex_)
    {
        UIRenderer::getInstance()->drawTexture(square_, *rect, color);
        return;
    }
    setColor(square_, color);
    renderTexture(square_, nullptr, rect);
}

int Engine::saveScreen(const char* filename) const
{
    Rect rect;
    rect.x = 0;
    rect.y = 0;
    getWindowSize(rect.w, rect.h);
    auto sur = SDL_RenderReadPixels(renderer_, &rect);
    SDL_SaveBMP(sur, filename);
    SDL_DestroySurface(sur);
    return 0;
}

int Engine::saveTexture(Texture* tex, const char* filename) const
{
    Rect rect;
    rect.x = 0;
    rect.y = 0;
    getTextureSize(tex, rect.w, rect.h);
    setRenderTarget(tex);
    auto sur = SDL_RenderReadPixels(renderer_, &rect);
    SDL_SaveBMP(sur, filename);
    SDL_DestroySurface(sur);
    resetRenderTarget();
    return 0;
}
