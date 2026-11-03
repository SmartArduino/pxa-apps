#include "Application.h"
#include "GameUtil.h"
#include "Engine.h"
#include "sqlite3.h"
#include "Cifa.h"
#include "ZipFile2.h"
#include <sys/stat.h>
extern "C" {
#include "port.h"
#include "font8x8.h"
}
static void text(SDL_Renderer* r,int x,int y,const char* s) {
    for(;*s;++s,x+=8)for(int row=0;row<8;++row)for(int col=0;col<8;++col)
        if(font8x8_basic[(unsigned char)*s][row]&(1<<col))SDL_RenderDrawPoint(r,x+col,y+row);
}
bool kys_backend_checks();
static bool render_check(SDL_Renderer* r) {
    auto previous=SDL_GetRenderTarget(r);
    auto target=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,16,16);
    if(!target || SDL_SetRenderTarget(r,target))return false;
    SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);
    KysVertex vertices[]={{{0,0},{.25f,.5f,1,1},{0,0}},{{16,0},{.25f,.5f,1,1},{1,0}},
                          {{16,16},{.25f,.5f,1,1},{1,1}},{{0,16},{.25f,.5f,1,1},{0,1}}};
    int indices[]={0,1,2,0,2,3};
    bool ok=kys_render_geometry(r,nullptr,vertices,4,indices,6)==0;
    SDL_Rect rect={0,0,16,16};auto surface=kys_read_pixels(r,&rect);
    Uint8 red=0,green=0,blue=0,alpha=0;
    ok=ok && surface && SDL_ReadSurfacePixel(surface,8,8,&red,&green,&blue,&alpha)
        && abs(int(red)-64)<=1 && abs(int(green)-128)<=1 && blue==255;
    if(surface)SDL_FreeSurface(surface);
    SDL_SetRenderTarget(r,previous);SDL_DestroyTexture(target);
    port_log("KYS CHECK geometry=%s pixel=%u,%u,%u",ok?"PASS":"FAIL",red,green,blue);
    return ok;
}
static bool audio_check() {
    // A brief, low-volume test tone checks accepted PCM writes on the real Host.
    int16_t samples[320];
    for(int block=0;block<10;++block) {
        for(int i=0;i<320;++i)samples[i]=int16_t(1000*sin(2*M_PI*440*(block*320+i)/16000.));
        port_pcm(samples,320,16000,1);
    }
    bool ok=port_audio_session()!=0;
    port_log("KYS CHECK pcm-submit=%s (audibility needs listening)",ok?"PASS":"FAIL");
    return ok;
}
static bool preflight() {
    const char* required[]={"config/kysmod.ini","font/chinese.ttf"};
    bool ok=true;
    for(auto p:required)if(!filefunc::fileExist(GameUtil::PATH()+p)) {port_log("KYS missing resource: %s",p);ok=false;}
    if(!filefunc::fileExist("/data/save/0.db") && !filefunc::fileExist("/data/save/0.zip")) {
        port_log("KYS missing resource: save/0.db or save/0.zip");ok=false;
    }
    return ok;
}
static bool materialize(const char* path) {
    // A read/write open probes only the private root. Read-only opens can fall
    // back to media. Keep the media handle open while creating the private copy.
    if(auto fp=fopen(path,"r+b")){fclose(fp);return true;}
    auto input=fopen(path,"rb");if(!input)return false;
    auto output=fopen(path,"wb");if(!output){fclose(input);return false;}
    char bytes[4096];size_t n;bool ok=true;
    while((n=fread(bytes,1,sizeof(bytes),input)))if(fwrite(bytes,1,n,output)!=n){ok=false;break;}
    ok=ok && !ferror(input) && fflush(output)==0;
    fclose(input);if(fclose(output))ok=false;
    if(!ok)remove(path);return ok;
}
static int resource_screen() {
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_AUDIO);
    auto window=SDL_CreateWindow("kys-cpp",0,0,320,240,0);
    auto renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!window || !renderer)return 1;
    auto pad=SDL_IsGameController(0)?SDL_GameControllerOpen(0):nullptr;
    port_log("KYS resource check: incomplete; gameplay has NOT been validated");
    int input_count=0;bool run=true;
    while(run) {
        SDL_Event e;
        while(SDL_PollEvent(&e)) {
            if(e.type==SDL_QUIT || (e.type==SDL_KEYUP && e.key.keysym.sym==SDLK_ESCAPE))run=false;
            if(e.type==SDL_CONTROLLERBUTTONUP || e.type==SDL_MOUSEBUTTONUP || e.type==SDL_KEYUP) {
                ++input_count;
                if ((e.type==SDL_CONTROLLERBUTTONUP && e.cbutton.button==SDL_CONTROLLER_BUTTON_A) || (e.type==SDL_KEYUP && e.key.keysym.sym==SDLK_RETURN)) {
                    bool checks=kys_backend_checks();checks=render_check(renderer) && checks;
                    checks=audio_check() && checks;
                    port_log("KYS PLATFORM CHECKS %s",checks?"PASS":"FAIL");
                }
                port_log("KYS input event type=%u count=%d",e.type,input_count);
                if(e.type==SDL_CONTROLLERBUTTONUP && e.cbutton.button==SDL_CONTROLLER_BUTTON_B)run=false;
            }
        }
        SDL_SetRenderDrawColor(renderer,15,23,35,255);SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer,220,184,100,255);text(renderer,32,32,"KYS-CPP / PXA");
        SDL_SetRenderDrawColor(renderer,225,229,237,255);
        text(renderer,16,64,"Original game resources needed");
        text(renderer,16,88,"config/kysmod.ini");text(renderer,16,104,"font/chinese.ttf");text(renderer,16,120,"save/0.db + maps + textures");
        text(renderer,16,152,"Copy game files to app media.");text(renderer,16,176,"Press keys or touch to test.");text(renderer,16,200,"A: checks / B: back");
        SDL_FRect box={220,192,70,18};SDL_SetRenderDrawColor(renderer,input_count%2?50:20,120,160,255);SDL_RenderFillRect(renderer,&box);
        SDL_RenderPresent(renderer);
    }
    if(pad)SDL_GameControllerClose(pad);
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
extern "C" int pxa_sdl_game_main(int,char**) {
    GameUtil::PATH()="/data/";
    port_log("KYS upstream f93be8a7; PXA offline port starting");
    if(!preflight())return resource_screen();
    mkdir("/data/config",0700);mkdir("/data/save",0700);
    if(!materialize("/data/config/kysmod.ini"))return 2;
    if(filefunc::fileExist("/data/save/0.db") && !materialize("/data/save/0.db"))return 2;
    Application app;return app.run();
}
