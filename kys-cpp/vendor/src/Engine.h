#pragma once

#include "sdl_compat.hpp"
extern "C" {
#include "SDL_ttf.h"
}

#include <algorithm>
#include <chrono>
#include <functional>
#include <print>
#include <string>

#include <unordered_map>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef M_PI
#define M_PI 3.141592653589793
#endif

//这里是底层部分，将SDL的函数均封装了一次
//如需更换底层，则要重新实现下面的全部功能，并重新定义全部常数和类型

//每个SDL的函数和结构通常仅出现一次，其余的均用别名

using AudioCallback = std::function<void(uint8_t*, int)>;
using Renderer = SDL_Renderer;
using Window = SDL_Window;
using Texture = SDL_Texture;
using Rect = SDL_Rect;
using FRect = SDL_FRect;
using Color = SDL_Color;
using Keycode = SDL_Keycode;
using Surface = SDL_Surface;
using Gamepad = SDL_GameController;
using Haptic = SDL_Haptic;
using PixelFormat = Uint32;
using FPoint = SDL_FPoint;
using EventFilter = SDL_EventFilter;

constexpr Uint32 TOUCH_MOUSE_ID = SDL_TOUCH_MOUSEID;

enum Align
{
    ALIGN_LEFT,
    ALIGN_MIDDLE,
    ALIGN_RIGHT
};

#define WINDOWPOS_CENTERED SDL_WINDOWPOS_CENTERED

//声音类型在其他文件中未使用
using AudioSpec = SDL_AudioSpec;
//这里直接使用SDL的事件结构
using EngineEvent = SDL_Event;

//这里直接照搬SDL
//更换底层需自己定义一套
//好像是瞎折腾
enum EngineEventType
{
    EVENT_FIRST = SDL_FIRSTEVENT,
    //按关闭按钮
    EVENT_QUIT = SDL_QUIT,
    //app事件
    EVENT_WILL_ENTER_BACKGROUND = SDL_APP_WILLENTERBACKGROUND,
    EVENT_DID_ENTER_BACKGROUND = SDL_APP_DIDENTERBACKGROUND,
    EVENT_WILL_ENTER_FOREGROUND = SDL_APP_WILLENTERFOREGROUND,
    EVENT_DID_ENTER_FOREGROUND = SDL_APP_DIDENTERFOREGROUND,
    //window
    //BP_WINDOWEVENT = SDL_WINDOWEVENT,
    //BP_SYSWMEVENT = SDL_SYSWMEVENT,
    //键盘
    EVENT_KEY_DOWN = SDL_KEYDOWN,
    EVENT_KEY_UP = SDL_KEYUP,
    EVENT_TEXT_EDITING = SDL_TEXTEDITING,
    EVENT_TEXT_INPUT = SDL_TEXTINPUT,
    //鼠标
    EVENT_MOUSE_MOTION = SDL_MOUSEMOTION,
    EVENT_MOUSE_BUTTON_DOWN = SDL_MOUSEBUTTONDOWN,
    EVENT_MOUSE_BUTTON_UP = SDL_MOUSEBUTTONUP,
    EVENT_MOUSE_WHEEL = SDL_MOUSEWHEEL,
    //手柄
    EVENT_JOYSTICK_AXIS_MOTION = SDL_JOYAXISMOTION,
    EVENT_JOYSTICK_BALL_MOTION = SDL_JOYBALLMOTION,
    EVENT_JOYSTICK_HAT_MOTION = SDL_JOYHATMOTION,
    EVENT_JOYSTICK_BUTTON_DOWN = SDL_JOYBUTTONDOWN,
    EVENT_JOYSTICK_BUTTON_UP = SDL_JOYBUTTONUP,
    EVENT_JOYSTICK_ADDED = SDL_JOYDEVICEADDED,
    EVENT_JOYSTICK_REMOVED = SDL_JOYDEVICEREMOVED,
    //游戏控制器
    EVENT_GAMEPAD_AXIS_MOTION = SDL_CONTROLLERAXISMOTION,
    EVENT_GAMEPAD_BUTTON_DOWN = SDL_CONTROLLERBUTTONDOWN,
    EVENT_GAMEPAD_BUTTON_UP = SDL_CONTROLLERBUTTONUP,
    EVENT_GAMEPAD_ADDED = SDL_CONTROLLERDEVICEADDED,
    EVENT_GAMEPAD_REMOVED = SDL_CONTROLLERDEVICEREMOVED,
    EVENT_GAMEPAD_REMAPPED = SDL_CONTROLLERDEVICEREMAPPED,
    //触摸
    EVENT_FINGER_DOWN = SDL_FINGERDOWN,
    EVENT_FINGER_UP = SDL_FINGERUP,
    EVENT_FINGER_MOTION = SDL_FINGERMOTION,

