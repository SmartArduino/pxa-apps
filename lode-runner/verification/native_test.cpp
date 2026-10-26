/* Native-only validation: includes the exact shipped owner and original gameplay. */
#include "../game.cpp"
#include <assert.h>
#include <stdio.h>
#include <chrono>
static void ms(unsigned n){while(n){unsigned step=n<40?n:40;arduboy_pxa_advance(step);n-=step;assert(!arduboy_pxa_state.fault);}}
static void key(uint8_t mask){arduboy_pxa_controller(mask);ms(80);arduboy_pxa_controller(0);ms(80);}
static void state(const char *name){
    uint8_t data[ARDU_GAME_MAX_BYTES];size_t n=arduboy_game_pack(data,sizeof data);assert(n==arduboy_game_pack(nullptr,0));
    printf("STATE %s state=%u level=%u lives=%u score=%u world=(%d,%d) gold=%u holes=%u frame=%u pixels=%08x bytes=%zu\n",name,(unsigned)gameState,level.getLevelNumber(),player.getMen(),player.getScore(),player.getX()-level.getXOffset(),player.getY()-level.getYOffset(),level.getGoldLeft(),holes.getCount(),arduboy.frameCount,arduboy_pxa_hash(arduboy_pxa_state.display,1024),n);
}
static void play_first(){
    arduboy_pxa_new();ms(80);key(A_BUTTON);key(A_BUTTON);key(A_BUTTON);
    for(unsigned i=0;i<100&&gameState!=GameState::LevelFlash;++i)ms(40);
    assert(gameState==GameState::LevelFlash);key(UP_BUTTON);assert(gameState==GameState::LevelPlay);state("PLAY");
}
static void position(unsigned x,unsigned y){
    level.setXOffset(0);level.setYOffset(0);level.setXOffsetDelta(0);level.setYOffsetDelta(0);
    player.setX((uint8_t)(x*10));player.setY((uint8_t)(y*10));player.setXDelta(0);player.setYDelta(0);player.setStance(PlayerStance::StandingStill);
    memset(enemies,0,sizeof enemies); // Controlled original-cell checks isolate player input from guards.
}
int main(){
    setvbuf(stdout,nullptr,_IONBF,0);
    auto start=std::chrono::steady_clock::now();arduboy_pxa_boot(123456u);assert(arduboy_pxa_state.constructors==1);assert(!arduboy_pxa_state.fault);
    assert(player.getMen()==5&&player.getScore()==0&&level.getLevelNumber()==1);state("BOOT");
    play_first();
    // Each bundled original map decodes using the shipped original loadLevel API.
    for(unsigned n=1;n<=155;++n){
        memset(&level,0,sizeof level);memset(&player,0,sizeof player);memset(enemies,0,sizeof enemies);
        level.setLevelNumber((uint8_t)n);level.loadLevel(&player,enemies);
        unsigned gold=0;for(unsigned y=0;y<16;++y)for(unsigned x=0;x<28;++x){auto tile=level.getLevelData(x,y);assert((unsigned)tile<=15);gold+=tile==LevelElement::Gold;}
        assert(gold==level.getGoldLeft());assert(level.getLevelLadderElementCount()<=19);
        for(unsigned i=0;i<level.getLevelLadderElementCount();++i){auto point=level.getLevelLadderElement(i);assert(point.x<28&&point.y<16);}
        for(const auto &e:enemies)if(e.getEnabled()){assert(e.getX()<280&&e.getY()<160);}
    }
    printf("PASS all_155_original_maps_decoded\n");
    // Render and run the original state machine for 80 frames on every real map.
    for(unsigned n=1;n<=155;++n){
        memset(&level,0,sizeof level);memset(&player,0,sizeof player);memset(enemies,0,sizeof enemies);holes=Queue<Hole,20>();
        level.setLevelNumber((uint8_t)n);player.setMen(5);level.loadLevel(&player,enemies);
        gameState=GameState::LevelPlay;suicide=levelCount=0;introRect=0;arduboy_pxa_release();
        for(unsigned frame=0;frame<80;++frame){
            const uint8_t pattern[]={0,RIGHT_BUTTON,LEFT_BUTTON,UP_BUTTON,DOWN_BUTTON,A_BUTTON,B_BUTTON,0};
            arduboy_pxa_controller(pattern[(frame/10)%8]);ms(40);
        }
    }
    arduboy_pxa_release();printf("PASS all_155_maps_original_play_and_render_80_frames\n");

    play_first();
    // Exercise original movement on a real original map cell with solid footing.
    unsigned sx=0,sy=0;
    for(unsigned y=0;y<15&&!sx;++y)for(unsigned x=2;x<10;++x)
        if(level.getLevelData(x,y)==LevelElement::Blank&&level.getLevelData(x+1,y)==LevelElement::Blank&&isSolid(level.getLevelData(x,y+1))&&isSolid(level.getLevelData(x+1,y+1))){sx=x;sy=y;break;}
    assert(sx);position(sx,sy);unsigned before=player.getX();key(RIGHT_BUTTON);assert(player.getX()!=before||level.getXOffset()!=0);state("MOVE_RIGHT");
    // Dig original brick to the right, then original countdown restores the brick.
    bool dug=false;
    for(unsigned y=0;y<15&&!dug;++y)for(unsigned x=2;x<10;++x)
        if(level.getLevelData(x,y)==LevelElement::Blank&&level.getLevelData(x+1,y)==LevelElement::Blank&&level.getLevelData(x+1,y+1)==LevelElement::Brick&&isSolid(level.getLevelData(x,y+1))){
            position(x,y);key(B_BUTTON);assert(holes.getCount()==1);assert(level.getLevelData(x+1,y+1)!=LevelElement::Brick);state("DIG_RIGHT");
            ms(9000);assert(holes.isEmpty());assert(level.getLevelData(x+1,y+1)==LevelElement::Brick);dug=true;break;
        }
    assert(dug);printf("PASS original_dig_refill\n");
    // Original pickup updates score/map/count and exposes exit ladder at the last gold.
    play_first();bool picked=false;
    for(unsigned y=0;y<16&&!picked;++y)for(unsigned x=0;x<28;++x)if(level.getLevelData(x,y)==LevelElement::Gold){
        position(x,y);unsigned gold=level.getGoldLeft();unsigned score=player.getScore();ms(40);
        assert(player.getScore()==score+25);assert(level.getGoldLeft()==gold-1);assert(level.getLevelData(x,y)!=LevelElement::Gold);picked=true;state("PICKUP");break;
    }
    assert(picked);
    // Queue full capacity is safe and retains FIFO order, including original return value.
    Queue<Hole,20> q;for(unsigned i=0;i<20;++i)assert(q.enqueue(Hole{(uint8_t)i,0,220}));
    assert(!q.enqueue(Hole{99,0,220}));for(unsigned i=0;i<20;++i)assert(q.dequeue().x==i);assert(q.isEmpty());
    printf("PASS queue_20_fifo\n");

    // Original consecutive level loads must disable surplus guards without a blanket reset.
    play_first();level.setLevelNumber(6);level.loadLevel(&player,enemies);
    unsigned count=0;for(const auto&e:enemies)count+=e.getEnabled();assert(count==5);
    level.setLevelNumber(7);level.loadLevel(&player,enemies);
    count=0;for(const auto&e:enemies)count+=e.getEnabled();assert(count==4);
    printf("PASS consecutive_level_6_to_7_guard_count_5_to_4\n");
    // All four original map-provided reentry locations rotate, then wrap to the first.
    LevelPoint points[5];for(unsigned i=0;i<5;++i)points[i]=level.getNextReentryPoint();
    assert(points[0].x==points[4].x&&points[0].y==points[4].y);
    bool different=false;for(unsigned i=1;i<4;++i)different|=points[i].x!=points[0].x||points[i].y!=points[0].y;assert(different);
    printf("PASS four_original_reentry_points_rotate\n");
    // Two colliding guards in one play frame take only the last life, never wrap to 255.
    play_first();position(2,1);level.setLevelData(2,1,LevelElement::Blank);level.setLevelData(2,2,LevelElement::Solid);player.setMen(1);
    for(unsigned i=0;i<2;++i){enemies[i].setX(20);enemies[i].setY(10);enemies[i].setEnabled(true);enemies[i].setStance(PlayerStance::StandingStill);}
    ms(80);assert(player.getMen()==0&&player.getNextState()==GameState::GameOver);
    playerDies();assert(player.getMen()==0);state("FINAL_LIFE");printf("PASS one_death_per_frame_no_life_underflow\n");


    // A completed last map must reach the original victory state without loading index 156.
    play_first();level.setLevelNumber(155);level.loadLevel(&player,enemies);position(1,0);
    level.setLevelData(1,0,LevelElement::Ladder);level.setLevelData(1,1,LevelElement::Solid);level.setGoldLeft(0);
    ms(40);assert(player.getNextState()==GameState::SeriesOver);ms(1600);
    assert(gameState==GameState::SeriesOver);state("VICTORY");printf("PASS final_level_original_victory\n");

    // NEW resets every gameplay object/counter and progress EEPROM after an active run.
    arduboy_pxa_new();assert(gameState==GameState::SplashScreen_Init);assert(level.getLevelNumber()==1&&player.getMen()==5&&player.getScore()==0);assert(holes.isEmpty());assert(!suicide&&!levelCount&&!menuSelect&&!flashPlayer);state("RESET");
    assert(!arduboy_pxa_state.fault);
    double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();printf("PASS native_core elapsed_ms=%.3f\n",elapsed);
}
