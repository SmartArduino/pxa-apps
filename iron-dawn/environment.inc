// SPDX-License-Identifier: MIT
// Battlefield members. Small fixed atlases; the runtime never decodes PNGs.
 void environment_prop(Raster& r,const World& w,Vec p,unsigned id,float scale=1){auto q=project(p);ShadowArt::base(r,art::environment_first+id,std::lround(q.x),std::lround(q.y),zoom*scale);Sprites::anchor(r,art::environment_first+id,std::lround(q.x),std::lround(q.y),zoom*scale,w.fog[cell(p)]?0:4);}
 bool prop_clear(const World& w,Vec p,float radius=1.8f)const{for(auto& a:w.e)if(a.hp>0&&w.building(a.kind)){float dx=a.p.x-p.x,dy=a.p.y-p.y;if(dx*dx+dy*dy<radius*radius)return false;}return true;}
 [[gnu::noinline]] void shore_details(Raster& r,const World& w,Vec p,unsigned hash){
  int x=int(p.x),y=int(p.y);bool lit=w.fog[cell(p)];auto stone=lit?rgb(112,119,94):rgb(53,70,60),foam=lit?rgb(93,135,125):rgb(40,72,70);
  for(int side=0;side<4;++side){int xx=x+(side==0)-(side==1),yy=y+(side==2)-(side==3);if(xx<0||yy<0||xx>=w.side||yy>=w.side)continue;int neighbor=yy*N+xx;if(w.terrain[neighbor]==water||w.terrain[neighbor]==bridge||!w.seen[neighbor])continue;
   Vec a,b,inside_a,inside_b,reeds=p;
   if(side<2){float edge=side==0?.5f:-.5f,inset=side==0?.35f:-.35f;a={p.x+edge,p.y-.5f};b={p.x+edge,p.y+.5f};inside_a={p.x+inset,p.y-.5f};inside_b={p.x+inset,p.y+.5f};reeds.x+=inset;}
   else {float edge=side==2?.5f:-.5f,inset=side==2?.35f:-.35f;a={p.x-.5f,p.y+edge};b={p.x+.5f,p.y+edge};inside_a={p.x-.5f,p.y+inset};inside_b={p.x+.5f,p.y+inset};reeds.y+=inset;}
   r.poly({project(a),project(b),project(inside_b),project(inside_a)},stone);wire(r,inside_a,inside_b,0,foam);
   if((hash+side)%3==0)environment_prop(r,w,reeds,5,.48f);
  }
 }
 [[gnu::noinline]] void land_details(Raster& r,const World& w,Vec p,unsigned hash){
  int c=cell(p);auto type=w.terrain[c];bool lit=w.fog[c];auto q=project(p);
  if(type==water){shore_details(r,w,p,hash);return;}
  if(w.ore[c]){environment_prop(r,w,p,7,.68f+(hash%2)*.06f);return;}
  if(type==ridge){environment_prop(r,w,p,2+(hash&1),.78f);return;}
  if(type==bridge)return;
  if(type==road){
   auto shoulder=lit?rgb(109,109,77):rgb(50,64,48);int x=int(p.x),y=int(p.y);
   for(int side=0;side<4;++side){int xx=x+(side==0)-(side==1),yy=y+(side==2)-(side==3);if(xx<0||yy<0||xx>=w.side||yy>=w.side||w.paved(center(yy*N+xx)))continue;if(side<2){float dx=side==0?.47f:-.47f;wire(r,{p.x+dx,p.y-.5f},{p.x+dx,p.y+.5f},0,shoulder);}else{float dy=side==2?.47f:-.47f;wire(r,{p.x-.5f,p.y+dy},{p.x+.5f,p.y+dy},0,shoulder);}}
   if(hash%7==0){wire(r,{p.x-.3f,p.y+.2f},{p.x-.02f,p.y+.1f},0,lit?rgb(27,42,39):rgb(20,33,31));wire(r,{p.x-.02f,p.y+.1f},{p.x+.06f,p.y-.18f},0,lit?rgb(27,42,39):rgb(20,33,31));}return;
  }
  if(type==crater||type==ruins){r.ellipse(q.x,q.y,12*zoom,5*zoom,lit?rgb(36,43,34):rgb(23,34,30));r.ring(q.x,q.y,13*zoom,6*zoom,lit?rgb(91,88,65):rgb(43,55,43));r.ellipse(q.x+2*zoom,q.y,7*zoom,3*zoom,lit?rgb(25,33,29):rgb(19,29,27));if(type==ruins)environment_prop(r,w,p,4,.52f);return;}
  if(type==woodland&&hash%3==0)environment_prop(r,w,{p.x+.13f,p.y+.12f},6,.44f);
  if(type==iron::ground||type==woodland){auto grass=lit?rgb(115,125,76):rgb(47,65,45);for(int tuft=0;tuft<(type==woodland?3:1);++tuft){int xx=int(q.x)+(int((hash>>(tuft*3))%19)-9)*zoom,yy=int(q.y)+(int((hash>>(tuft*2))%7)-3)*zoom;r.line(xx,yy,xx-zoom,yy-2*zoom,grass);r.line(xx,yy,xx+2*zoom,yy-zoom,grass);}}
 }
 void tall_environment(Raster& r,const World& w,Vec p,unsigned hash){
  auto type=w.terrain[cell(p)];if(w.ore[cell(p)]||type==water||type>=road)return;
  bool tree=(type==woodland&&hash%3==0)||(type==iron::ground&&int(p.x)>12&&hash%71==0);
  if(!tree&&!(type==iron::ground&&(hash%43==0||hash%67==0)))return;if(!prop_clear(w,p))return;
  if(tree)environment_prop(r,w,p,hash&1,.78f+(hash%3)*.06f);
  else if(type==iron::ground&&hash%43==0)environment_prop(r,w,p,2,.4f);
  else if(type==iron::ground&&hash%67==0)environment_prop(r,w,p,4,.44f);
 }
