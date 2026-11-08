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
  for(int side:{-15,12}){r.rect(x+side,y-24,3,24,Ink);r.rect(x+side+1,y-23,1,22,farm_palette::Wood);}
  for(int yy:{-22,-17}){r.rect(x-17,y+yy,35,4,Ink);r.rect(x-16,y+yy,33,2,farm_palette::Wood);r.rect(x-15,y+yy,31,1,Gold);}
  r.rect(x-18,y-11,37,6,Ink);r.rect(x-17,y-11,35,3,farm_palette::Wood);r.rect(x-16,y-11,33,1,Gold);
  for(int side:{-18,16}){r.rect(x+side,y-14,3,3,Ink);r.rect(x+side,y-14,2,1,Gold);}
}
inline void village_planter(Raster&r,int x,int y,int season,bool flowers) {
  r.rect(x-12,y-9,25,9,Ink);r.rect(x-11,y-8,23,7,farm_palette::Wood);
  for(int i=-9;i<11;i+=5){r.rect(x+i,y-7,1,6,farm_palette::WoodDark);r.rect(x+i+1,y-7,1,5,Gold);}
  r.rect(x-13,y-10,27,3,Ink);r.rect(x-12,y-10,25,1,Gold);
  for(int i=-8;i<=8;i+=8) {
    r.line(x+i,y-10,x+i,y-18,Green);r.rect(x+i-3,y-14,3,2,Leaf);r.rect(x+i+1,y-16,3,2,Green);
    if(flowers){uint16_t colors[]={rgb(213,146,144),rgb(229,191,95),rgb(194,117,61),Cream};r.rect(x+i-2,y-20,5,3,colors[season%4]);r.rect(x+i-1,y-21,3,5,colors[season%4]);r.rect(x+i,y-19,1,1,Gold);}
  }
}