    //剪贴板
    EVENT_CLIPBOARD_UPDATE = SDL_CLIPBOARDUPDATE,
    //拖放文件
    EVENT_DROP_FILE = SDL_DROPFILE,
    //渲染改变
    EVENT_RENDER_TARGETS_RESET = SDL_RENDER_TARGETS_RESET,

    //窗口事件
    EVENT_WINDOW_SHOWN = 0x10000 + SDL_WINDOWEVENT_SHOWN,
    EVENT_WINDOW_HIDDEN = 0x10000 + SDL_WINDOWEVENT_HIDDEN,
    EVENT_WINDOW_EXPOSED = 0x10000 + SDL_WINDOWEVENT_EXPOSED,

    EVENT_WINDOW_MOVED = 0x10000 + SDL_WINDOWEVENT_MOVED,

    EVENT_WINDOW_RESIZED = 0x10000 + SDL_WINDOWEVENT_RESIZED,
    EVENT_WINDOW_PIXEL_SIZE_CHANGED = 0x10000 + SDL_WINDOWEVENT_SIZE_CHANGED,
    EVENT_WINDOW_MINIMIZED = 0x10000 + SDL_WINDOWEVENT_MINIMIZED,
    EVENT_WINDOW_MAXIMIZED = 0x10000 + SDL_WINDOWEVENT_MAXIMIZED,
    EVENT_WINDOW_RESTORED = 0x10000 + SDL_WINDOWEVENT_RESTORED,

    EVENT_WINDOW_MOUSE_ENTER = 0x10000 + SDL_WINDOWEVENT_ENTER,
    EVENT_WINDOW_MOUSE_LEAVE = 0x10000 + SDL_WINDOWEVENT_LEAVE,
    EVENT_WINDOW_FOCUS_GAINED = 0x10000 + SDL_WINDOWEVENT_FOCUS_GAINED,
    EVENT_WINDOW_FOCUS_LOST = 0x10000 + SDL_WINDOWEVENT_FOCUS_LOST,
    EVENT_WINDOW_CLOSE_REQUESTED = 0x10000 + SDL_WINDOWEVENT_CLOSE,

    EVENT_LAST = SDL_LASTEVENT
};

enum KeyBoard
{
    K_UNKNOWN = SDLK_UNKNOWN,
    K_LEFT = SDLK_LEFT,
    K_RIGHT = SDLK_RIGHT,
    K_UP = SDLK_UP,
    K_DOWN = SDLK_DOWN,
    K_SPACE = SDLK_SPACE,
    K_ESCAPE = SDLK_ESCAPE,
    K_RETURN = SDLK_RETURN,
    K_DELETE = SDLK_DELETE,
    K_BACKSPACE = SDLK_BACKSPACE,
    K_TAB = SDLK_TAB,
    K_0 = SDLK_0,
    K_1 = SDLK_1,
    K_2 = SDLK_2,
    K_3 = SDLK_3,
    K_4 = SDLK_4,
    K_PLUS = SDLK_PLUS,
    K_COMMA = SDLK_COMMA,
    K_MINUS = SDLK_MINUS,
    K_PERIOD = SDLK_PERIOD,
    K_EQUALS = SDLK_EQUALS,
    K_A = SDLK_a,
    K_B = SDLK_b,
    K_C = SDLK_c,
    K_D = SDLK_d,
    K_E = SDLK_e,
    K_F = SDLK_f,
    K_G = SDLK_g,
    K_H = SDLK_h,
    K_I = SDLK_i,
    K_J = SDLK_j,
    K_K = SDLK_k,
    K_L = SDLK_l,
    K_M = SDLK_m,
    K_N = SDLK_n,
    K_O = SDLK_o,
    K_P = SDLK_p,
    K_Q = SDLK_q,
    K_R = SDLK_r,
    K_S = SDLK_s,
    K_T = SDLK_t,
    K_U = SDLK_u,
    K_V = SDLK_v,
    K_W = SDLK_w,
    K_X = SDLK_x,
    K_Y = SDLK_y,
    K_Z = SDLK_z,
    K_PAGEUP = SDLK_PAGEUP,
    K_PAGEDOWN = SDLK_PAGEDOWN,
};

