// SPDX-License-Identifier: MIT
#include "thinking.hpp"
#include <algorithm>
#include <charconv>
#include <numeric>
namespace sibo::thinking {
uint32_t next(uint32_t &seed) {
  if(!seed)seed=0x715ab13;
  seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;
}
bool legal(const Grid &g,unsigned cell,unsigned value) {
  if(cell>=16 || !value || value>4)return false;
  for(unsigned i=0;i<16;++i) {
    if(i==cell || g[i]!=value)continue;
    if(i/4==cell/4 || i%4==cell%4 || (i/8==cell/8 && i%4/2==cell%4/2))return false;
  }
  return true;
}
static unsigned search(Grid &g,Grid *answer) {
  unsigned cell=16,least=5;
  for(unsigned i=0;i<16;++i)if(!g[i]) {
    unsigned count=0;for(unsigned n=1;n<=4;++n)count+=legal(g,i,n);
    if(!count)return 0;
    if(count<least){least=count;cell=i;}
  }
  if(cell==16){if(answer)*answer=g;return 1;}
  unsigned count=0;
  for(unsigned n=1;n<=4 && count<2;++n)if(legal(g,cell,n)) {
    g[cell]=uint8_t(n);count+=search(g,answer);g[cell]=0;
  }
  return std::min(2u,count);
}
unsigned solutions(Grid g,Grid *answer) {
  for(unsigned i=0;i<16;++i)if(g[i] && !legal(g,i,g[i]))return 0;
  return search(g,answer);
}
void Sudoku::generate(uint32_t &seed) {
  std::array<uint8_t,4> digits{1,2,3,4};
  for(unsigned i=3;i>0;--i)std::swap(digits[i],digits[next(seed)%(i+1)]);
  std::array<unsigned,4> rows{0,1,2,3},cols{0,1,2,3};
  for(auto *order:{&rows,&cols}) {
    if(next(seed)&1){std::swap((*order)[0],(*order)[2]);std::swap((*order)[1],(*order)[3]);}
    if(next(seed)&1)std::swap((*order)[0],(*order)[1]);
    if(next(seed)&1)std::swap((*order)[2],(*order)[3]);
  }
  for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
    given[r*4+c]=digits[(rows[r]*2+rows[r]/2+cols[c])%4];
  std::array<unsigned,16> order{};std::iota(order.begin(),order.end(),0);
  for(unsigned i=15;i>0;--i)std::swap(order[i],order[next(seed)%(i+1)]);
  unsigned removed=0;
  for(auto i:order) {
    auto old=given[i];given[i]=0;
    if(solutions(given)!=1)given[i]=old;else if(++removed==9)break;
  }
  cells=given;hints=0;credited=false;
}
bool Sudoku::conflict(unsigned i) const {return i<16 && cells[i] && !legal(cells,i,cells[i]);}
bool Sudoku::complete() const {
  for(unsigned i=0;i<16;++i)if(!cells[i] || conflict(i))return false;
  return true;
}
bool Sudoku::fill(unsigned i,unsigned value) {
  if(i>=16 || given[i] || value>4 || cells[i]==value)return false;
  cells[i]=uint8_t(value);return true;
}
bool Sudoku::hint(unsigned i) {
  Grid answer{};
  if(i>=16 || given[i] || solutions(given,&answer)!=1 || cells[i]==answer[i])return false;
  cells[i]=answer[i];hints=std::min(1000u,hints+1);return true;
}
Rational::Rational(int64_t a,int64_t b):n(a),d(b) {
  if(!d){n=0;d=1;return;}
  if(d<0){n=-n;d=-d;}
  const auto divisor=std::gcd(n,d);n/=divisor;d/=divisor;
}
std::string Rational::text() const {return std::to_string(n)+(d==1?"":"/"+std::to_string(d));}
Board board(const std::array<uint8_t,4> &values) {
  Board b;for(unsigned i=0;i<4;++i)b.cards[i]={Rational(values[i]),std::to_string(values[i])};return b;
}
bool Board::apply(Move m) {
  if(count<2 || m.a>=count || m.b>=count || m.a==m.b || unsigned(m.op)>3)return false;
  const auto a=cards[m.a].value,b=cards[m.b].value;
  // Public model API also rejects out-of-domain caller-supplied fractions.
  const auto bounded=[](Rational x){return x.n>=-1000000 && x.n<=1000000 && x.d>0 && x.d<=1000000;};
  if(!bounded(a)||!bounded(b))return false;
  Rational result;
  switch(m.op) {
    case Op::add:result=Rational(a.n*b.d+b.n*a.d,a.d*b.d);break;
    case Op::sub:result=Rational(a.n*b.d-b.n*a.d,a.d*b.d);break;
    case Op::mul:result=Rational(a.n*b.n,a.d*b.d);break;
    case Op::div:if(!b.n)return false;result=Rational(a.n*b.d,a.d*b.n);break;
  }
  if(!bounded(result))return false;
  const char *symbols[]={"+","−","×","÷"};
  Card merged{result,"("+cards[m.a].expression+symbols[unsigned(m.op)]+cards[m.b].expression+")"};
  std::array<Card,4> output{};unsigned used=0;
  for(unsigned i=0;i<count;++i)if(i!=m.a && i!=m.b)output[used++]=cards[i];
  output[used++]=std::move(merged);cards=std::move(output);count=used;return true;
}
static bool solve_impl(const Board &b,std::array<Move,3> &moves,unsigned depth,unsigned &budget) {
  if(!budget)return false;
  --budget;
  if(b.count==1)return b.won();
  for(unsigned i=0;i<b.count;++i)for(unsigned j=i+1;j<b.count;++j) {
    const Move choices[]={{i,j,Op::add},{i,j,Op::mul},{i,j,Op::sub},{j,i,Op::sub},{i,j,Op::div},{j,i,Op::div}};
    for(auto m:choices) {
      auto trial=b;
      if(trial.apply(m) && solve_impl(trial,moves,depth+1,budget)){moves[depth]=m;return true;}
      if(!budget)return false;
    }
  }
  return false;
}
bool solve(Board b,std::array<Move,3> &moves) {
  if(b.count<1 || b.count>4)return false;
  unsigned budget=20000;return solve_impl(b,moves,0,budget);
}
Board TwentyFour::current() const {
  auto result=board(original);
  for(unsigned i=0;i<std::min(3u,depth);++i)if(!result.apply(moves[i]))break;
  return result;
}
void TwentyFour::generate(uint32_t &seed) {
  std::array<Move,3> answer{};
  for(unsigned tries=0;tries<32;++tries) {
    for(auto &n:original)n=uint8_t(1+next(seed)%9);
    if(solve(board(original),answer))break;
    if(tries==31)original={3,3,8,8};
  }
  moves={};depth=0;hinted=false;credited=false;
}
bool TwentyFour::apply(Move move) {
  if(depth>=3)return false;
  auto b=current();if(!b.apply(move))return false;
  moves[depth++]=move;return true;
}
bool TwentyFour::undo(){if(!depth)return false;--depth;return true;}
int Fractions::relation() const {auto a=n*rd,b=rn*d;return a<b?-1:a>b?1:0;}
void Fractions::question(uint32_t &seed) {
  d=2+next(seed)%11;rd=2+next(seed)%11;n=1+next(seed)%d;rn=1+next(seed)%rd;answered=false;
}
bool Fractions::answer(int comparison) {
  if(answered || done>=5 || comparison< -1 || comparison>1)return false;
  answered=true;++done;if(comparison==relation())++score;return true;
}
void State::credit_sudoku() {
  if(!sudoku.credited && sudoku.complete()) {
    sudoku.credited=true;auto &count=sudoku.hints?sudoku_help:sudoku_alone;count=std::min(1000000u,count+1);
  }
}
void State::credit_cards() {
  if(!cards.credited && cards.current().won()) {
    cards.credited=true;auto &count=cards.hinted?cards_help:cards_alone;count=std::min(1000000u,count+1);
  }
}
std::string State::encode() const {
  std::string out="1";
  const auto put=[&](uint32_t x){out+=' ';out+=std::to_string(x);};
  put(seed);for(auto n:sudoku.given)put(n);for(auto n:sudoku.cells)put(n);
  put(sudoku.hints);put(sudoku.credited);for(auto n:cards.original)put(n);
  put(cards.depth);for(unsigned i=0;i<cards.depth;++i){put(cards.moves[i].a);put(cards.moves[i].b);put(unsigned(cards.moves[i].op));}
  put(cards.hinted);put(cards.credited);
  put(fractions.n);put(fractions.d);put(fractions.rn);put(fractions.rd);put(fractions.done);put(fractions.score);put(fractions.answered);
  put(sudoku_alone);put(sudoku_help);put(cards_alone);put(cards_help);return out;
}
bool State::decode(std::string_view bytes) {
  if(bytes.empty() || bytes.size()>512)return false;
  State parsed;
  auto get=[&](unsigned limit,uint32_t &value){
    if(bytes.empty())return false;
    auto result=std::from_chars(bytes.data(),bytes.data()+bytes.size(),value);
    if(result.ec!=std::errc{} || value>limit)return false;
    bytes.remove_prefix(size_t(result.ptr-bytes.data()));
    if(!bytes.empty()){if(bytes.front()!=' ')return false;bytes.remove_prefix(1);}
    return true;
  };
  uint32_t n=0;
  if(!get(1,n)||n!=1||!get(UINT32_MAX,parsed.seed))return false;
  for(auto &v:parsed.sudoku.given){if(!get(4,n))return false;v=uint8_t(n);}
  for(auto &v:parsed.sudoku.cells){if(!get(4,n))return false;v=uint8_t(n);}
  if(!get(1000,parsed.sudoku.hints)||!get(1,n))return false;parsed.sudoku.credited=n;
  for(auto &v:parsed.cards.original){if(!get(9,n))return false;v=uint8_t(n);}
  if(!get(3,parsed.cards.depth))return false;
  auto b=board(parsed.cards.original);
  for(unsigned i=0;i<parsed.cards.depth;++i) {
    auto &m=parsed.cards.moves[i];
    if(!get(3,m.a)||!get(3,m.b)||!get(3,n))return false;m.op=Op(n);
    if(!b.apply(m))return false;
  }
  if(!get(1,n))return false;parsed.cards.hinted=n;
  if(!get(1,n))return false;parsed.cards.credited=n;
  auto &f=parsed.fractions;
  if(!get(12,f.n)||!get(12,f.d)||!get(12,f.rn)||!get(12,f.rd)||!get(5,f.done)||!get(5,f.score)||!get(1,n))return false;f.answered=n;
  if(f.d<2||f.rd<2||f.n>f.d||f.rn>f.rd||f.score>f.done||(!f.answered&&f.done==5)||(f.answered&&!f.done))return false;
  if(!get(1000000,parsed.sudoku_alone)||!get(1000000,parsed.sudoku_help)||!get(1000000,parsed.cards_alone)||!get(1000000,parsed.cards_help)||!bytes.empty())return false;
  auto &s=parsed.sudoku;
  bool started=std::any_of(s.given.begin(),s.given.end(),[](auto x){return x!=0;});
  if(!started && (s.hints || s.credited))return false;
  if(started && solutions(s.given)!=1)return false;
  for(unsigned i=0;i<16;++i)if((s.given[i] && s.cells[i]!=s.given[i])||(!started && s.cells[i]))return false;
  const auto nonzero=std::count_if(parsed.cards.original.begin(),parsed.cards.original.end(),[](auto x){return x!=0;});
  if(nonzero!=0 && nonzero!=4)return false;
  if(!nonzero && (parsed.cards.depth||parsed.cards.hinted||parsed.cards.credited))return false;
  *this=std::move(parsed);return true;
}
} // namespace sibo::thinking
