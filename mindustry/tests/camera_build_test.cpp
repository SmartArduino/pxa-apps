// SPDX-License-Identifier: GPL-3.0-only
#define MINDUSTRY_UI_SUPPORT_ONLY
#include "ui_test.cpp"
int main(int argc,char **argv){
 CHECK(gfx::attach(1,PXA_RASTER_CAP_TEXTURED_QUAD|PXA_RASTER_CAP_PAINTER_POLYGON|PXA_RASTER_CAP_AFFINE_UV|PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA));
 pocket::WIDTH=pocket::HEIGHT=480;milky::screen={480,480,{0,0,480,480}};
 s.generate(154,0,false);s.researched=(1u<<Blocks)-1;for(auto &v:s.stock)v=4000;
 resetUI();view=World;hasSave=true;
 cameraX=24.5f;cameraY=35.f;
 int factory=s.queueBuild(GroundFactory,24,3);CHECK(factory>=0);
 if(argc>1){
  std::array<uint8_t,3900> bytes{};auto n=s.save(bytes.data(),bytes.size());CHECK(n);
  auto *file=std::fopen(argv[1],"wb");CHECK(file);CHECK(std::fwrite(bytes.data(),1,n,file)==n);std::fclose(file);return 0;
 }
 game::tick(50);CHECK(s.buildings[factory].buildState==1);
 auto before=s.playerY;cameraX=25.5f;cameraY=4.5f;
 game::tick(50);CHECK(s.playerY<before && s.buildings[factory].buildState==1);
 for(int i=0;i<20;++i)game::tick(50);
 CHECK(s.builder==factory && s.buildings[factory].buildState==2);
 CHECK(s.buildings[factory].progress>0 && s.buildings[factory].progress<BuildComplete);
 CHECK(std::hypot(s.playerX-25.5f,s.playerY-4.5f)<10);
 std::array<uint8_t,3900> bytes{};auto n=s.save(bytes.data(),bytes.size());State saved;CHECK(saved.load(bytes.data(),n));
 CHECK(saved.buildCount==1 && saved.buildings[saved.buildQueue[0]].buildState==2);
 view=Paused;before=s.playerY;game::tick(1000);CHECK(s.playerY==before);
 view=World;for(int i=0;i<130;++i)game::tick(50);
 CHECK(!s.buildings[factory].buildState && !s.buildCount);
 CHECK(std::hypot(s.playerX-25.5f,s.playerY-4.5f)<=ViewAttractRange+.01f);
 // Camera attraction stops within the original 15-world-unit radius.
 auto x=s.playerX,y=s.playerY;for(int i=0;i<40;++i)game::tick(50);CHECK(s.playerX==x && s.playerY==y);
 cameraX=40;cameraY=15;for(int i=0;i<40;++i)game::tick(50);CHECK(s.playerX>35 && s.playerY>10);
 // Global infinite-resource sandbox deliberately keeps the upstream instant,
 // unlimited-range rule, rather than inheriting survival construction delays.
 s.generate(154,0,true);auto stock=s.stock[Copper];factory=s.queueBuild(GroundFactory,24,3);CHECK(factory>=0);s.step();
 CHECK(!s.buildings[factory].buildState && s.stock[Copper]==stock);
 gfx::detach();std::printf("%d camera/builder checks passed: follow, attraction, pause, range, gradual survival construction, save and original sandbox\n",checks);
}
