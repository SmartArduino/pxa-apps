/* SPDX-License-Identifier: MIT. Test-only original Cube rules and drawing calls. */
#include <stdarg.h>
#include <stdint.h>
#include "../common/retro-games/puzzles/vendor/cube.c"
void fatal(const char *f,...){va_list a;va_start(a,f);vfprintf(stderr,f,a);va_end(a);fputc('\n',stderr);exit(2);}
void frontend_default_colour(frontend *f,float *c){(void)f;c[0]=c[1]=c[2]=.9F;}
struct polygon_record {int n,fill,outline,p[8];};
static struct polygon_record records[64];static int nrecords;static char original_status[256];
void draw_rect(drawing*d,int x,int y,int w,int h,int c){(void)d;(void)x;(void)y;(void)w;(void)h;(void)c;}
void draw_update(drawing*d,int x,int y,int w,int h){(void)d;(void)x;(void)y;(void)w;(void)h;}
void status_bar(drawing*d,const char*s){(void)d;snprintf(original_status,sizeof(original_status),"%s",s);}
void draw_polygon(drawing*d,const int*p,int n,int fill,int outline){(void)d;assert(n<=4 && nrecords<64);struct polygon_record*r=&records[nrecords++];r->n=n;r->fill=fill;r->outline=outline;memcpy(r->p,p,2*n*sizeof(int));}
static bool actual_move(char*m){return m && m!=MOVE_UI_UPDATE && m!=MOVE_NO_EFFECT && m!=MOVE_UNUSED;}
static game_state *from_id(const char*id){char*b=dupstr(id),*desc=strchr(b,':');if(!desc)fatal("Missing description");*desc++=0;game_params*p=default_params();decode_params(p,b);const char*e=validate_params(p,true);if(!e)e=validate_desc(p,desc);if(e)fatal("Original validation: %s",e);game_state*s=new_game(NULL,p,desc);sfree(b);free_params(p);return s;}
static int face_count(const game_state*s){int n=0;for(int i=0;i<s->solid->nfaces;i++)n+=!!s->facecolours[i];return n;}
static int floor_count(const game_state*s){int n=0;for(int i=0;i<s->grid->nsquares;i++)n+=GET_SQUARE(s,i);return n;}
static uint32_t state_key(const game_state*s){uint32_t k=s->current;for(int i=0;i<6;i++)k|=(uint32_t)!!s->facecolours[i]<<(4+i);for(int i=0;i<16;i++)k|=(uint32_t)GET_SQUARE(s,i)<<(10+i);return k;}
static void decode_search_node(game_state*s,uint32_t k){s->current=k&15;s->completed=s->movecount=0;for(int i=0;i<6;i++)s->facecolours[i]=(k>>(4+i))&1;s->bluemask[0]=k>>10;}
struct search_node{uint32_t key,parent;char move;};
/* Every candidate edge is evaluated by the unchanged original execute_move.
 * The compact key only stores canonical colours/position. This is native QA,
 * never linked into the Guest and never injected into its game state. */
