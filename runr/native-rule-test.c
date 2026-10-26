/* Native test frontend and fixtures only. Full original minigames.ino included. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
static uint8_t screen[4096], pressed, storage[32];
static unsigned sound_counts[4], draw_pixels, cases;
#include "minigames.ino"
static inline void SAF_FE_drawPixel(uint8_t x,uint8_t y,uint8_t c){assert(x<64&&y<64);screen[y*64+x]=c;draw_pixels++;}
static inline void SAF_FE_playSound(uint8_t s){assert(s<4);sound_counts[s]++;}
static inline void SAF_FE_save(uint8_t i,uint8_t d){assert(i<32);storage[i]=d;}
static inline uint8_t SAF_FE_load(uint8_t i){assert(i<32);return storage[i];}
static inline uint8_t SAF_FE_buttonPressed(uint8_t b){return (pressed>>b)&1;}
static inline const char *SAF_FE_extension(const char *s){(void)s;return SAF_FE_emptyString;}
static void check(int c,const char *n){printf("{\"name\":\"%s\",\"passed\":%s}\n",n,c?"true":"false");fflush(stdout);cases++;if(!c)exit(2);}
static uint32_t hash_screen(void){uint32_t h=2166136261u;for(int i=0;i<4096;i++){h^=screen[i];h*=16777619u;}return h;}
static void reset(void){memset(memory,0,sizeof memory);memset(_SAF_buttonStates,0,sizeof _SAF_buttonStates);memset(storage,0,sizeof storage);memset(_SAF_saveMemory,0,sizeof _SAF_saveMemory);_SAF_saveMemoryLoaded=0;_SAF_frame=0;pressed=0;memset(screen,0,sizeof screen);memset(sound_counts,0,sizeof sound_counts);draw_pixels=0;runnerInit();stepFunction=runnerStep;SAF_randomSeed(0);}
static void frame(uint8_t b){pressed=b;SAF_FE_loop();}
static unsigned list_count(void){unsigned n=0;while(n<128&&memory[2*n])n++;return n;}
static int normal_list(void){unsigned n=list_count();if(n>5)return 0;for(unsigned i=0;i<n;i++){if(memory[2*i]>7||memory[2*i+1]<1||memory[2*i+1]>72)return 0;if(i&&memory[2*i-1]>=memory[2*i+1])return 0;}return memory[2*n]==0;}
static void obstacle(uint8_t type,uint8_t x){memory[0]=type;memory[1]=x;memory[2]=0;}
static void dump_ppm(const char *path){FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n64 64\n255\n");for(int i=0;i<4096;i++){uint8_t rgb[3];SAF_colorToRGB(screen[i],rgb,rgb+1,rgb+2);assert(fwrite(rgb,1,3,f)==3);}fclose(f);}
int main(int argc,char **argv){
 if(argc>1&&!strcmp(argv[1],"missing-sentinel")){reset();for(int i=0;i<256;i+=2){memory[i]=1;memory[i+1]=72;}runnerAddObstacle(1);return 4;}
 reset();check(RUNNER_POSITION==0&&RUNNER_PLAYER_PHASE==0&&RUNNER_NEXT_OBSTACLE_IN==0&&RUNNER_SHIELD_COUNTDOWN==0&&RUNNER_STATE==0&&RUNNER_SCORE==0&&memory[0]==0,"original runnerInit resets all six state fields and empty sentinel");
 check(SAF_FPS==25&&SAF_MS_PER_FRAME==40&&SAF_SCREEN_WIDTH==64&&SAF_SCREEN_HEIGHT==64,"original 25fps 40ms 64x64 constants");
 frame(1<<SAF_BUTTON_A);check(RUNNER_PLAYER_PHASE==7&&RUNNER_POSITION==1&&list_count()==1&&memory[1]==72&&sound_counts[SAF_SOUND_BUMP]==1&&SAF_frame()==1,"original A edge jump and first birth after scroll");
 while(RUNNER_PLAYER_PHASE){frame(1<<SAF_BUTTON_A);}unsigned landed_frame=SAF_frame();frame(1<<SAF_BUTTON_A);check(RUNNER_PLAYER_PHASE==0&&SAF_frame()==landed_frame+1&&SAF_buttonPressed(SAF_BUTTON_A)>1,"held A does not repeat jump on landing");
 frame(0);frame(1<<SAF_BUTTON_A);check(RUNNER_PLAYER_PHASE==7,"release and second A edge jumps again");
 reset();frame(1<<SAF_BUTTON_UP);while(RUNNER_PLAYER_PHASE)frame(1<<SAF_BUTTON_UP);frame(1<<SAF_BUTTON_UP);check(RUNNER_PLAYER_PHASE==7&&SAF_buttonPressed(SAF_BUTTON_UP)>1,"held UP auto-jumps after landing");
 reset();frame(1<<SAF_BUTTON_DOWN);check(RUNNER_PLAYER_PHASE==255&&runnerPlayerTallness()==4&&runnerPlayerHeight()==0,"held DOWN ducks to original height4 phase255");frame(0);check(RUNNER_PLAYER_PHASE==0&&runnerPlayerTallness()==8,"DOWN release restores original standing tallness8");
 reset();for(unsigned t=1;t<=7;t++){uint8_t w,h,e;runnerGetObstacleSize(t,72,&w,&h,&e);int ok=w>0&&h>0&&w<=8&&h<=8&&e<=20;if(t==1)ok&=w==5&&h==6&&e==0;if(t==2)ok&=w==5&&h==6&&e==6;if(t==3)ok&=w==4&&h==8&&e==0;if(t==4)ok&=w==8&&h==3&&e==0;if(t==5)ok&=w==3&&h==6&&e<=7;if(t>=6)ok&=w==4&&h==4&&e==20;char n[96];snprintf(n,sizeof n,"original obstacle type%u dimensions/elevation and takeability",t);check(ok&&runnerObstacleIsTakeable(t)==(t>=6),n);}
 reset();obstacle(1,20);runnerResolveCollisions();check(RUNNER_STATE==1&&sound_counts[SAF_SOUND_BOOM]==1&&list_count()==1,"original standing wall collision loses without altering list");
 reset();RUNNER_SCORE=9;obstacle(1,20);runnerResolveCollisions();check(RUNNER_STATE==2&&getHiScore(4)==9&&storage[8]==9&&storage[9]==0,"original loss high score updates native 32byte session storage");
 reset();obstacle(2,20);RUNNER_PLAYER_PHASE=255;runnerResolveCollisions();check(RUNNER_STATE==0&&list_count()==1,"original duck clears ceiling without substitute collision rule");
 RUNNER_PLAYER_PHASE=0;runnerResolveCollisions();check(RUNNER_STATE==1,"original standing collides with same ceiling");
 reset();obstacle(1,20);RUNNER_PLAYER_PHASE=56;runnerResolveCollisions();check(runnerPlayerHeight()>6&&RUNNER_STATE==0,"original sinusoidal jump avoids low wall");
 reset();RUNNER_PLAYER_PHASE=56;obstacle(6,20);runnerResolveCollisions();check(RUNNER_SCORE==5&&memory[0]==0&&sound_counts[SAF_SOUND_CLICK]==1,"original jump takes bonus removes item and adds5 points");
 reset();RUNNER_PLAYER_PHASE=56;obstacle(7,20);runnerResolveCollisions();check(RUNNER_SHIELD_COUNTDOWN==255&&memory[0]==0,"original jump takes shield countdown255");
 RUNNER_PLAYER_PHASE=0;obstacle(1,20);runnerResolveCollisions();check(RUNNER_STATE==0&&RUNNER_SHIELD_COUNTDOWN==0&&memory[0]==0,"original shield absorbs one collision and removes obstacle");
 reset();RUNNER_SHIELD_COUNTDOWN=10;frame(0);check(RUNNER_SHIELD_COUNTDOWN==9,"original shield decrements one per active frame");
 for(unsigned t=1;t<=7;t++){reset();obstacle(t,1);runnerScroll();char n[96];snprintf(n,sizeof n,"original offscreen type%u removed exactly x0 and score matches",t);check(memory[0]==0&&RUNNER_POSITION==1&&RUNNER_SCORE==(t<=5?2:0),n);}
 reset();memory[0]=1;memory[1]=1;memory[2]=2;memory[3]=20;memory[4]=6;memory[5]=40;memory[6]=0;runnerScroll();check(list_count()==2&&memory[0]==2&&memory[1]==19&&memory[2]==6&&memory[3]==39&&memory[4]==0,"original offscreen compaction preserves sorted pair list and sentinel");runnerRemoveObstacle(0);check(list_count()==1&&memory[0]==6&&memory[1]==39&&memory[2]==0,"original collision remove compacts head and terminates");
 reset();runnerDraw();uint32_t ground_hash=hash_screen();check(screen[32*64+8]==SAF_COLOR_BLACK&&screen[40*64]==SAF_COLOR_GRAY_DARK&&draw_pixels>=4096,"original runnerDraw full frame contains player and ground");if(argc>1)dump_ppm(argv[1]);
 RUNNER_PLAYER_PHASE=56;runnerDraw();check(hash_screen()!=ground_hash&&screen[32*64+8]==SAF_COLOR_WHITE,"original jump drawing changes full4096pixel frame");
 reset();RUNNER_STATE=1;frame(1<<SAF_BUTTON_A);check(RUNNER_STATE==0&&RUNNER_POSITION==0&&RUNNER_SCORE==0&&memory[0]==0,"original A edge restarts loss without same-frame scroll");
 reset();RUNNER_STATE=1;frame(1<<SAF_BUTTON_B);check(RUNNER_STATE==0&&RUNNER_POSITION==0,"original B edge restarts loss");
 reset();for(int i=0;i<18;i++)frame(1<<SAF_BUTTON_B);check(stepFunction==menuStep&&SAF_buttonPressed(SAF_BUTTON_B)==18,"original B hold18 returns original menu");
 reset();RUNNER_STATE=1;for(int i=0;i<300;i++)frame(1<<SAF_BUTTON_C);check(SAF_buttonPressed(SAF_BUTTON_C)==255,"original SAF held counter saturates at255");
 unsigned births=0,max_alive=0,position_wraps=0,min_gap=999,max_gap=0,last_birth=0;reset();RUNNER_POSITION=65500;for(unsigned tick=0;tick<131072;tick++){uint16_t before=RUNNER_POSITION;runnerScroll();if(RUNNER_POSITION<before)position_wraps++;if(RUNNER_NEXT_OBSTACLE_IN==0){runnerAddObstacle((births%7)+1);if(births){unsigned gap=tick-last_birth;if(gap<min_gap)min_gap=gap;if(gap>max_gap)max_gap=gap;}last_birth=tick;births++;}else RUNNER_NEXT_OBSTACLE_IN--;unsigned n=list_count();if(n>max_alive)max_alive=n;if(!normal_list()){fprintf(stderr,"bad lifecycle tick%u count%u\n",tick,n);exit(2);}if((tick&1023)==0)runnerDraw();}check(position_wraps==2&&births>6000&&max_alive<=5&&min_gap>=16,"original scroll/add lifecycle131072ticks crosses position wraps and alive<=5");printf("{\"lifecycle\":{\"ticks\":131072,\"births\":%u,\"max_alive\":%u,\"max_sentinel_index\":%u,\"position_wraps\":%u,\"min_birth_gap\":%u,\"max_birth_gap\":%u}}\n",births,max_alive,2*max_alive,position_wraps,min_gap,max_gap);
 reset();RUNNER_POSITION=2560;unsigned zero_rng_seed=256;for(unsigned seed=0;seed<256;seed++){SAF_randomSeed(seed);if(SAF_random()%8==0){zero_rng_seed=seed;break;}}assert(zero_rng_seed<256);unsigned extreme_max=0,extreme_births=0;for(unsigned tick=0;tick<10000;tick++){runnerScroll();if(RUNNER_NEXT_OBSTACLE_IN==0){SAF_randomSeed(zero_rng_seed);runnerAddObstacle(1);if(RUNNER_NEXT_OBSTACLE_IN!=15){fprintf(stderr,"bad minimum-gap fixture\n");exit(2);}extreme_births++;}else RUNNER_NEXT_OBSTACLE_IN--;unsigned n=list_count();if(n>extreme_max)extreme_max=n;if(!normal_list()){fprintf(stderr,"bad extreme tick%u\n",tick);exit(2);}}check(extreme_max==5&&extreme_births==625,"native original minimum15countdown gives16birthgap maximum5alive sentinel10");printf("{\"minimum_gap_fixture\":{\"ticks\":10000,\"births\":625,\"rng_seed_reapplied_native_only\":%u,\"maximum_alive\":5,\"maximum_sentinel_index\":10,\"birth_gap_frames\":16}}\n",zero_rng_seed);
 unsigned real_frames=0,losses=0,restarts=0,real_max=0;for(unsigned seed=0;seed<256;seed++){reset();SAF_randomSeed(seed);for(unsigned tick=0;tick<512;tick++){uint8_t keys=0;if(RUNNER_STATE){losses++;if(SAF_buttonPressed(SAF_BUTTON_A)==0){keys=1<<SAF_BUTTON_A;restarts++;}}else if((tick%128)<96)keys=1<<SAF_BUTTON_UP;else keys=1<<SAF_BUTTON_DOWN;frame(keys);if(stepFunction!=runnerStep||!normal_list()){fprintf(stderr,"bad real seed%u tick%u\n",seed,tick);exit(2);}unsigned n=list_count();if(n>real_max)real_max=n;real_frames++;}}check(real_frames==131072&&losses>0&&restarts>0&&real_max<=5,"original SAF loop256seeds131072realframes with jump duck collisions and restart");printf("{\"normal_frames\":{\"frames\":%u,\"seeds\":256,\"loss_frames\":%u,\"restart_edges\":%u,\"max_alive\":%u}}\n",real_frames,losses,restarts,real_max);
 reset();for(unsigned i=0;i<126;i++){memory[2*i]=1;memory[2*i+1]=72;}memory[252]=0;runnerAddObstacle(2);check(list_count()==127&&memory[254]==0&&memory[252]==2&&memory[253]==72,"native abnormal capacity126 can append127 and retains sentinel254");runnerAddObstacle(3);check(memory[0]==0&&memory[254]==3&&memory[255]==72,"original abnormal append at sentinel254 wraps sentinel to0 and clears visible list");
 printf("{\"summary\":{\"passed\":true,\"checks\":%u,\"original_game_core_unchanged\":true,\"guest_executed\":false,\"gui_executed\":false}}\n",cases);return 0;
}
