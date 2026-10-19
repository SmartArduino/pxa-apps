// Build once against the pinned, unmodified upstream OpenGL engine, and once
// against vendor/classic (_GAPI_SW), then compare identical scripted cameras.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#ifdef OL_REFERENCE_GL
#include <SDL2/SDL.h>
#endif
#include "game.h"

static int milliseconds;
int osGetTimeMS() { return milliseconds; }
bool osJoyReady(int i) { return i == 0; }
void osJoyVibrate(int, float, float) {}
const char *osFixFileName(const char *name) { return name; }

static void capture(const char *directory, const char *name) {
    char filename[1024];
    snprintf(filename, sizeof(filename), "%s/%s.ppm", directory, name);
    FILE *out = fopen(filename, "wb");
    if (!out) abort();
    fprintf(out, "P6\n320 240\n255\n");
#ifdef OL_REFERENCE_GL
    uint8_t rgba[320 * 240 * 4];
    glReadPixels(0, 0, 320, 240, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    for (int y = 239; y >= 0; --y)
        for (int x = 0; x < 320; ++x) fwrite(rgba + (y * 320 + x) * 4, 1, 3, out);
#else
    for (int i = 0; i < 320 * 240; ++i) {
        uint16_t c = GAPI::swColor[i];
        uint8_t rgb[] = {uint8_t(((c >> 11) * 255 + 15) / 31),
            uint8_t((((c >> 5) & 63) * 255 + 31) / 63), uint8_t(((c & 31) * 255 + 15) / 31)};
        fwrite(rgb, 1, 3, out);
    }
#endif
    fclose(out);
    auto *p = Game::level->players[0];
    auto *c = Game::level->camera;
    printf("POSE %s lara=%.3f,%.3f,%.3f yaw=%.6f camera=%.3f,%.3f,%.3f\n",
        name, p->pos.x, p->pos.y, p->pos.z, p->angle.y, c->eye.pos.x, c->eye.pos.y, c->eye.pos.z);
    printf("VIEW %s fov=%.3f aspect=%.6f target=%.3f,%.3f,%.3f direction=%.6f,%.6f,%.6f viewport=%d,%d,%d,%d\n",
        name,c->fov,c->aspect,c->target.pos.x,c->target.pos.y,c->target.pos.z,
        c->mViewInv.e02,c->mViewInv.e12,c->mViewInv.e22,
        Core::viewport.x,Core::viewport.y,Core::viewport.z,Core::viewport.w);
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
#ifdef OL_REFERENCE_GL
    if (SDL_Init(SDL_INIT_VIDEO)) return 3;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    auto *window = SDL_CreateWindow("OpenLara original reference", 0, 0, 320, 240,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window || !SDL_GL_CreateContext(window)) {fprintf(stderr, "%s\n", SDL_GetError()); return 4;}
#endif
    Core::width = 320; Core::height = 240; Core::defLang = 0;
    Game::init(argv[1]);
#ifdef OL_REFERENCE_GL
    // Match the handheld's original TR1 palette/vertex-lighting settings.
    Core::settings.detail.setFilter(Core::Settings::LOW);
    Core::settings.detail.setLighting(Core::Settings::LOW);
    Core::settings.detail.setShadows(Core::Settings::LOW);
    Core::settings.detail.setWater(Core::Settings::LOW);
    delete shaderCache; shaderCache = new ShaderCache();
    Game::startLevel(new Stream(argv[1]));
#else
    static uint16_t pixels[320 * 240];
    GAPI::resize(); GAPI::swColor = pixels;
#endif
    Core::settings.audio.sound = Core::settings.audio.music = 0;
    Core::resetTime();
    auto frames = [](int count, JoyKey key = jkNone) {
        if (key != jkNone) Input::setJoyDown(0, key, true);
        for (int i = 0; i < count; ++i) {milliseconds += 34; if (Game::update()) Game::render();}
        if (key != jkNone) Input::setJoyDown(0, key, false);
    };
    frames(180); capture(argv[2], "start");
    frames(25, jkRight); frames(20); capture(argv[2], "turned");
    frames(65, jkUp); frames(30); capture(argv[2], "walked");
    frames(50, jkLeft); frames(20); capture(argv[2], "reverse");
    frames(90, jkUp); frames(30); capture(argv[2], "doorway");
    Game::deinit();
#ifdef OL_REFERENCE_GL
    SDL_DestroyWindow(window); SDL_Quit();
#endif
}