enum MouseButton
{
    BUTTON_LEFT = SDL_BUTTON_LEFT,
    BUTTON_MIDDLE = SDL_BUTTON_MIDDLE,
    BUTTON_RIGHT = SDL_BUTTON_RIGHT
};

enum BP_GameControllerButton
{
    GAMEPAD_BUTTON_INVALID = SDL_CONTROLLER_BUTTON_INVALID,
    GAMEPAD_BUTTON_SOUTH = SDL_CONTROLLER_BUTTON_A,
    GAMEPAD_BUTTON_EAST = SDL_CONTROLLER_BUTTON_B,
    GAMEPAD_BUTTON_WEST = SDL_CONTROLLER_BUTTON_X,
    GAMEPAD_BUTTON_NORTH = SDL_CONTROLLER_BUTTON_Y,
    GAMEPAD_BUTTON_BACK = SDL_CONTROLLER_BUTTON_BACK,
    GAMEPAD_BUTTON_GUIDE = SDL_CONTROLLER_BUTTON_GUIDE,
    GAMEPAD_BUTTON_START = SDL_CONTROLLER_BUTTON_START,
    GAMEPAD_BUTTON_LEFT_STICK = SDL_CONTROLLER_BUTTON_LEFTSTICK,
    GAMEPAD_BUTTON_RIGHT_STICK = SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    GAMEPAD_BUTTON_LEFT_SHOULDER = SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    GAMEPAD_BUTTON_RIGHT_SHOULDER = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    GAMEPAD_BUTTON_DPAD_UP = SDL_CONTROLLER_BUTTON_DPAD_UP,
    GAMEPAD_BUTTON_DPAD_DOWN = SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    GAMEPAD_BUTTON_DPAD_LEFT = SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    GAMEPAD_BUTTON_DPAD_RIGHT = SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    GAMEPAD_BUTTON_MISC1 = SDL_CONTROLLER_BUTTON_MISC1,
    GAMEPAD_BUTTON_RIGHT_PADDLE1 = SDL_CONTROLLER_BUTTON_PADDLE1,
    GAMEPAD_BUTTON_LEFT_PADDLE1 = SDL_CONTROLLER_BUTTON_PADDLE2,
    GAMEPAD_BUTTON_RIGHT_PADDLE2 = SDL_CONTROLLER_BUTTON_PADDLE3,
    GAMEPAD_BUTTON_LEFT_PADDLE2 = SDL_CONTROLLER_BUTTON_PADDLE4,
    GAMEPAD_BUTTON_TOUCHPAD = SDL_CONTROLLER_BUTTON_TOUCHPAD,
    GAMEPAD_BUTTON_COUNT = SDL_CONTROLLER_BUTTON_MAX,
};

enum GameControllerAxis
{
    GAMEPAD_AXIS_INVALID = SDL_CONTROLLER_AXIS_INVALID,
    GAMEPAD_AXIS_LEFTX = SDL_CONTROLLER_AXIS_LEFTX,
    GAMEPAD_AXIS_LEFTY = SDL_CONTROLLER_AXIS_LEFTY,
    GAMEPAD_AXIS_RIGHTX = SDL_CONTROLLER_AXIS_RIGHTX,
    GAMEPAD_AXIS_RIGHTY = SDL_CONTROLLER_AXIS_RIGHTY,
    GAMEPAD_AXIS_LEFT_TRIGGER = SDL_CONTROLLER_AXIS_TRIGGERLEFT,
    GAMEPAD_AXIS_RIGHT_TRIGGER = SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
    GAMEPAD_AXIS_COUNT = SDL_CONTROLLER_AXIS_MAX,
};

enum TextureAccess
{
    TEXTUREACCESS_STATIC = SDL_TEXTUREACCESS_STATIC,       /**< Changes rarely, not lockable */
    TEXTUREACCESS_STREAMING = SDL_TEXTUREACCESS_STREAMING, /**< Changes frequently, lockable */
    TEXTUREACCESS_TARGET = SDL_TEXTUREACCESS_TARGET        /**< Texture can be used as a render target */
};

