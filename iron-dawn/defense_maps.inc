// SPDX-License-Identifier: MIT
 void init_river_defense(){
  // Reproduce the starter deposits used by the published defense seeds.
  for (int y=0;y<side;++y) for (int x=0;x<side;++x) {
   random();
   bool mine=(x>=3&&x<=6&&y>=13&&y<=16)||(x>=19&&x<=22&&y>=6&&y<=9)||(x>=7&&x<=10&&y>=21&&y<=23);
   if(mine) ore[y*N+x]=180+random()%76;
  }
  for(int y=20;y<=21;++y)for(int x=16;x<=17;++x)ore[y*N+x]=160+unsigned(x*19+y*11)%65;
  deploy(0, {{hq,{5.5f,8.5f}}, {power,{3.5f,7}}, {refinery,{4,11}}, {barracks,{8,8}},
             {harvester,{5,12.5f}}, {tank,{8.5f,11}}, {tank,{9.5f,12}}, {rifle,{8,13}}, {rocket,{10.5f,11.5f}}});
  rng = extra.seed;
  for (int y=1;y<side-1;++y) for (int x=1;x<side-1;++x) {
   int c=y*N+x;
   terrain[c]=ore[c]?ground:(random()%13==0?woodland:ground);
  }
  for(int y=1;y<side-1;++y)if(!defense_crossing(y))for(int x=17;x<=18;++x){terrain[y*N+x]=water;ore[y*N+x]=0;}
  add(factory,0,{7,5});add(power,0,{3,4});add(turret,0,{10,9});
  add(supply,2,{13,8});add(supply,2,{9,18});add(supply,2,{19,16});
  for(int y=18;y<21;++y)for(int x=13;x<16;++x)ore[y*N+x]=200+random()%55;
  build_defense_landscape();
 }

 // Terrain is stored in the existing byte grid; published save prefixes stay intact.
 void build_defense_landscape(){
  for(int y=2;y<side-2;++y){auto crossing=[&](int row){return defense_crossing(row);};if(crossing(y-1)||crossing(y)||crossing(y+1))continue;for(int side=0;side<2;++side){int x=side?19:16,c=y*N+x;unsigned hash=x*97+y*53+extra.seed;if(hash%4!=0||ore[c]||terrain[c]!=ground)continue;bool occupied=false;for(auto& a:e)if(a.hp>0&&dist(a.p,center(c))<2.5f){occupied=true;break;}if(!occupied)terrain[c]=water;}}
  auto rocks=[&](int left,int top,int width,int height){for(int y=top;y<top+height;++y)for(int x=left;x<left+width;++x){int c=y*N+x;if(terrain[c]==water||ore[c])continue;bool occupied=false;for(auto& a:e)if(a.hp>0&&dist(a.p,center(c))<2.3f){occupied=true;break;}if(!occupied)terrain[c]=ridge;}};
  rocks(13,3,2,3);rocks(12,12,2,3);rocks(20,18,2,3);
  auto paving=[&](int x,int y){if(x<=0||y<=0||x>=side-1||y>=side-1)return;int c=y*N+x;if(!ore[c]&&terrain[c]!=water)terrain[c]=road;};
  for(int x=2;x<=25;++x)for(int y=9;y<=10;++y)paving(x,y);for(int y=3;y<=25;++y)for(int x=10;x<=11;++x)paving(x,y);
  for(int x=8;x<=24;++x)for(int y=17;y<=18;++y)paving(x,y);for(int y=9;y<=23;++y)for(int x=20;x<=21;++x)paving(x,y);
  // Soft woodland patches leave roads, mine cells and building approaches clear.
  for(Vec patch:{Vec{9,16},Vec{16,6},Vec{23,13},Vec{8,21}})for(int y=int(patch.y)-2;y<=int(patch.y)+2;++y)for(int x=int(patch.x)-2;x<=int(patch.x)+2;++x){int c=y*N+x;Vec p=center(c);if(terrain[c]!=ground||ore[c]||dist(p,patch)>2.2f)continue;bool occupied=false;for(auto& a:e)if(a.hp>0&&building(a.kind)&&dist(a.p,p)<2.4f){occupied=true;break;}if(!occupied)terrain[c]=woodland;}
  for(int y=1;y<side-1;++y)if(defense_crossing(y))for(int x=17;x<=18;++x)terrain[y*N+x]=bridge,ore[y*N+x]=0;
  // Forward deposits reward opening a second mine without replacing the safe starter ore.
  for(int y=20;y<=21;++y)for(int x=16;x<=17;++x){int c=y*N+x;if(terrain[c]==ground||terrain[c]==woodland)ore[c]=160+unsigned(x*19+y*11+chapter*23)%65;}
 }

 void init_reef_defense(){
  rng=extra.seed;
  patch(ground,1,1,16,side-2);
  patch(ground,32,1,15,side-2);
  landform(woodland,8,19+int(extra.seed%4),5,6);
  landform(ridge,13,27,3,5);
  landform(woodland,43,37,3,6);
  landform(crater,35,10,3,4);
  landform(ground,24,7,3,2);
  landform(ground,24,40,3,2);
  route({{10,12},{36,12},{36,36},{10,36}},true);
  route({{12,9},{12,38}});
  route({{36,24},{42,24},{42,31}});
  deposit(42,28);deposit(41,36);deposit(8,28);
  deploy(0, {{hq,{39,24}}, {power,{44,19}}, {power,{44,24}}, {power,{41,15}},
             {refinery,{42,26}}, {barracks,{41,20}}, {factory,{38,18}}, {harvester,{43.5f,28.5f}},
             {tank,{35,23}}, {tank,{35,27}}, {rifle,{34,24}}, {rocket,{35,25}},
             {turret,{34.5f,19.5f}}, {shipyard,{32,20}}, {patrol_boat,{30.5f,22.5f}},
             {landing_craft,{30.5f,18.5f}}, {helipad,{39,29}}});
  deploy(2, {{supply,{33.5f,12.5f}}, {supply,{33.5f,35.5f}}, {supply,{13.5f,31.5f}}});
 }

 void init_ridge_defense(){
  rng=extra.seed;
  patch(ground,1,1,side-2,side-2);
  landform(ridge,14,21,5,11);landform(ridge,31,19,5,11);
  landform(ridge,21,13,2,4);landform(ridge,28,8,3,4);
  landform(woodland,8,27+int(extra.seed%4),5,6);
  landform(woodland,38,28,4,7);landform(crater,24,25,5,4);
  route({{6,8},{6,31},{21,31},{21,40}});
  route({{22,6},{25,6},{25,32},{23,32},{23,40}});
  route({{38,10},{38,31},{27,31},{27,38}});
  deposit(30,38);deposit(17,35);deposit(9,18);
  deploy(0, {{hq,{23,38}}, {power,{19,40}}, {power,{25,41}}, {refinery,{29,37}},
             {barracks,{27,39}}, {factory,{19,38}}, {harvester,{31.5f,39.5f}},
             {tank,{22,34}}, {tank,{25,34}}, {artillery,{24,35}}, {rifle,{26,35}},
             {rocket,{24,33.5f}}, {turret,{23.5f,32.5f}}});
  deploy(2, {{supply,{11.5f,31.5f}}, {supply,{36.5f,31.5f}}, {supply,{24.5f,23.5f}}});
 }

 bool naval_raid(int next)const{
  return extra.mode&&defense_map()==DefenseMap::reef&&next>=2&&next%3==2;
 }

 Vec naval_entry(int next)const{
  return {28.5f,next%2?43.5f:5.5f};
 }

 Vec naval_objective(Vec base)const{
  int dock=nearest(base,shipyard,0);
  if(dock>=0){
   Vec approach=shore_exit(e[dock].p);
   if(open_for(patrol_boat,approach))return approach;
  }
  return {30.5f,24.5f};
 }
