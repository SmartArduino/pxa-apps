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

static void hex_out(const uint8_t *data,size_t n){for(size_t i=0;i<n;i++)printf("%02x",data[i]);}
static int hex_read(const char*s,uint8_t*data,size_t n){if(strlen(s)!=n*2)return 0;for(size_t i=0;i<n;i++){unsigned value;if(sscanf(s+2*i,"%2x",&value)!=1)return 0;data[i]=value;}return 1;}
static unsigned core_game(void){return stepFunction==runnerStep?4:(stepFunction==menuStep?5:255);}
static void json_state(void){unsigned n=list_count();printf("{\"frame\":%u,\"rng\":%u,\"core_game\":%u,\"menu_item\":%u,\"first_click\":%u,\"position\":%u,\"phase\":%u,\"height\":%u,\"tallness\":%u,\"next\":%u,\"shield\":%u,\"state\":%u,\"score\":%u,\"sentinel_index\":%u,\"obstacles\":[",(unsigned)SAF_frame(),_SAF_currentRandom,core_game(),menuItem,firstClick,RUNNER_POSITION,RUNNER_PLAYER_PHASE,runnerPlayerHeight(),runnerPlayerTallness(),RUNNER_NEXT_OBSTACLE_IN,RUNNER_SHIELD_COUNTDOWN,RUNNER_STATE,RUNNER_SCORE,n*2);for(unsigned i=0;i<n;i++){if(i)printf(",");printf("[%u,%u]",memory[2*i],memory[2*i+1]);}printf("],\"memory\":\"");hex_out(memory,512);printf("\",\"save\":\"");hex_out(_SAF_saveMemory,32);printf("\",\"held\":\"");hex_out(_SAF_buttonStates,7);printf("\",\"pixels\":\"");hex_out(screen,4096);printf("\",\"pixels_fnv1a32\":%u}\n",hash_screen());fflush(stdout);}
int main(void){reset();menuItem=4;firstClick=1;char command[16],memhex[1025],savehex[65],heldhex[15];unsigned f,r,g,m,first,b,count;while(scanf("%15s",command)==1){if(!strcmp(command,"RESET")){reset();menuItem=4;firstClick=1;runnerDraw();json_state();}else if(!strcmp(command,"LOAD")){if(scanf("%u %u %u %u %u %1024s %64s %14s",&f,&r,&g,&m,&first,memhex,savehex,heldhex)!=8||r>255||(g!=4&&g!=5)||m>=5||first>1)return 3;reset();if(!hex_read(memhex,memory,512)||!hex_read(savehex,_SAF_saveMemory,32)||!hex_read(heldhex,_SAF_buttonStates,7))return 3;memcpy(storage,_SAF_saveMemory,32);_SAF_saveMemoryLoaded=1;_SAF_frame=f;_SAF_currentRandom=r;stepFunction=g==4?runnerStep:menuStep;menuItem=m;firstClick=first;json_state();}else if(!strcmp(command,"STEP")){if(scanf("%u %u",&b,&count)!=2||b>127||count>100000)return 3;for(unsigned i=0;i<count;i++)frame(b);json_state();}else if(!strcmp(command,"INIT")){runnerInit();memset(_SAF_buttonStates,0,sizeof _SAF_buttonStates);stepFunction=runnerStep;menuItem=4;firstClick=1;runnerDraw();json_state();}else if(!strcmp(command,"RELEASE")){memset(_SAF_buttonStates,0,sizeof _SAF_buttonStates);pressed=0;json_state();}else if(!strcmp(command,"RESUME")){memset(_SAF_buttonStates,0,sizeof _SAF_buttonStates);pressed=0;stepFunction=runnerStep;json_state();}else if(!strcmp(command,"DRAW")){if(stepFunction==runnerStep)runnerDraw();else menuStep();json_state();}else return 3;}return 0;}
