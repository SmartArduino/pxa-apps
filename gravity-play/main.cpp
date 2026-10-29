// SPDX-License-Identifier: MIT
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/ui_display.hpp>
#include <pxa/game_upload.hpp>
#include <cstdio>
#include <unistd.h>
#include "core.hpp"
#include "render.hpp"
namespace gravity {
enum class Screen {home,dice,maze,pause,settings,result};
struct App {
  pxa::Context* ctx=nullptr;pxa::ui::DisplayMetrics display;
  std::optional<pxa::game::Renderer> renderer;
  std::array<std::optional<pxa::Asset>,font_pages> fonts;
  pxa::game::DrawBuffer<24576> draw;
  pxa::input::Controller controller;
  pxa::SensorSubscription acceleration,rotation;pxa::Permission acc_permission,gyro_permission;
  Motion motion;Maze maze;Random random;
  Screen screen=Screen::home,paused_from=Screen::maze;
  unsigned focus=0,level=0,dice_count=2,sensitivity=1,axis=0;
  unsigned dice[2]{3,5},previous_total=0,last_total=0,rolls=0,unlocked=0,gyro_samples=0;
  std::array<uint32_t,18> record{};
  bool ready=false,connecting=false,loaded=false,saving=false,save_pending=false,
    dirty=true,sensor_present=false,gyro_present=false,manual=false,wait_release=false,dragging=false,backgrounded=false;
  std::string sensor_message="正在检查体感";
  uint64_t now=0,last_tick=0,last_report=0,roll_start=0,frame_count=0,connection_epoch=0;
  float rolling=0,gyro_energy=0,heading=0;Vec touch{};int down_x=0,down_y=0;uint8_t pointer_owner=0;
  int w=240,h=284,scale=1,board_x=0,board_y=0,cell=20;
  pxa::Result<void> on_start(pxa::Context& c,std::span<const std::byte> config){
    ctx=&c;if(auto d=pxa::ui::decode_start_display(config))display=*d;
    uint32_t seed=0;if(getentropy(&seed,sizeof seed)||!seed)seed=0x9e3779b9;random.state=seed;
    (void)c.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);
    return c.tasks().start(initialize());
  }
  void log(std::string_view value){(void)ctx->log().write(pxa::LogLevel::info,value);}
  pxa::Result<void> upload_palette(){std::array<uint16_t,256> palette{};palette.fill(white);
    return pxa::game::Upload(*renderer,draw.bytes()).palette(palette);}
  pxa::Task<void> initialize(){
    std::array<std::byte,72> data{};auto saved=co_await ctx->storage().get("gravity-v1",data);
    if(saved&&*saved==data.size())for(unsigned i=0;i<record.size();++i)record[i]=pxa::wire::get32(data.data()+4*i);
    if(record[0]==0x47525631&&record[1]<=11&&record[2]<3&&record[3]<4&&record[4]>=1&&record[4]<=2&&record[5]<=1){
      unlocked=record[1];sensitivity=record[2];axis=record[3];dice_count=record[4];manual=record[5];}
    else record={};
    loaded=true;
    scale=std::min(display.width,display.height)>=400&&!(display.width%2)&&!(display.height%2)?2:1;
    w=int(display.width/scale);h=int(display.height/scale);
    pxa::game::RenderOptions options;options.width=w;options.height=h;options.buffers=2;
    options.direct_scanout=true;options.max_draw_bytes=draw.capacity();
    auto created=co_await ctx->game().create(options);
    if(!created)co_return std::unexpected(created.error());renderer.emplace(std::move(*created));
    if(auto sent=upload_palette();!sent)co_return sent;
    for(unsigned i=0;i<font_pages;++i){char path[60];std::snprintf(path,sizeof path,"assets/font-%u.pxr",i);
      auto asset=co_await ctx->assets().load(pxa::AssetKind::texture,path);if(!asset)co_return std::unexpected(asset.error());
      fonts[i].emplace(std::move(*asset));if(auto bound=renderer->bind_asset(*fonts[i],{uint8_t(i)});!bound)co_return bound;}
    std::array<pxa::SensorDescriptor,8> list{};auto found=co_await ctx->sensors().list(list);
    if(found)for(unsigned i=0;i<*found;++i){if(list[i].semantic.view()=="motion.acceleration")sensor_present=true;if(list[i].semantic.view()=="motion.angular-velocity")gyro_present=true;}
    sensor_message=sensor_present?"六轴体感已就绪":"可用按键或触屏练习";
    ready=true;(void)ctx->clock().set_period(33);last_tick=0;log("Gravity ready");render();co_return pxa::Result<void>{};
  }
  void release_sensors(){ctx->foreground_tasks().cancel();connecting=false;++connection_epoch;
    acceleration.reset();rotation.reset();acc_permission.reset();gyro_permission.reset();motion.suspend();}
  void connect(){if(manual||connecting||acceleration||!ctx->foreground())return;connecting=true;
    sensor_message="请允许使用体感";if(!ctx->foreground_tasks().start(connect_sensors(connection_epoch)))connecting=false;}
  pxa::Task<void> connect_sensors(uint64_t epoch){
    std::array<pxa::SensorDescriptor,8> list{};auto found=co_await ctx->sensors().list(list);
    if(!found||!*found){sensor_message="未找到体感，可切换按键";connecting=false;dirty=true;co_return pxa::Result<void>{};}
    for(unsigned i=0;i<*found;++i){auto semantic=list[i].semantic.view();
      bool acc=semantic=="motion.acceleration",gyro=semantic=="motion.angular-velocity";
      if(!acc&&!gyro)continue;
      if(gyro&&screen!=Screen::dice)continue;
      auto permission=co_await ctx->permissions().acquire("sensor.read",semantic);
      if(epoch!=connection_epoch||!ctx->foreground()){connecting=false;co_return pxa::Result<void>{};}
      if(!permission){if(acc)sensor_message="体感未授权，可在设置切换按键";continue;}
      auto subscription=co_await ctx->sensors().subscribe(list[i],std::clamp(50u,list[i].min_period_ms,list[i].max_period_ms),*permission);
      if(epoch!=connection_epoch||!ctx->foreground()){connecting=false;co_return pxa::Result<void>{};}
      if(subscription){if(acc){acc_permission=std::move(*permission);acceleration=std::move(*subscription);sensor_present=true;}
        else {gyro_permission=std::move(*permission);rotation=std::move(*subscription);gyro_present=true;}}
    }
    if(acceleration){sensor_message="轻轻倾斜就能玩";
      log(rotation?"Gravity sensors subscribed 50ms acc=1 gyro=1":"Gravity sensors subscribed 50ms acc=1 gyro=0");}
    else sensor_message="体感连接失败，可切换按键";
    connecting=false;dirty=true;co_return pxa::Result<void>{};
  }
  void save(){save_pending=true;if(!saving){saving=true;if(!ctx->tasks().start(persist()))saving=false;}}
  pxa::Task<void> persist(){while(save_pending){save_pending=false;record[0]=0x47525631;record[1]=unlocked;record[2]=sensitivity;record[3]=axis;record[4]=dice_count;record[5]=manual;
      std::array<std::byte,72> data{};for(unsigned i=0;i<record.size();++i)pxa::wire::put32(data.data()+4*i,record[i]);
      auto result=co_await ctx->storage().set("gravity-v1",data);if(!result){log("Gravity save failed");break;}}
    saving=false;co_return pxa::Result<void>{};}
  int menu_count() const {return screen==Screen::home?4:screen==Screen::settings?5:screen==Screen::pause?5:screen==Screen::result?3:screen==Screen::dice?3:0;}
  void home(){release_sensors();screen=Screen::home;focus=0;dragging=false;touch={};rolling=0;last_tick=0;dirty=true;
    sensor_message=manual?"按键练习模式":sensor_present?"六轴体感已就绪":"可用按键或触屏练习";}
  void start_maze(){release_sensors();screen=Screen::maze;focus=0;Random map_random{0x72615631u+level*0x9e3779b9u};maze.generate(level,w>h,map_random,h>w*3/2);motion.rotation=axis;
    if(!manual){motion.calibrate();connect();}last_tick=0;dirty=true;log("Gravity maze started");}
  void start_dice(){release_sensors();screen=Screen::dice;focus=0;rolling=0;connect();last_tick=0;dirty=true;}
  void roll(){if(rolling||screen!=Screen::dice)return;previous_total=last_total;
    for(auto &d:dice)d=random.below(6)+1;rolling=1.25f;roll_start=now;++rolls;dirty=true;
    last_total=dice[0]+(dice_count==2?dice[1]:0);
    char text[100];std::snprintf(text,sizeof text,"Gravity roll n=%u count=%u result=%u,%u previous=%u",rolls,dice_count,dice[0],dice[1],previous_total);log(text);}
  void pause(){if(screen!=Screen::maze)return;paused_from=screen;screen=Screen::pause;focus=0;maze.velocity={};touch={};dragging=false;release_sensors();dirty=true;}
  void resume(){screen=Screen::maze;focus=0;last_tick=0;if(!manual){motion.calibrate();connect();}dirty=true;}
  void activate(){
    if(screen==Screen::home){if(focus==0)start_dice();else if(focus==1){level=unlocked;start_maze();}else if(focus==2){screen=Screen::settings;focus=0;dirty=true;}else(void)ctx->window().request_back();return;}
    if(screen==Screen::dice){if(focus==0)roll();else if(focus==1){dice_count=3-dice_count;save();dirty=true;}else home();return;}
    if(screen==Screen::maze){pause();return;}
    if(screen==Screen::pause){if(focus==0)resume();else if(focus==1){manual=false;save();resume();}
      else if(focus==2){manual=!manual;save();resume();}else if(focus==3)start_maze();else home();return;}
    if(screen==Screen::result){if(focus==0){level=(level+1)%12;start_maze();}else if(focus==1)start_maze();else home();return;}
    if(screen==Screen::settings){if(focus==0)sensitivity=(sensitivity+1)%3;
      else if(focus==1){axis=(axis+1)%4;motion.rotation=axis;}
      else if(focus==2)manual=!manual;else if(focus==3){level=0;start_maze();return;}else{home();return;}
      save();dirty=true;}
  }
  void key(uint32_t buttons){using namespace pxa::input;
    if(buttons&controller_start)buttons|=controller_a;
    if(buttons&controller_b){back();return;}
    if(screen==Screen::maze){if(manual){if(buttons&controller_up)heading-=.392699f;if(buttons&controller_down)heading+=.392699f;dirty=true;}
      else if(buttons&controller_up){motion.calibrate();maze.velocity={};dirty=true;}
      if(buttons&controller_a)activate();return;}
    int count=menu_count();if(!count)return;
    if(buttons&controller_up){focus=(focus+count-1)%count;dirty=true;}
    if(buttons&controller_down){focus=(focus+1)%count;dirty=true;}
    if(buttons&controller_a)activate();
  }
  void on_controller(pxa::Context&,const pxa::input::ControllerState& state){
    if(!ready)return;
    if(!controller.update(state))return;if(!state.connected){controller.reset();wait_release=true;return;}
    if(wait_release){if(!controller.buttons())wait_release=false;return;}
    uint32_t edges=0;using namespace pxa::input;
    for(auto mask:{controller_up,controller_down,controller_a,controller_b,controller_start})if(controller.pressed(mask))edges|=mask;
    if(edges)key(edges);
  }
  void back(){if(screen==Screen::home)(void)ctx->window().request_back();else if(screen==Screen::maze)pause();else if(screen==Screen::pause)resume();else home();}
  pxa::BackAction on_back(){if(screen==Screen::home)return pxa::BackAction::close;back();return pxa::BackAction::stay;}
  void on_pointer(pxa::Context&,const pxa::input::Pointer& p){
    if(!ready)return;
    int x=pxa::ui::canvas_to_surface_coordinate(p.x,display)/scale;
    int y=pxa::ui::canvas_to_surface_coordinate(p.y,display)/scale;
    if(p.action()==pxa::input::PointerPhase::down){if(dragging)return;dragging=true;pointer_owner=p.pointer_id;down_x=x;down_y=y;
      if(screen==Screen::maze){if(y<34){pause();return;}touch={};}return;}
    if(!dragging||p.pointer_id!=pointer_owner)return;
    if(p.action()==pxa::input::PointerPhase::move&&screen==Screen::maze){touch={std::clamp((x-down_x)/45.f,-1.f,1.f),std::clamp((y-down_y)/45.f,-1.f,1.f)};dirty=true;return;}
    if(!p.ended())return;dragging=false;touch={};if(p.action()==pxa::input::PointerPhase::cancel)return;
    if(screen==Screen::maze)return;
    if(std::abs(x-down_x)>12||std::abs(y-down_y)>12)return;
    if(screen==Screen::dice&&y<dice_actions_y()){roll();return;}
    int index=hit_menu(x,y);if(index>=0){focus=unsigned(index);activate();}
  }
  int dice_actions_y() const {return h-44;}
  int row_y(int index) const {if(screen==Screen::home)return 61+index*std::max(36,(h-86)/4);
    if(screen==Screen::dice)return dice_actions_y();
    int top=screen==Screen::result?79:screen==Screen::pause?66:53;
    return top+index*std::max(29,(h-top-19)/std::max(1,menu_count()));}
  int row_height() const {if(screen==Screen::home)return std::max(32,(h-86)/4-5);
    if(screen==Screen::dice)return 32;
    int top=screen==Screen::result?79:screen==Screen::pause?66:53;
    return std::max(26,(h-top-19)/std::max(1,menu_count())-5);}
  int hit_menu(int x,int y) const {if(x<10||x>w-10)return -1;
    if(screen==Screen::dice){if(y<dice_actions_y()||y>=dice_actions_y()+32)return -1;return std::clamp((x-12)/((w-18)/3),0,2);}
    for(int i=0;i<menu_count();++i)if(y>=row_y(i)&&y<row_y(i)+row_height())return i;return -1;}
  pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& event){
    if(event.is<pxa::SensorSample>()){
      if(!ctx->foreground())return true;
      if(auto sample=acceleration.sample(event)){
        bool was=motion.calibrating;
        bool restored=!motion.fresh(sample->timestamp_us);
        bool shaken=motion.acceleration(sample->values,sample->timestamp_us);
        dirty|=restored;
        if(was&&!motion.calibrating){dirty=true;log("Gravity calibrated");}
        if(shaken&&screen==Screen::dice)roll();
      }else if(auto gyro=rotation.sample(event)){
        ++gyro_samples;float energy=0;for(auto value:gyro->values)energy+=std::abs(float(value))*.000001f;gyro_energy=std::min(1.4f,energy);
      }return true;
    }
    auto tick=pxa::decode_clock_tick(event);if(!tick)return false;
    now=tick->timestamp_us;float dt=last_tick&&now>=last_tick?std::min(.05f,float(now-last_tick)*1e-6f):0;last_tick=now;
    if(!ready||!ctx->foreground())return true;
    if(rolling>0){rolling=std::max(0.f,rolling-dt);dirty=true;}
    if(screen==Screen::maze){Vec tilt{};bool active=false;
      if(dragging){tilt=touch;active=true;}else if(manual){tilt={std::cos(heading)*.52f,std::sin(heading)*.52f};active=true;}
      else if(motion.fresh(now)&&motion.calibrated){tilt=motion.tilt();active=true;}
      if(active){auto stars=maze.collected;auto seconds=unsigned(maze.elapsed);
        int previous_x=int(maze.ball.x*cell),previous_y=int(maze.ball.y*cell);
        maze.tick(dt,tilt,sensitivity==0?.65f:sensitivity==1?1.f:1.45f);
        if(stars!=maze.collected)log("Gravity star collected");
        // Retain identical images while the ball is still or moves less than
        // one output pixel. Physics and sensor sampling continue independently.
        dirty|=stars!=maze.collected||seconds!=unsigned(maze.elapsed)||
          previous_x!=int(maze.ball.x*cell)||previous_y!=int(maze.ball.y*cell);
        if(maze.won){screen=Screen::result;focus=0;unlocked=std::max(unlocked,std::min(11u,level+1));
          unsigned slot=6+level;auto ms=uint32_t(maze.elapsed*1000);if(!record[slot]||ms<record[slot])record[slot]=ms;
          save();release_sensors();dirty=true;log("Gravity maze won");}}
      else if(dt>0&&now/200000!=uint64_t((now-uint64_t(dt*1000000))/200000))dirty=true;
    }
    if(dirty)render();
    if(now-last_report>=5000000&&renderer){last_report=now;if(auto t=renderer->telemetry()){
      char s[240];std::snprintf(s,sizeof s,"Gravity perf frames=%llu visible=%llu draw=%u raster_us=%u rejected=%u samples=%u gyro=%u ball=%d,%d",
        (unsigned long long)t->submitted_frames,(unsigned long long)t->visible_frames,t->last_draw_list_bytes,t->last_host_raster_us,t->rejected_lists,motion.samples,gyro_samples,int(maze.ball.x*1000),int(maze.ball.y*1000));log(s);}}
    return true;
  }
  void on_background(pxa::Context&){backgrounded=true;release_sensors();controller.reset();wait_release=true;dragging=false;touch={};maze.velocity={};last_tick=0;(void)ctx->clock().set_period(0);if(screen==Screen::maze){screen=Screen::pause;focus=0;}dirty=true;}
  void on_foreground(pxa::Context&){last_tick=0;wait_release=backgrounded;dirty=true;(void)ctx->clock().set_period(33);
    (void)ctx->window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);if(screen==Screen::dice)connect();}
  void button(Paint& p,int i,std::string_view label,uint16_t color=mint){int y=row_y(i),height=row_height();
    int x=screen==Screen::dice?12+i*((w-18)/3):12;
    int width=screen==Screen::dice?(w-36)/3:w-24;
    if(focus==unsigned(i))p.round(x-3,y-2,width+6,height+4,teal,9);
    p.round(x,y,width,height,color,7);p.text(x+width/2,y+(height-font_cell_h)/2,label,ink,1);}
  void render(){if(!ready||!renderer||!ctx->foreground())return;
    pxa::game::Frame frame(*renderer,draw.bytes());frame.clear({cream});
    Paint p{frame,renderer->supports(pxa::game::RenderCapability::sprite_texel_alpha),w,h};
    char text[120];
    if(screen==Screen::home){
      p.circle(w-25,22,29,mint);p.star(w-26,22,11,gold);p.text(14,12,"重力游乐场",ink);
      p.text(14,36,"小小动作，大大冒险",muted);
      button(p,0,"摇摇骰子",pink);button(p,1,"弹珠迷宫",mint);button(p,2,"体感设置",lilac);button(p,3,"退出",rgb(241,236,223));
      int icon_y=row_y(0)+(row_height()-26)/2;p.die(24,icon_y,26,5);
      p.marble(37,row_y(1)+row_height()/2,12);
      p.circle(37,row_y(2)+row_height()/2,12,purple);p.circle(37,row_y(2)+row_height()/2,5,lilac);
      p.text(w/2,h-29,sensor_message,muted,1);
    }else if(screen==Screen::dice){
      p.text(14,10,"摇摇骰子",ink);p.text(w-14,11,manual?"确认摇一摇":"轻摇或确认",muted,2);
      int available=dice_actions_y()-76,size=std::min(dice_count==2?(w-54)/2:w/2,available-20);
      p.round(12,68,w-24,available+2,rgb(249,236,216),12);
      size=std::max(28,size);int center_y=72+(available-size)/2;
      for(unsigned i=0;i<dice_count;++i){int x=dice_count==1?(w-size)/2:((w-2*size-18)/2+int(i)*(size+18)),y=center_y;
        if(rolling){float t=float(now-roll_start)*.000001f,amplitude=rolling*9;
          x+=int(std::sin(t*31+i*2)*amplitude);y+=int(std::cos(t*25+i)*amplitude*(1+gyro_energy*.4f));}
        unsigned shown=rolling?unsigned((now/70000+i*2)%6)+1:dice[i];p.die(x,y,size,shown,i?rgb(241,235,255):white);}
      if(!rolling){if(previous_total)std::snprintf(text,sizeof text,"点数 %u · 上次 %u",dice[0]+(dice_count==2?dice[1]:0),previous_total);
        else std::snprintf(text,sizeof text,"点数 %u",dice[0]+(dice_count==2?dice[1]:0));p.text(w/2,37,text,teal,1);}
      else p.text(w/2,37,"好运翻滚中…",coral,1);
      button(p,0,"摇一摇",pink);button(p,1,dice_count==1?"单骰":"双骰",lilac);button(p,2,"首页",mint);
    }else if(screen==Screen::maze){
      std::snprintf(text,sizeof text,"星光迷宫 %02u",level+1);p.text(12,8,text,ink);p.text(w-12,8,"暂停",teal,2);
      unsigned stars=unsigned(std::popcount(maze.collected));std::snprintf(text,sizeof text,"星星 %u / 3",stars);p.text(12,31,text,muted);
      std::snprintf(text,sizeof text,"%02u:%02u",unsigned(maze.elapsed)/60,unsigned(maze.elapsed)%60);p.text(w-12,31,text,muted,2);
      int available=h-87;cell=std::max(5,std::min((w-22)/maze.w,available/maze.h));board_x=(w-cell*maze.w)/2;board_y=58+(available-cell*maze.h)/2;
      p.round(board_x-5,board_y-5,maze.w*cell+10,maze.h*cell+10,rgb(193,215,199),8);
      p.rect(board_x,board_y,maze.w*cell,maze.h*cell,rgb(244,241,218));
      for(int y=0;y<maze.h;++y)for(int x=0;x<maze.w;){if(!maze.wall(x,y)){++x;continue;}int start=x;while(x<maze.w&&maze.wall(x,y))++x;
        int xx=board_x+start*cell,yy=board_y+y*cell,ww=(x-start)*cell;p.rect(xx,yy,ww,cell,rgb(112,157,140));p.rect(xx,yy,ww,std::max(1,cell/7),rgb(152,190,166));p.rect(xx,yy+cell-2,ww,2,rgb(76,128,114));}
      p.circle(board_x+int((maze.w-1.5f)*cell),board_y+int((maze.h-1.5f)*cell),std::max(3,cell/3),stars==3?gold:rgb(207,203,179));
      for(unsigned i=0;i<3;++i)if(!(maze.collected&(1u<<i)))p.star(board_x+(maze.stars[i]%maze.w)*cell+cell/2,board_y+(maze.stars[i]/maze.w)*cell+cell/2,std::max(3,cell/3),gold);
      p.marble(board_x+int(maze.ball.x*cell),board_y+int(maze.ball.y*cell),std::max(2,int(Maze::radius*cell)));
      std::string_view hint=manual?"上下转向 · 确认暂停":!acceleration?"未连接体感，暂停切换按键":motion.calibrating?"保持不动，正在校准":!motion.fresh(now)?"等待读数 · 确认可暂停":"收集星星，滚向金色出口";
      if(manual){int x=board_x+int(maze.ball.x*cell),y=board_y+int(maze.ball.y*cell);p.circle(x+int(std::cos(heading)*cell*.6f),y+int(std::sin(heading)*cell*.6f),2,coral);}
      p.text(w/2,h-29,hint,teal,1);
    }else if(screen==Screen::pause){
      p.text(14,13,"歇一会儿",ink);p.text(14,35,"小弹珠在等你回来",muted);
      button(p,0,"继续冒险");button(p,1,"重新校准",lilac);button(p,2,manual?"切换到体感":"切换到按键",pink);
      button(p,3,"重玩本关",lilac);button(p,4,"回到首页");
    }else if(screen==Screen::settings){
      p.text(14,12,"体感设置",ink);
      const char* speed[]={"轻柔","标准","灵敏"};std::snprintf(text,sizeof text,"灵敏度：%s",speed[sensitivity]);button(p,0,text,mint);
      std::snprintf(text,sizeof text,"方向旋转：%u度",axis*90);button(p,1,text,lilac);
      button(p,2,manual?"控制：按键练习":"控制：倾斜体感",pink);
      button(p,3,"从第一关开始",mint);button(p,4,"回到首页",rgb(241,236,223));
      p.text(w/2,h-29,"触屏可拖动控制弹珠",muted,1);
    }else if(screen==Screen::result){
      p.text(w/2,12,"星星收集成功！",teal,1);for(int i=0;i<3;++i)p.star(w/2+(i-1)*31,45,11,gold);
      std::snprintf(text,sizeof text,"本关用时 %u 秒",unsigned(maze.elapsed));
      // The result uses the same compact menu geometry on small displays.
      button(p,0,"下一关",mint);button(p,1,"再挑战一次",lilac);button(p,2,"回到首页",pink);p.text(w/2,h-29,text,muted,1);
    }
    auto result=frame.submit();dirty=!result;
    if(result)++frame_count;else if(result.error()!=pxa::Error::would_block){
      char error[100];std::snprintf(error,sizeof error,"Gravity frame rejected error=%d bytes=%u",int(result.error()),unsigned(frame.bytes_used()));log(error);}
  }
};
}
PXA_APPLICATION(gravity::App)
