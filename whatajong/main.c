/* SPDX-License-Identifier: MIT. QuickJS/PXA boundary, no browser or host filesystem. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wasi/api.h>
#include "quickjs/quickjs.h"
#include "pxa_framebuffer.h"
#include "font.h"
#include "game-bundle.h"
#define WIDTH 800
#define HEIGHT 600
static uint8_t pixels[WIDTH*HEIGHT];
static uint8_t upload[PXA_RASTER_UPLOAD_HEADER_BYTES+sizeof(pixels)];
static pxfb_t fb;
static JSRuntime *runtime;
static JSContext *context;
static int fatal;
static unsigned trace_sequence;
static void rect(int x,int y,int w,int h,uint8_t color) {
    if(x<0){w+=x;x=0;} if(y<0){h+=y;y=0;}
    if(x+w>WIDTH)w=WIDTH-x; if(y+h>HEIGHT)h=HEIGHT-y;
    if(w<=0||h<=0)return;
    for(int row=y;row<y+h;row++)memset(pixels+row*WIDTH+x,color,w);
}
static void text_at(int x,int y,const char *text,uint8_t color,int scale) {
    if(scale<1||scale>3)scale=1;
    for(;*text;text++,x+=8*scale) {
        unsigned char code=(unsigned char)*text;
        if(code<32||code>126)code='?';
        const uint8_t *rows=brogue_font[code-32].rows;
        for(int yy=0;yy<12;yy++)for(int xx=0;xx<8;xx++)
            if(rows[yy]&(128u>>xx))rect(x+xx*scale,y+yy*scale,scale,scale,color);
    }
}
static JSValue native_rect(JSContext *ctx,JSValueConst self,int argc,JSValueConst *args) {
    (void)self;int32_t n[5]={0};for(int i=0;i<argc&&i<5;i++)JS_ToInt32(ctx,n+i,args[i]);
    rect(n[0],n[1],n[2],n[3],n[4]);return JS_UNDEFINED;
}
static JSValue native_text(JSContext *ctx,JSValueConst self,int argc,JSValueConst *args) {
    (void)self;if(argc<5)return JS_UNDEFINED;int32_t x,y,c,s;
    JS_ToInt32(ctx,&x,args[0]);JS_ToInt32(ctx,&y,args[1]);
    JS_ToInt32(ctx,&c,args[3]);JS_ToInt32(ctx,&s,args[4]);
    const char *text=JS_ToCString(ctx,args[2]);if(text){text_at(x,y,text,c,s);JS_FreeCString(ctx,text);}return JS_UNDEFINED;
}
static JSValue native_trace(JSContext *ctx,JSValueConst self,int argc,JSValueConst *args) {
    (void)self;if(argc){const char *s=JS_ToCString(ctx,args[0]);if(s){
        /* Public SDK caps each log message at 256 bytes. Serialize large
           snapshots as numbered chunks; never silently discard diagnostics. */
        size_t length=strlen(s);unsigned parts=(unsigned)((length+179)/180),sequence=++trace_sequence;
        for(unsigned part=0;part<parts;part++) {
            char line[256],number[12];unsigned at=0;
            const char *prefix="WHATAJONG CHUNK ";while(*prefix)line[at++]=*prefix++;
            unsigned values[]={sequence,part,parts};
            for(unsigned i=0;i<3;i++) {
                unsigned value=values[i],n=0;do{number[n++]=(char)('0'+value%10);value/=10;}while(value);
                while(n)line[at++]=number[--n];line[at++]=' ';
            }
            size_t offset=part*180,count=length-offset;if(count>180)count=180;
            memcpy(line+at,s+offset,count);line[at+count]=0;
            pxa_log_write(PXA_LOG_LEVEL_INFO,line);
        }
        JS_FreeCString(ctx,s);}}
    return JS_UNDEFINED;
}
static JSValue native_random(JSContext *ctx,JSValueConst self,int argc,JSValueConst *args) {
    (void)self;(void)argc;(void)args;uint32_t value=0;__wasi_random_get((uint8_t*)&value,sizeof(value));
    return JS_NewFloat64(ctx,value/4294967296.0);
}
static int exception(JSValue value) {
    if(!JS_IsException(value)){JS_FreeValue(context,value);return 0;}
    JSValue ex=JS_GetException(context);const char *s=JS_ToCString(context,ex);
    if(s){pxa_log_write(PXA_LOG_LEVEL_ERROR,s);JS_FreeCString(context,s);}
    JSValue stack=JS_GetPropertyStr(context,ex,"stack");s=JS_ToCString(context,stack);
    if(s){pxa_log_write(PXA_LOG_LEVEL_ERROR,s);JS_FreeCString(context,s);}
    JS_FreeValue(context,stack);JS_FreeValue(context,ex);fatal=1;return -1;
}
static int call(const char *name,int argc,JSValue *args) {
    JSValue global=JS_GetGlobalObject(context),fn=JS_GetPropertyStr(context,global,name);
    JSValue result=JS_Call(context,fn,global,argc,args);
    JS_FreeValue(context,fn);JS_FreeValue(context,global);
    for(int i=0;i<argc;i++)JS_FreeValue(context,args[i]);return exception(result);
}
static void install(const char *name,JSCFunction *fn,int argc) {
    JSValue global=JS_GetGlobalObject(context);
    JS_SetPropertyStr(context,global,name,JS_NewCFunction(context,fn,name,argc));
    JS_FreeValue(context,global);
}
int32_t pxa_app_start(const uint8_t *config,uint32_t length) {
    memset(&fb,0,sizeof(fb));fb.width=WIDTH;fb.height=HEIGHT;
    fb.pixels=pixels;fb.upload=upload;fb.upload_bytes=sizeof(upload);fatal=0;trace_sequence=0;
    const uint16_t colors[]={0x0927,0x0841,0x198b,0x8410,0x3186,0x5aeb,0x33b0,0xff03,0xffba,0xbc31,0x23a5,0xd922,0x143c};
    for(unsigned i=0;i<sizeof(colors)/sizeof(colors[0]);i++)fb.palette[i]=colors[i];
    runtime=JS_NewRuntime();if(!runtime)return PXA_STATUS_RESOURCE_LIMIT;
    JS_SetMemoryLimit(runtime,24*1024*1024);JS_SetMaxStackSize(runtime,768*1024);
    context=JS_NewContext(runtime);if(!context)return PXA_STATUS_RESOURCE_LIMIT;
    install("pxaRect",native_rect,5);install("pxaText",native_text,5);
    install("pxaTrace",native_trace,1);install("pxaRandom",native_random,0);
    const char bootstrap[]="globalThis.crypto={getRandomValues(a){for(let i=0;i<a.length;i++)a[i]=Math.floor(pxaRandom()*256);return a}};globalThis.__pxa_element=()=>null;globalThis.__pxa_fragment=()=>null;globalThis.__pxa_timers=[];globalThis.__pxa_now=0;globalThis.setTimeout=(fn,ms=0)=>{const id=__pxa_timers.length;__pxa_timers.push({fn,at:__pxa_now+ms});return id};globalThis.clearTimeout=id=>{__pxa_timers[id]=null};globalThis.__pxaAdvanceTimers=ms=>{__pxa_now=ms;let count=0;for(let i=0;i<__pxa_timers.length;i++){const t=__pxa_timers[i];if(t&&t.at<=ms){__pxa_timers[i]=null;t.fn();count++}}return count};";
    if(exception(JS_Eval(context,bootstrap,sizeof(bootstrap)-1,"pxa-bootstrap.js",JS_EVAL_TYPE_GLOBAL)))return PXA_STATUS_INTERNAL;
    if(exception(JS_Eval(context,(const char*)whatajong_js,whatajong_js_bytes,"whatajong-original.bundle.js",JS_EVAL_TYPE_GLOBAL)))return PXA_STATUS_INTERNAL;
    fb.pointer_controller=1;int32_t status=pxfb_start(&fb,config,length,100);
    pxa_log_write(PXA_LOG_LEVEL_INFO,"WHATAJONG QuickJS ready: original rules/maps, touch/text frontend");return status;
}
int32_t pxa_app_on_event(const uint8_t *bytes,uint32_t length) {
    uint32_t flags=pxfb_event(&fb,bytes,length);
    if(flags&PXFB_ERROR)return fb.error;
    if(fatal)return PXA_STATUS_INTERNAL;
    if(flags&PXFB_POINTER&&fb.pointer_phase==PXA_POINTER_DOWN) {
        JSValue args[]={JS_NewInt32(context,fb.pointer_x),JS_NewInt32(context,fb.pointer_y)};
        if(call("whatajongTouch",2,args))return PXA_STATUS_INTERNAL;
    }
    if(flags&PXFB_BACK){JSValue args[]={JS_NewInt32(context,720),JS_NewInt32(context,32)};call("whatajongTouch",2,args);}
    if(flags&PXFB_TICK){JSValue args[]={JS_NewUint32(context,fb.time_ms)};if(call("whatajongTick",1,args))return PXA_STATUS_INTERNAL;}
    if(flags&(PXFB_READY|PXFB_TICK|PXFB_POINTER|PXFB_BACK)) {
        int32_t status=pxfb_present(&fb);if(status<0)return status;
    }
    return PXA_STATUS_OK;
}
void pxa_app_stop(uint32_t reason) {
    (void)reason;pxfb_stop(&fb);
    if(context)JS_FreeContext(context);if(runtime)JS_FreeRuntime(runtime);
    context=NULL;runtime=NULL;
}