enum BlendMode
{
    BLENDMODE_NONE = SDL_BLENDMODE_NONE,
    BLENDMODE_BLEND = SDL_BLENDMODE_BLEND,
    BLENDMODE_BLEND_PREMULTIPLIED = SDL_BLENDMODE_BLEND_PREMULTIPLIED,
    BLENDMODE_ADD = SDL_BLENDMODE_ADD,
    BLENDMODE_ADD_PREMULTIPLIED = SDL_BLENDMODE_ADD_PREMULTIPLIED,
    BLENDMODE_MOD = SDL_BLENDMODE_MOD,
    BLENDMODE_MUL = SDL_BLENDMODE_MUL,
    BLENDMODE_INVALID = SDL_BLENDMODE_INVALID
};

class Engine
{
private:
    Engine();
    virtual ~Engine();

public:
    static Engine* getInstance()
    {
        static Engine e;
        return &e;
    }

#ifdef __ANDROID__
    static void extractAssetsIfNeeded();
#endif

    //图形相关
private:
    bool inited_ = false;
    Window* window_ = nullptr;
    Renderer* renderer_ = nullptr;
    Texture* tex_ = nullptr;
    //Texture* tex2_ = nullptr;
    //Texture* logo_ = nullptr;
    Rect rect_;
    bool full_screen_ = false;
    bool keep_ratio_ = true;
    bool texture_top_left_highlight_enabled_ = true;

    int ui_w_ = 1024, ui_h_ = 640;
    int base_ui_w_ = 1024, base_ui_h_ = 640;
    int win_w_, win_h_, min_x_, min_y_, max_x_, max_y_;
    double rotation_ = 0;
    double ratio_x_ = 1, ratio_y_ = 1;

    int render_times_ = 0;

    int max_texture_size_ = 0;

    int window_mode_ = 0;    //0-窗口和渲染器自行创建，1-窗口和渲染器由外部创建
    bool renderer_self_ = false;

    std::unordered_map<std::string, Texture*> tex_map_;

public:
    int init(void* handle = nullptr, int handle_type = 0, int maximized = 0, const std::string& str = "", int fullscreen = 0);

    Window* getWindow() const { return window_; }

    void getWindowSize(int& w, int& h) const { SDL_GetWindowSize(window_, &w, &h); }

    void getWindowMaxSize(int& w, int& h) const { SDL_GetWindowMaximumSize(window_, &w, &h); }

    int getWindowWidth() const;
    int getWindowHeight() const;

    int getUIWidth() const { return ui_w_; }

    int getUIHeight() const { return ui_h_; }

    int getMaxTextureSize() const { return max_texture_size_; }

    int getMaxWindowWidth() const { return max_x_ - min_x_; }

    int getMaxWindowHeight() const { return max_y_ - min_y_; }

    void getWindowPosition(int& x, int& y) const { SDL_GetWindowPosition(window_, &x, &y); }

    bool getWindowIsMaximized() const { return SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED; }

    void setWindowIsMaximized(bool b) const;
    void setWindowSize(int w, int h);

    void setUISize(int w, int h)
    {
        ui_w_ = w;
        ui_h_ = h;
        base_ui_w_ = w;
        base_ui_h_ = h;
    }

    void getUISize(int& w, int& h) const
    {
        w = ui_w_;
        h = ui_h_;
    }

    void setWindowPosition(int x, int y) const;

    void setWindowTitle(const std::string& str) const { SDL_SetWindowTitle(window_, str.c_str()); }

    Renderer* getRenderer() const { return renderer_; }

    void createMainTexture(PixelFormat pixfmt, TextureAccess a, int w, int h);
    void resizeMainTexture(int w, int h) const;
    void createAssistTexture(const std::string& name, int w, int h);
    bool resizeRenderTexturesToWindow();
    void setPresentPosition(Texture* tex);    //设置贴图的位置

    //void getPresentSize(int& w, int& h) { w = rect_.w; h = rect_.h; }
    int getPresentWidth() const { return rect_.w; }

    int getPresentHeight() const { return rect_.h; }

    float getDisplayScale() const
    {
        if (ui_w_ == 0)
        {
            return 1.0f;
        }
        return std::max(1.0f, float(rect_.w) / ui_w_);
    }

    void getPresentRect(int& x, int& y, int& w, int& h) const
    {
        x = rect_.x;
        y = rect_.y;
        w = rect_.w;
        h = rect_.h;
    }

    bool windowToUISpace(int wx, int wy, int& ux, int& uy, bool clamp = false) const;
    void uiToWindowSpace(int ux, int uy, int& wx, int& wy) const;

    Texture* getMainTexture() const { return tex_; }

