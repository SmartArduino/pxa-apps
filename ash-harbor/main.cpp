// SPDX-License-Identifier: MIT
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/ui_display.hpp>
#include <pxa/game_upload.hpp>
#include "combat.hpp"
#include "art.hpp"
#include "camera.hpp"
#include "game_audio.hpp"
#include "frame_cache.hpp"
#include "render_pacing.hpp"
#include "touch.hpp"
#include "ui_layout.hpp"
#include "text_layout.hpp"
#include "netplay.hpp"
#include "battle_feedback.hpp"
#include <cstdio>
#include <cstring>

namespace ash {
static_assert(std::size(art::glyphs)==74,"Both letter cases, digits and punctuation must be packed");
constexpr uint16_t rgb(unsigned r,unsigned g,unsigned b){return ((r&248)<<8)|((g&252)<<3)|(b>>3);}
constexpr auto ink=rgb(9,17,30),panel=rgb(20,36,52),gold=rgb(246,192,79),
    teal=rgb(58,199,182),red=rgb(222,79,52),ivory=rgb(255,231,170);
enum class View {home,play,pause,commands,result,opening,roster,practice,opponent,stages,online,address};
constexpr const char* difficultyNames[]={"入门","标准","高手"};
constexpr const char* practiceNames[]={"站立","全防御","受击后防","自由对练"};
constexpr const char* stageNames[]={"雨港夜渡","竹影晨院","雪关古门","灯河石桥"};
constexpr const char* shortStageNames[]={"雨港","竹影","雪关","灯河"};
struct Contact {bool active=false;int role=0,x=0,y=0;uint32_t mask=0;};
struct App {
    pxa::Context* ctx=nullptr;
    pxa::ui::DisplayMetrics display;
    std::optional<pxa::game::Renderer> renderer;
    pxa::game::DrawBuffer<24576> buffer;
    std::array<pxa::game::Sprite,128> spriteBatch{};
    unsigned spriteCount=0;uint8_t spriteSlot=0;
    bool spriteOpaque=false,spriteFlash=false;uint16_t spriteTint=0;
    void flushSprites(pxa::game::Frame& frame){
        if(!spriteCount)return;
        frame.sprites({spriteSlot},std::span(spriteBatch).first(spriteCount),
            {.transparent_index0=!spriteOpaque,.solid_color=spriteFlash,.color={spriteTint}});
        spriteCount=0;
    }
    void enqueueSprite(pxa::game::Frame& frame,uint8_t slot,const pxa::game::Sprite& item,bool opaque,bool flash,uint16_t tint){
        if(spriteCount&&(spriteSlot!=slot||spriteOpaque!=opaque||spriteFlash!=flash||spriteTint!=tint||spriteCount==spriteBatch.size()))flushSprites(frame);
        spriteSlot=slot;spriteOpaque=opaque;spriteFlash=flash;spriteTint=tint;
        spriteBatch[spriteCount++]=item;
    }
    std::array<std::optional<pxa::Asset>,art::pages> textures;
    std::array<bool,art::pages> needed{};
    std::array<unsigned,art::pages> pageUsed{};
    static constexpr unsigned textureSlotCapacity=48;
    unsigned textureSlotCount=16;
    static constexpr unsigned mirrorCapacity=8;
    std::array<int16_t,mirrorCapacity> mirrorPages{};
    std::array<unsigned,mirrorCapacity> mirrorUsed{};
    std::array<bool,mirrorCapacity> mirrorNeeded{};
    unsigned normalSlots()const{return textureSlotCount>16?textureSlotCount-mirrorCapacity:textureSlotCount;}
    std::array<uint8_t,art::pages> pageSlots{};
    std::array<int16_t,textureSlotCapacity> slotPages{};
    pxa::input::Controller controller;
    GameAudio sounds;
    Arena arena;
    BattleFeedback feedback;
    Netplay net;bool online=false;uint32_t localKeys=0;
    uint32_t netMetricsAt=0;bool addressInvalid=false;
    char addressText[16]{};unsigned addressLength=0;int addressCursor=0;
    FramePixels framePixels;
    std::array<bool,art::pages> uploaded{};
    bool warming=false,warmRunning=false;
    unsigned warmGeneration=0;
    bool pageReady(unsigned page)const{return uploaded[page]||bool(textures[page]);}
    View view=View::home,returnView=View::home;
    std::array<Contact,16> contacts;
    int width=320,height=240,menu=0,character=0;
    int rosterCursor=0,storyIndex=0,arcadeStage=0;
    int difficulty=1,practiceMode=0,practiceOpponent=-1;
    bool inputHistory=true,practiceChanged=false;
    int opponentCursor=1,arcadeFirst=-1;
    int stageChoice=0,stageIndex=0,stageCursor=0;
    View stageReturn=View::home;
    bool selectingPractice=false,arcadeTour=true;
    float storyAge=0;
    bool storySeen=false,storyPresented=false,progressWritable=false,progressPending=false,progressSaving=false;
    float scale=1,ox=0,oy=0,accumulator=0;
    Camera camera;bool worldLayer=false;
    int controlsLift=6,menuLift=0,uiLeft=12,uiRight=308,hudShift=0,dpadShift=0;
    unsigned navigationBottom=0,navigationLeft=0;
    std::array<float,2> healthTrail{100,100},trailHold{};
    uint64_t last=0;
    unsigned animation=0,drawErrors=0;uint64_t renderMetricsAt=0;RenderPacing renderPacing;
    bool ready=false,resizing=false,dirty=true,exiting=false,waitRelease=false,loading=false,resizeWanted=false,virtualKeys=false;
    void log(std::string_view s){(void)ctx->log().write(pxa::LogLevel::info,s);}
    void state(const char* reason){char b[512];std::snprintf(b,sizeof b,
        "Ash Harbor %s view=%d phase=%d hp=%d,%d meter=%d combo=%d damage=%d blocks=%d move=%d round=%d character=%d touch=%d held=%u opponent=%d stage=%d difficulty=%d practice=%d history=%d scenery=%d net_tick=%u local=%d remote_meter=%d",
        reason,int(view),int(arena.phase),arena.f[0].hp,arena.f[1].hp,arena.f[0].meter,
        arena.f[0].combo,arena.damageEvents,arena.blocks,arena.lastMove,arena.round,
        view==View::home||view==View::roster||view==View::opening?character:arena.f[0].character,int(virtualKeys),unsigned(arena.f[0].held),arena.f[1].character,arcadeStage+1,difficulty,practiceMode,int(inputHistory),stageIndex,arena.tick,online?net.player()+1:0,arena.f[1].meter);log(std::string_view{b}.substr(0,256));
        // The Log service accepts 256 bytes; keep optional feedback separate.
        if(auto name=feedback.moveName(arena,localPlayer());*name){std::snprintf(b,sizeof b,"Ash Harbor notice local_move=%s",name);log(b);}
        if(arena.grabOwner>=0||std::string_view{reason}.find("throw")!=std::string_view::npos||std::string_view{reason}.find("grab")!=std::string_view::npos){
            std::snprintf(b,sizeof b,"Ash Throw %s hp=%d,%d owner=%d age=%d escapes=%u",reason,arena.f[0].hp,arena.f[1].hp,arena.grabOwner,arena.grabAge,arena.escapes);log(b);}
    }
    void on_error(pxa::Context&,pxa::Error e){char b[70];std::snprintf(b,sizeof b,"Ash Harbor error=%d",int(e));log(b);}
    pxa::Result<void> on_start(pxa::Context& c,std::span<const std::byte> config){
        ctx=&c;sounds.assetOwner=this;
        sounds.reserveAssets=[](void* owner,unsigned bytes){return static_cast<App*>(owner)->reserveAudio(bytes);};
        auto d=pxa::ui::decode_start_display(config);
        if(!d)return std::unexpected(d.error());display=*d;
        // Capture the host's reserved input regions before immersive bars hide.
        navigationBottom=std::max(display.system_bars.bottom,display.system_gestures.bottom);
        navigationLeft=std::max(display.system_bars.left,display.system_gestures.left);
        (void)c.window().request_orientation(pxa::OrientationPreference::landscape);
        (void)c.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);
        (void)c.window().keep_screen_on(true);
        arena.reset();return c.tasks().start(initialize());
    }
    pxa::Task<void> initialize(){
        std::array<std::byte,8> data{};
        auto saved=co_await ctx->storage().get("ash-progress-v1",data);
        if(saved&&*saved==data.size()&&data[0]==std::byte('A')&&data[1]==std::byte('H')&&data[2]==std::byte('S')&&data[3]==std::byte(1)&&unsigned(data[4])<=1&&unsigned(data[5])<characterCount){
            storySeen=bool(data[4]);character=unsigned(data[5]);progressWritable=true;
            if(unsigned(data[6])>=1&&unsigned(data[6])<=3)difficulty=unsigned(data[6])-1;
        }else if(!saved&&saved.error()==pxa::Error::not_found)progressWritable=true;
        std::array<std::byte,1> stageData{};
        auto stageSaved=co_await ctx->storage().get("ash-stage-v1",stageData);
        if(stageSaved&&*stageSaved==1&&unsigned(stageData[0])<4)stageChoice=unsigned(stageData[0]);
        stageIndex=stageChoice;
        view=storySeen?View::home:View::opening;
        auto sound=co_await sounds.open(*ctx);if(!sound)log("Ash Harbor audio unavailable");
        co_return co_await openRenderer();
    }
    pxa::Task<void> openRenderer(){
        ready=false;renderer.reset();for(auto& t:textures)t.reset();
        pageSlots.fill(255);slotPages.fill(-1);uploaded.fill(false);mirrorPages.fill(-1);mirrorNeeded.fill(false);storyPresented=false;
        auto extent=pixelSurface(display.width,display.height);width=extent.width;height=extent.height;
        pxa::game::RenderOptions options;options.width=width;options.height=height;
        options.buffers=2;options.direct_scanout=true;options.max_draw_bytes=buffer.capacity();
        auto made=co_await ctx->game().create(options);
        if(!made)co_return std::unexpected(made.error());
        renderer.emplace(std::move(*made));
        textureSlotCount=renderer->supports(pxa::game::RenderCapability::texture_slots_48)?
            std::min<unsigned>(textureSlotCapacity,renderer->info().max_textures):16;
        char rendererMessage[96];std::snprintf(rendererMessage,sizeof rendererMessage,"Ash Harbor renderer slots=%u capabilities=%u",textureSlotCount,unsigned(renderer->capabilities()));log(rendererMessage);
        if(textureSlotCount>renderer->info().max_textures)co_return std::unexpected(pxa::Error::limit_exceeded);
        if(auto r=pxa::game::Upload(*renderer,buffer.bytes()).palette(art::palette);!r)co_return r;
        for(unsigned i=0;i<art::static_pages;++i){auto r=loadPage(i);if(!r)co_return r;}
        for(unsigned actor=0;actor<2;++actor){auto r=loadPage(art::fighters[actor][0][0].page);if(!r)co_return r;}
        layout();ready=true;resizing=false;dirty=true;last=0;
        (void)ctx->clock().set_period(16);log("Ash Harbor ready");state("launch");render();
        co_return pxa::Result<void>{};
    }
    pxa::Result<void> loadPage(unsigned page){
        char path[64];std::snprintf(path,sizeof path,"assets/atlas-%u.pxr",page);
        unsigned slot=page<art::static_pages?page:art::static_pages;
        while(slot<normalSlots()&&slotPages[slot]>=0)++slot;
        if(slot>=normalSlots())return std::unexpected(pxa::Error::resource_limit);
        auto read=framePixels.load(page);if(!read)return read;
        auto bound=framePixels.bind(*renderer,page,slot);if(!bound)return bound;
        uploaded[page]=true;
        // The 48-slot renderer keeps the whole match resident. Upload copies
        // the pixels, so retaining a second copy in Guest memory only forces
        // memory.grow to find another large contiguous block on small boards.
        // Legacy 16-slot renderers still cache fighter pixels for streaming.
        if(page<art::static_pages||textureSlotCount>16)
            std::vector<std::byte>().swap(framePixels.pages[page]);
        pageSlots[page]=slot;slotPages[slot]=page;pageUsed[page]=animation;
        return pxa::Result<void>{};
    }
    // A bounded renderer-side cache lets mirrored fighters use the 2D sprite
    // path instead of rasterizing two affine triangles on every frame.
    int mirroredSlot(unsigned page){
        if(textureSlotCount<=16)return -1;
        unsigned victim=mirrorCapacity;
        for(unsigned i=0;i<mirrorCapacity;++i){
            if(mirrorPages[i]==int(page)){mirrorUsed[i]=animation;mirrorNeeded[i]=true;return normalSlots()+i;}
            if(!mirrorNeeded[i]&&(victim==mirrorCapacity||mirrorPages[i]<0||mirrorUsed[i]<mirrorUsed[victim]))victim=i;
        }
        if(victim==mirrorCapacity)return -1;
        if(!framePixels.load(page))return -1;
        auto& pixels=framePixels.pages[page];
        mirrorFramePage(page,pixels);
        auto bound=framePixels.bind(*renderer,page,normalSlots()+victim);
        std::vector<std::byte>().swap(pixels);
        if(!bound)return -1;
        mirrorPages[victim]=page;mirrorUsed[victim]=animation;mirrorNeeded[victim]=true;
        return normalSlots()+victim;
    }
    bool releasePage(unsigned victim){
        // Replace only this slot: a full bind_assets would clear uploaded peers.
        std::array<std::byte,1> transparent{};
        if(!pxa::game::Upload(*renderer,buffer.bytes()).texture({pageSlots[victim]},1,1,transparent))return false;
        slotPages[pageSlots[victim]]=-1;pageSlots[victim]=255;
        textures[victim].reset();uploaded[victim]=false;return true;
    }
    bool reserveAudio(unsigned incoming){
        if(!renderer)return true;
        for(;;){unsigned bytes=sounds.resident_bytes()+incoming,victim=art::pages;
            for(unsigned i=0;i<art::pages;++i)if(textures[i]){
                bytes+=art::page_bytes[i];
                if(i>=art::static_pages&&!needed[i]&&(victim==art::pages||pageUsed[i]<pageUsed[victim]))victim=i;
            }
            if(bytes<=496*1024)return true;
            if(victim==art::pages)return false;
            if(!releasePage(victim))return false;
        }
    }
    pxa::Task<void> streamPages(){
        for(unsigned page=0;page<art::pages;++page){
            if(!needed[page]||pageReady(page))continue;
            for(;;){unsigned count=0,bytes=0,victim=art::pages;
                for(unsigned i=art::static_pages;i<art::pages;++i)if(pageReady(i)){
                    ++count;bytes+=art::page_bytes[i];
                    if(!needed[i]&&(victim==art::pages||pageUsed[i]<pageUsed[victim]))victim=i;
                }
                unsigned limit=(textureSlotCount>16?1152u:448u)*1024;
                if((count<normalSlots()-art::static_pages&&bytes+art::page_bytes[page]<=limit)||victim==art::pages)break;
                if(!releasePage(victim)){loading=false;co_return std::unexpected(pxa::Error::resource_limit);}
            }
            auto r=loadPage(page);
            if(!r){loading=false;dirty=true;
                if(r.error()==pxa::Error::would_block||r.error()==pxa::Error::busy||r.error()==pxa::Error::quota_exceeded||r.error()==pxa::Error::resource_limit)
                    co_return pxa::Result<void>{};
                co_return r;
            }
        }
        loading=false;dirty=true;co_return pxa::Result<void>{};
    }
    void layout(){
        // Display insets are surface pixels, not density-independent UI units.
        scale=fitScale(width,height);
        ox=std::floor((width-320*scale)*.5f);oy=std::floor((height-240*scale)*.5f);
        auto horizontal=[&](unsigned n){return float(n)*width/display.width;};
        auto vertical=[&](unsigned n){return float(n)*height/display.height;};
        unsigned bottom=std::max({display.safe.bottom,display.system_bars.bottom,display.system_gestures.bottom,navigationBottom});
        float letterbox=height-(oy+240*scale);
        controlsLift=6+std::max(0,int(std::ceil((vertical(bottom)-letterbox)/scale)));
        menuLift=std::max(0,controlsLift-12);
        uiLeft=std::max(12,int(std::ceil((horizontal(std::max(display.safe.left,display.system_bars.left))-ox)/scale))+4);
        uiRight=std::min(308,int(std::floor((width-horizontal(std::max(display.safe.right,display.system_bars.right))-ox)/scale))-4);
        hudShift=std::max(0,int(std::ceil((vertical(std::max(display.safe.top,display.system_bars.top))-oy)/scale))-4);
        dpadShift=std::max(0,int(std::ceil((horizontal(std::max({display.safe.left,display.system_gestures.left,navigationLeft}))-ox)/scale))+30-42);
        char b[190];std::snprintf(b,sizeof b,"Ash Harbor layout width=%d height=%d scale=%.4f x=%d y=%d lift=%d menu_lift=%d dpad_shift=%d bottom=%u",width,height,double(scale),int(ox),int(oy),controlsLift,menuLift,dpadShift,bottom);log(b);
    }
    int px(float x)const{return int(std::floor(ox+(worldLayer?camera.x+x*camera.zoom:x)*scale+.5f));}
    int py(float y)const{return int(std::floor(oy+(worldLayer?camera.y+y*camera.zoom:y)*scale+.5f));}
    int size(float n)const{return std::max(1,int(n*scale*(worldLayer?camera.zoom:1.f)+.5f));}
    void rect(pxa::game::Frame& frame,float x,float y,float w,float h,uint16_t color){
        if(w<=0||h<=0)return;
        flushSprites(frame);
        int a=px(x)*16,b=py(y)*16,c=px(x+w)*16,d=py(y+h)*16;
        frame.quad({int16_t(a),int16_t(b),int16_t(c),int16_t(b),int16_t(c),int16_t(d),int16_t(a),int16_t(d)},{color});
    }
    void tile(pxa::game::Frame& frame,art::Tile t,float x,float y,float w=0,float h=0,bool flash=false,bool flip=false,uint16_t tint=ivory,bool opaque=false){
        if(px(x+(w?w:t.w))<=0||py(y+(h?h:t.h))<=0||px(x)>=width||py(y)>=height)return;
        needed[t.page]=true;if(pageReady(t.page))pageUsed[t.page]=animation;
        uint8_t slot=pageReady(t.page)?pageSlots[t.page]:0;
        if(flip)if(int mirrored=mirroredSlot(t.page);mirrored>=0){
            slot=mirrored;t.x=art::page_width[t.page]-t.x-t.w;flip=false;
        }
        if(flip){
            flushSprites(frame);
            int a=px(x)*16,b=py(y)*16,c=px(x+(w?w:t.w))*16,d=py(y+(h?h:t.h))*16;
            int u=t.x*16,v=t.y*16,uu=(t.x+t.w)*16,vv=(t.y+t.h)*16;
            frame.textured_quad({slot},{pxa::game::Vertex{int16_t(a),int16_t(b),int16_t(uu),int16_t(v),0,0},
                pxa::game::Vertex{int16_t(c),int16_t(b),int16_t(u),int16_t(v),0,0},
                pxa::game::Vertex{int16_t(c),int16_t(d),int16_t(u),int16_t(vv),0,0},
                pxa::game::Vertex{int16_t(a),int16_t(d),int16_t(uu),int16_t(vv),0,0}},
                {.affine_uv=true,.painter=true,.transparent_index0=true});return;
        }
        pxa::game::Sprite s{int16_t(px(x)),int16_t(py(y)),uint16_t(size(w?w:t.w)),uint16_t(size(h?h:t.h)),t.x,t.y,t.w,t.h};
        enqueueSprite(frame,slot,s,opaque,flash,flash?tint:0);
    }
    void text(pxa::game::Frame& frame,std::string_view s,float x,float y,int h=11,int align=0){
        if(!s.empty()&&uint8_t(s[0])>=128){
            for(const auto& label:art::chinese)if(s==label.text){int factor=h>=24?2:1;float w=label.tile.w*factor;
                if(align){auto ink=PixelText(s).ink(0,label.count);x-=(ink.x+ink.w/(align==1?2.f:1.f))*factor;}
                tile(frame,label.tile,x,y,w,label.tile.h*factor,true);return;}
            log("Ash Harbor missing Chinese label");return;
        }
        int factor=h>=30?3:h>=16?2:1,advance=6*factor;
        if(align){auto ink=PixelText(s).ink(0,s.size());x-=(ink.x+ink.w/(align==1?2.f:1.f))*factor;}
        constexpr std::string_view chars="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-:/.!?><=()abcdefghijklmnopqrstuvwxyz";
        for(char ch:s){auto pos=chars.find(ch);if(pos!=chars.npos){auto t=art::glyphs[pos];needed[t.page]=true;pageUsed[t.page]=animation;
            for(int shadow=1;shadow>=0;--shadow){pxa::game::Sprite glyph{int16_t(px(x+shadow)),int16_t(py(y+shadow)),uint16_t(size(6*factor)),uint16_t(size(12*factor)),t.x,t.y,t.w,t.h};
                frame.sprites({pageSlots[t.page]},{&glyph,1},{.transparent_index0=true,.solid_color=true,.color={shadow?ink:ivory}});}}
            x+=advance;}
    }
    // Keep complete pixel glyphs inside a measured content rectangle.
    void textBox(pxa::game::Frame& frame,std::string_view s,UiRect box,int align=0,int maxLines=1){
        PixelText run(s);if(!run.valid){log("Ash Harbor missing Chinese label");return;}
        auto plan=planText(run,box,maxLines);
        if(plan.elided){
            char message[160];std::snprintf(message,sizeof message,"Ash Harbor label elided: %.*s",int(s.size()),s.data());log(message);
        }
        float top=box.y+std::floor((box.h-plan.height)*.5f)-plan.top;
        for(unsigned i=0;i<plan.count;++i){auto line=plan.lines[i];
            float x=box.x+(align==1?std::floor((box.w-line.ink.w)*.5f):align==2?box.w-line.ink.w:0)-line.ink.x;
            if(run.label){auto part=run.label->tile;auto first=art::label_glyphs[run.label->first+line.first];
                auto last=art::label_glyphs[run.label->first+line.first+line.count-1];
                part.x+=first.x;part.w=last.x+last.advance-first.x;tile(frame,part,x,top+i*14,0,0,true);}
            else text(frame,s.substr(line.first,line.count),x,top+i*14,12);
        }
    }
    void hintLabel(pxa::game::Frame& frame,const char* hint,const char* name,UiRect box){
        PixelText a(hint),b(name);auto ai=a.ink(0,a.count),bi=b.ink(0,b.count);
        int w=ai.w+4+bi.w,t=std::min(ai.y,bi.y),bottom=std::max(ai.y+ai.h,bi.y+bi.h);
        if(w>box.w){textBox(frame,name,box,1);return;}
        auto at=textOrigin(box,{0,int16_t(t),int16_t(w),int16_t(bottom-t)},1);
        text(frame,hint,at.x-ai.x,at.y,12);text(frame,name,at.x+ai.w+4-bi.x,at.y,12);
    }
    void numberBox(pxa::game::Frame& frame,int value,UiRect box){
        char number[24];std::snprintf(number,sizeof number,"%d",value);textBox(frame,number,box,2);
    }
    UiRect viewport()const{return {-ox/scale,-oy/scale,width/scale,height/scale};}
    UiRect storyPanel()const{
        auto v=viewport();
        // Use the full viewport width, while respecting physical side insets.
        float left=std::max(v.x+8,(float(std::max(display.safe.left,display.system_bars.left))*width/display.width-ox)/scale+8);
        float right=std::min(v.x+v.w-8,(width-float(std::max(display.safe.right,display.system_bars.right))*width/display.width-ox)/scale-8);
        float top=std::min(126-controlsLift/4,210-controlsLift-52);
        return {left,top,right-left,240-controlsLift-top};
    }
    void number(pxa::game::Frame& frame,int n,float x,float y,int h=16,int align=0){
        char b[20];std::snprintf(b,sizeof b,"%d",n);
        if(h<16){text(frame,b,x,y,h,align);return;}
        int count=std::strlen(b),w=count*13+3;
        if(align)x-=w/(align==1?2.f:1.f);
        for(char* c=b;*c;++c){if(*c>='0'&&*c<='9')tile(frame,art::ui::digits[*c-'0'],x,y);x+=13;}
    }
    // Nine-slice the generated bevel. Corners stay at their native pixel size.
    void nine(pxa::game::Frame& frame,art::Tile t,int x,int y,int w,int h,int edge=10){
        auto same=[&](art::Tile other){return t.page==other.page&&t.x==other.x&&t.y==other.y;};
        int ex=same(art::ui::button_idle)||same(art::ui::button_focus)?16:edge;
        int ey=std::min({edge,(h-1)/2,(int(t.h)-1)/2});
        int sx[]={0,ex,int(t.w)-ex},sy[]={0,ey,int(t.h)-ey};
        int sw[]={ex,int(t.w)-ex*2,ex},sh[]={ey,int(t.h)-ey*2,ey};
        int dx[]={x,x+ex,x+w-ex},dy[]={y,y+ey,y+h-ey};
        int dw[]={ex,w-ex*2,ex},dh[]={ey,h-ey*2,ey};
        for(int r=0;r<3;++r)for(int c=0;c<3;++c)if(dw[c]>0&&dh[r]>0){
            art::Tile part{t.page,uint16_t(t.x+sx[c]),uint16_t(t.y+sy[r]),uint16_t(sw[c]),uint16_t(sh[r])};
            for(int yy=0;yy<dh[r];yy+=sh[r])for(int xx=0;xx<dw[c];xx+=sw[c]){
                auto piece=part;piece.w=uint16_t(std::min(sw[c],dw[c]-xx));piece.h=uint16_t(std::min(sh[r],dh[r]-yy));
                tile(frame,piece,dx[c]+xx,dy[r]+yy);
            }
        }
    }
    void fill(pxa::game::Frame& frame,art::Tile t,int x,int y,int full,int amount,bool right=false){
        int length=std::clamp(amount,0,full);if(!length)return;
        t.w=uint16_t(std::min(int(t.w),length));
        tile(frame,t,right?x+full-length:x,y,length,t.h,false,right);
    }
    void button(pxa::game::Frame& frame,const char* name,int x,int y,int w,int h,bool focus=false,const char* hint=nullptr){
        nine(frame,focus?art::ui::button_focus:art::ui::button_idle,x,y,w,h);
        UiRect content{float(x+8),float(y+3),float(w-16),float(h-6)};
        PixelText run(name);float contentWidth=run.ink(0,run.count).w;
        if(hint){PixelText prefix(hint);contentWidth+=prefix.ink(0,prefix.count).w+4;}
        if(focus&&content.x+std::floor((content.w-contentWidth)*.5f)>=x+24)
            tile(frame,art::ui::icon_cursor,x+8,y+(h-12)/2);
        if(hint)hintLabel(frame,hint,name,content);else textBox(frame,name,content,1);
    }
    void iconButton(pxa::game::Frame& frame,art::Tile icon,int x,int y){
        nine(frame,art::ui::action_idle,x,y,20,20,4);tile(frame,icon,x+4,y+4);
    }
    void audioButton(pxa::game::Frame& frame,int x,int y){
        nine(frame,art::ui::action_idle,x,y,20,20,4);textBox(frame,sounds.enabled?"声":"静",{float(x+3),float(y+3),14,14},1);
    }
    void actor(pxa::game::Frame& frame,const Fighter& p,bool home=false){
        int n=home?(animation/9)%4:arena.sprite(p);float ground=home?190:193;
        rect(frame,p.x-17,ground-1,34,3,ink);
        auto pose=actorPose(p,n,ground);
        tile(frame,pose.tile,pose.x,pose.y,0,0,
             !home&&p.action==Action::hurt&&(p.age/3)%2,p.face<0);
    }
    void backplate(pxa::game::Frame& frame,float ground=190){
        int scene=view==View::home||view==View::roster||view==View::opponent||view==View::stages?stageChoice:stageIndex;
        for(unsigned i=0;i<4;++i)tile(frame,art::stages[scene][i],int(i%2)*256-96,ground-art::stage_ground+int(i/2)*256,0,0,false,false,ivory,true);
    }
    void frameFighters(){
        auto a=framingBounds(arena.f[0],arena.sprite(arena.f[0]));
        auto b=framingBounds(arena.f[1],arena.sprite(arena.f[1]));
        float left=float(std::max(display.safe.left,display.system_bars.left))*width/display.width;
        float right=float(std::max(display.safe.right,display.system_bars.right))*width/display.width;
        float bottom=float(std::max(display.safe.bottom,display.system_bars.bottom))*height/display.height;
        float top=arena.training?(inputHistory?82.f:68.f):78.f;
        Bounds view{std::max(-96.f,(left-ox)/scale)+4,top+hudShift,
                    std::min(416.f,(width-right-ox)/scale)-4,std::min(238.f,(height-bottom-oy)/scale)-2};
        camera=fitCamera(unite(a,b),view);
    }
    unsigned localPlayer()const{return online?unsigned(net.player()):0;}
    void hud(pxa::game::Frame& frame){
        for(int i=0;i<2;++i){auto& p=arena.f[i];int x=i?189:7;
            int y=4+hudShift,w=std::min(124,(uiRight-uiLeft-48)/2);x=i?uiRight-w:uiLeft;
            nine(frame,i?art::ui::health_right:art::ui::health_left,x,y,w,36,7);
            const auto& who=roster[p.character];
            if(w<112){char score[8];std::snprintf(score,sizeof score,"%d/2",arena.wins[i]);
                textBox(frame,score,{float(i?x+8:x+w-26),float(y+4),18,14});
                textBox(frame,who.name,{float(i?x+34:x+8),float(y+4),float(w-42),14},i?2:0);
            }else{
                tile(frame,who.effect?art::ui::icon_wave:art::ui::icon_flame,i?x+w-20:x+8,y+5);
                textBox(frame,who.name,{float(x+(i?37:23)),float(y+4),float(w-60),14},i?2:0);
            }
            int track=w-14;
            fill(frame,art::ui::fills[3],x+7,y+26,track,int(healthTrail[i]*track/100),i);
            fill(frame,art::ui::fills[p.hp<30?2:i?1:0],x+7,y+26,track,p.hp*track/100,i);
            // Meter uses its own illustrated strip under the main health plaque.
            rect(frame,x+5,y+38,w-10,6,ink);
            fill(frame,art::ui::fills[who.effect?5:4],x+7,y+38,track,p.meter*track/100,i);
            if(w>=112)for(int j=0;j<2;++j)tile(frame,arena.wins[i]>j?art::ui::icon_star:art::ui::icon_empty_star,
                i?x+8+j*13:x+w-33+j*13,y+4);
        }
        int center=(uiLeft+uiRight)/2;
        nine(frame,art::ui::timer,center-22,4+hudShift,44,40,7);
        if(arena.training)textBox(frame,"训练",{float(center-18),float(4+hudShift),36,40},1);
        else number(frame,(arena.time+59)/60,center,13+hudShift,22,1);
        iconButton(frame,art::ui::icon_help,uiRight-43,52+hudShift);
        iconButton(frame,art::ui::icon_pause,uiRight-21,52+hudShift);
        audioButton(frame,uiRight-65,52+hudShift);
        if(online){char b[24];std::snprintf(b,sizeof b,"%dP  %ums",net.player()+1,net.ping);textBox(frame,b,{float(uiLeft+2),float(54+hudShift),140,12});}
        else if(!arena.training){if(arcadeTour){char stage[12];std::snprintf(stage,sizeof stage,"%d/3",arcadeStage+1);text(frame,"STAGE",uiLeft+2,54+hudShift,10);text(frame,stage,uiLeft+40,54+hudShift,10);}else text(frame,"自由对战",uiLeft+2,54+hudShift,12);}
        else{
            nine(frame,art::ui::action_idle,uiLeft-4,50+hudShift,uiRight-uiLeft-64,inputHistory?32:19,6);
            int columns=uiRight-uiLeft>=236?3:2;
            int infoLeft=uiLeft+4,infoRight=uiRight-76,cell=(infoRight-infoLeft)/columns;
            const char* labels[]={"连击","伤害","最佳"};
            int values[]={arena.f[0].combo,arena.f[0].comboDamage,arena.bestDamage};
            for(int i=0;i<columns;++i){int x=infoLeft+i*cell;
                textBox(frame,labels[i],{float(x),float(54+hudShift),24,12});
                numberBox(frame,values[i],{float(x+28),float(54+hudShift),float(cell-32),12});
            }
            if(inputHistory){
                textBox(frame,"输入",{float(infoLeft),float(67+hudShift),24,12});
                int historyX=infoLeft+29,capacity=std::clamp((infoRight-historyX)/36,1,int(arena.inputs.size()));
                unsigned count=std::min<unsigned>(arena.inputCount,capacity);
                int historyCell=(infoRight-historyX)/capacity;
                for(unsigned i=0;i<count;++i){uint32_t mask=arena.inputs[(arena.inputCount-count+i)%arena.inputs.size()];
                    int x=historyX+i*historyCell,y=67+hudShift;
                    auto direction=directionGlyph(mask);
                    if(*direction)textBox(frame,direction,{float(x),float(y),12,12});else rect(frame,x+5,y+5,2,2,teal);
                    char token[5];int n=0;
                    if(mask&Punch)token[n++]='P';if(mask&Kick)token[n++]='K';if(mask&Evade)token[n++]='E';if(mask&Skill)token[n++]='S';token[n]=0;
                    textBox(frame,token,{float(x+12),float(y),float(historyCell-12),12});
                }
            }
        }
        if(!arena.training&&arena.f[1-localPlayer()].comboTimer&&arena.f[localPlayer()].combo>1){
            int x=localPlayer()?uiRight-66:uiLeft+4;
            number(frame,arena.f[localPlayer()].combo,x,91,22);text(frame,"命中",x+36,100,12);
            number(frame,arena.f[localPlayer()].comboDamage,x,121,12);text(frame,"伤害",x+36,121,12);
        }
        if(arena.phase==Phase::fight)text(frame,feedback.moveName(arena,localPlayer()),160,160,10,1);
    }
    void controls(pxa::game::Frame& frame){
        if(!virtualKeys)return;
        const int x=stickX+dpadShift,y=stickY-controlsLift;
        tile(frame,art::ui::dpad,x-stickRadius,y-stickRadius);
        uint32_t held=online?localKeys:arena.f[0].held;
        uint32_t direction=held&(Up|Down|Left|Right);
        const int offsets[][2]={{26,0},{46,8},{50,26},{46,46},{26,50},{7,46},{0,26},{7,8}};
        for(unsigned i=0;i<8;++i)if(direction==stickDirections[i])
            tile(frame,art::dpad_arrows[i],x-stickRadius+offsets[i][0],y-stickRadius+offsets[i][1],0,0,true,false,gold);
        struct Key {const char* name;int x,y;uint32_t bit;};
        const Key keys[]={{"闪",214,180,Evade},{"拳",214,209,Punch},{"脚",248,209,Kick},{"技",282,209,Skill}};
        for(auto k:keys){int yy=k.y-controlsLift;
            tile(frame,held&k.bit?art::ui::action_pressed:art::ui::action_idle,k.x,yy);
            textBox(frame,k.name,{float(k.x+4),float(yy+3),20,22},1);
        }
    }
    void match(pxa::game::Frame& frame){
        frameFighters();worldLayer=true;
        // Super freeze darkens the entire viewport, independent of camera zoom.
        if(arena.flash>0)frame.clear({panel});else backplate(frame,193);
        actor(frame,arena.f[0]);actor(frame,arena.f[1]);
        for(auto& s:arena.shots)if(s.active){
            tile(frame,art::projectiles[arena.f[s.owner].character][projectileFrame(s)],s.x-16,193-s.y-16,0,0,false,s.speed<0);
        }
        if(arena.hit.age<16){int extent=arena.hitAction==Action::super?64:32;
            tile(frame,art::effects[0][8+arena.hit.age/4],arena.hit.x-extent/2,193-arena.hit.y-extent/2,extent,extent);}
        worldLayer=false;
        hud(frame);controls(frame);
        if(arena.phase==Phase::fight&&arena.escapeAge<45){
            nine(frame,art::ui::button_idle,108,86+hudShift,104,24,7);
            textBox(frame,"投技挣脱",{116,float(89+hudShift),88,18},1);
        }
        if(arena.phase==Phase::intro){
            if(arena.phaseAge<48){tile(frame,art::ui::round,110,94);number(frame,arena.round,195,94,22);}
            else tile(frame,art::ui::fight,160-art::ui::fight.w/2,94);
        }else if(arena.phase==Phase::round_end){
            if(arena.winner<0)text(frame,"平局",160,97,24,1);
            else tile(frame,art::ui::ko,160-art::ui::ko.w/2,76);
            nine(frame,art::ui::button_idle,108,142,104,24,7);
            textBox(frame,arena.winner<0?"平局":localRoundWon(arena,localPlayer())?"胜利":"对手获胜",{116,145,88,18},1);
        }
    }
    void commands(pxa::game::Frame& frame){
        int squeeze=std::max(0,controlsLift-8),row=18-std::min(4,(squeeze+7)/8);
        int footer=186-squeeze,first=std::min(94-squeeze/3,footer-16-3*row);
        nine(frame,art::ui::modal,uiLeft,8,uiRight-uiLeft,232-controlsLift,16);
        textBox(frame,"招式表",{float(uiLeft+16),18,float(uiRight-uiLeft-32),12},1);
        textBox(frame,"面向右侧，反向时左右互换",{float(uiLeft+16),34,float(uiRight-uiLeft-32),12},1);
        if(first>=68)textBox(frame,"方向依次输入，最后按拳或脚",{float(uiLeft+16),50,float(uiRight-uiLeft-32),12},1);
        if(first>=90){textBox(frame,"上跳下蹲",{float(uiLeft+12),70,96,12});textBox(frame,"拳接脚接必杀",{float(uiRight-84),70,72,12});}
        // Direction glyphs come from the game's pixel atlas, without font fallback.
        const char* motions[]={"↓↘→","→↓↘","↓↙←","↓↘→↓↘→"};
        const auto& names=roster[online?arena.f[net.player()].character:character].arts;
        for(int i=0;i<4;++i){
            int y=first+i*row,x=uiLeft+12,attack=x+(i==3?72:36)+5;
            text(frame,motions[i],x,y,12);text(frame,"+",attack,y,12);
            text(frame,i==2?"脚":"拳",attack+9,y,12);
            int nameX=std::max(uiLeft+124,uiRight-126);
            textBox(frame,names[i],{float(nameX),float(y),float(std::min(72,uiRight-42-nameX)),12});
            if(i==3)text(frame,"满气",uiRight-42,y,12);
        }
        text(frame,"拳+脚：投技／挣脱",uiLeft+12,footer,12);
        text(frame,"后拉防御",uiRight-60,footer,12);
        button(frame,"返回",106,210-controlsLift,108,24,true);
    }
    void opening(pxa::game::Frame& frame){
        auto cover=coverImage(art::story_width[storyIndex],art::story_height[storyIndex],viewport(),storyIndex==1?.525f:.5f);
        float sourceX=0;
        for(auto part:art::story[storyIndex]){
            needed[part.page]=true;if(pageReady(part.page))pageUsed[part.page]=animation;
            uint8_t slot=pageReady(part.page)?pageSlots[part.page]:0;
            int a=px(cover.x+sourceX*cover.scale)*16,b=py(cover.y)*16;
            int c=px(cover.x+(sourceX+part.w)*cover.scale)*16,d=py(cover.y+part.h*cover.scale)*16;
            int u=part.x*16,v=part.y*16,uu=(part.x+part.w)*16,vv=(part.y+part.h)*16;
            frame.textured_quad({slot},{pxa::game::Vertex{int16_t(a),int16_t(b),int16_t(u),int16_t(v),0,0},
                pxa::game::Vertex{int16_t(c),int16_t(b),int16_t(uu),int16_t(v),0,0},
                pxa::game::Vertex{int16_t(c),int16_t(d),int16_t(uu),int16_t(vv),0,0},
                pxa::game::Vertex{int16_t(a),int16_t(d),int16_t(u),int16_t(vv),0,0}},
                {.affine_uv=true,.painter=true,.transparent_index0=true});
            sourceX+=part.w;
        }
        static constexpr const char* titles[]={"港灯邀擂","各有所求","今夜开擂"};
        static constexpr const char* lines[][2]={
            {"烬港夜擂将开，四封请帖送出。","胜者可入听潮阁，问清往日旧事。"},
            {"陆炎赴旧约，霁岚守港灯。","韩震为工友出拳，唐绫寻人而来。"},
            {"鼓声已起，四路拳师齐聚码头。","这一战，各凭本事。"}};
        auto panel=storyPanel();
        nine(frame,art::ui::button_idle,int(panel.x),5,100,26,8);
        textBox(frame,titles[storyIndex],{panel.x+8,11,84,12},1);
        nine(frame,art::ui::modal,int(panel.x),int(panel.y),int(panel.w),int(panel.h),12);
        textBox(frame,lines[storyIndex][0],{panel.x+12,panel.y+10,panel.w-24,12},1);
        textBox(frame,lines[storyIndex][1],{panel.x+12,panel.y+26,panel.w-24,12},1);
        int y=210-controlsLift;
        button(frame,"跳过",int(panel.x)+12,y,76,24,false);
        button(frame,storyIndex==2?"入港":"下一幕",int(panel.x+panel.w)-102,y,90,24,true);
        for(int i=0;i<3;++i)tile(frame,i==storyIndex?art::ui::icon_star:art::ui::icon_empty_star,143+i*13,y+6);
    }
    void fighterCard(pxa::game::Frame& frame,int i,int x,int y,int w,int h,bool focus){
        nine(frame,focus?art::ui::button_focus:art::ui::button_idle,x,y,w,h,8);
        int portrait=std::clamp(std::min(h-8,w-76),20,56);
        tile(frame,art::portraits[i],x+4,y+(h-portrait)/2,portrait,portrait);
        int rows=h>=60?3:2,top=h>=60?5:4,inner=h-8;
        const char* names[]={roster[i].name,roster[i].style,roster[i].roman};
        for(int row=0;row<rows;++row){int a=top+inner*row/rows,b=top+inner*(row+1)/rows;
            textBox(frame,names[row],{float(x+portrait+12),float(y+a),float(w-portrait-20),float(b-a)});}
    }
    void chooseRoster(pxa::game::Frame& frame){
        backplate(frame);textBox(frame,"选择拳师",{float(uiLeft+2),4,60,14});
        textBox(frame,roster[rosterCursor].motive,{float(uiLeft+70),4,float(uiRight-uiLeft-74),14},2);
        int cardWidth=(uiRight-uiLeft-8)/2;auto grid=cardGrid(controlsLift);
        for(int i=0;i<characterCount;++i)fighterCard(frame,i,uiLeft+(i%2)*(cardWidth+8),grid.row(i),cardWidth,grid.height,i==rosterCursor);
        int y=210-controlsLift;
        button(frame,"返回",uiLeft+8,y,76,24,false);button(frame,"选定",uiRight-98,y,90,24,true);
    }
    void chooseOpponent(pxa::game::Frame& frame){
        backplate(frame);text(frame,selectingPractice?"选择陪练":"选择对手",uiLeft+2,5,12);
        text(frame,"我方",uiRight-60,5,12);text(frame,roster[character].name,uiRight-4,5,12,2);
        int cardWidth=(uiRight-uiLeft-8)/2;auto grid=cardGrid(controlsLift,true);
        for(int i=0;i<characterCount;++i)fighterCard(frame,i,uiLeft+(i%2)*(cardWidth+8),grid.row(i),cardWidth,grid.height,i==opponentCursor);
        if(!selectingPractice){int x=std::max(92,uiLeft+52),w=std::min(136,uiRight-76-x);button(frame,opponentCursor==character?"镜像对战":arcadeTour?"三战巡回":"自由对战",x,192-controlsLift,w,20);
            text(frame,"SELECT",uiLeft+8,196-controlsLift,12);}
        button(frame,"场景",uiRight-68,192-controlsLift,60,20,false,"X");
        button(frame,"返回",uiLeft+8,218-controlsLift,76,22);
        button(frame,selectingPractice?"开始训练":"开战",uiRight-110,218-controlsLift,102,22,true);
    }
    void openStages(){stageReturn=view;stageCursor=stageChoice;view=View::stages;resetInputs();dirty=true;state("stages-open");}
    void selectStage(){stageChoice=stageCursor;view=stageReturn;resetInputs();saveProgress();dirty=true;state("stage-selected");}
    void chooseStages(pxa::game::Frame& frame){
        backplate(frame);text(frame,"选择擂台",160,6,12,1);
        int w=(uiRight-uiLeft-8)/2;auto grid=cardGrid(controlsLift);
        for(int i=0;i<4;++i){int x=uiLeft+(i%2)*(w+8),y=grid.row(i);
            nine(frame,i==stageCursor?art::ui::button_focus:art::ui::button_idle,x,y,w,grid.height,8);
            auto preview=art::stage_previews[i];float zoom=std::min(1.f,float(grid.height-25)/preview.h);
            tile(frame,preview,x+(w-preview.w*zoom)*.5f,y+4,preview.w*zoom,preview.h*zoom);
            textBox(frame,stageNames[i],{float(x+8),float(y+grid.height-19),float(w-16),14},1);
        }
        button(frame,"返回",uiLeft+8,210-controlsLift,76,24);
        button(frame,"选定",uiRight-98,210-controlsLift,90,24,true);
    }
    void practicePanel(pxa::game::Frame& frame){
        auto layout=practiceLayout(controlsLift);
        nine(frame,art::ui::modal,uiLeft,8,uiRight-uiLeft,232-controlsLift,16);
        textBox(frame,"训练设置",{float(uiLeft+16),12,float(uiRight-uiLeft-32),float(layout.rowTop-16)},1);
        const char* labels[]={"陪练方式","陪练拳师","输入记录"};
        const char* values[]={practiceNames[practiceMode],roster[practiceOpponent].name,inputHistory?"开启":"关闭"};
        for(int i=0;i<3;++i){int y=layout.rowTop+i*layout.rowStep;
            nine(frame,menu==i?art::ui::button_focus:art::ui::button_idle,uiLeft+12,y,uiRight-uiLeft-24,layout.rowHeight,7);
            if(menu==i)tile(frame,art::ui::icon_cursor,uiLeft+20,y+(layout.rowHeight-12)/2);
            textBox(frame,labels[i],{float(uiLeft+35),float(y+4),float(uiRight-uiLeft-151),float(layout.rowHeight-8)});
            tile(frame,art::ui::icon_left,uiRight-104,y+(layout.rowHeight-12)/2);tile(frame,art::ui::icon_right,uiRight-28,y+(layout.rowHeight-12)/2);
            textBox(frame,values[i],{float(uiRight-88),float(y+4),56,float(layout.rowHeight-8)},1);
        }
        button(frame,"重置位置",92,layout.resetY,136,layout.resetHeight,menu==3);
        if(layout.showStats){
            int half=(uiRight-uiLeft-32)/2;
            textBox(frame,"最高连击",{float(uiLeft+16),float(layout.statsY),48,12});
            numberBox(frame,arena.bestCombo,{float(uiLeft+68),float(layout.statsY),float(half-56),12});
            textBox(frame,"最高伤害",{float(uiLeft+16+half),float(layout.statsY),48,12});
            numberBox(frame,arena.bestDamage,{float(uiLeft+68+half),float(layout.statsY),float(half-56),12});
        }
        button(frame,"返回练习",106,layout.returnY,108,24,menu==4);
    }
    void openOnline(){net.leave();online=false;menu=0;view=View::online;net.message="同一网络内双人对战";resetInputs();dirty=true;state("online-open");}
    void leaveOnline(bool home=false){net.leave();online=false;menu=0;view=home?View::home:View::online;net.message="同一网络内双人对战";resetInputs();dirty=true;last=0;}
    void openNetwork(bool host){if(net.busy)return;resetInputs();menu=0;
        uint32_t seed=uint32_t(last)^uint32_t(last>>32)^(++animation*0x9e3779b9u);
        auto r=ctx->tasks().start(net.open(*ctx,host,character,stageChoice,seed));
        if(!r){net.message="网络连接不可用";net.status=Netplay::Status::failed;}dirty=true;}
    void networkTick(uint32_t now){
        int damage=arena.damageEvents,grab=arena.grabOwner;auto escapes=arena.escapes;net.poll(now,arena);
        if(net.status==Netplay::Status::failed&&online){online=false;view=View::online;menu=0;resetInputs();sounds.pause();state("online-disconnected");}
        if(net.resetWanted){net.resetWanted=false;online=true;arcadeTour=false;
            arena.reset(false,net.characters[0],net.characters[1]);arena.ai=false;stageIndex=net.stage;
            healthTrail={100,100};trailHold={};view=View::online;menu=0;accumulator=0;last=0;resetInputs();
            warming=true;++warmGeneration;feedback={};sounds.begin(arena,localPlayer());state("online-loading");}
        if(online&&!warming)net.ready();
        if(net.snapshot){net.snapshot=false;if(online&&!warming){feedback.observe(arena);sounds.step(arena,localPlayer());
            if(arena.escapes!=escapes)state("online-throw-escape");
            else if(arena.grabOwner>=0&&arena.grabOwner!=grab)state("online-grab");
            if(damage!=arena.damageEvents)state("online-hit");}}
        if(online&&net.playing()&&!warming){
            if(net.paused()){
                if(view==View::play){view=View::pause;menu=0;resetInputs();sounds.pause();}
                if(net.host){net.incoming.release();arena.clearInput();}
            }else if(view==View::online||view==View::pause){view=arena.phase==Phase::match_end?View::result:View::play;last=0;accumulator=0;resetInputs();state("online-play");}
        }
        if(online&&now-netMetricsAt>1000){netMetricsAt=now;state("online-sync");char b[128];std::snprintf(b,sizeof b,"Ash LAN metrics rtt_ms=%u rx_age_ms=%u pending=%u tick=%u",net.ping,net.connected?net.now-net.lastRx:0,net.outgoing.count,arena.tick);log(b);}
        if(view==View::online||view==View::address)dirty=true;
    }
    void onlinePanel(pxa::game::Frame& frame){
        backplate(frame);nine(frame,art::ui::modal,uiLeft,8,uiRight-uiLeft,232-controlsLift,16);
        textBox(frame,view==View::address&&addressInvalid&&!addressLayout(controlsLift).prompt?"地址格式无效":"局域网对战",{float(uiLeft+14),21,float(uiRight-uiLeft-28),12},1);
        auto label=[&](const char* s,int y){textBox(frame,s,{float(uiLeft+14),float(y),float(uiRight-uiLeft-28),12},1);};
        int footer=210-controlsLift;
        int addressY=std::min(145,footer-28),actionY=std::min(105,addressY-32),choiceY=std::min(65,actionY-32);

        if(view==View::address){
            auto keys=addressLayout(controlsLift);
            if(keys.prompt)label(addressInvalid?"地址格式无效":"输入房主地址",43);
            textBox(frame,addressLength?addressText:"192.168.1.100",{float(uiLeft+20),float(keys.inputY),float(uiRight-uiLeft-40),12},1);
            const char* digits[]={"1","2","3","4","5","6","7","8","9",".","0","<"};
            for(int i=0;i<12;++i){auto box=keys.key(i);button(frame,digits[i],box.x,box.y,box.w,box.h,addressCursor==i);}
            button(frame,"返回",uiLeft+12,footer,76,24,addressCursor==13);
            button(frame,"加入",uiRight-88,footer,76,24,addressCursor==12);
            return;
        }
        using S=Netplay::Status;
        if(net.status==S::idle||net.status==S::failed){
            if(choiceY>=60)label(net.message,43);
            int w=std::min(124,(uiRight-uiLeft-32)/2),right=uiRight-12-w;
            button(frame,roster[character].name,uiLeft+12,choiceY,w,24,menu==0);
            button(frame,stageNames[stageChoice],right,choiceY,w,24,menu==1);
            button(frame,"创建房间",uiLeft+12,actionY,w,26,menu==2);
            button(frame,"搜索房间",right,actionY,w,26,menu==3);
            button(frame,"输入地址",106,addressY,108,24,menu==4);
            button(frame,"返回",106,footer,108,24,menu==5);return;
        }
        if(net.status==S::searching){
            auto list=roomListLayout(controlsLift,net.roomCount,menu);
            label(net.roomCount?(uiRight-uiLeft-28<204?stageNames[net.rooms[std::clamp(menu,0,int(net.roomCount)-1)].stage]:"选择房间加入"):"正在搜索房间",43);
            if(!net.roomCount)label("也可直接输入房主地址",79);
            if(int(net.roomCount)>list.capacity){iconButton(frame,art::ui::icon_left,uiRight-44,40);iconButton(frame,art::ui::icon_right,uiRight-22,40);}
            for(int i=list.first;i<std::min(int(net.roomCount),list.first+list.capacity);++i){auto& r=net.rooms[i];char b[40];std::snprintf(b,sizeof b,"%u.%u.%u.%u",unsigned(r.address.address[0]),unsigned(r.address.address[1]),unsigned(r.address.address[2]),unsigned(r.address.address[3]));
                int x=uiLeft+14,y=65+list.step*(i-list.first),w=uiRight-uiLeft-28,h=list.step-2;
                nine(frame,menu==i?art::ui::button_focus:art::ui::button_idle,x,y,w,h);
                if(menu==i&&w>=204)tile(frame,art::ui::icon_cursor,x+7,y+(h-12)/2);
                int content=x+(w>=204?24:12);
                textBox(frame,roster[r.character].name,{float(content),float(y+3),36,float(h-6)});
                textBox(frame,b,{float(content+42),float(y+3),90,float(h-6)});
                if(w>=204)textBox(frame,w>=232?stageNames[r.stage]:shortStageNames[r.stage],{float(x+w-(w>=232?78:42)),float(y+3),float(w>=232?62:26),float(h-6)},2);
            }
            button(frame,"返回",uiLeft+12,footer,76,24,menu==4);
            button(frame,"输入地址",uiRight-112,footer,100,24,menu==5);return;
        }
        label(net.status==S::loading?"正在准备对战":net.message,49);
        char b[40];
        if(net.status==S::hosting){
            if(net.local.address[0]!=std::byte{}){std::snprintf(b,sizeof b,"%u.%u.%u.%u",unsigned(net.local.address[0]),unsigned(net.local.address[1]),unsigned(net.local.address[2]),unsigned(net.local.address[3]));label(b,std::min(83,footer-60));}
            else label("对方点击搜索房间即可加入",std::min(83,footer-60));
            label(roster[character].name,std::min(112,footer-40));label(stageNames[stageChoice],std::min(138,footer-20));
        }else if(net.connected){label(roster[net.characters[0]].name,std::min(89,footer-64));label("VS",std::min(111,footer-42));label(roster[net.characters[1]].name,std::min(133,footer-20));}
        button(frame,"返回",106,footer,108,24,true);
    }
    void onlineActivate(int i){
        using S=Netplay::Status;
        if(net.status==S::idle||net.status==S::failed){
            if(i==0){character=(character+1)%characterCount;saveProgress();}
            else if(i==1){stageChoice=(stageChoice+1)%4;saveProgress();}
            else if(i==2)openNetwork(true);
            else if(i==3)openNetwork(false);
            else if(i==4){view=View::address;addressCursor=0;openNetwork(false);}
            else leaveOnline(true);
        }else if(net.status==S::searching){
            if(i>=0&&i<int(net.roomCount))net.join(net.rooms[i].address,net.rooms[i].id);
            else if(i==5){view=View::address;addressCursor=0;}
            else if(i==4)openOnline();
        }else openOnline();dirty=true;
    }
    void addressActivate(int i){
        const char* digits="123456789.0";addressInvalid=false;
        if(i<11&&addressLength<15){addressText[addressLength++]=digits[i];addressText[addressLength]=0;}
        else if(i==11&&addressLength)addressText[--addressLength]=0;
        else if(i==12){std::array<uint8_t,4> ip{};
            if(lan::ipv4(addressText,ip)&&net.socket){net.join(pxa::NetworkEndpoint::ipv4(ip[0],ip[1],ip[2],ip[3],lan::port));view=View::online;}
            else {addressInvalid=true;if(!net.socket){view=View::online;net.message="网络尚未就绪，请重试";net.status=Netplay::Status::failed;}}}
        else if(i==13){view=View::online;menu=5;}dirty=true;
    }
    void onlineController(uint32_t pressed){using namespace pxa::input;
        if(pressed&controller_b){if(view==View::address){view=View::online;menu=5;}else if(net.active())openOnline();else leaveOnline(true);return;}
        if(view==View::address){
            if(pressed&controller_left)addressCursor=addressNavigate(addressCursor,-1,0,controlsLift);
            if(pressed&controller_right)addressCursor=addressNavigate(addressCursor,1,0,controlsLift);
            if(pressed&controller_up)addressCursor=addressNavigate(addressCursor,0,-1,controlsLift);
            if(pressed&controller_down)addressCursor=addressNavigate(addressCursor,0,1,controlsLift);
            if(pressed&(controller_a|controller_start))addressActivate(addressCursor);
        }else{
            if(pressed&(controller_up|controller_left))menu=(menu+5)%6;
            if(pressed&(controller_down|controller_right))menu=(menu+1)%6;
            if(pressed&(controller_a|controller_start))onlineActivate(menu);
        }dirty=true;
    }
    void onlineTap(int x,int y){int footer=210-controlsLift;
        int addressY=std::min(145,footer-28),actionY=std::min(105,addressY-32),choiceY=std::min(65,actionY-32);

        if(view==View::address){auto keys=addressLayout(controlsLift);
            for(int i=0;i<12;++i){auto box=keys.key(i);if(inside(x,y,box.x,box.y,box.w,box.h)){addressCursor=i;addressActivate(i);return;}}
            if(inside(x,y,uiLeft+12,footer,76,24))addressActivate(13);
            else if(inside(x,y,uiRight-88,footer,76,24))addressActivate(12);return;}
        using S=Netplay::Status;
        if(net.status==S::idle||net.status==S::failed){
            int w=std::min(124,(uiRight-uiLeft-32)/2),right=uiRight-12-w;
            const int xs[]={uiLeft+12,right,uiLeft+12,right,106,106},ys[]={choiceY,choiceY,actionY,actionY,addressY,footer};
            for(int i=0;i<6;++i)if(inside(x,y,xs[i],ys[i],i<4?w:108,i==2||i==3?26:24)){menu=i;onlineActivate(i);break;}
        }else if(net.status==S::searching){
            auto list=roomListLayout(controlsLift,net.roomCount,menu);
            if(int(net.roomCount)>list.capacity){
                if(inside(x,y,uiRight-44,40,20,20)){menu=std::max(0,list.first-list.capacity);dirty=true;return;}
                if(inside(x,y,uiRight-22,40,20,20)){menu=std::min(int(net.roomCount)-1,list.first+list.capacity);dirty=true;return;}
            }
            for(int i=list.first;i<std::min(int(net.roomCount),list.first+list.capacity);++i)if(inside(x,y,uiLeft+14,65+list.step*(i-list.first),uiRight-uiLeft-28,list.step-2)){menu=i;onlineActivate(i);return;}
            if(inside(x,y,uiLeft+12,footer,76,24))onlineActivate(4);
            else if(inside(x,y,uiRight-112,footer,100,24))onlineActivate(5);
        }else if(inside(x,y,106,footer,108,24))openOnline();
    }
    void draw(pxa::game::Frame& frame){
        if(view==View::online||view==View::address){onlinePanel(frame);return;}
        if(view==View::practice){practicePanel(frame);return;}
        if(view==View::opening){opening(frame);return;}
        if(view==View::roster){chooseRoster(frame);return;}
        if(view==View::stages){chooseStages(frame);return;}
        if(view==View::opponent){chooseOpponent(frame);return;}
        if(view==View::home){
            auto home=homeLayout(menuLift);backplate(frame);
            Fighter a,b;a.character=character;b.character=(character+1)%characterCount;
            a.x=80;b.x=240;b.face=-1;
            actor(frame,a,true);actor(frame,b,true);
            float logoScale=std::min(1.f,float(home.logoHeight)/art::logo.h);
            tile(frame,art::logo,160-art::logo.w*logoScale/2,4,art::logo.w*logoScale,art::logo.h*logoScale);
            audioButton(frame,uiRight-21,home.audio);
            auto difficultyBox=homeControl(0,uiLeft,uiRight,home.toolbar);
            for(int i=1;i<4;++i){auto box=homeControl(i,uiLeft,uiRight,home.toolbar);button(frame,i==1?"联机":i==2?"场景":"剧情",box.x,box.y,box.w,box.h,i==1&&menu==3);}
            nine(frame,art::ui::button_idle,difficultyBox.x,difficultyBox.y,difficultyBox.w,difficultyBox.h,7);
            textBox(frame,"L",{difficultyBox.x+4,difficultyBox.y+3,8,14},1);
            textBox(frame,difficultyNames[difficulty],{difficultyBox.x+15,difficultyBox.y+3,difficultyBox.w-30,14},1);
            textBox(frame,"R",{difficultyBox.x+difficultyBox.w-12,difficultyBox.y+3,8,14},1);
            nine(frame,art::ui::button_focus,126,home.character,68,26);
            tile(frame,art::ui::bracket_left,119,home.character-1);
            tile(frame,art::ui::bracket_right,187,home.character-1);
            tile(frame,art::ui::icon_left,111,home.character+7);tile(frame,art::ui::icon_right,197,home.character+7);
            textBox(frame,roster[character].name,{132,float(home.character+3),56,20},1);
            for(int i=0;i<3;++i){auto box=homeAction(i,uiLeft,uiRight,home.actions);
                button(frame,i==0?"街机":i==1?"训练":"招式",box.x,box.y,box.w,box.h,menu==i);}
            return;
        }
        match(frame);
        if(view==View::commands){commands(frame);return;}
        if(view==View::pause||view==View::result){
            int lift=std::max(0,controlsLift-18);
            nine(frame,art::ui::modal,66,52-lift,188,164,16);
            audioButton(frame,228,62-lift);
            if(view==View::result&&!online&&arcadeTour&&arena.wins[0]>=2&&arcadeStage==characterCount-2)textBox(frame,"夜擂通关",{90,float(62-lift),140,24},1);
            else if(view==View::result&&arena.wins[online?net.player():0]>=2)tile(frame,art::ui::win,160-art::ui::win.w/2,62-lift);
            else textBox(frame,view==View::pause?(online&&(net.stalled()||net.remoteStall)?"等待网络恢复":online&&net.remotePause?"对手已暂停":"已暂停"):"再接再厉",{90,float(62-lift),140,24},1);
            if(view==View::pause&&arena.training)button(frame,"设置",73,62-lift,52,22,false,"X");
            const char* items[4]={view==View::pause?"继续":arcadeTour&&arena.wins[0]>=2&&arcadeStage<characterCount-2?"下一战":"再来一局","招式",view==View::result?"换个对手":"重开","主菜单"};
            if(online){items[0]=view==View::pause?(net.localPause?"继续":"等待对手继续"):(net.localRematch?"等待对手再战":"再来一局");items[2]="退出房间";}
            for(int i=0;i<4;++i)button(frame,items[i],83,94+i*28-lift,154,24,menu==i);
        }
    }
    void render(){
        if(!ready||!renderer||!ctx->foreground())return;
        // Keep simulation/input at 60 Hz, but do not build another DrawList
        // while the previous one is still being rasterized. Mailbox overwrite
        // otherwise spends CPU encoding and copying frames nobody can see.
        auto metrics=renderer->telemetry();
        if(metrics){
            if(last-renderMetricsAt>=1000000){renderMetricsAt=last;char b[160];
                std::snprintf(b,sizeof b,"Ash Render submitted=%llu rendered=%llu dropped=%llu raster_us=%u",
                    (unsigned long long)metrics->submitted_frames,(unsigned long long)metrics->rendered_frames,
                    (unsigned long long)metrics->dropped_frames,metrics->last_host_raster_us);log(b);}
            if(!renderPacing.ready(last,metrics->submitted_frames,metrics->rendered_frames,metrics->dropped_frames,view!=View::play)){dirty=true;return;}
        }
        needed.fill(false);mirrorNeeded.fill(false);worldLayer=false;pxa::game::Frame frame(*renderer,buffer.bytes());frame.clear({ink});draw(frame);flushSprites(frame);
        bool missing=false;for(unsigned i=0;i<art::pages;++i)missing|=needed[i]&&!pageReady(i);
        if(missing){dirty=true;if(!loading&&!warming){loading=true;auto task=ctx->tasks().start(streamPages());if(!task){loading=false;log("Ash Harbor texture stream could not start");}}return;}
        auto r=frame.submit();dirty=!r;if(r&&view==View::opening)storyPresented=true;
        if(!r&&r.error()!=pxa::Error::would_block&&drawErrors++<4){
            char b[100];std::snprintf(b,sizeof b,"Ash Harbor render error=%d bytes=%u",int(r.error()),unsigned(frame.bytes_used()));log(b);
        }
    }
    void resetInputs(){for(auto& c:contacts)c={};localKeys=0;
        if(net.connected){net.setInput(0);if(net.host)arena.feed(0,0);}else arena.clearInput();}
    pxa::Task<void> persistProgress(){
        while(progressPending){progressPending=false;
            std::array<std::byte,8> data{std::byte('A'),std::byte('H'),std::byte('S'),std::byte(1),std::byte(storySeen),std::byte(character),std::byte(difficulty+1)};
            auto saved=co_await ctx->storage().set("ash-progress-v1",data);
            if(!saved){log("Ash Harbor progress save failed");break;}
            std::array<std::byte,1> stageData{std::byte(stageChoice)};
            if(auto scene=co_await ctx->storage().set("ash-stage-v1",stageData);!scene){log("Ash Harbor scene save failed");break;}
            log("Ash Harbor progress saved");
        }
        progressSaving=false;co_return pxa::Result<void>{};
    }
    void saveProgress(){if(!progressWritable)return;progressPending=true;if(!progressSaving){progressSaving=true;if(!ctx->tasks().start(persistProgress()))progressSaving=false;}}
    void nextStory(bool skip=false){
        if(skip||storyIndex==2){storySeen=true;view=View::home;menu=0;resetInputs();saveProgress();state("story-finished");}
        else{++storyIndex;storyAge=0;storyPresented=false;resetInputs();state("story-next");}
        last=0;dirty=true;
    }
    void replayStory(){view=View::opening;storyIndex=0;storyAge=0;storyPresented=false;last=0;resetInputs();state("story-replay");dirty=true;}
    void openRoster(){rosterCursor=character;view=View::roster;resetInputs();state("roster");dirty=true;}
    void selectCharacter(){character=rosterCursor;view=View::home;menu=0;resetInputs();saveProgress();state("selected");dirty=true;}
    void openOpponent(bool training){selectingPractice=training;opponentCursor=training?(practiceOpponent<0?(character+1)%characterCount:practiceOpponent):(arcadeFirst<0?(character+1)%characterCount:arcadeFirst);view=View::opponent;resetInputs();last=0;dirty=true;state("opponent-open");}
    void confirmOpponent(){arcadeStage=0;if(selectingPractice)practiceOpponent=opponentCursor;else{arcadeFirst=opponentCursor;if(arcadeFirst==character)arcadeTour=false;}start(selectingPractice);}
    void start(bool training){
        stageIndex=(stageChoice+(!training&&arcadeTour?arcadeStage:0))%4;
        if(practiceOpponent<0)practiceOpponent=(character+1)%characterCount;
        int first=arcadeFirst<0?(character+1)%characterCount:arcadeFirst;
        arena.reset(training,character,training?practiceOpponent:arcadeTour?tourOpponent(character,first,arcadeStage):first);
        arena.difficulty=Difficulty(difficulty);arena.aiStage=training?0:arcadeStage;arena.practice=Practice(practiceMode);
        healthTrail={100,100};trailHold={};view=View::play;menu=0;last=0;accumulator=0;resetInputs();feedback={};sounds.begin(arena);state("start");dirty=true;
        warming=true;++warmGeneration;
    }
    pxa::Task<void> warmFighters(){
        unsigned generation=0;
        do{
            generation=warmGeneration;
            auto wanted=FramePixels::fighterPages(arena.f[0].character,arena.f[1].character);
            for(auto tile:art::stages[stageIndex])wanted[tile.page]=true;
            framePixels.retain(wanted);
            if(textureSlotCount>16)for(unsigned page=art::static_pages;page<art::pages;++page)
                if(uploaded[page]&&!wanted[page]&&!releasePage(page)){warming=warmRunning=false;co_return std::unexpected(pxa::Error::resource_limit);}
            for(unsigned page=art::static_pages;page<art::pages;++page)if(wanted[page]){
                auto result=textureSlotCount>16?
                    (pageReady(page)?pxa::Result<void>{}:loadPage(page)):
                    framePixels.load(page);
                if(!result){warming=warmRunning=false;co_return result;}
                if(generation!=warmGeneration)break;
            }
        }while(generation!=warmGeneration);
        if(textureSlotCount>16){
            mirrorNeeded.fill(false);
            for(int frame:{0,4,8,12,32,36,40,44})
                (void)mirroredSlot(actorPose(arena.f[1],frame).tile.page);
        }
        warming=warmRunning=false;accumulator=0;last=0;resetInputs();
        char message[128];std::snprintf(message,sizeof message,"Ash Harbor frame-bank ready bytes=%u reads=%u uploads=%u",framePixels.bytes(),framePixels.reads,framePixels.uploads);log(message);
        co_return pxa::Result<void>{};
    }
    void changeDifficulty(int delta){difficulty=(difficulty+delta+3)%3;saveProgress();dirty=true;state("difficulty");}
    void openPractice(){if(!arena.training)return;view=View::practice;menu=0;practiceChanged=false;last=0;resetInputs();sounds.pause();dirty=true;state("practice-open");}
    void changePractice(int row,int delta){
        if(row==0){practiceMode=(practiceMode+delta+4)%4;practiceChanged=true;}
        if(row==1){practiceOpponent=(practiceOpponent+delta+characterCount)%characterCount;practiceChanged=true;}
        if(row==2)inputHistory=!inputHistory;
        dirty=true;state("practice-setting");
    }
    void closePractice(bool reset=false){
        if(reset||practiceChanged)start(true);
        else{view=View::play;menu=0;last=0;resetInputs();dirty=true;}
        state("practice-close");
    }
    void pause(){if(view!=View::play)return;if(online)net.pause(true);view=View::pause;menu=0;resetInputs();last=0;sounds.pause();state("pause");dirty=true;}
    void activate(int i){
        sounds.play(motion::Cue::click);dirty=true;
        if(view==View::home){if(i==3){openOnline();return;}if(i<2)openOpponent(i==1);else{returnView=view;view=View::commands;}}
        else if(view==View::commands){view=returnView;last=0;resetInputs();}
        else if(view==View::pause||view==View::result){
            if(online){if(i==0){if(view==View::result)net.rematch();else net.pause(false);}
                else if(i==1){returnView=view;view=View::commands;}else leaveOnline(i==3);resetInputs();return;}
            if(i==0){if(view==View::result){if(arcadeTour&&arena.wins[0]>=2){if(arcadeStage<characterCount-2)++arcadeStage;else arcadeStage=0;}start(arena.training);}else{view=View::play;last=0;resetInputs();}}
            else if(i==1){returnView=view;view=View::commands;}
            else if(i==2){if(view==View::result)openOpponent(false);else start(arena.training);}
            else{view=View::home;menu=0;resetInputs();}
        }
        state("menu");
    }
    pxa::BackAction on_back(){
        if(exiting)return pxa::BackAction::close;
        if(view==View::online||view==View::address)leaveOnline(true);
        else if(view==View::opening)nextStory(true);
        else if(view==View::stages){view=stageReturn;resetInputs();dirty=true;}
        else if(view==View::home){exiting=true;(void)ctx->window().request_back();}
        else if(view==View::play)pause();
        else if(view==View::commands)activate(0);
        else if(view==View::practice)closePractice();
        else if(view==View::pause)activate(0);
        else if(online)leaveOnline(true);
        else{view=View::home;menu=0;resetInputs();dirty=true;}
        return pxa::BackAction::stay;
    }
    void on_controller(pxa::Context&,const pxa::input::ControllerState& s){
        uint32_t before=controller.buttons();
        if(!ready||!controller.update(s))return;
        if(!s.connected){resetInputs();waitRelease=true;return;}
        if(waitRelease){if(!controller.buttons())waitRelease=false;return;}
        uint32_t down=controller.buttons(),pressed=0;
        for(unsigned i=0;i<12;++i)if(controller.pressed(1u<<i))pressed|=1u<<i;
        if(before!=down&&virtualKeys){virtualKeys=false;for(auto& c:contacts)c={};dirty=true;state("controller-controls");}
        using namespace pxa::input;
        if(view==View::online||view==View::address){onlineController(pressed);return;}
        if(view==View::opening){
            if(pressed&(controller_b|controller_select))nextStory(true);
            else if((pressed&(controller_a|controller_start|controller_right))&&storyAge>.18f)nextStory();
            return;
        }else if(view==View::stages){
            if(pressed&(controller_left|controller_right))stageCursor^=1;
            if(pressed&(controller_up|controller_down))stageCursor^=2;
            if(pressed&(controller_a|controller_start))selectStage();
            else if(pressed&controller_b){view=stageReturn;resetInputs();}
            dirty=true;return;
        }else if(view==View::roster){
            if(pressed&(controller_left|controller_right))rosterCursor^=1;
            if(pressed&(controller_up|controller_down))rosterCursor^=2;
            if(pressed&(controller_a|controller_start))selectCharacter();
            else if(pressed&controller_b){view=View::home;resetInputs();}
            dirty=true;return;
        }else if(view==View::practice){
            if(pressed&controller_up)menu=(menu+4)%5;
            if(pressed&controller_down)menu=(menu+1)%5;
            if(menu<3&&(pressed&(controller_left|controller_right)))changePractice(menu,(pressed&controller_left)?-1:1);
            if(pressed&(controller_a|controller_start)){if(menu<3)changePractice(menu,1);else closePractice(menu==3);}
            else if(pressed&controller_b)closePractice();
            dirty=true;return;
        }else if(view==View::opponent){
            if(pressed&controller_x){openStages();return;}
            if(pressed&(controller_left|controller_right))opponentCursor^=1;
            if(pressed&(controller_up|controller_down))opponentCursor^=2;
            if(!selectingPractice&&(pressed&controller_select)){arcadeTour=!arcadeTour;state("opponent-mode");}
            if(pressed&(controller_a|controller_start))confirmOpponent();
            else if(pressed&controller_b){view=View::home;menu=selectingPractice?1:0;resetInputs();dirty=true;}
            dirty=true;return;
        }else if(view==View::play){
            if(pressed&controller_start){pause();return;}
            if(pressed&controller_select){if(online)net.pause(true);returnView=View::pause;view=View::commands;resetInputs();dirty=true;return;}
            if(arena.training&&(pressed&(controller_l|controller_r))&&(down&(controller_l|controller_r))==(controller_l|controller_r)){start(true);return;}
            updateInput();
        }else if(view==View::commands){if(pressed&(controller_a|controller_b|controller_start|controller_select))activate(0);}
        else{
            int count=4;
            if(view==View::home&&(pressed&(controller_l|controller_r))){changeDifficulty(pressed&controller_l?-1:1);return;}
            if(view==View::pause&&arena.training&&(pressed&controller_x)){openPractice();return;}
            if(pressed&controller_y){sounds.toggle();dirty=true;return;}
            if(pressed&(controller_up|controller_left))menu=(menu+count-1)%count;
            if(pressed&(controller_down|controller_right))menu=(menu+1)%count;
            if(view==View::home&&(pressed&controller_select)){character=(character+1)%characterCount;saveProgress();}
            if(view==View::home&&(pressed&controller_x)){openRoster();return;}
            if(pressed&(controller_a|controller_start))activate(menu);
            if(pressed&controller_b)on_back();
            dirty=true;
        }
    }
    static bool inside(int x,int y,int a,int b,int w,int h){return x>=a&&y>=b&&x<a+w&&y<b+h;}
    uint32_t touchMask(int x,int y,int role){
        y+=controlsLift;
        if(role==1){
            return stickDirection(x-dpadShift-stickX,y-stickY);
        }
        if(inside(x,y,212,178,34,27))return Evade;
        if(inside(x,y,212,207,34,31))return Punch;
        if(inside(x,y,246,207,34,31))return Kick;
        if(inside(x,y,280,207,38,31))return Skill;
        return 0;
    }
    void updateInput(){uint32_t mask=virtualKeys?0:controller.buttons()&(15|Punch|Kick|Evade|Skill);
        for(const auto& c:contacts)if(c.active)mask|=c.mask;
        if((mask&(Left|Right))==(Left|Right))mask&=~(Left|Right);
        if((mask&(Up|Down))==(Up|Down))mask&=~(Up|Down);
        localKeys=mask;if(online){if(net.host)arena.feed(0,mask);else net.setInput(mask);}else arena.feed(0,mask);dirty=true;
    }
    void on_pointer(pxa::Context&,const pxa::input::Pointer& p){
        if(!ready||p.pointer_id>=contacts.size())return;
        using P=pxa::input::PointerPhase;
        float sx=pxa::ui::canvas_to_surface_coordinate(p.x,display)*float(width)/display.width;
        float sy=pxa::ui::canvas_to_surface_coordinate(p.y,display)*float(height)/display.height;
        int x=int((sx-ox)/scale),y=int((sy-oy)/scale);
        auto& c=contacts[p.pointer_id];
        if(p.action()==P::down){
            auto bounds=viewport();
            if(view==View::opening){if(x<bounds.x||x>=bounds.x+bounds.w||y<bounds.y||y>=bounds.y+bounds.h)return;}
            else if(x<0||x>=320||y<0||y>=240)return;
            virtualKeys=true;dirty=true;
            c={true,0,x,y,0};
            if((view==View::home&&inside(x,y,uiRight-21,homeLayout(menuLift).audio,20,20))||
               (view==View::play&&inside(x,y,uiRight-65,52+hudShift,20,20))||
               ((view==View::pause||view==View::result)&&inside(x,y,228,62-std::max(0,controlsLift-18),20,20))){sounds.toggle();c.active=false;return;}
            if(view==View::play){
                if(arena.training&&inside(x,y,(uiLeft+uiRight)/2-20,4+hudShift,40,44)){openPractice();return;}
                if(inside(x,y,uiRight-21,52+hudShift,21,22)){pause();return;}
                if(inside(x,y,uiRight-43,52+hudShift,21,22)){if(online)net.pause(true);returnView=View::pause;view=View::commands;resetInputs();dirty=true;return;}
                c.role=inside(x,y,dpadShift,169-controlsLift,88,70)?1:2;
            }
        }
        if(!c.active)return;
        if(view==View::play){
            c.mask=p.ended()?0:touchMask(x,y,c.role);if(p.ended())c.active=false;updateInput();
            if(p.action()!=P::move)state(p.action()==P::down?"touch-down":p.action()==P::cancel?"touch-cancel":"touch-up");return;
        }
        if(p.action()==P::up&&std::abs(x-c.x)<15&&std::abs(y-c.y)<15){
            if(view==View::online||view==View::address){onlineTap(x,y);}
            else if(view==View::opening){
                if(inside(x,y,int(storyPanel().x)+12,210-controlsLift,76,24))nextStory(true);
                else if(storyAge>.18f)nextStory();
            }else if(view==View::roster){
                int cardWidth=(uiRight-uiLeft-8)/2;
                auto grid=cardGrid(controlsLift);
                for(int i=0;i<characterCount;++i)if(inside(x,y,uiLeft+(i%2)*(cardWidth+8),grid.row(i),cardWidth,grid.height)){rosterCursor=i;dirty=true;break;}
                if(inside(x,y,uiLeft+8,210-controlsLift,76,24)){view=View::home;dirty=true;}
                else if(inside(x,y,uiRight-98,210-controlsLift,90,24))selectCharacter();
            }else if(view==View::stages){
                int w=(uiRight-uiLeft-8)/2;
                auto grid=cardGrid(controlsLift);
                for(int i=0;i<4;++i)if(inside(x,y,uiLeft+(i%2)*(w+8),grid.row(i),w,grid.height)){stageCursor=i;dirty=true;}
                if(inside(x,y,uiLeft+8,210-controlsLift,76,24)){view=stageReturn;resetInputs();dirty=true;}
                else if(inside(x,y,uiRight-98,210-controlsLift,90,24))selectStage();
            }else if(view==View::opponent){
                if(inside(x,y,uiRight-68,192-controlsLift,60,20)){openStages();return;}
                int cardWidth=(uiRight-uiLeft-8)/2;
                auto grid=cardGrid(controlsLift,true);
                for(int i=0;i<characterCount;++i)if(inside(x,y,uiLeft+(i%2)*(cardWidth+8),grid.row(i),cardWidth,grid.height)){opponentCursor=i;dirty=true;}
                int modeX=std::max(92,uiLeft+52),modeW=std::min(136,uiRight-76-modeX);
                if(!selectingPractice&&inside(x,y,modeX,192-controlsLift,modeW,20)){arcadeTour=!arcadeTour;dirty=true;state("opponent-mode");}
                if(inside(x,y,uiLeft+8,218-controlsLift,76,22)){view=View::home;menu=selectingPractice?1:0;dirty=true;}
                else if(inside(x,y,uiRight-110,218-controlsLift,102,22))confirmOpponent();
            }else if(view==View::practice){
                auto layout=practiceLayout(controlsLift);
                for(int i=0;i<3;++i)if(inside(x,y,uiLeft+12,layout.rowTop+i*layout.rowStep,uiRight-uiLeft-24,layout.rowHeight)){menu=i;changePractice(i,x<uiRight-95?-1:1);break;}
                if(inside(x,y,92,layout.resetY,136,layout.resetHeight))closePractice(true);
                else if(inside(x,y,106,210-controlsLift,108,24))closePractice();
            }else if(view==View::pause&&arena.training&&inside(x,y,73,62-std::max(0,controlsLift-18),52,22)){openPractice();}
            else if(view==View::home){auto home=homeLayout(menuLift);
                int toolbar=-1;
                for(int i=0;i<4;++i){auto box=homeControl(i,uiLeft,uiRight,home.toolbar);if(inside(x,y,box.x,box.y,box.w,box.h))toolbar=i;}
                if(toolbar==1)openOnline();
                else if(toolbar==2)openStages();
                else if(toolbar==3)replayStory();
                else if(toolbar==0){auto box=homeControl(0,uiLeft,uiRight,home.toolbar);changeDifficulty(x<box.x+box.w/2?-1:1);}
                else if(inside(x,y,108,home.character-1,18,28)){character=(character+characterCount-1)%characterCount;saveProgress();dirty=true;}
                else if(inside(x,y,194,home.character-1,20,28)){character=(character+1)%characterCount;saveProgress();dirty=true;}
                else if(inside(x,y,126,home.character-5,68,34))openRoster();
                else for(int i=0;i<3;++i){auto box=homeAction(i,uiLeft,uiRight,home.actions);if(inside(x,y,box.x,box.y,box.w,box.h)){activate(i);break;}}
            }else if(view==View::commands){if(inside(x,y,106,210-controlsLift,108,24))activate(0);}
            else for(int i=0;i<4;++i)if(inside(x,y,83,94+28*i-std::max(0,controlsLift-18),154,24)){activate(i);break;}
        }
        if(p.ended())c.active=false;
    }
    void on_background(pxa::Context&){if(net.active()||online){net.leave();online=false;view=View::online;net.message="已离开房间，请重新连接";}pause();resetInputs();controller.reset();waitRelease=true;sounds.suspend();last=0;(void)ctx->window().keep_screen_on(false);(void)ctx->clock().set_period(0);}
    void on_foreground(pxa::Context&){
        sounds.resume();last=0;dirty=true;
        (void)ctx->window().request_orientation(pxa::OrientationPreference::landscape);
        (void)ctx->window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);
        (void)ctx->window().keep_screen_on(true);
        if(ready)(void)ctx->clock().set_period(16);
    }
    pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& e){
        sounds.event(e);
        if(auto d=pxa::ui::decode_display_metrics(e)){
            bool changed=display.width!=d->width||display.height!=d->height;display=*d;
            if(changed)resizeWanted=true;
            layout();dirty=true;return true;
        }
        auto tick=pxa::decode_clock_tick(e);if(!tick)return false;
        if(!ready||!ctx->foreground())return true;
        if(resizeWanted&&!loading&&!resizing&&!warming){resizeWanted=false;pause();resizing=true;auto r=ctx->tasks().start(openRenderer());if(!r)return std::unexpected(r.error());return true;}
        uint64_t now=tick->timestamp_us;
        float dt=last&&now>last?std::min(.1f,float(now-last)*1e-6f):0;last=now;
        networkTick(uint32_t(now/1000));
        bool helpHome=view==View::commands&&returnView==View::home;
        int music=view==View::home||view==View::opening||view==View::roster||view==View::opponent||view==View::stages||view==View::online||view==View::address||helpHome?0:view==View::result?(arena.wins[online?net.player():0]>=2?2:3):(stageIndex==0?1:3+stageIndex);
        sounds.tick(now,dt,music,view==View::pause||view==View::practice||(view==View::commands&&!helpHome));
        ++animation;
        if(view==View::opening&&storyPresented){storyAge+=dt;if(storyAge>=7.f)nextStory();}
        if(warming&&!loading&&!warmRunning){warmRunning=true;if(!ctx->tasks().start(warmFighters()))warming=warmRunning=false;}
        if(view==View::play){
            if(warming||(online&&(!net.started||net.paused())))accumulator=0;else accumulator+=dt;int damage=arena.damageEvents,block=arena.blocks,lastMove=arena.lastMove,round=arena.round,grab=arena.grabOwner;auto escapes=arena.escapes;
            unsigned steps=0;while(!warming&&accumulator>=1.f/60&&steps++<6){
                if(!online||net.host){if(online){arena.feed(0,localKeys);arena.feed(1,net.incoming.step());}arena.step();feedback.observe(arena);sounds.step(arena,localPlayer());}
                accumulator-=1.f/60;}
            for(unsigned i=0;i<2;++i){
                if(arena.f[i].hp>=healthTrail[i]){healthTrail[i]=arena.f[i].hp;trailHold[i]=0;}
                else if(arena.damageEvents!=damage)trailHold[i]=.3f;
                else if(trailHold[i]>0)trailHold[i]-=dt;
                else healthTrail[i]=std::max(float(arena.f[i].hp),healthTrail[i]-dt*45);
            }
            if(arena.escapes!=escapes)state("throw-escape");
            else if(arena.grabOwner>=0&&arena.grabOwner!=grab)state("grab");
            if(arena.damageEvents!=damage)state("hit");
            else if(arena.blocks!=block)state("block");
            else if(arena.lastMove!=lastMove)state("move");
            if(round!=arena.round)state("round");
            if(arena.phase==Phase::match_end){view=View::result;menu=0;resetInputs();waitRelease=true;state("result");}
            dirty=true;
        }else if(view==View::home)dirty=true;
        if(dirty)render();return true;
    }
};
}
PXA_APPLICATION(ash::App)
