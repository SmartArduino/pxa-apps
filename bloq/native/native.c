/* Native QA of fixed-commit original BLOQ, CC0 source; I/O-only SAF frontend. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SAF_PLATFORM_NULL
#include "minigames.ino"
static unsigned passed;
#define CHECK(c,n) do { if(!(c)){fprintf(stderr,"FAIL %s line=%d\n",n,__LINE__);exit(1);} printf("PASS %s\n",n);passed++; } while(0)
static unsigned active(void){unsigned n=0;for(int i=0;i<150;i++)n+=(memory[i]&128)!=0;return n;}
static unsigned solid(void){unsigned n=0;for(int i=0;i<150;i++)n+=blocksSquareIsSolid(i);return n;}
static void keys(unsigned mask){for(int i=0;i<7;i++)_SAF_buttonStates[i]=(mask>>i)&1;}
static void reset(unsigned type){memset(test_keys,0,7);memset(_SAF_buttonStates,0,7);SAF_randomSeed(0);blocksInit();memset(memory,0,150);blocksSpawnBlock(type);BLOCKS_NEXT_LEVEL_IN=1000;BLOCKS_NEXT_MOVE=100;stepFunction=blocksStep;}
static void tick(unsigned mask){keys(mask);blocksStep();_SAF_frame++;}
static void show(void){printf("{\"state\":%u,\"speed\":%u,\"next\":%u,\"wait\":%u,\"level\":%u,\"score\":%u,\"frame\":%u,\"rng\":%u,\"board\":\"",BLOCKS_STATE,BLOCKS_SPEED,BLOCKS_NEXT_MOVE,BLOCKS_WAIT_TIMER,BLOCKS_LEVEL,BLOCKS_SCORE,(unsigned)SAF_frame(),_SAF_currentRandom);for(int i=0;i<150;i++)printf("%02x",memory[i]);puts("\"}");}
int main(int argc,char **argv){
 if(argc>1 && !strcmp(argv[1],"empty-harddrop")){reset(7);memset(memory,0,150);keys(1<<SAF_BUTTON_A);puts("EMPTY_ACTIVE_HARDDROP_ENTER");fflush(stdout);blocksStep();puts("EMPTY_ACTIVE_HARDDROP_RETURN");return 0;}
 if(argc>1 && !strcmp(argv[1],"replay")){unsigned seed,mask;if(scanf("%u",&seed)!=1)return 2;SAF_randomSeed(seed);blocksInit();stepFunction=blocksStep;show();while(scanf("%u",&mask)==1){tick(mask);show();}return 0;}
 blocksInit();CHECK(active()==4 && solid()==0 && BLOCKS_SPEED==25 && BLOCKS_NEXT_MOVE==25 && BLOCKS_LEVEL==0 && BLOCKS_SCORE==0,"init original four-cell active/25-speed/zero-score");
 unsigned types=0;for(unsigned t=0;t<8;t++){reset(t);CHECK(active()==4,"spawn four active cells");for(int i=0;i<150;i++)if(memory[i]&128)CHECK((memory[i]&7)==(t?t:7),"spawn exact original type");types++;}CHECK(types==8,"all eight random residues include original square alias");
 for(unsigned t=1;t<=7;t++){reset(t);for(int j=0;j<3;j++)CHECK(blocksFallStep()==1,"valid piece falls");uint8_t save[150];memcpy(save,memory,150);blocksRotate(0);blocksRotate(1);CHECK(!memcmp(save,memory,150),"CW plus CCW preserves original piece");for(int j=0;j<4;j++)blocksRotate(0);CHECK(!memcmp(save,memory,150),"four CW rotations preserve original piece");}
 reset(6);for(int j=0;j<3;j++)blocksFallStep();uint8_t origin[150],rotated[150];memcpy(origin,memory,150);blocksRotate(0);memcpy(rotated,memory,150);int target=-1;for(int i=0;i<150;i++)if(rotated[i] && !origin[i]){target=i;break;}CHECK(target>=0,"I rotation has new destination");memcpy(memory,origin,150);memory[target]=2;memcpy(origin,memory,150);blocksRotate(0);CHECK(!memcmp(origin,memory,150),"rotation rejects solid collision atomically");
 reset(7);for(int j=0;j<10;j++)blocksMoveHorizontally(1);memcpy(origin,memory,150);blocksMoveHorizontally(1);CHECK(!memcmp(origin,memory,150) && memory[0]&128 && memory[10]&128,"left wall rejects wrapped movement");for(int j=0;j<15;j++)blocksMoveHorizontally(0);memcpy(origin,memory,150);blocksMoveHorizontally(0);CHECK(!memcmp(origin,memory,150) && memory[9]&128 && memory[19]&128,"right wall rejects wrapped movement");
 reset(7);memory[3]=1;memcpy(origin,memory,150);blocksMoveHorizontally(1);CHECK(!memcmp(origin,memory,150),"horizontal solid collision rejects movement");
 reset(7);memory[24]=1;memcpy(origin,memory,150);CHECK(blocksFallStep()==0 && !memcmp(origin,memory,150),"gravity rejects solid collision without mutation");
 reset(7);unsigned falls=0;while(blocksFallStep()){falls++;if(falls>15)return 3;}CHECK(falls==13 && memory[144]&128 && memory[145]&128,"gravity stops square exactly at floor");memcpy(origin,memory,150);CHECK(blocksFallStep()==0&&!memcmp(origin,memory,150),"floor collision is stable");
 reset(7);tick(1<<SAF_BUTTON_A);CHECK(BLOCKS_SCORE==1 && solid()==4 && active()==4 && BLOCKS_NEXT_MOVE==25 && BLOCKS_STATE==0,"ordinary A harddrop lands scores one and spawns next piece");
 reset(7);BLOCKS_NEXT_LEVEL_IN=0;BLOCKS_NEXT_MOVE=25;tick(0);CHECK(BLOCKS_LEVEL==1 && BLOCKS_SPEED==22 && BLOCKS_NEXT_LEVEL_IN==1499 && BLOCKS_NEXT_MOVE==24,"original first step raises level reduces interval three");
 reset(1);for(int j=0;j<3;j++)blocksFallStep();memcpy(origin,memory,150);blocksRotate(0);memcpy(rotated,memory,150);memcpy(memory,origin,150);tick(1<<SAF_BUTTON_UP);CHECK(!memcmp(rotated,memory,150),"ordinary UP applies original clockwise rotation");tick(1<<SAF_BUTTON_DOWN);CHECK(!memcmp(origin,memory,150),"ordinary DOWN applies original counterclockwise rotation");
 for(unsigned rows=1;rows<=2;rows++){reset(7);for(unsigned y=15-rows;y<15;y++)for(int x=0;x<10;x++)if(x!=4&&x!=5)memory[y*10+x]=2;tick(1<<SAF_BUTTON_A);unsigned flashing=0;for(int i=0;i<150;i++)flashing+=memory[i]==255;CHECK(BLOCKS_SCORE==1 && BLOCKS_WAIT_TIMER==20 && flashing==rows*10,"completed lines flash before delayed score");for(int j=0;j<18;j++)tick(0);CHECK(BLOCKS_SCORE==1 && BLOCKS_WAIT_TIMER==2,"line score waits until original countdown one");tick(0);CHECK(BLOCKS_SCORE==1+rows*10 && BLOCKS_WAIT_TIMER==1,"line removal scores ten per complete original row");CHECK(solid()==(rows==1?2:0),"line collapse retains only nonremoved landed cells");tick(0);CHECK(BLOCKS_WAIT_TIMER==0,"line flash wait ends");}
 reset(7);for(int i=0;i<20;i++)if(!(memory[i]&128))memory[i]=2;tick(1<<SAF_BUTTON_A);CHECK(BLOCKS_STATE==1 && BLOCKS_SCORE==1 && getHiScore(2)==1,"spawn collision triggers original game over and high score RAM");tick(1<<SAF_BUTTON_B);CHECK(BLOCKS_STATE==0 && BLOCKS_SCORE==0 && active()==4 && getHiScore(2)==1,"ordinary B restarts lost game preserves session high score");
 reset(7);keys(0);_SAF_buttonStates[SAF_BUTTON_B]=18;blocksStep();CHECK(stepFunction==menuStep,"original eighteen-frame B hold exits to original menu");
 reset(7);memset(test_screen,0,4096);blocksDraw();CHECK(test_screen[2*64+24]==SAF_COLOR_WHITE && test_screen[2*64+40]==SAF_COLOR_GREEN_DARK && test_screen[0]==SAF_COLOR_GRAY_DARK,"original board and square palette render in 64x64 framebuffer");BLOCKS_STATE=1;blocksDraw();CHECK(test_screen[0]==SAF_COLOR_RED_DARK,"original game-over background renders red");
 reset(7);memset(memory,0,150);memcpy(origin,memory,150);CHECK(blocksFallStep()==1 && !memcmp(origin,memory,150),"raw no-active FallStep returns true without mutation (known busyloop precondition)");
 printf("RESULT passed=%u\n",passed);return 0;
}
