// SPDX-License-Identifier: MIT
// Included inside art: ground anchors follow the same one-row contact rule.
inline void village_board(Raster&r,int x,int y) {
  for(int side:{-18,16}){r.rect(x+side,y-30,3,30,Ink);r.rect(x+side+1,y-28,1,27,farm_palette::Wood);}
  r.rect(x-23,y-38,47,28,Ink);r.rect(x-21,y-36,43,24,farm_palette::Wood);
  r.rect(x-21,y-36,43,2,Gold);r.rect(x-22,y-13,45,2,farm_palette::WoodDark);
  r.rect(x-16,y-32,14,17,Ink);r.rect(x-15,y-31,12,15,Cream);r.rect(x-15,y-31,12,3,rgb(161,94,60));
  for(int yy=0;yy<3;++yy)for(int xx=0;xx<3;++xx)r.rect(x-13+xx*3,y-26+yy*3,2,2,yy==1&&xx==1?Leaf:farm_palette::Wood);
  for(int i=0;i<2;++i){int xx=x+2+i*9,yy=y-32+i*3;r.rect(xx,yy,9,15,farm_palette::WoodDark);r.rect(xx+1,yy+1,7,12,Cream);r.rect(xx+3,yy+4,4,1,farm_palette::Wood);r.rect(xx+2,yy+7,4,1,farm_palette::Wood);}
  for(int row=0;row<7;++row){int half=22+row;r.rect(x-half,y-44+row,half*2+1,1,Ink);r.rect(x-half+1,y-44+row,half*2-1,1,row%3?rgb(171,106,57):Gold);}
  r.rect(x-28,y-38,57,2,farm_palette::WoodDark);
}
inline void village_bench(Raster&r,int x,int y) {
  furniture(r,furniture_sprites::Bench,x,y);
}
inline void village_planter(Raster&r,int x,int y,int season,bool flowers) {
  furniture(r,furniture_sprites::Planter,x,y,(flowers?0:4)+(season&3));
}