    void getAssistTextureSize(const std::string& name, int& w, int& h) { getTextureSize(tex_map_[name], w, h); }

private:
    Texture* createTexture(PixelFormat pix_fmt, TextureAccess a, int w, int h) const;
    Texture* createYUVTexture(int w, int h) const;
    static void updateYUVTexture(Texture* t, uint8_t* data0, int size0, uint8_t* data1, int size1, uint8_t* data2, int size2);
    Texture* createTexture(int w, int h);
    Texture* createRenderedTexture(int w, int h);
    static void updateTexture(Texture* t, uint8_t* buffer, int pitch);
    static int lockTexture(Texture* t, Rect* r, void** pixel, int* pitch);
    static void unlockTexture(Texture* t);

public:
    static void destroyTexture(Texture* t) { SDL_DestroyTexture(t); }

public:
    //void showLogo() { renderTexture(logo_, nullptr, nullptr); }

    void renderPresent() const;

    void renderClear() const { SDL_RenderClear(renderer_); }

    static void setTextureAlphaMod(Texture* t, uint8_t alpha) { SDL_SetTextureAlphaMod(t, alpha); }

    static void setTextureScaleNearest(Texture* t) { SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest); }
    static void setTextureScaleLinear(Texture* t) { SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear); }

    static void getTextureSize(Texture* t, int& w, int& h)
    {
        float wf, hf;
        SDL_GetTextureSize(t, &wf, &hf);
        w = wf;
        h = hf;
    }

    void setRenderTarget(Texture* t) const {
        SDL_SetRenderTarget(renderer_, t);
        SDL_RenderSetScale(renderer_, t && t == tex_ ? 320.0f/ui_w_ : 1.0f,
                                      t && t == tex_ ? 180.0f/ui_h_ : 1.0f);
    }

    Texture* getRenderTarget() const { return SDL_GetRenderTarget(renderer_); }

    void resetRenderTarget() const { setRenderTarget(nullptr); }

    static void createWindow() {}

    static void createRenderer() {}

    void renderTexture(Texture* t = nullptr, double angle = 0);
    void renderTexture(Texture* t, int x, int y, int w = 0, int h = 0, double angle = 0, int inPresent = 0);
    void renderTexture(Texture* t, Rect* rect0, Rect* rect1, double angle = 0, int inPresent = 0);
    void renderTexture(Texture* t, Rect* rect0, const std::vector<FPoint>& v, const std::vector<FPoint>& v2);
    void renderTextureMesh(Texture* t, const std::vector<FPoint>& v, const std::vector<FPoint>& v2,
        const std::vector<Color>& colors, const std::vector<int>& indices, float top_left_brightness = 0.0f);
    void renderTextureLight(Texture* t, Rect* rect0, Rect* rect1, const std::vector<Color>& colors,
        const std::vector<float>& brightness_v = {}, double angle = 0);

    void setTextureTopLeftHighlightEnabled(bool enabled) { texture_top_left_highlight_enabled_ = enabled; }

    void destroy() const;
    bool isFullScreen();
    void setFullScreen(bool b);
    void toggleFullscreen();
    Texture* loadImage(const std::string& filename, int as_white = 0);
    Texture* loadImageFromMemory(const std::string& content, int as_white = 0) const;
    static void toWhite(Surface* sur);
    bool setKeepRatio(bool b);
    Texture* transRGBABitmapToTexture(const uint8_t* src, uint32_t color, int w, int h, int stride) const;

    double setRotation(double r) { return rotation_ = r; }

    void resetWindowPosition();

    void setRatio(double x, double y)
    {
        ratio_x_ = x;
        ratio_y_ = y;
    }

    static void setColor(Texture* tex, Color c);
    void fillColor(Color color, int x, int y, int w, int h, BlendMode blend = BLENDMODE_BLEND) const;

    void setRenderMainTexture() const { setRenderTarget(tex_); }

    void renderMainTextureToWindow();

    static int setTextureBlendMode(Texture* t) { return SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND); }

    void resetRenderTimes(int t = 0) { render_times_ = t; }

    int getRenderTimes() const { return render_times_; }

    //除了纹理管理器和字体管理器，其余的类都只能使用string来标识纹理，避免泄露
    void createRenderedTexture(const std::string& name, int w, int h);
    void renderTextureToMain(const std::string& name);
    Texture* getTexture(const std::string& name) { return tex_map_[name]; }
    void setRenderTarget(const std::string& name) { setRenderTarget(tex_map_[name]); }

    //事件相关