static char *normal_plan(const game_state*opening,int *visited){
 if(opening->params.solid!=CUBE || opening->params.d1!=4 || opening->params.d2!=4 || face_count(opening)+floor_count(opening)!=6)fatal("Native search supports original default 4x4 with six blue colours only");
 const uint32_t capacity=1200000;struct search_node*q=malloc(capacity*sizeof(*q));unsigned char*seen=calloc(1U<<23,1);if(!q||!seen)fatal("Native search allocation");
 game_state*scratch=dup_game(opening);q[0]=(struct search_node){state_key(opening),0,0};seen[q[0].key>>3]|=1U<<(q[0].key&7);uint32_t tail=1,goal=UINT32_MAX;
 for(uint32_t head=0;head<tail && goal==UINT32_MAX;head++){
  decode_search_node(scratch,q[head].key);
  for(int dir=0;dir<4;dir++){char move[2]={"LRUD"[dir],0};game_state*next=execute_move(scratch,move);if(!next)continue;uint32_t key=state_key(next);bool won=game_status(next)==1;free_game(next);if(seen[key>>3]&(1U<<(key&7)))continue;if(tail==capacity)fatal("Native search node bound");seen[key>>3]|=1U<<(key&7);q[tail]=(struct search_node){key,head,move[0]};if(won)goal=tail;tail++;if(won)break;}
 }
 if(goal==UINT32_MAX)fatal("No ordinary original-move completion found");int length=0;for(uint32_t i=goal;i;i=q[i].parent)length++;char*plan=snewn(length+1,char);plan[length]=0;for(uint32_t i=goal;i;i=q[i].parent)plan[--length]=q[i].move;*visited=tail;free_game(scratch);free(seen);free(q);
 game_state*verify=dup_game(opening);for(char*p=plan;*p;p++){char m[2]={*p,0};game_state*t=execute_move(verify,m);assert(t);free_game(verify);verify=t;}assert(game_status(verify)==1 && face_count(verify)==6 && floor_count(verify)==0);free_game(verify);return plan;
}
static void self_test(void){
 game_params*p=default_params();assert(p->solid==CUBE && p->d1==4 && p->d2==4 && !validate_params(p,true));assert(validate_desc(p,"ZZZZ,0") && validate_desc(p,"7E00x0"));
 game_state*s=from_id("c4x4:7E00,0");game_drawstate*ds=game_new_drawstate(NULL,s);game_set_size(NULL,ds,p,48);int w,h;game_compute_size(p,48,NULL,&w,&h);assert(w==240 && h==240);assert(interpret_move(s,NULL,ds,48,48,LEFT_BUTTON)==MOVE_NO_EFFECT);assert(interpret_move(s,NULL,ds,32,48,LEFT_BUTTON)==MOVE_NO_EFFECT);assert(interpret_move(s,NULL,ds,48,32,LEFT_BUTTON)==MOVE_NO_EFFECT);assert(interpret_move(s,NULL,ds,64,48,RIGHT_BUTTON)==MOVE_UNUSED);assert(interpret_move(s,NULL,ds,64,48,LEFT_RELEASE)==MOVE_UNUSED);assert(execute_move(s,"L")==NULL && execute_move(s,"U")==NULL && execute_move(s,"S")==NULL && execute_move(s,"X")==NULL);
 char*m=interpret_move(s,NULL,ds,64,48,LEFT_BUTTON);assert(actual_move(m)&&!strcmp(m,"R"));game_state*r=execute_move(s,m);sfree(m);assert(r && r->current==1 && r->movecount==1 && face_count(r)==1 && floor_count(r)==5 && r->facecolours[lowest_face(r->solid)]==1 && !GET_SQUARE(r,1));
 game_state*r2=execute_move(r,"R"),*r3=execute_move(r2,"L");assert(r2 && r3 && face_count(r2)==2 && face_count(r3)==1 && GET_SQUARE(r3,1));assert(s->current==0 && face_count(s)==0 && floor_count(s)==6);
 assert(game_anim_length(s,r,1,NULL)==ROLLTIME && game_flash_length(s,r,1,NULL)==0);nrecords=0;game_redraw(NULL,ds,NULL,r,1,NULL,0,0);assert(nrecords>16 && !strcmp(original_status,"Moves: 1"));struct polygon_record settled[64];int nsettled=nrecords;memcpy(settled,records,sizeof(records));nrecords=0;game_redraw(NULL,ds,s,r,1,NULL,ROLLTIME/2,0);assert(nrecords!=nsettled || memcmp(settled,records,nsettled*sizeof(*records)));
 int visited;char*plan=normal_plan(s,&visited);game_state*won=dup_game(s);for(char*c=plan;*c;c++){char text[2]={*c,0};game_state*t=execute_move(won,text);assert(t);free_game(won);won=t;}assert(game_status(won)==1 && face_count(won)==6 && !floor_count(won));int completed=won->completed;game_state*reward=NULL;for(int i=0;i<4&&!reward;i++){char text[2]={"LRUD"[i],0};reward=execute_move(won,text);}assert(reward && reward->completed==completed && reward->movecount==won->movecount+1 && face_count(reward)==6 && floor_count(reward)==0);
 random_state*rs=random_new("cube-native",11);char*desc=new_game_desc(p,rs,NULL,false);assert(!validate_desc(p,desc));game_state*generated=new_game(NULL,p,desc);assert(face_count(generated)==0 && floor_count(generated)==6 && !GET_SQUARE(generated,generated->current));
 printf("PASS: original 4x4 generation, angular Left input, centre/boundary/Right/release rejection, face permutation and bidirectional floor swap, 0.13s polygon roll, no S, genuine six-face completion (%zu normal moves/%d native nodes), completed reward rolling\n",strlen(plan),visited);
 free_game(generated);sfree(desc);random_free(rs);free_game(reward);free_game(won);sfree(plan);free_game(r3);free_game(r2);free_game(r);game_free_drawstate(NULL,ds);free_game(s);free_params(p);
}
int main(int argc,char**argv){
 if(argc==2&&!strcmp(argv[1],"--self-test")){self_test();return 0;}
 if(argc==3&&!strcmp(argv[1],"--plan")){game_state*s=from_id(argv[2]);int visited;char*plan=normal_plan(s,&visited);printf("{\"plan\":\"%s\",\"native_nodes\":%d,\"ordinary_original_execute_move_only\":true}\n",plan,visited);sfree(plan);free_game(s);return 0;}
 if(argc<3)fatal("Usage: oracle ID tile [tap:x,y right:x,y ...] or --plan ID");game_state*s=from_id(argv[1]);game_drawstate*ds=game_new_drawstate(NULL,s);game_set_size(NULL,ds,&s->params,atoi(argv[2]));char**moves=snewn(argc,char*);int n=0;
 for(int i=3;i<argc;i++){int x,y,button=LEFT_BUTTON;if(sscanf(argv[i],"tap:%d,%d",&x,&y)!=2){if(sscanf(argv[i],"right:%d,%d",&x,&y)!=2)fatal("Bad gesture");button=RIGHT_BUTTON;}char*m=interpret_move(s,NULL,ds,x,y,button);if(actual_move(m)){game_state*t=execute_move(s,m);if(!t)fatal("Original rejected normal move");free_game(s);s=t;moves[n++]=m;}interpret_move(s,NULL,ds,x,y,button==LEFT_BUTTON?LEFT_RELEASE:RIGHT_RELEASE);}
 nrecords=0;game_redraw(NULL,ds,NULL,s,1,NULL,0,0);int w,h;game_compute_size(&s->params,atoi(argv[2]),NULL,&w,&h);int cx=(int)(s->grid->squares[s->current].x*GRID_SCALE)+ds->ox,cy=(int)(s->grid->squares[s->current].y*GRID_SCALE)+ds->oy;int size=(int)ds->gridscale/3;
 printf("{\"width\":%d,\"height\":%d,\"board_size\":[%d,%d],\"current\":%d,\"centre\":[%d,%d],\"status\":%d,\"completed_at\":%d,\"movecount\":%d,\"faces_blue\":%d,\"floor_blue\":%d,\"animation_seconds\":%.2f,\"original_status\":\"%s\",\"faces\":[",s->params.d1,s->params.d2,w,h,s->current,cx,cy,game_status(s),s->completed,s->movecount,face_count(s),floor_count(s),(double)ROLLTIME,original_status);
 for(int i=0;i<s->solid->nfaces;i++)printf("%s%d",i?",":"",s->facecolours[i]);printf("],\"floor\":[");for(int i=0;i<s->grid->nsquares;i++)printf("%s%d",i?",":"",(int)GET_SQUARE(s,i));printf("],\"legal\":[");
 const int dx[]={-1,1,0,0},dy[]={0,0,-1,1};for(int i=0;i<4;i++){int px=cx+dx[i]*size,py=cy+dy[i]*size;assert(px>=0 && px<w && py>=0 && py<h);char*m=interpret_move(s,NULL,ds,px,py,LEFT_BUTTON);bool legal=actual_move(m);int target=s->current;if(legal){assert(m[0]=="LRUD"[i] && !m[1]);game_state*t=execute_move(s,m);assert(t);target=t->current;free_game(t);}printf("%s{\"direction\":\"%c\",\"legal\":%s,\"target\":%d,\"click\":[%d,%d]}",i?",":"","LRUD"[i],legal?"true":"false",target,px,py);if(legal)sfree(m);}printf("],\"polygons\":[");
 for(int i=0;i<nrecords;i++){struct polygon_record*r=&records[i];printf("%s{\"fill\":%d,\"points\":[",i?",":"",r->fill);for(int j=0;j<2*r->n;j++)printf("%s%d",j?",":"",r->p[j]);printf("]}");}printf("],\"moves\":[");for(int i=0;i<n;i++)printf("%s\"%s\"",i?",":"",moves[i]);puts("]}");for(int i=0;i<n;i++)sfree(moves[i]);sfree(moves);game_free_drawstate(NULL,ds);free_game(s);return 0;
}
