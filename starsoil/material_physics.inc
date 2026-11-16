// SPDX-License-Identifier: MIT
// Included in core.cpp's private namespace. No heap, global mutable state or
// gameplay RNG consumption; resting chunks sleep until a nearby edit/move.
void wake_materials(State &s, int x, int y) {
  for(int cy=std::max(0,y-1)/16;cy<=std::min(int(s.height)-1,y+1)/16;++cy)
    for(int cx=std::max(0,x-8)/16;cx<=std::min(int(s.width)-1,x+8)/16;++cx)
      s.materials.active[cy] |= uint64_t(1)<<cx;
}
bool material_visited(const MaterialMotion &m,uint32_t at) {
  const uint32_t key=at+1;
  unsigned slot=((key^(key>>8))*2654435761u)>>24;
  while(m.moved[slot]) {
    if(m.moved[slot]==key)return true;
    slot=(slot+1)&255;
  }
  return false;
}
void visit_material(MaterialMotion &m,uint32_t at) {
  const uint32_t key=at+1;
  unsigned slot=((key^(key>>8))*2654435761u)>>24;
  while(m.moved[slot] && m.moved[slot]!=key)slot=(slot+1)&255;
  m.moved[slot]=key;
}
bool liquid_tile(int t) {return t==Water || t==Lava;}
bool transfer_material(State &s,int x,int y,int xx,int yy,bool &retry) {
  if(!inside(s,xx,yy))return false;
  auto &m=s.materials;
  const int from=index(x,y),to=index(xx,yy),t=s.tiles[from],under=s.tiles[to];
  if(under!=Air && !(t==Sand && liquid_tile(under)))return false;
  if(material_visited(m,uint32_t(to)) || (t==Sand && occupied(s,xx,yy))) {
    retry=true;return false;
  }
  s.tiles[to]=uint8_t(t);s.tiles[from]=uint8_t(under);
  visit_material(m,uint32_t(to));
  if(under!=Air)visit_material(m,uint32_t(from));
  ++m.moves;++m.total_moves;s.dirty=true;
  wake_materials(s,x,y);wake_materials(s,xx,yy);
  return true;
}
int liquid_descent(const State &s,int x,int y,int dx,int t) {
  for(int d=1;d<=8;++d) {
    int xx=x+dx*d;
    if(!inside(s,xx,y))break;
    int at=tile(s,xx,y);
    if(at!=Air && at!=t)break;
    int below=tile(s,xx,y+1);
    if(below==Air || (below==Platform && tile(s,xx,y+2)==Air))return d;
  }
  return 99;
}
void simulate_material_cell(State &s,int x,int y,bool &retry) {
  auto &m=s.materials;const int t=tile(s,x,y);
  if(t!=Sand && !liquid_tile(t))return;
  if(t==Lava && m.step%3) {retry=true;return;}
  if(material_visited(m,uint32_t(index(x,y)))) {retry=true;return;}
  if(transfer_material(s,x,y,x,y+1,retry))return;
  if(liquid_tile(t) && tile(s,x,y+1)==Platform &&
     transfer_material(s,x,y,x,y+2,retry))return;
  const int first=(hash(uint32_t(index(x,y))^m.step)&1)?1:-1;
  for(int dx:{first,-first}) {
    const int side=tile(s,x+dx,y);
    if(side!=Air && !(t==Sand && liquid_tile(side)))continue;
    if(transfer_material(s,x,y,x+dx,y+1,retry))return;
  }
  if(t==Sand)return;
  // A cell seeks a downhill outlet or relieves a column above it. Isolated
  // shallow pools rest instead of sliding endlessly left/right on level rock.
  int best=0,distance=100;
  const bool pressure=tile(s,x,y-1)==t;
  for(int dx:{first,-first}) {
    if(tile(s,x+dx,y)!=Air)continue;
    int d=liquid_descent(s,x,y,dx,t);
    if((d<=8 || pressure) && d<distance) {best=dx;distance=d;}
  }
  if(best)transfer_material(s,x,y,x+best,y,retry);
}
void process_material_chunk(State &s,int chunk) {
  auto &m=s.materials;const int cx=chunk%64,cy=chunk/64;
  m.active[cy]&=~(uint64_t(1)<<cx);
  bool retry=false;
  for(int y=std::min(int(s.height)-2,cy*16+15);y>=std::max(1,cy*16);--y)
    for(int i=0;i<16;++i) {
      const int x=cx*16+((m.step&1)?15-i:i);
      if(!inside(s,x,y))continue;
      if(m.moves>=64) {retry=true;break;}
      ++m.processed_cells;simulate_material_cell(s,x,y,retry);
    }
  if(retry)m.active[cy]|=uint64_t(1)<<cx;
}
int next_material_chunk(MaterialMotion &m) {
  const int row=m.world_cursor/64,col=m.world_cursor%64;
  for(int i=0;i<16;++i) {
    const int y=(row+i)%16;
    uint64_t bits=m.active[y];
    if(i==0 && col)bits&=~((uint64_t(1)<<col)-1);
    if(bits) {
      int result=y*64+__builtin_ctzll(bits);
      m.world_cursor=uint16_t((result+1)%1024);return result;
    }
  }
  if(col) {
    uint64_t bits=m.active[row]&((uint64_t(1)<<col)-1);
    if(bits) {int result=row*64+__builtin_ctzll(bits);m.world_cursor=uint16_t((result+1)%1024);return result;}
  }
  return -1;
}
void material_step(State &s) {
  auto &m=s.materials;
  ++m.step;m.moves=m.processed_cells=0;std::fill(std::begin(m.moved),std::end(m.moved),0u);
  int processed[4]={-1,-1,-1,-1},count=0;
  auto already=[&](int chunk) {for(int i=0;i<count;++i)if(processed[i]==chunk)return true;return false;};
  const int pcx=std::clamp(ix(s.player.x),0,int(s.width)-1)/16;
  const int pcy=std::clamp(ix(s.player.y),0,int(s.height)-1)/16;
  for(int pass=0;pass<3;++pass) {
    for(int scan=0;scan<25;++scan) {
      int at=m.local_cursor;m.local_cursor=uint16_t((at+1)%25);
      int cx=pcx+at%5-2,cy=pcy+at/5-2;
      if(cx<0 || cx>=64 || cy<0 || cy>=16)continue;
      int chunk=cy*64+cx;
      if(!(m.active[cy]&(uint64_t(1)<<cx)) || already(chunk))continue;
      processed[count++]=chunk;process_material_chunk(s,chunk);break;
    }
  }
  for(int tries=0;tries<4;++tries) {
    int chunk=next_material_chunk(m);
    if(chunk<0)break;
    if(already(chunk))continue;
    processed[count++]=chunk;process_material_chunk(s,chunk);break;
  }
}