private:
    uint64_t time_;

public:
    static void delay(double t) { SDL_Delay((Uint32)std::max(0.0, t)); }

    static double getTicks()
    {
        static auto start = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()).time_since_epoch().count();
        return 1e-6 * (std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()).time_since_epoch().count() - start);
    }

    uint64_t tic() { return time_ = getTicks(); }

    void toc()
    {
        if (getTicks() != time_)
        {
            std::print("{}\n", getTicks() - time_);
        }
    }

    static void getMouseState(int& x, int& y);
    void getMouseStateInStartWindow(int& x, int& y) const;
    void setMouseState(int x, int y) const;
    void setMouseStateInStartWindow(int x, int y) const;
    int pollEvent(EngineEvent& e);

    static int pollEvent() { return SDL_PollEvent(nullptr); }

    static int pushEvent(EngineEvent& e) { return SDL_PushEvent(&e); }

    static void flushEvent() { SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT); }

    static void free(void* mem) { SDL_free(mem); }

    static bool checkKeyPress(Keycode key);

    static bool checkMouseButtonPress(MouseButton button)
    {
        return SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(button);
    }

    static void addEventWatch(KysEventFilter filter, void* userdata)
    {
        kys_add_event_watch(filter, userdata);
    }

private:
    std::vector<Gamepad*> game_controllers_;
    std::vector<int> nintendo_switch_;
    Gamepad* cur_game_controller_ = nullptr;
    double prev_controller_press_ = 0;
    double interval_controller_press_ = 0;
    std::unordered_map<int, int> virtual_stick_button_;
    int16_t virtual_stick_axis_[6] = { 0 };
    float touch_pinch_scale_ = 1.0f;
    SDL_FPoint touch_camera_pan_{};

public:
    bool gameControllerGetButton(int key);
    int16_t gameControllerGetAxis(int axis);
    void gameControllerRumble(int l, int h, uint32_t time) const;

    void setInterValControllerPress(double t) { interval_controller_press_ = t; }

    void setGameControllerButton(int key, int value) { virtual_stick_button_[key] = value; }

    void setGameControllerAxis(int axis, int16_t value) { virtual_stick_axis_[axis] = value; }

    void setTouchPinchScale(float scale) { touch_pinch_scale_ = scale; }

    float consumeTouchPinchScale()
    {
        float scale = touch_pinch_scale_;
        touch_pinch_scale_ = 1.0f;
        return scale;
    }

    void setTouchCameraPan(float x, float y) { touch_camera_pan_ = { x, y }; }

    SDL_FPoint consumeTouchCameraPan()
    {
        SDL_FPoint pan = touch_camera_pan_;
        touch_camera_pan_ = {};
        return pan;
    }

    void clearGameControllerButton()
    {
        for (auto& i : virtual_stick_button_)
        {
            i.second = 0;
        }
    }

    void clearGameControllerAxis()
    {
        for (auto& i : virtual_stick_axis_)
        {
            i = 0;
        }
    }

    void checkGameControllers();

    //UI相关
private:
    Texture* square_ = nullptr;

    Texture* createRectTexture(int w, int h, int style) const;

public:
    Texture* createTextTexture(const std::string& fontname, const std::string& text, int size, Color c) const;
    Texture* createTextTexture(TTF_Font* font, const std::string& text, Color c) const;
    int showMessage(const std::string& content) const;
    void renderSquareTexture(Rect* rect, Color color, uint8_t alpha);

public:
    //标题;
    std::string title_;

public:
    int saveScreen(const char* filename) const;
    int saveTexture(Texture* tex, const char* filename) const;

    //输入相关
    void startTextInput() { SDL_StartTextInput(); }

    void stopTextInput() { SDL_StopTextInput(); }

    void setTextInputArea(int x, int y, int w = 0, int h = 0)
    {
        Rect r = { x, y, w, h };
        SDL_SetTextInputRect(&r);
    }

};

struct DisableStream
{
    DisableStream(FILE* f)
    {
        f_ = f;
        setvbuf(f, buf_, _IOFBF, BUFSIZ);
    }

    ~DisableStream()
    {
        memset(buf_, 0, BUFSIZ);
        setvbuf(f_, nullptr, _IONBF, 0);
    }

    void clear() { memset(buf_, 0, BUFSIZ); }

private:
    char buf_[BUFSIZ] = { 0 };
    FILE* f_;
};
